#!/usr/bin/env bash
# The published-basis count (02-78's instrument, with num_col on the line so
# a solve can be attributed to an instance) plus 02-80's public-API detectors,
# on the WORKING TREE's src/. Output goes beside this script, under the label
# given, so an older reading is never overwritten.
#
#   run-count-and-signs.sh <label>
set -u
label="${1:?label}"
out="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$out/../../.." && pwd)"
wt="${TMPDIR:-/tmp}/jaos-wt-probe-$$"
cd "$root" || exit 9

echo "tree: $(git rev-parse --short HEAD) dirty=$(git status --porcelain src | wc -l)" | tee "$out/$label-count.txt"

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
trap 'cd "$root"; git worktree remove --force "$wt" >/dev/null 2>&1' EXIT
cp "$root"/src/*.c "$root"/src/*.h "$wt/src/" || exit 2
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
                "PUBBASIS optimal=%d num_row=%lld num_col=%lld basic=%lld off=%lld@NL@",
                outcome == JAOS_SOLVE_OPTIMAL ? 1 : 0,
                (long long)(o ? o->num_row : -1),
                (long long)(o ? o->num_col : -1), basic,
                basic < 0 ? 0LL : basic - (long long)o->num_row);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""".replace("@NL@", "\\n") + tail)
open(p, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-v -lm || exit 2

./build/diag/run-v -j 12 -o "$out/$label-nl.txt" > "$out/$label-nl.log" 2>&1
./build/diag/run-v -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$out/$label-kn.txt" > "$out/$label-kn.log" 2>&1

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
    }' "$out/$label-$f.log"
done | tee -a "$out/$label-count.txt"
grep -h '^PUBBASIS' "$out/$label-nl.log" | grep -v 'off=0$' | sort | uniq -c > "$out/$label-wrong-nl.txt"
grep -h '^PUBBASIS' "$out/$label-kn.log" | grep -v 'off=0$' | sort | uniq -c > "$out/$label-wrong-kn.txt"
grep -c 'gate: PASS' "$out/$label-nl.log" "$out/$label-kn.log" | tee -a "$out/$label-count.txt"

# 02-80's public-API detectors, netlib only (Kennington is clean, D170).
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
m=bench/measurements/02-80
gcc-14 $P src/*.c "$m/redcost-signs.c" -o build/diag/signs -lm || exit 2
gcc-14 $P src/*.c "$m/crosstab.c"      -o build/diag/xtab  -lm || exit 2
{
  echo "### reduced-cost sign breaches, netlib"
  ./build/diag/signs bench/instances
  echo
  echo "### crosstab against the count promise, netlib"
  ./build/diag/xtab bench/instances
} > "$out/$label-signs.txt" 2>&1
echo "probe $label done" | tee -a "$out/$label-count.txt"
