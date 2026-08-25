#!/bin/bash
# Does refresh's repair sweep ever run while in_phase1 is set?
#
# The patch and the probe live beside this file. The worktree goes under
# mktemp -d, OUTSIDE the repository: make clean is rm -rf build and anyone
# else's make configs would delete it mid-run.
#
# Writes bench/measurements/02-105/sweep-census.txt, replacing it. Redirect
# elsewhere for a fresh reading without losing the committed evidence.
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

python3 "$here/patch.py" src/simplex.c || exit 2
ln -s "$root/bench/instances" bench/instances 2>/dev/null
make all >/dev/null 2>&1 || { echo "worktree library build failed"; exit 2; }
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" build/release/libjaos.a \
    -o "$D/probe" -lm || { echo "probe build failed"; exit 2; }

{
  echo "# refresh's repair sweep during a forced-primal solve, netlib standard set"
  echo "# tree: $ref, patched by patch.py in a worktree"
  echo "# only instances with at least one sweep are listed"
  echo
  "$D/probe" bench/instances/*.mps
  echo "probe rc=$?"
} | tee "$here/sweep-census.txt"
