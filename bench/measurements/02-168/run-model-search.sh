#!/usr/bin/env bash
# The small-model search, on the WORKING tree, in both builds.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet src include || echo ' WITH UNCOMMITTED src/ CHANGES')"
gcc-14 $P src/*.c "$here/model-search.c" -o "$D/ms" -lm || exit 2
echo "## default build"
"$D/ms"
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/model-search.c" -o "$D/ms2" -lm || exit 2
echo "## -DJAOS_NO_PRESOLVE"
"$D/ms2"
