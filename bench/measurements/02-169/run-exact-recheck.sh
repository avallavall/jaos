#!/usr/bin/env bash
# D173's exact-objective oracle, copied here and run against a NAMED tree,
# so the parent and the candidate can be compared without touching the
# record 02-83 owns. The oracle itself is unchanged and validates itself
# before every reading, as it does there.
#
#   run-exact-recheck.sh <tree-root> [instance ...]
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"; shift
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/exact-objective.c" -o "$D/s" -lm || exit 2
"$D/s" --selftest > "$D/self.txt" 2>&1 || {
    echo "the accumulator failed its own cases; no reading taken" >&2
    cat "$D/self.txt" >&2
    exit 4
}
echo "# tree: $tree, retire_lent_bounds occurrences: $(grep -c retire_lent_bounds src/simplex.c)"
if [ $# -eq 0 ]; then set -- bench/instances/*.mps; fi
"$D/s" bench/netlib.manifest "$@"
