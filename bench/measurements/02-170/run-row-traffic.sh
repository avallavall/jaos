#!/usr/bin/env bash
# finnis's rows against a named tree, so the parent and the candidate can be
# compared. `docs/tolerances.md`'s worked examples are read from here.
#   run-row-traffic.sh <tree-root> [row ...]
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"; shift
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/row-traffic.c" -o "$D/rt" -lm || exit 2
echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
"$D/rt" bench/instances/finnis.mps "$@"
