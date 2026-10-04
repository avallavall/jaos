#!/usr/bin/env bash
# The cut check behind 02-328: generated fixed-charge networks, the tree
# against brute force, on a library whose network cut rounds start at one
# linked column.
#
#   fcnet.sh [RUNS] [SEED] [OUT]      default 1000 models, seed 1
#
# Builds its own library in OUT. A model that fails a check is written to
# OUT as fNNNNN.mps.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-328
RUNS=${1:-1000}
SEED=${2:-1}
OUT=${3:-$HOME/fcnet-02-328}
mkdir -p "$OUT"
rm -f "$OUT"/f*.mps
make -s B="$OUT/build" EXTRA_CFLAGS=-DJAOS_MIP_NET_MIN_COLS_VALUE=1 \
    "$OUT/build/release/libjaos.a" > "$OUT/build.log" 2>&1 || {
    tail -20 "$OUT/build.log"
    exit 1
}
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -Iinclude \
    -o "$OUT/fcnet" "$HERE/fcnet.c" "$OUT/build/release/libjaos.a" -lm \
    -pthread || exit 1
"$OUT/fcnet" "$RUNS" "$SEED" "$OUT"
