#!/bin/bash
# The readings behind 02-376, the working tree against HEAD: the 3000
# generated conic models of 02-253 and the 3000 mixed-integer ones of
# 02-255, QPLIB's 13 continuous QCQPs (02-316's qcqp.sh) and its 14
# mixed-integer ones (miqcqp.sh) at 1e10, then `make cblib` and the
# mixed-integer QCQPs at 1e11 on the working tree. HEAD is built in
# ~/jaos-head from git archive; records land in ~/read376.
#
#   read.sh       needs gcc-14 and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-376
OUT=$HOME/read376
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make all cli > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make all cli build/bench/run > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
mkdir -p $OUT/new-bin
cp $R/build/cli/jaos $R/build/release/libjaos.a $OUT/new-bin/
for arm in head new; do
  T=$([ $arm = head ] && echo $HOME/jaos-head/build || echo $OUT/new-bin)
  JB=$([ $arm = head ] && echo $T/cli/jaos || echo $T/jaos)
  LB=$([ $arm = head ] && echo $T/release/libjaos.a || echo $T/libjaos.a)
  (
    for seed in 1 2 3; do
      echo "== seed $seed"
      (cd $R && LIB=$LB bash bench/measurements/02-253/conic.sh 1000 $seed $OUT/gen-$arm-$seed | tail -16)
    done > $OUT/gen-$arm.txt 2>&1
    for seed in 1 2 3; do
      echo "== seed $seed"
      (cd $R && LIB=$LB bash bench/measurements/02-255/misocp.sh 1000 $seed $OUT/mip-$arm-$seed 2>&1 | grep -v lto-)
    done > $OUT/mip-$arm.txt 2>&1
    (cd $R && JB=$JB OUT=$OUT/qcqp-$arm.txt bash bench/measurements/02-316/qcqp.sh)
    JB=$JB OUT=$OUT/miqcqp-$arm.txt bash $H/miqcqp.sh
  ) &
done
wait
(cd $R && make cblib J=3 > $OUT/cblib.log 2>&1)
JB=$OUT/new-bin/jaos LIMIT=100000000000 OUT=$OUT/miqcqp-new-1e11.txt bash $H/miqcqp.sh
echo done > $OUT/done
