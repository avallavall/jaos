#!/bin/bash
# Why did D138/D139 cost netlib its warm ratio (0.0696 -> 0.2553)?
#
# The committed warm.txt predates D138/D139. Re-measured at HEAD (item 4 of
# TODO's ordered list), a dozen-plus instances that warm-started in 0..6
# iterations now run warm == cold, and fffff800 gained its warm start.
#
# The hypothesis, from numerics-reviewer's reading of the guard candidate:
# build_warm_basis counts the MAPPED basis on the reduced model
# (src/simplex.c:923), and jm_presolve_run's mapping drops every stored-basic
# member whose row/column presolve removes (src/presolve.c:1651-1659).
# Pre-D138/D139 the published basis was wrong in ORIG space in a way the
# mapping cancelled: the extra BASIC members sat exactly on removed rows and
# columns, so the mapped count came out at rrow and the warm start ran.
# Post-D138/D139 the published basis is right in orig space — the swap put
# surviving rows' logicals AT bounds — and the mapping now comes out short,
# so build_warm_basis falls back cold.
#
# One line per presolve run that maps a stored basis:
#   WMAP origbasic (count of BASIC in the orig stored basis) vs nr,
#        redbasic (count in the mapped+corrected reduced basis) vs rrow.
#
# Predictions, stated before the run: the warm==cold instances read
# origbasic == nr (their publish is exact, D139) and redbasic < rrow; the
# pre-D138/D139 record's warm winners read origbasic > nr with redbasic ==
# rrow only because the excess sat on removed members. Kennington (all 32
# publishes exact, D139) shows how many of its 11 measured warm solves map
# short the same way.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-51-wmap"
out="$here/warm-mapping.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#endif
""" + head)

anchor = """    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;"""
assert s.count(anchor) == 1
s = s.replace(anchor, """#ifdef JAOS_DIAG
    if (m->start_col_status != nullptr && m->start_row_status != nullptr) {
        long long ob = 0, rb = 0;
        for (int64_t dgi = 0; dgi < nc; dgi++)
            ob += m->start_col_status[dgi] == JAOS_BASIS_BASIC;
        for (int64_t dgi = 0; dgi < nr; dgi++)
            ob += m->start_row_status[dgi] == JAOS_BASIS_BASIC;
        for (int64_t dgi = 0; dgi < rcol; dgi++)
            rb += p->reduced.start_col_status[dgi] == JAOS_BASIS_BASIC;
        for (int64_t dgi = 0; dgi < rrow; dgi++)
            rb += p->reduced.start_row_status[dgi] == JAOS_BASIS_BASIC;
        char dbuf[160];
        int dk = snprintf(dbuf, sizeof dbuf,
            "WMAP origbasic=%lld nr=%lld redbasic=%lld rrow=%lld\\n",
            ob, (long long)nr, rb, (long long)rrow);
        if (dk > 0 && dk < (int)sizeof dbuf) {
            ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
        }
    }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-wmap -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-wmap -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/warm-wmap -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# One line per presolve run that mapped a stored basis (the warm solve"
echo "# of each instance). origbasic vs nr is the ORIG-space count; redbasic"
echo "# vs rrow is what build_warm_basis actually judges."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "WMAP " "$d/$f.log"); ok=$(grep -c "^WMAP " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^WMAP /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        n++
        oex = (v["origbasic"] == v["nr"])
        rex = (v["redbasic"] == v["rrow"])
        if (oex && rex)        a++
        else if (oex && !rex) { b++; d = v["rrow"] - v["redbasic"]
                                if (d > 0) bshort++; else blong++
                                if (d > 0 && d > worst) worst = d }
        else if (!oex && rex)  c++
        else                   e++
    }
    END {
        printf "%d warm solves mapped a stored basis\n", n
        printf "  %-52s %6d\n", "orig exact, mapped exact  -> warm runs", a
        printf "  %-52s %6d\n", "orig exact, mapped WRONG  -> falls back cold", b
        printf "  %-52s %6d\n", "    of those, mapped SHORT of rrow", bshort
        printf "  %-52s %6d\n", "    worst shortfall", worst
        printf "  %-52s %6d\n", "orig wrong, mapped exact  -> warm runs anyway", c
        printf "  %-52s %6d\n", "orig wrong, mapped wrong  -> falls back cold", e
    }' "$d/$f.log"
    echo
done
echo "# prediction: the newly-cold instances are the 'orig exact, mapped"
echo "# WRONG (short)' class; 'orig wrong, mapped exact' is the pre-D138/139"
echo "# accident that used to run warm and now cannot exist on Kennington."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
