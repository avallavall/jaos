#!/bin/bash
# Does the frozen-row window take its number from the row's far end?
#
#   min 0  s.t.  -1e12 <= x0 + x1 <= 0,  x0 and x1 both cost 0 in [1e-4, 1]
#
# Both columns are cost-0 bounded singletons, so both relax and freeze the row,
# which is then empty. Infeasible by 2e-4, and the frozen-row test is the last
# word because an emptied frozen row is deleted with everything else.
#
# `ps_bound_scale(-1e12, 0)` is 1e12, so the window was 1.78e-3 -- taken
# entirely from the LOWER bound for a test on the UPPER side.
#
# Four builds, and the second model is the control: the same shape with an
# INFINITE lower bound was refused correctly all along, which is only
# explicable if the finite one was supplying the number.
#
# Usage: run-far-bound.sh [git-ref]     default: 0078244, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-0078244}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

# the parent, and the parent's parent, to show the defect predates D159
for pair in "pre $ref" "d159 0ac44fd"; do
    set -- $pair; tag=$1; r=$2
    mkdir -p "$D/$tag"
    git show "$r:src/presolve.c" > "$D/$tag/presolve.c" 2>/dev/null || continue
    for f in src/*.c src/*.h; do
        b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/$tag/$b"
    done
    gcc-14 $P "$D/$tag"/*.c "$here/far-bound.c" -o "$D/bin-$tag" -lm || exit 2
done
gcc-14 $P                    src/*.c "$here/far-bound.c" -o "$D/now" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/far-bound.c" -o "$D/ref" -lm || exit 2

{
  echo "G-rl=-1e12 is infeasible by 2e-4. G-control-inf is the same model with"
  echo "an infinite lower bound and must be refused on every tree."
  echo
  echo "-- before D159 (0ac44fd), so the defect predates it"
  "$D/bin-d159" | sed 's/^/   /'
  echo "-- the parent of this change ($ref)"
  "$D/bin-pre"  | sed 's/^/   /'
  echo "-- the working tree, the far bound dropped"
  "$D/now"      | sed 's/^/   /'
  echo "-- REFERENCE BUILD (-DJAOS_NO_PRESOLVE), the oracle"
  "$D/ref"      | sed 's/^/   /'
} | tee "$here/far-bound.txt"
