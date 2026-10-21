#!/bin/bash
# The readings behind 02-404: warmqp.c on seeds 1 to 3, 1000 models of 8
# edits each, for the QP and for the MIQP. Records land in ~/read404.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read404
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/warmqp $R/bench/measurements/02-404/warmqp.c $R/build/release/libjaos.a -lm -pthread || { echo "warmqp build failed" > $O/done; exit 1; }
for md in qp miqp; do
  for s in 1 2 3; do
    echo "== $md seed $s"
    $O/warmqp 1000 $s $md 8
  done
done > $O/warm.txt 2>&1
echo done > $O/done
