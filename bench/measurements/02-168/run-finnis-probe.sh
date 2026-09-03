#!/usr/bin/env bash
# finnis against a given src/ tree, with the retirement's log line.
#   run-finnis-probe.sh [tree-root]
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:-$(cd "$here/../../.." && pwd)}"
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/finnis-probe.c" -o "$D/fp" -lm || exit 2
echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
"$D/fp" bench/instances/finnis.mps
