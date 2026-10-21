#!/bin/bash
# The readings behind 02-405: structedit.c on seeds 1 to 3, 2000 models of
# 40 edits each. Records land in ~/read405.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read405
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/structedit $R/bench/measurements/02-405/structedit.c $R/build/release/libjaos.a -lm -pthread || { echo "structedit build failed" > $O/done; exit 1; }
for s in 1 2 3; do
  echo "== seed $s"
  $O/structedit 2000 $s 40 $O
done > $O/structedit.txt 2>&1
echo done > $O/done
