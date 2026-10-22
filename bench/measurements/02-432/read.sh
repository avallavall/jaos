#!/bin/bash
# The reading behind 02-432: 02-431's permcone.c in mixed mode on seeds 1
# to 10, 20000 models a seed, with the working tree's library. Records land
# in ~/read432.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read432
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/permcone \
    $R/bench/measurements/02-431/permcone.c $R/build/release/libjaos.a -lm -pthread || exit 1
for s in $(seq 1 10); do
  ($O/permcone 20000 $s > $O/mixed-$s.txt 2>&1) &
done
wait
for s in $(seq 1 10); do echo "== mixed seed $s"; cat $O/mixed-$s.txt; done > $O/mixed.txt
echo done > $O/done
