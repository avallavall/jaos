#!/bin/bash
# The reading behind 02-431: permcone.c on seeds 1 to 10, 20000 models a
# seed, with the discrete shapes (mixed) and without them (continuous),
# with the library given (default: the working tree's). Records land in
# OUT (default ~/read431).
#
#   LIB=libjaos.a OUT=dir read.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=${LIB:-$R/build/release/libjaos.a}
O=${OUT:-$HOME/read431}
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/permcone \
    $R/bench/measurements/02-431/permcone.c $L -lm -pthread || exit 1
(
  for s in $(seq 1 10); do echo "== mixed seed $s"; $O/permcone 20000 $s; done
) > $O/mixed.txt 2>&1 &
(
  for s in $(seq 1 10); do echo "== continuous seed $s"; $O/permcone 20000 $s continuous; done
) > $O/continuous.txt 2>&1 &
wait
echo done > $O/done
