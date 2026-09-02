#!/bin/bash
# What are the empty intersections actually made of?
#
# The clamp took the assert-enabled build from 11 aborting instances to 2
# (bandm, dfl001). Widening the window until those two pass would be fitting
# a constant to two instances, which is how this project loses weeks. So
# print what the failing records are made of instead.
#
# For EVERY singleton-column replay whose intersection is empty, emit the
# numbers the decision is made from, plus the two candidate scales:
#   |rest| + ends  -- what the current assert uses
#   the column's own box width, and whether rec->lo > rec->hi at all
# The last one matters most: if the COLUMN's stored box is itself empty, the
# gap is not rounding in the division and no window over the division's
# inputs can be the right test.
#
# src/ is read and never written; the patch lives in a throwaway worktree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-61-gap"
out="$here/gap-probe.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
git diff > "$wt/cand.diff"
cd "$wt" || exit 2
[ -s cand.diff ] && { git apply cand.diff || { echo "candidate did not apply"; exit 2; }; }
for dir in instances instances-infeas instances-kennington; do
    [ -d "$root/bench/$dir" ] && { rm -rf "bench/$dir"; ln -s "$root/bench/$dir" "bench/$dir"; }
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
const char *dg_gap_tag;
#endif
""" + head)

anchor = """        const jaos_basis_status rs = orig->sol_row_status[i];"""
assert s.count(anchor) == 1, "the status site after the intersection is not in the tree being probed"
s = s.replace(anchor, """#ifdef JAOS_DIAG
        if (want_lo > want_hi) {
            const double ends2 = (isfinite(rl) ? fabs(rl) : 0.0) +
                                 (isfinite(ru) ? fabs(ru) : 0.0);
            const double sc = (fabs(rest) + ends2) / fabs(rec->coef);
            char gb[512];
            int gk = snprintf(gb, sizeof gb,
                "GAP tag=%s row=%lld col=%lld gap=%.6e "
                "want_lo=%.17g want_hi=%.17g reclo=%.17g rechi=%.17g "
                "boxempty=%d rest=%.17g rl=%.17g ru=%.17g coef=%.17g "
                "scale=%.6e tol=%.6e need=%.3f\\n",
                dg_gap_tag != NULL ? dg_gap_tag : "?",
                (long long)i, (long long)j, want_lo - want_hi,
                want_lo, want_hi, rec->lo, rec->hi,
                rec->lo > rec->hi ? 1 : 0,
                rest, rl, ru, rec->coef,
                sc, ps_round_tol(sc),
                ps_round_tol(sc) > 0.0 ? (want_lo - want_hi) / ps_round_tol(sc)
                                       : -1.0);
            if (gk > 0 && gk < (int)sizeof gb) {
                ssize_t gw = write(2, gb, (size_t)gk); (void)gw;
            }
        }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)

r = sys.argv[1] + "/bench/run.c"
s = open(r, encoding="utf-8").read()
# Tag each forked child with its instance name, the same way 02-60 did.
mark = """    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return false;
    t->instances++;"""
assert s.count(mark) == 1, "could not find the per-instance entry in bench/run.c"
s = s.replace(mark, mark + """
#ifdef JAOS_DIAG
    { extern const char *dg_gap_tag; dg_gap_tag = e->name; }
#endif""")
open(r, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-gap -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-gap -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1

{
echo "# Every singleton-column replay whose intersection is empty, at HEAD"
echo "# with the clamp applied. need = gap / tol; need <= 1 passes the assert."
echo
grep "^GAP " "$d/nl.log" | sed 's/^GAP //' | sort
echo
echo "# ---- summary ----"
grep "^GAP " "$d/nl.log" | awk '{
    for (k = 1; k <= NF; k++) { split($k, kv, "="); v[kv[1]] = kv[2] }
    n++
    if (v["boxempty"] == 1) be++
    if (v["need"] > 1.0) fail++
    if (v["need"] > worst) { worst = v["need"]; wtag = v["tag"] }
    tags[v["tag"]]++
} END {
    printf "empty intersections total: %d, on %d instances\n", n, length(tags)
    printf "of those, the COLUMN box itself is empty (rec->lo > rec->hi): %d\n", be
    printf "records the current window does NOT cover: %d\n", fail
    printf "worst need = %.3f on %s\n", worst, wtag
    printf "instances: "
    for (t in tags) printf "%s(%d) ", t, tags[t]
    printf "\n"
}'
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
