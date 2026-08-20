#!/bin/bash
# What is left in the published objective once the accumulation is exact, and
# what it is.
#
# Four numbers for the same point, on the instance in the standard set with the
# worst cancellation: the naive double sum, the compensated double sum, the
# same ROUNDED products added in long double, and the products themselves taken
# in long double. The third minus the second is what the accumulation still
# loses; the fourth minus the third is what rounding each `c_j * x_j` to a
# double loses, and no accumulator can reach that one.
#
# Usage: run-split-the-error.sh [instance ...]
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/split-the-error.c" -o "$D/s" -lm || exit 2
set -- "${@:-bench/instances/finnis.mps}"
[ $# -eq 1 ] && [ "$1" = "bench/instances/finnis.mps" ] && \
    set -- bench/instances/finnis.mps bench/instances/scagr7.mps
"$D/s" "$@" | tee "$here/split-the-error.txt"
