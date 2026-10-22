#!/bin/bash
# 02-425's warmcone.c with pairs.patch, misocp mode on seed 1, on e92ca4c
# and on the working tree's library, then pairs.py over the two records.
# Records land in ~/pairs426.
#
#   pairs.sh      needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-426
O=$HOME/pairs426
rm -rf $O; mkdir -p $O/head $O/new-lib
git -C $R archive e92ca4c | tar -x -C $O/head
(cd $O/head && make all > build.log 2>&1) || { echo "head build failed"; exit 1; }
(cd $R && make all > $O/new-build.log 2>&1) || { echo "new build failed"; exit 1; }
cp $R/build/release/libjaos.a $O/new-lib/
cp $R/bench/measurements/02-425/warmcone.c $O/warmcone.c
(cd $O && patch -s -p1 < $H/pairs.patch) || exit 1
for arm in head new; do
  L=$([ $arm = head ] && echo $O/head/build/release/libjaos.a || echo $O/new-lib/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/w-$arm $O/warmcone.c $L -lm -pthread || exit 1
  mkdir -p $O/$arm-run
  (cd $O/$arm-run && $O/w-$arm 300 1 8 misocp > $O/$arm.txt 2>&1) &
done
wait
python3 $H/pairs.py $O/head.txt $O/new.txt > $O/pairs.txt
