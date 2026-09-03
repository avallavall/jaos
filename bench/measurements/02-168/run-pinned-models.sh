#!/usr/bin/env bash
# The two pinned models against a given src/ tree, both builds.
#   run-pinned-models.sh <tree-root>
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tree="${1:?give a tree root}"
cd "$tree" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
echo "# tree: $tree @ $(git -C "$tree" rev-parse --short HEAD)$(git -C "$tree" diff --quiet src include || echo ' WITH UNCOMMITTED src/ CHANGES')"
gcc-14 $P src/*.c "$here/pinned-models.c" -o "$D/pm" -lm || exit 2
echo "## default build"
"$D/pm"
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/pinned-models.c" -o "$D/pm2" -lm || exit 2
echo "## -DJAOS_NO_PRESOLVE"
"$D/pm2"
