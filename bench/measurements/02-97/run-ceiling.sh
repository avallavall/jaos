#!/bin/bash
# D185 — an absolute bar on the suboptimality bound, and the case it must reject.
#
# `RSUB_FLOOR` and `RSUB_REGRESSION_FACTOR` have their zero point in the
# baseline: they report a bound that GETS worse. A bound that was already bad
# when the baseline was written reads as permanently fine, which is how `pilot`
# published a point 2.31e-05 above the optimum with no predicate saying a word.
#
# `RSUB_CEILING` asks a question the baseline cannot answer: is the bound
# acceptable at all?
#
# Two halves, and the second is the one that matters. A bar nothing reaches is
# indistinguishable from a bar that is never evaluated, so this builds the
# pre-D184 solver against the NEW runner and requires it to fire.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
OLD="${1:-bc398a5}"          # the tree before DUAL_TOL went to 1e-9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"; git -C "$root" worktree prune' EXIT

{
echo "# D185 — RSUB_CEILING at $(git rev-parse --short HEAD) plus the working change"
echo
echo "######## 1. the bar fires on nothing today ########"
for t in netlib netlib-infeas netlib-kennington; do
    make "$t" J=12 >"$D/$t.log" 2>&1
    echo "-- make $t exit=$?"
    grep -E "instances:|gate:|baseline:|OVER-CEILING" "$D/$t.log"
done
echo
echo "  the records must be byte-identical: the bar prints nothing when it"
echo "  does not fire, so it cannot change the format (D-13)."
git -C "$root" status --short bench/results/ || true
echo "  (no lines above == byte-identical to the committed records)"
echo
echo "######## 2. THE CASE IT MUST REJECT ########"
echo "  the same runner against the solver as it was at $OLD, where pilot"
echo "  publishes 6.91e-05. If this stays quiet the bar is decoration."
wt="$D/wt"
git worktree add --detach "$wt" "$OLD" >/dev/null 2>&1 || { echo "worktree failed"; exit 2; }
rm -rf "$wt/bench/instances" && ln -s "$root/bench/instances" "$wt/bench/instances"
cp "$root/bench/run.c" "$wt/bench/run.c"       # new runner, old solver
echo "  canary, the one variable:"
echo "    $OLD        $(grep -m1 'constexpr double DUAL_TOL' "$wt/src/simplex.c" | tr -s ' ')"
echo "    working tree $(grep -m1 'constexpr double DUAL_TOL' "$root/src/simplex.c" | tr -s ' ')"
if [ "$(grep -m1 'constexpr double DUAL_TOL' "$wt/src/simplex.c")" = \
     "$(grep -m1 'constexpr double DUAL_TOL' "$root/src/simplex.c")" ]; then
    echo "    ABORT: the two trees agree on DUAL_TOL, so there is nothing to reject."
    exit 3
fi
( cd "$wt" && make bench >/dev/null 2>&1 ) || { echo "  build failed"; exit 2; }
echo
( cd "$wt" && ./build/bench/run -j 12 -o "$D/old.txt" pilot pilot87 wood1p adlittle 2>&1 ) \
    | grep -E "OVER-CEILING|gate:|rsub=" \
    | sed -E 's/^([a-z0-9-]+).*rsub=([^)]*)\).*/   \1 rsub=\2/' | sed 's/^/  /'
git worktree remove --force "$wt" >/dev/null 2>&1
} | tee "$here/ceiling.txt"
