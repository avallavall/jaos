#!/bin/bash
# Which of phase 1's four refusals are reached at all.
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
  echo "# phase 1's four refusals: which are reached, over the standard set"
  echo "# tree: $ref plus patch.py, in a worktree"
  echo "# only instances that reached one are listed"
  echo
  "$D/probe" bench/instances/*.mps
  echo "probe rc=$?"
} | tee "$here/refusal-census.txt"
