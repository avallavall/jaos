#!/bin/bash
# 02-255's 3000 generated mixed-integer conic models, seeds 1 to 3, on the
# HEAD and working-tree libraries that read.sh leaves in ~/jaos-head and
# ~/read373/new-bin. Run read.sh first.
#
#   mip.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/read373
for arm in head new; do
  LB=$([ $arm = head ] && echo $HOME/jaos-head/build/release/libjaos.a || echo $OUT/new-bin/libjaos.a)
  (
    for seed in 1 2 3; do
      echo "== seed $seed"
      (cd $R && LIB=$LB bash bench/measurements/02-255/misocp.sh 1000 $seed $OUT/mip-$arm-$seed 2>&1 | grep -v lto-)
    done > $OUT/mip-$arm.txt 2>&1
  ) &
done
wait
echo done > $OUT/mip-done
