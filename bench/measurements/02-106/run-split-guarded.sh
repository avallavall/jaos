#!/bin/bash
# The primal/dual iteration split with phase 2 guarded as well as phase 1.
#
# D191 reported only the agreement count for this experiment (54 -> 20). The
# question here is the iteration split: does phase 2 stop after one iteration
# because the lending zeroes what primal_price reads?
#
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2
python3 "$here/patch-guard2.py" src/simplex.c || exit 2
ln -s "$root/bench/instances" bench/instances 2>/dev/null
make all >/dev/null 2>&1 || { echo "library build failed"; exit 2; }
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/split.c" build/release/libjaos.a \
    -o "$D/split" -lm || { echo "probe build failed"; exit 2; }
{
  echo "# forced-primal solves with PHASE 2 GUARDED as well as phase 1"
  echo "# tree: $ref plus patch-guard2.py, in a worktree"
  echo "# p2% is phase-2 primal iterations as a share of the whole solve"
  echo
  "$D/split" bench/instances/*.mps
} | tee "$here/split-guarded.txt"
