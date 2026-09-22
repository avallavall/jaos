#!/bin/bash
# Compares two trees line for line over 02-230's generated models.
#
#   compare.sh OUT_DIR TREE_A TREE_B      each TREE a path to a checkout
#
# SPDX-License-Identifier: Apache-2.0
S=${1:-/tmp/relax-02-297}
mkdir -p $S
H=$(cd "$(dirname "$0")/../02-230" && pwd)/relax.c
for w in "${2:?tree A}" "${3:?tree B}"; do
  cd "$w" || exit 9
  make -s -j4 build/release/libjaos.a 2>&1 | grep " error"
  gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$S/relax-$(basename $w)" $H build/release/libjaos.a -lm || exit 1
done
A=$(basename "$2"); B=$(basename "$3")
for seed in 1 2; do
  timeout 1800 "$S/relax-$A" 2000 $seed 100000000 > "$S/$A-$seed.txt"
  timeout 1800 "$S/relax-$B" 2000 $seed 100000000 > "$S/$B-$seed.txt"
  if diff -q "$S/$A-$seed.txt" "$S/$B-$seed.txt" > /dev/null; then
    echo "seed $seed: identical, $(tail -1 "$S/$A-$seed.txt")"
  else
    echo "seed $seed: DIFFERS"
    diff "$S/$A-$seed.txt" "$S/$B-$seed.txt" | head -20
  fi
done
