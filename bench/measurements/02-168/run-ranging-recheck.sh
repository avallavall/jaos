#!/usr/bin/env bash
# D258's ranging population driver, copied here and run on the WORKING
# tree. 02-167 refused `finnis`, whose four columns rested on lent bounds;
# this says whether it still does. The original script is NOT re-run: it
# owns D258's and D260's reading and would overwrite it.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/ranging-population.c" -o "$D/rp" -lm || exit 2
{
  echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet src include || echo ' WITH UNCOMMITTED src/ CHANGES')"
  echo "### netlib"
  "$D/rp" bench/instances
  echo
  echo "### kennington"
  "$D/rp" bench/instances-kennington
} | tee "$here/ranging-recheck.txt"
