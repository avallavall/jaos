#!/bin/bash
# The linear half of 02-435's reading: mirror.c on seeds 1 to 10, 20000
# models a seed, in each of its four modes, with the library given
# (default: the working tree's). Records land in OUT (default
# ~/readlin435).
#
#   LIB=libjaos.a OUT=dir readlin.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=${LIB:-$R/build/release/libjaos.a}
O=${OUT:-$HOME/readlin435}
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mirror \
    $R/bench/measurements/02-435/mirror.c $L -lm -pthread || exit 1
for mode in lp mixed qp miqp; do
  for s in $(seq 1 10); do
    ($O/mirror 20000 $s $mode > $O/$mode-$s.txt 2>&1) &
  done
  wait
  for s in $(seq 1 10); do echo "== $mode seed $s"; cat $O/$mode-$s.txt; done > $O/$mode.txt
done
echo done > $O/done
