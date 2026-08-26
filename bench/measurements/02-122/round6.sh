#!/bin/bash
# Stop round five -- it is measuring the version numerics-reviewer rejected --
# and sweep the candidate that stands. Then the clean gate-set census.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
pkill -f sweep-cheap.sh
pkill -f round5.sh
pkill -f 'bench/primal'
sleep 2
rm -f "$here"/cheap-2.txt "$here"/cheap-2.log "$here"/cheap-5.txt "$here"/cheap-5.log
cd /mnt/c/Users/vall-/Desktop/projectes/jaos && git worktree prune

bash "$here/sweep-candidate.sh" 0 1 3e-1 2
echo "== the candidate's sweep is in =="
bash "$here/census-pivot-scale.sh" gate-sets
echo "round6 done"
