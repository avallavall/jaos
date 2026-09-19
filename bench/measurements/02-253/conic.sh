#!/usr/bin/env bash
# The reading behind 02-253: generated convex QCQPs and SOCPs, each answer
# judged by the checker, by a copy, by the MPS the model writes, and by the
# same model with its quadratic rows rewritten as rotated cones.
#
#   conic.sh [RUNS] [SEED] [OUT]      default 1000 models, seed 1
#
# Needs `make all`. A model that fails a check is written to OUT as
# fNNNNN.mps.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-253
RUNS=${1:-1000}
SEED=${2:-1}
OUT=${3:-$HOME/conic-02-253}
LIB=${LIB:-build/release/libjaos.a}
mkdir -p "$OUT"
rm -f "$OUT"/f*.mps "$OUT"/c*.mps

gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -Iinclude \
    -o "$OUT/conic" "$HERE/conic.c" "$LIB" -lm -pthread || exit 1
"$OUT/conic" "$RUNS" "$SEED" "$OUT"
