#!/usr/bin/env bash
# The small-model family search against a given src/ tree, both builds.
#   run-random-search.sh <tree-root> [count]
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"
n="${2:-200000}"
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
gcc-14 $P src/*.c "$here/random-search.c" -o "$D/rs" -lm || exit 2
echo "## default build"
"$D/rs" "$n"
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/random-search.c" -o "$D/rs2" -lm || exit 2
echo "## -DJAOS_NO_PRESOLVE"
"$D/rs2" "$n"
