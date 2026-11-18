#!/bin/bash
# Builds shadow.c against the tree's library and runs it over two seeds.
#
#   shadow.sh [COUNT [BUDGET]]
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 9
make -s -j4 build/release/libjaos.a 2>&1 | grep " error"
S=${TMPDIR:-/tmp}
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -Isrc \
    -o "$S/shadow" bench/measurements/02-486/shadow.c \
    build/release/libjaos.a -lm || exit 1
for seed in 1 2; do
  echo "seed $seed"
  "$S/shadow" "${1:-2000}" $seed ${2:+"$2"}
done
