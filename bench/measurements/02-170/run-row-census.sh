#!/usr/bin/env bash
# The row census over the standard set, on a named tree.
#   run-row-census.sh <tree-root> <out-suffix>
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"
tag="${2:?give an output suffix}"
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Wno-format-truncation -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/row-census.c" -o "$D/rc" -lm || exit 2
{
  echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
  "$D/rc" bench/instances
} | tee "$here/row-census-$tag.txt"
