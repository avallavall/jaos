#!/usr/bin/env bash
# The reading behind 02-228. Builds `basis.c` against the library in the tree
# and runs it over six seeds. Every model is an LP whose rows all hold the
# zero point, so it is feasible, and whose boxes take every shape the basis
# format has a letter for: boxed, one sided either way, free and fixed.
#
# `basis.c` takes: runs, seed, and a dump flag (any non-empty string prints
# the failing model). It exits non-zero when any of the five properties
# broke.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make all` first.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
OUT=${1:-/tmp/basis}
mkdir -p "$OUT" build
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/basis" \
    bench/measurements/02-228/basis.c build/release/libjaos.a -lm || exit 1
for seed in 1 2 3 4 5 6; do
    echo "seed $seed:"
    "$OUT/basis" 4000 "$seed" | tail -3
done
