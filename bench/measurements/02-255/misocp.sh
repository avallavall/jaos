#!/usr/bin/env bash
# The reading behind 02-255: generated mixed-integer conic models, the
# conic branch and bound against brute force over the integer values.
#
#   misocp.sh [RUNS] [SEED] [OUT]      default 1000 models, seed 1
#
# Needs `make all`. A model that fails a check is written to OUT as
# fNNNNN.mps.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-255
RUNS=${1:-1000}
SEED=${2:-1}
OUT=${3:-$HOME/misocp-02-255}
LIB=${LIB:-build/release/libjaos.a}
mkdir -p "$OUT"
rm -f "$OUT"/f*.mps

gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -Iinclude \
    -o "$OUT/misocp" "$HERE/misocp.c" "$LIB" -lm -pthread || exit 1
"$OUT/misocp" "$RUNS" "$SEED" "$OUT"
