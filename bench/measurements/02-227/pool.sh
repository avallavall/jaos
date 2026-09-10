#!/usr/bin/env bash
# The reading behind 02-227. Builds `pool.c` against the library in the tree
# and runs it over six seeds. Every model is a MIP whose boxes and rows all
# hold the zero point, so it is feasible and the tree has somewhere to walk,
# and the pool has more than one point to keep.
#
# `pool.c` takes: runs, seed, and a dump flag (any non-empty string prints
# the failing model). It exits non-zero when any of the six properties broke.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make all` first.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
OUT=${1:-/tmp/pool}
mkdir -p "$OUT"
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/pool" \
    bench/measurements/02-227/pool.c build/release/libjaos.a -lm || exit 1
for seed in 1 2 3 4 5 6; do
    printf 'seed %s: ' "$seed"
    "$OUT/pool" 4000 "$seed" | tail -1
done
