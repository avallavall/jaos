#!/bin/bash
# The reading behind 02-396, the working tree against HEAD: algos.c on
# seeds 1 to 3, 1000 LPs each, every one solved by the dual, the primal,
# the barrier, PDLP and the concurrent solve. HEAD is built in ~/jaos-head
# from git archive; records land in ~/read396.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/read396
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make -s build/release/libjaos.a > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make -s build/release/libjaos.a > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
for arm in head new; do
  L=$([ $arm = head ] && echo $HOME/jaos-head/build/release/libjaos.a || echo $R/build/release/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $OUT/algos-$arm $R/bench/measurements/02-396/algos.c $L -lm -pthread || exit 1
  for s in 1 2 3; do echo "== seed $s"; $OUT/algos-$arm 1000 $s; done > $OUT/$arm.txt 2>&1
done
echo done > $OUT/done
