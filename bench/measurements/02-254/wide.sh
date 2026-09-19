#!/usr/bin/env bash
# The cone-width reading of 02-254: one cone of W members at widths from 8
# to 200000, each solve's status, error against -||a||, iterations and work.
#
#   wide.sh [OUT]      needs `make all`
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-254
OUT=${1:-$HOME/wide-02-254}
LIB=${LIB:-build/release/libjaos.a}
mkdir -p "$OUT"
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -Iinclude \
    -o "$OUT/wide" "$HERE/wide.c" "$LIB" -lm -pthread || exit 1
for w in 8 16 32 64 128 256 512 1024 4096 20000 50000 200000; do
    "$OUT/wide" "$w"
done
