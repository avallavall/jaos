#!/bin/bash
# Builds range.c against the tree's library and checks its verdicts with
# range_check.py over two seeds.
#
#   range.sh [COUNT]
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 9
make -s -j4 build/release/libjaos.a 2>&1 | grep " error"
S=${TMPDIR:-/tmp}
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -Isrc \
    -o "$S/range" bench/measurements/02-369/range.c \
    build/release/libjaos.a -lm || exit 1
for seed in 1 2; do
  echo "seed $seed"
  "$S/range" "${1:-4000}" $seed | python3 bench/measurements/02-369/range_check.py
done
