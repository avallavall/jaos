#!/usr/bin/env bash
# The reading behind 02-226. Builds `formats.c` against the library in the
# tree and runs it over six seeds. Each model goes to every format that can
# express it, comes back, and is solved: the status, the objective and the
# node count all have to match.
#
# `formats.c` takes: runs, seed, and a dump flag (any non-empty string
# prints the failing model). It exits non-zero when anything disagreed.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make all` first.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
OUT=${1:-/tmp/formats}
mkdir -p "$OUT" build
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/formats" \
    bench/measurements/02-226/formats.c build/release/libjaos.a -lm || exit 1
for seed in 1 2 3 4 5 6; do
    printf 'seed %s: ' "$seed"
    "$OUT/formats" 3000 "$seed" | tail -1
done
