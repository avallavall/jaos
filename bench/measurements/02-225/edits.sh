#!/usr/bin/env bash
# The reading behind 02-225. Builds `edits.c` against the library in the tree
# and runs it over six seeds, twice: once with every discrete shape the
# generator builds, once with the integer marks, the SOS sets, the
# semi-continuous marks and the indicator rows taken off, so a disagreement
# is placed inside the LP or inside the tree.
#
# `edits.c` takes: runs, seed, a dump flag (any non-empty string prints the
# failing model), and an "lp" flag (any fourth argument clears the discrete
# structure). It exits non-zero when anything disagreed.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make all` first.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
OUT=${1:-/tmp/edits}
mkdir -p "$OUT"
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/edits" \
    bench/measurements/02-225/edits.c build/release/libjaos.a -lm || exit 1
for seed in 1 2 3 4 5 6; do
    printf 'mip seed %s: ' "$seed"
    "$OUT/edits" 6000 "$seed" | tail -1
done
for seed in 1 2 3 4 5 6; do
    printf 'lp  seed %s: ' "$seed"
    "$OUT/edits" 6000 "$seed" "" lp | tail -1
done
