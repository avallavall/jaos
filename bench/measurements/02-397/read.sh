#!/bin/bash
# The readings behind 02-397, the working tree against HEAD: mipopts.c on
# seeds 1 to 3, 1000 MIPs each under 49 option variants; then
# `make miplib J=2` on the working tree. HEAD is built in ~/jaos-head
# from git archive; records land in ~/read397.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/read397
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make -s build/release/libjaos.a > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make -s build/release/libjaos.a > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
for arm in head new; do
  L=$([ $arm = head ] && echo $HOME/jaos-head/build/release/libjaos.a || echo $R/build/release/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $OUT/mipopts-$arm $R/bench/measurements/02-397/mipopts.c $L -lm -pthread || exit 1
  ( for s in 1 2 3; do echo "== seed $s"; $OUT/mipopts-$arm 1000 $s; done > $OUT/$arm.txt 2>&1 ) &
done
wait
(cd $R && make miplib J=2 > $OUT/miplib.log 2>&1)
echo done > $OUT/done
