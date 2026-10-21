#!/bin/bash
# The readings behind 02-408: answers.c on seeds 1 to 3, 3000 models each.
# Records land in ~/read408.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read408
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/answers $R/bench/measurements/02-408/answers.c $R/build/release/libjaos.a -lm -pthread || { echo "answers build failed" > $O/done; exit 1; }
for s in 1 2 3; do
  echo "== seed $s"
  $O/answers 3000 $s $O
done > $O/answers.txt 2>&1
echo done > $O/done
