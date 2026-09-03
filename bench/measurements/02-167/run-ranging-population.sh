#!/usr/bin/env bash
# Ranging over the standard set and Kennington on the WORKING tree's src/,
# built in a temp dir outside the repository; writes beside this script.
#   run-ranging-population.sh
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
} | tee "$here/ranging-population.txt"
