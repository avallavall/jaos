#!/bin/bash
# warmqc.c with pairs.patch, miqcp mode on seeds 1 to 8, on aba9958 and on
# the working tree's library, then 02-426's pairs.py over each seed's two
# records. Records land in ~/pairs427.
#
#   pairs.sh      needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-427
O=$HOME/pairs427
rm -rf $O; mkdir -p $O/head $O/new-lib
git -C $R archive aba9958 | tar -x -C $O/head
(cd $O/head && make all > build.log 2>&1) || { echo "head build failed"; exit 1; }
(cd $R && make all > $O/new-build.log 2>&1) || { echo "new build failed"; exit 1; }
cp $R/build/release/libjaos.a $O/new-lib/
cp $H/warmqc.c $O/warmqc.c
(cd $O && patch -s -p1 < $H/pairs.patch) || exit 1
for arm in head new; do
  L=$([ $arm = head ] && echo $O/head/build/release/libjaos.a || echo $O/new-lib/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/w-$arm $O/warmqc.c $L -lm -pthread || exit 1
done
for s in 1 2 3 4 5 6 7 8; do
  $O/w-head 1000 $s 8 miqcp > $O/head-$s.txt 2>&1 &
  $O/w-new 1000 $s 8 miqcp > $O/new-$s.txt 2>&1 &
  wait
  echo "== seed $s"
  python3 $R/bench/measurements/02-426/pairs.py $O/head-$s.txt $O/new-$s.txt
done > $O/pairs.txt
