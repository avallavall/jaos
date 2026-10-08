#!/bin/bash
# Builds lattice.c against the tree's library and runs it over two seeds.
#
#   lattice.sh [COUNT]
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 9
make -s -j4 build/release/libjaos.a 2>&1 | grep " error"
S=${TMPDIR:-/tmp}
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -Isrc \
    -o "$S/lattice" bench/measurements/02-362/lattice.c \
    build/release/libjaos.a -lm || exit 1
for seed in 1 2; do
  echo "seed $seed"
  "$S/lattice" "${1:-2000}" $seed
done
