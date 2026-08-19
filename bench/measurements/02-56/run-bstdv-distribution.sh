#!/bin/bash
# The two sides of D147's guard: what does the settling loop's final dual
# violation read on every LEGITIMATE gate solve at HEAD?
#
# One BSTDV line per take_best_if_better call (every exit of the settling
# loop passes through it), carrying both candidates the publish will choose
# between: the saved best point's dviol and the current settled point's,
# plus the dual tolerance the guard would compare against.
#
# Prediction, stated before the run: the legitimate population sits at or
# under dual_tol (pilot87's known residue is 8.37e-09), the hostile degen2
# read 35.34, and the margin is orders of magnitude. Any gate solve above
# the tolerance names the margin question before the guard can land.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-56-bstdv"
out="$here/bstdv-distribution.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
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

anchor = """static jaos_status take_best_if_better(sx *s, bool *ok)
{
    *ok = true;"""
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + """
#ifdef JAOS_DIAG
    {
        char b[224];
        int k = snprintf(b, sizeof b,
            "BSTDV valid=%d bst=%.17e cur=%.17e tol=%.3e\\n",
            s->bst_valid ? 1 : 0, s->bst_dviol, settled_dual_violation(s),
            s->dual_tol);
        if (k > 0 && k < (int)sizeof b) {
            ssize_t w = write(2, b, (size_t)k); (void)w;
        }
    }
#endif""")
open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-bstdv -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-bstdv -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-bstdv -j 12 -m bench/netlib-infeas.manifest \
    -d bench/instances-infeas -e infeasible -o "$d/ni.txt" > "$d/ni.log" 2>&1
./build/diag/run-bstdv -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Every take_best_if_better call on the three gate sets. The publish"
echo "# takes the better of (bst, cur); the guard would read that winner."
echo
for f in nl ni kn; do
    case $f in nl) L=netlib;; ni) L=netlib-infeas;; kn) L=kennington;; esac
    all=$(grep -c "BSTDV " "$d/$f.log"); ok=$(grep -c "^BSTDV " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^BSTDV /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        n++
        tol = v["tol"] + 0
        pub = (v["valid"] == 1 && v["bst"] + 0 < v["cur"] + 0) \
                  ? v["bst"] + 0 : v["cur"] + 0
        if (pub > mx) { mx = pub }
        if (pub <= tol)            a++
        else if (pub <= 10 * tol)  b++
        else if (pub <= 1e-3)      c++
        else                       d2++
        if (v["bst"] + 0 > mxb && v["valid"] == 1) mxb = v["bst"] + 0
        if (v["cur"] + 0 > mxc) mxc = v["cur"] + 0
    }
    END {
        printf "%d settling exits\n", n
        printf "  %-46s %6d\n", "published dviol <= dual_tol", a
        printf "  %-46s %6d\n", "in (dual_tol, 10*dual_tol]", b
        printf "  %-46s %6d\n", "in (10*dual_tol, 1e-3]", c
        printf "  %-46s %6d\n", "above 1e-3", d2
        printf "  worst published %.3e; worst bst %.3e; worst cur %.3e\n",
               mx, mxb, mxc
    }' "$d/$f.log"
    echo
done
echo "# the hostile degen2 baseline for comparison: 35.34 (02-55)."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
