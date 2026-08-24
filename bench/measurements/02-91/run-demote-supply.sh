#!/bin/bash
# D179 — does a rule wider than the firing row have anything to work with?
#
# Public API only, no instrumented build and no worktree. Runs both sets that
# publish a solution.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/demote-supply.c" -o "$D/a" -lm || exit 2
{
  echo "### netlib standard"
  "$D/a" bench/instances
  echo
  echo "### netlib-kennington"
  "$D/a" bench/instances-kennington
} | tee "$here/demote-supply.txt"
