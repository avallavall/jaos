#!/bin/bash
# Round two. Two things the first round left open:
#   1. the other two gate sets -- census.txt only covered the standard 94, and
#      the safety claim ("no C below 5.4855 can move the gate") has to hold on
#      netlib-infeas and netlib-kennington too;
#   2. five more settings around C=1, because C=1 gives 55 ok and C=5 gives 54,
#      so the outcome is not flat there and one point is not a plateau.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
bash "$here/census-pivot-scale.sh" gate-sets
bash "$here/sweep-pivot-margin.sh" 1e-2 3e-1 5e-1 2 3 > "$here/sweep-round2.txt" 2>&1
echo "round2 done"
