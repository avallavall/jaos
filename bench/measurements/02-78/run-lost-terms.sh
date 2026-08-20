#!/bin/bash
# Does the simplex lose a row's small terms behind a large one?
#
# The model is D162's, unchanged (bench/measurements/02-72/shift-count.c):
#
#   row R:  x0 + x1 + (k smalls) + w1 + w2  ==  k*2^-25 + 1e-7
#   row S:  x1 + z                          == -1e9
#
#   x0     fixed at +1e9
#   x1     in [-1e9-1, -1e9+1]
#   smalls fixed at 2^-25, a quarter of an ulp of 1e9
#   w1, w2 in [0, 2e-7], cost 1
#   z      fixed at 0
#
# A is feasible at an exactly representable point: x0 = 1e9, x1 = -1e9, every
# small at 2^-25, w1 = 1e-7, w2 = 0. B is the control, the same shape 1e-2
# away from any feasible point, and it must be refused on every build.
#
# `-DJAOS_NO_PRESOLVE` is the configuration that shows the defect. Presolve
# removes the fixed columns before the simplex sees them and since D165
# subtracts them with the residue kept, so the shipping build already answered
# A. The reference build hands the whole model to the simplex, which sums the
# row in column order, meets +1e9 first and drops all k smalls.
#
# Usage: run-lost-terms.sh [git-ref]      default: f3a7798, D168's parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-f3a7798}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
git show "$ref:src/simplex.c" > "$D/before/simplex.c" || exit 2
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = simplex.c ] || cp "$f" "$D/before/$b"
done

{
  echo "A is feasible with an exactly representable point; B is the same shape"
  echo "1e-2 away from any feasible point and must be refused everywhere."
  echo
  for k in 64 128 256 512; do
      gcc-14 $P -DKSMALL=$k -DJAOS_NO_PRESOLVE "$D/before"/*.c \
             "$here/../02-72/shift-count.c" -o "$D/rb$k" -lm || exit 2
      gcc-14 $P -DKSMALL=$k -DJAOS_NO_PRESOLVE src/*.c \
             "$here/../02-72/shift-count.c" -o "$D/ra$k" -lm || exit 2
      gcc-14 $P -DKSMALL=$k src/*.c \
             "$here/../02-72/shift-count.c" -o "$D/sa$k" -lm || exit 2
      echo "KSMALL=$k"
      echo "   -- REFERENCE BUILD, the parent ($ref)"
      "$D/rb$k" | sed 's/^/      /'
      echo "   -- REFERENCE BUILD, the compensated accumulation"
      "$D/ra$k" | sed 's/^/      /'
      echo "   -- shipping build, the compensated accumulation"
      "$D/sa$k" | sed 's/^/      /'
  done
} | tee "$here/lost-terms.txt"
