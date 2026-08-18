#!/usr/bin/env bash
# 02-41's published-basis count, run on the WORKING TREE's presolve.c rather
# than on HEAD. The gate cannot see this change (the digest covers x and y and
# not the basis, bench/run.c says so), so this probe is the evidence.
set -u
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
wt="$root/build/diag/wt-verify"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cp "$root/src/presolve.c" "$wt/src/presolve.c" || exit 2
cd "$wt" || exit 2
for dir in instances instances-infeas instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()
head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#endif""")
tail = """    sx_free(&s);
    jm_presolve_free(&p);
    return st;"""
assert s.count(tail) == 1
s = s.replace(tail, """#ifdef JAOS_DIAG
    {
        const jaos_model *o = p.orig;
        long long basic = -1;
        if (o != nullptr && o->sol_col_status != nullptr &&
            o->sol_row_status != nullptr) {
            basic = 0;
            for (int64_t j = 0; j < o->num_col; j++)
                basic += o->sol_col_status[j] == JAOS_BASIS_BASIC;
            for (int64_t i = 0; i < o->num_row; i++)
                basic += o->sol_row_status[i] == JAOS_BASIS_BASIC;
        }
        char buf[256];
        int k = snprintf(buf, sizeof buf,
                "PUBBASIS optimal=%d num_row=%lld basic=%lld off=%lld\\n",
                outcome == JAOS_SOLVE_OPTIMAL ? 1 : 0,
                (long long)(o ? o->num_row : -1), basic,
                basic < 0 ? 0LL : basic - (long long)o->num_row);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + tail)
open(p, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-v -lm || exit 2

d=$(mktemp -d)
./build/diag/run-v -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-v -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    awk -v L="$L" '/^PUBBASIS/ {
        delete v; n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        if (v["optimal"] != "1") next
        opt++; o = v["off"] + 0; tot += o
        if (o == 0) exact++; else { wrong++
            if (o > 0) { over++; if (o > wo) wo = o }
            else       { under++; if (-o > wu) wu = -o } }
    }
    END {
        printf "%-12s optimal=%d exact=%d WRONG=%d (over=%d under=%d) worst +%d/-%d  SUM=%+d\n",
               L, opt, exact, wrong, over, under, wo, wu, tot
    }' "$d/$f.log"
done
rm -rf "$d"
cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
