#!/usr/bin/env bash
# The four models the retirement does not clean up, against a given tree.
#   run-survivors.sh <tree-root>
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
gcc-14 $P src/*.c "$here/survivors.c" -o "$D/sv" -lm || exit 2
echo "## default build"
"$D/sv"
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/survivors.c" -o "$D/sv2" -lm || exit 2
echo "## -DJAOS_NO_PRESOLVE"
"$D/sv2"
