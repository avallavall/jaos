#!/bin/bash
# The model that separates the shipped window from the shift-counted one, and
# the sweep over the number of removals that makes it a pin rather than one
# reading.
#
# The campaign flips no verdict on any of the 139, so only a constructed model
# can separate the two windows. This is that model, at four removal counts:
# 128 is where both trees accept and 256 is where only the counted window does.
#
# Three builds. The reference build is included and it does NOT arbitrate this
# one: it refuses the model at every count, including the counts where the
# shipped window accepts, because the solver sums the row in column order and
# loses the same terms presolve lost. What settles the question instead is that
# the feasible point is exactly representable — x0 = 1e9, x1 = -1e9, every
# small at 2^-25, w1 = T - 2^-17 and w2 = 0 make the activity exactly T.
#
# Usage: run-shift-count.sh [git-ref]      default: 4c5f58f, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-4c5f58f}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
git show "$ref:src/presolve.c" > "$D/before/presolve.c" || exit 2
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/before/$b"
done

{
  echo "A is feasible with an exactly representable point; B is the same shape"
  echo "1e-2 away from any feasible point and must be refused everywhere."
  echo
  for k in 64 128 256 512; do
      gcc-14 $P -DKSMALL=$k "$D/before"/*.c "$here/shift-count.c" -o "$D/b$k" -lm || exit 2
      gcc-14 $P -DKSMALL=$k src/*.c         "$here/shift-count.c" -o "$D/a$k" -lm || exit 2
      gcc-14 $P -DKSMALL=$k -DJAOS_NO_PRESOLVE src/*.c \
                                            "$here/shift-count.c" -o "$D/r$k" -lm || exit 2
      echo "KSMALL=$k"
      echo "   -- the parent ($ref), the shipped window"
      "$D/b$k" | sed 's/^/      /'
      echo "   -- the working tree, the shifts counted"
      "$D/a$k" | sed 's/^/      /'
      echo "   -- REFERENCE BUILD (-DJAOS_NO_PRESOLVE), which cannot arbitrate this"
      "$D/r$k" | sed 's/^/      /'
  done
} | tee "$here/shift-count.txt"
