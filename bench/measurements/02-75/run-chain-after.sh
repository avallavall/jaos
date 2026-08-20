#!/bin/bash
# 02-74's chained model on the compensated tree, and what it publishes.
#
# D164 refused the window repair because it published `optimal` with both rows
# violated by 7.5 times CHECK_TOL. This is the same model and the same program,
# against the repair that removes the error instead of covering it.
#
# Usage: run-chain-after.sh [git-ref]      default: cd68630, D164
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-cd68630}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
S=bench/measurements/02-74/chain-answer.c

mkdir -p "$D/before"
git show "$ref:src/presolve.c" > "$D/before/presolve.c" || exit 2
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/before/$b"
done
gcc-14 $P                    "$D/before"/*.c "$S" -o "$D/b" -lm || exit 2
gcc-14 $P                    src/*.c         "$S" -o "$D/a" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c         "$S" -o "$D/r" -lm || exit 2

{
  echo "The chained model: feasible exactly at x1 = 1e9 - 2^-17, w1 = 2^-23."
  echo
  echo "-- the parent ($ref), which refuses it"
  "$D/b" | sed 's/^/   /'
  echo "-- the working tree, the residue kept"
  "$D/a" | sed 's/^/   /'
  echo "-- REFERENCE BUILD (-DJAOS_NO_PRESOLVE), the oracle"
  "$D/r" | sed 's/^/   /'
} | tee "$here/chain-after.txt"
