#!/bin/bash
# How much of the shared iteration cap is spent before each method starts.
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2
python3 "$here/patch.py" src/simplex.c || exit 2
ln -s "$root/bench/instances" bench/instances 2>/dev/null
make all >/dev/null 2>&1 || { echo "library build failed"; exit 2; }
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" build/release/libjaos.a \
    -o "$D/probe" -lm || { echo "probe build failed"; exit 2; }
{
  echo "# the shared iteration cap, and what each method found already spent"
  echo "# tree: $ref plus patch.py, in a worktree"
  echo "# -1 means the method did not run at all"
  echo
  "$D/probe" bench/instances/*.mps
} | tee "$here/cap-census.txt"
