#!/bin/bash
# The reading behind 02-435: mirrorcone.c on seeds 1 to 10, 20000 models a
# seed, in mixed and in continuous mode, with the library given (default:
# the working tree's). Records land in OUT (default ~/read435).
#
#   LIB=libjaos.a OUT=dir read.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=${LIB:-$R/build/release/libjaos.a}
O=${OUT:-$HOME/read435}
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mirrorcone \
    $R/bench/measurements/02-435/mirrorcone.c $L -lm -pthread || exit 1
for mode in mixed continuous; do
  arg=$([ $mode = continuous ] && echo continuous)
  for s in $(seq 1 10); do
    ($O/mirrorcone 20000 $s $arg > $O/$mode-$s.txt 2>&1) &
  done
  wait
  for s in $(seq 1 10); do echo "== $mode seed $s"; cat $O/$mode-$s.txt; done > $O/$mode.txt
done
echo done > $O/done
