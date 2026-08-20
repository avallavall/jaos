#!/bin/bash
# Does the published objective lose the terms a large one hides?
#
#   costs in column order:  +1e16,  1 (k times),  -1e16
#   every column fixed at 1, one row holding them all and binding nothing
#
# The answer is k. Summed in column order the running total is 1e16 while the
# k ones arrive, and one ulp of 1e16 is 2, so each of them is below half an ulp
# and the total does not move; the -1e16 then brings it to zero.
#
# `jaos_check_solution` accumulates in `long double` and is printed beside it:
# the two are the same library's answer to the same question about the same
# point, and they disagreed by 100%.
#
# Usage: run-objective.sh [git-ref]      default: ba69a88, D169's parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-ba69a88}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
for f in src/*.c src/*.h; do
    git show "$ref:$f" > "$D/before/$(basename "$f")" || exit 2
done

{
  for k in 64 256; do
      for cfg in "" "-DJAOS_NO_PRESOLVE"; do
          name=${cfg:-shipping}
          gcc-14 $P -DKSMALL=$k $cfg "$D/before"/*.c \
                 "$here/objective-model.c" -o "$D/b" -lm || exit 2
          gcc-14 $P -DKSMALL=$k $cfg src/*.c \
                 "$here/objective-model.c" -o "$D/a" -lm || exit 2
          echo "k=$k  $name  -- the parent ($ref)"
          "$D/b" | sed 's/^/      /'
          echo "k=$k  $name  -- compensated, and summed on the published model"
          "$D/a" | sed 's/^/      /'
      done
  done
} | tee "$here/objective.txt"
