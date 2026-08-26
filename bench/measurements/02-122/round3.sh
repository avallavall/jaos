#!/bin/bash
# Round two's census (the other two gate sets) is worth having; its sweep is
# not, because it re-runs the version that bills an extra O(nrow) scan and so
# shortens every primal solve's 10x work budget. Wait for the census, stop the
# sweep, and run the corrected one over the whole range instead.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=/mnt/c/Users/vall-/Desktop/projectes/jaos

# The sweep's output file is created by the shell redirection before the build
# starts, so its appearance is the signal that the census is done.
for _ in $(seq 1 240); do
    [ -f "$here/sweep-round2.txt" ] && break
    sleep 10
done

pkill -f sweep-pivot-margin.sh
pkill -f round2.sh
pkill -f 'bench/primal'
sleep 2
rm -f "$here/sweep-round2.txt"
cd "$root" && git worktree prune

echo "== gate-set census =="
cat "$here/census-gate-sets.txt"

echo
echo "== corrected sweep =="
bash "$here/sweep-cheap.sh" 0 1e-5 1e-3 1e-1 3e-1 1 2 5
echo "round3 done"
