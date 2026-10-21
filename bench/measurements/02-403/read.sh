#!/bin/bash
# The readings behind 02-403: warmmod.c on seeds 1 to 3, 1000 models of 8
# edits each, for the LP under the dual and the primal simplex and for the
# MIP under the dual. Records land in ~/read403.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read403
rm -rf $O; mkdir -p $O
(cd $R && make -s build/release/libjaos.a > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/warmmod $R/bench/measurements/02-403/warmmod.c $R/build/release/libjaos.a -lm -pthread || { echo "warmmod build failed" > $O/done; exit 1; }
for arm in "lp dual" "lp primal" "mip dual"; do
  set -- $arm
  for s in 1 2 3; do
    echo "== $1 $2 seed $s"
    $O/warmmod 1000 $s $1 $2 8
  done
done > $O/warm.txt 2>&1
echo done > $O/done
