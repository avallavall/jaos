#!/bin/bash
# 02-431's permcone.c on seeds 1 to 10, 20000 models a seed, in mixed and
# in continuous mode, with the working tree's library. Records land in
# ~/perm433.
#
#   perm.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/perm433
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/permcone \
    $R/bench/measurements/02-431/permcone.c $R/build/release/libjaos.a -lm -pthread || exit 1
for mode in mixed continuous; do
  arg=$([ $mode = continuous ] && echo continuous)
  for s in $(seq 1 10); do
    ($O/permcone 20000 $s $arg > $O/$mode-$s.txt 2>&1) &
  done
  wait
  for s in $(seq 1 10); do echo "== $mode seed $s"; cat $O/$mode-$s.txt; done > $O/$mode.txt
done
echo done > $O/done
