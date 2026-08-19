#!/bin/bash
# Would the row's TRAFFIC be the right scale for the emptiness assert?
#
# The first window used |rest| + |row bounds|. gap-probe.txt says why that
# fails on 22 records: the failing rows are equalities at zero whose partial
# activity has cancelled to ~1e-14, so |rest| IS the residue and the scale
# collapses onto the quantity it is supposed to bound. fp-numerics states the
# rule being broken — a sum is known to eps * SUM OF MAGNITUDES, not to
# eps * |sum|.
#
# So accumulate the traffic beside the compensated sum ps_row_add already
# keeps, and re-read every empty intersection against it. This probe DECIDES
# whether that design is worth building; it does not build it.
#
# Prints, per empty intersection, need_old and need_new. The question is
# whether max(need_new) < 1 with the SHIPPING PRESOLVE_ROUND_ULPS = 8, with
# no new constant and no fitting.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-61-traffic"
out="$here/traffic-probe.txt"
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

# A file-scope traffic array, sized and cleared where rowc is.
head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
double *dg_traffic;          /* parallel to rowc: SUM of |term| per row */
int64_t dg_traffic_n;
#endif
""" + head)

# Accumulate it in the one place every term goes through.
add = """static void ps_row_add(jaos_model *orig, double *rowc, int64_t i, double t)
{
    const double s = orig->sol_row[i];"""
assert s.count(add) == 1
s = s.replace(add, """static void ps_row_add(jaos_model *orig, double *rowc, int64_t i, double t)
{
#ifdef JAOS_DIAG
    if (dg_traffic != NULL && i >= 0 && i < dg_traffic_n)
        dg_traffic[i] += fabs(t);
#endif
    const double s = orig->sol_row[i];""")

# Allocate beside each rowc.
alloc = """    double *rowc = calloc((size_t)orig->num_row + 1, sizeof *rowc);"""
n = s.count(alloc)
assert n == 2, f"expected 2 rowc allocations, found {n}"
s = s.replace(alloc, alloc + """
#ifdef JAOS_DIAG
    free(dg_traffic);
    dg_traffic = calloc((size_t)orig->num_row + 1, sizeof *dg_traffic);
    dg_traffic_n = orig->num_row + 1;
#endif""")

# Read both windows at the empty-intersection site.
anchor = """        const double xv = want_lo < rec->lo ? rec->lo"""
assert s.count(anchor) == 1, "the clamp is not in the tree being probed"
s = s.replace(anchor, """#ifdef JAOS_DIAG
        if (want_lo > want_hi) {
            const double ends2 = (isfinite(rl) ? fabs(rl) : 0.0) +
                                 (isfinite(ru) ? fabs(ru) : 0.0);
            const double old_s = (fabs(rest) + ends2) / fabs(rec->coef);
            const double traf = (dg_traffic != NULL && i < dg_traffic_n)
                                    ? dg_traffic[i] : 0.0;
            const double new_s = (traf + ends2) / fabs(rec->coef);
            const double g = want_lo - want_hi;
            char gb[400];
            int gk = snprintf(gb, sizeof gb,
                "TRAF gap=%.6e traffic=%.6e rest=%.17g "
                "need_old=%.3f need_new=%.3f\\n",
                g, traf, rest,
                ps_round_tol(old_s) > 0.0 ? g / ps_round_tol(old_s) : -1.0,
                ps_round_tol(new_s) > 0.0 ? g / ps_round_tol(new_s) : -1.0);
            if (gk > 0 && gk < (int)sizeof gb) {
                ssize_t gw = write(2, gb, (size_t)gk); (void)gw;
            }
        }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-traf -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-traf -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-traf -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1
./build/diag/run-traf -j 12 -m bench/netlib-infeas.manifest \
    -d bench/instances-infeas -o "$d/if.txt" > "$d/if.log" 2>&1

{
echo "# Every empty intersection, all three sets, both candidate windows."
echo "# need = gap / tol at the SHIPPING PRESOLVE_ROUND_ULPS = 8."
echo "# The question: does max(need_new) come in under 1 with no new constant?"
echo
for f in nl kn if; do
    case $f in nl) L="netlib";; kn) L="kennington";; if) L="netlib-infeas";; esac
    echo "######## $L ########"
    grep "^TRAF " "$d/$f.log" | awk '{
        for (k = 2; k <= NF; k++) { split($k, kv, "="); v[kv[1]] = kv[2] }
        n++
        if (v["need_old"] > mo) { mo = v["need_old"] }
        if (v["need_new"] > mn) { mn = v["need_new"]; mng = v["gap"]; mnt = v["traffic"] }
        if (v["need_old"] > 1.0) fo++
        if (v["need_new"] > 1.0) fn++
        if (v["traffic"] == 0) z++
    } END {
        if (n == 0) { print "  no empty intersections"; exit }
        printf "  empty intersections:              %d\n", n
        printf "  NOT covered by |rest| window:     %d (worst need %.3f)\n", fo, mo
        printf "  NOT covered by traffic window:    %d (worst need %.3f)\n", fn, mn
        printf "  worst traffic-window case: gap=%s traffic=%s\n", mng, mnt
        printf "  records whose traffic reads 0:    %d\n", z
    }'
    echo
done
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
