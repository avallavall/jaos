#!/bin/bash
# D184 — DUAL_TOL 1e-7 -> 1e-9, the campaign and one attribution.
#
# The maintainer chose this on D174's sweep. What the sweep did not separate is
# that DUAL_TOL is read in TWO units:
#
#   a RATE bound, everywhere:   s->d[v] < -s->dual_tol
#   an OBJECTIVE bound, once:   can_move(), wrong_way * |other - value| > dual_tol
#
# `can_move`'s comment argues at length that the PRODUCT is the right quantity
# because it has no space, and never says what it is compared against. So
# tightening the constant by two decades also makes that one site 100x more
# eager, and nobody asked for that half.
#
# The variant holds can_move at 1e-7 while everything else goes to 1e-9, so the
# difference between the two runs is that site alone.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"; git -C "$root" worktree prune' EXIT

{
echo "# D184 — DUAL_TOL 1e-9, at $(git rev-parse --short HEAD) plus the working change"
echo
echo "######## 1. the three gate sets, working tree ########"
for t in netlib netlib-infeas netlib-kennington; do
    make "$t" J=12 >"$D/$t.log" 2>&1
    echo "-- make $t exit=$?"
    grep -E "instances:|gate:|baseline:" "$D/$t.log"
    grep -E "REGRESSED|IMPROVED" "$D/$t.log" | sed 's/^/   /'
done
echo
echo "######## 2. attribution: is the cost can_move's half or the rest? ########"
wt="$D/wt"
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree failed"; exit 2; }
rm -rf "$wt/bench/instances" && ln -s "$root/bench/instances" "$wt/bench/instances"
cp "$root/src/simplex.c" "$wt/src/simplex.c"
# can_move keeps the old threshold; everything else is 1e-9.
sed -i 's|    return wrong_way \* fabs(other - nonbasic_value(s, v)) > s->dual_tol;|    return wrong_way * fabs(other - nonbasic_value(s, v)) > 1e-7;|' \
    "$wt/src/simplex.c"
if [ "$(grep -c 'nonbasic_value(s, v)) > 1e-7;' "$wt/src/simplex.c")" -ne 1 ]; then
    echo "  PATCH FAILED: can_move was not rewritten, so the two runs are one binary"
    exit 3
fi
( cd "$wt" && make bench >/dev/null 2>&1 ) || { echo "  variant build failed"; exit 2; }
echo "  variant binary md5 $(md5sum "$wt/build/bench/run" | cut -c1-12)"
echo "  shipping binary md5 $(md5sum "$root/build/bench/run" | cut -c1-12)"
( cd "$wt" && ./build/bench/run -j 12 -o "$D/variant-netlib.txt" >/dev/null 2>&1 )
echo
echo "  netlib work, variant (can_move at 1e-7) against the working tree (all 1e-9):"
python3 "$here/attribute.py" "$D/variant-netlib.txt" "$root/bench/results/netlib.txt"
git worktree remove --force "$wt" >/dev/null 2>&1
} | tee "$here/dual-tol.txt"
