#!/bin/bash
# The readings behind 02-411: scaled.c on seeds 1 to 3, 1000 models each,
# with scales up to 1e2, 1e4 and 1e6. Records land in ~/read411.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read411
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/scaled $R/bench/measurements/02-411/scaled.c $R/build/release/libjaos.a -lm -pthread || { echo "scaled build failed" > $O/done; exit 1; }
for k in 2 4 6; do
  for s in 1 2 3; do
    echo "== scale 1e$k seed $s"
    $O/scaled 1000 $s $k
  done
done > $O/scaled.txt 2>&1
echo done > $O/done
