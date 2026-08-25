#!/bin/bash
# The primal/dual iteration split over the standard set, at this tree.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
make all >/dev/null 2>&1 || { echo "library build failed"; exit 2; }
mkdir -p build/bench
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/split.c" build/release/libjaos.a \
    -o build/bench/split -lm || { echo "probe build failed"; exit 2; }
{
  echo "# forced-primal solves: how many iterations each method actually ran"
  echo "# tree: $(git rev-parse --short HEAD)"
  echo "# p2% is phase-2 primal iterations as a share of the whole solve"
  echo
  build/bench/split bench/instances/*.mps
} | tee "$here/split.txt"
