#!/usr/bin/env bash
# D264's one failing instance, replayed: for each side of cplex2's IIS
# that a cold re-solve does not need (the first three), rebuild the kept
# set the deletion filter held when it reached that side and re-solve
# without it, cold and warm, with the checker's report on the point or on
# the ray. Builds outside the repository; overwrites cplex2-replay.txt.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
D=$(mktemp -d) || exit 9
trap 'rm -rf "$D"' EXIT
cd "$root" || exit 9
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off \
    -Iinclude -Isrc src/*.c "$here/cplex2-replay.c" -o "$D/p" -lm || exit 9
{
    echo "# tree: $(git -C "$root" rev-parse --short HEAD)$(git -C "$root" diff --quiet -- src include || echo ' + uncommitted src/include')"
    "$D/p" bench/instances-infeas/cplex2.mps
} | tee "$here/cplex2-replay.txt"
