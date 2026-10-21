#!/bin/bash
# always.py at K = 0.1 on the other QP sets, against the tree: 02-248's
# 6000 generated QPs, QPLIB's 11 continuous convex QPs and Maros-Meszaros.
# Needs always.sh's copies in ~/pr401b and ~/pr401-0.1 and QPLIB in
# ~/qplib; records land in ~/pr403.
#
#   broad.sh      needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=~/pr403; rm -rf $O; mkdir -p $O
for arm in base new; do
  T=$([ $arm = base ] && echo ~/pr401b || echo ~/pr401-0.1)
  mkdir -p $T/bench; cp $R/bench/*.c $T/bench/ 2>/dev/null; cp $R/bench/*.h $T/bench/ 2>/dev/null
  (cd $T && make -s build/bench/run build/release/libjaos.a > $O/build-$arm.log 2>&1) || { echo "build $arm failed" >> $O/status; continue; }
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$T/include -o $O/push-$arm $M/02-248/push.c $T/build/release/libjaos.a -lm -pthread
  mkdir -p $O/gen-$arm
  for seed in 1 2 3 4 5 6; do echo "== seed $seed"; $O/push-$arm 1000 $seed $O/gen-$arm | tail -8; done > $O/gen-$arm.txt 2>&1
  echo "gen $arm done" >> $O/status
  for n in QPLIB_8495 QPLIB_8515 QPLIB_8616 QPLIB_8785 QPLIB_8790 QPLIB_8792 QPLIB_8845 QPLIB_8906 QPLIB_8938 QPLIB_8991 QPLIB_9002; do
    echo $HOME/qplib/$n.qplib
  done | JB=$T/build/cli/jaos xargs -P 3 -I{} bash $M/02-295/cqp1.sh {} pr403$arm 100000000000 | sort > $O/cqp-$arm.txt
  echo "cqp $arm done" >> $O/status
  (cd $R && $T/build/bench/run -j 6 -m bench/maros-meszaros.manifest -d bench/instances-maros-meszaros -b bench/maros-meszaros.baseline -o $O/mm-$arm.txt > $O/mm-$arm.log 2>&1)
  echo "mm $arm done" >> $O/status
done
echo done > $O/done
