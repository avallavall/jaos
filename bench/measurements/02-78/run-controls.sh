#!/bin/bash
# Where the model stops being feasible, and what it publishes on the way.
#
# Two claims from `numerics-reviewer` are settled here, and one of them was
# wrong:
#
#   - "at slack 0 the objective is exactly 1e-7 at every feasible point."
#     **Not so.** `x1` is a variable, and the two builds publish different
#     values: 1.0000000000000074e-07 shipping, 1.1920928955078125e-07 under
#     `-DJAOS_NO_PRESOLVE`, the second being 2^-23 — one ulp of `x1`'s own
#     magnitude. The test's window admits both and rejects 0, 2e-7 and 4e-7.
#
#   - "slack 5e-6 is genuinely infeasible and separates an accurate sum from a
#     wider window." **Confirmed.** It is infeasible by about 4.7e-6, which
#     `w1 + w2` cannot reach against its cap of 4e-7, and it sits inside a
#     window widened to cover the 7.63e-6 that was being lost. INFEASIBLE on
#     both builds at both trees.
#
# Usage: run-controls.sh [git-ref]      default: f3a7798, D168's parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-f3a7798}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
for f in src/*.c src/*.h; do
    git show "$ref:$f" > "$D/before/$(basename "$f")" || exit 2
done

{
  for cfg in "" "-DJAOS_NO_PRESOLVE"; do
      name=${cfg:-shipping}
      gcc-14 $P $cfg "$D/before"/*.c "$here/controls.c" -o "$D/p" -lm || exit 2
      gcc-14 $P $cfg src/*.c         "$here/controls.c" -o "$D/c" -lm || exit 2
      echo "== $name : the parent ($ref)"
      "$D/p"
      echo "== $name : the working tree"
      "$D/c"
  done
} | tee "$here/controls.txt"
