#!/bin/bash
# The corrected sweep, reordered. Each setting costs about twenty minutes,
# because every one of them lets `pilot87` run its phase 1 to the work limit
# instead of refusing at iteration 17165. Round three ran them in increasing
# order, which puts the candidate sixth.
#
# The decisive four first: C=1 is the candidate (one ulp of the column's
# largest entry), 3e-1 and 2 say whether it is a plateau or a spike, and 5
# is the last value the census says cannot reach the gate (`wood1p` sits at
# 5.4855). The census already settles the other side of the sweep: below
# 3.3457e-06 the floor decides nothing at all.
#
# Then the clean gate-set census, then the three small settings for the shape.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
pkill -f sweep-cheap.sh
pkill -f round3.sh
pkill -f round4.sh
pkill -f 'bench/primal'
sleep 2
rm -f "$here/cheap-1e-5.txt" "$here/cheap-1e-5.log"
cd /mnt/c/Users/vall-/Desktop/projectes/jaos && git worktree prune

bash "$here/sweep-cheap.sh" 1 3e-1 2 5
echo "== the four that decide it are in =="
bash "$here/census-pivot-scale.sh" gate-sets
bash "$here/sweep-cheap.sh" 1e-1 1e-3 1e-5
echo "round5 done"
