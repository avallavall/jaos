#!/bin/bash
# Where D162's shift count still does not reach.
#
# D162 put the count on three reads of the row's running difference. There is a
# FOURTH, the singleton row's fold, and there is a case the count cannot cover
# at all: an error already inside the value a fold fixed a column at.
#
# Five models, all with exactly representable feasible points. The reference
# build arbitrates these, which is what D162's own model could not do.
#
# Usage: run-fold-and-chain.sh [git-ref]     default: c61a931, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-c61a931}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
git show "$ref:src/presolve.c" > "$D/before/presolve.c" || exit 2
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/before/$b"
done

gcc-14 $P                    "$D/before"/*.c "$here/fold-and-chain.c" -o "$D/b" -lm || exit 2
gcc-14 $P                    src/*.c         "$here/fold-and-chain.c" -o "$D/a" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c         "$here/fold-and-chain.c" -o "$D/r" -lm || exit 2

{
  echo "FOLD and CHAIN are feasible with exactly representable points."
  echo "FOLD-control and EDGE-control are not, and must be refused everywhere."
  echo "END is the case that separates D162's second revision from its first."
  echo
  echo "-- the parent ($ref), which is D162"
  "$D/b" | sed 's/^/   /'
  echo "-- the working tree, the fold counting its shifts too"
  "$D/a" | sed 's/^/   /'
  echo "-- REFERENCE BUILD (-DJAOS_NO_PRESOLVE), the oracle"
  "$D/r" | sed 's/^/   /'
} | tee "$here/fold-and-chain.txt"
