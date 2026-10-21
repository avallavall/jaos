#!/bin/bash
# The readings behind 02-409: iis.c on seeds 1 to 3, 3000 models each.
# Records land in ~/read409.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read409
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/iis $R/bench/measurements/02-409/iis.c $R/build/release/libjaos.a -lm -pthread || { echo "iis build failed" > $O/done; exit 1; }
for s in 1 2 3; do
  echo "== seed $s"
  $O/iis 3000 $s
done > $O/iis.txt 2>&1
echo done > $O/done
