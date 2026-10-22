#!/bin/bash
# 02-425's warmcone.c with 02-426's pairs.patch, misocp mode on seeds 1 to
# 3, on HEAD and on the working tree's library, then 02-426's pairs.py
# over each seed's two records. Records land in ~/pairs431.
#
#   pairs.sh      needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$R/bench/measurements/02-426
O=$HOME/pairs431
rm -rf $O; mkdir -p $O/head $O/new-lib
git -C $R archive HEAD | tar -x -C $O/head
(cd $O/head && make all > build.log 2>&1) || { echo "head build failed" > $O/done; exit 1; }
(cd $R && make all > $O/new-build.log 2>&1) || { echo "new build failed" > $O/done; exit 1; }
cp $R/build/release/libjaos.a $O/new-lib/
cp $R/bench/measurements/02-425/warmcone.c $O/warmcone.c
(cd $O && patch -s -p1 < $P/pairs.patch) || { echo "patch failed" > $O/done; exit 1; }
for arm in head new; do
  L=$([ $arm = head ] && echo $O/head/build/release/libjaos.a || echo $O/new-lib/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/w-$arm $O/warmcone.c $L -lm -pthread || { echo "build failed" > $O/done; exit 1; }
  for s in 1 2 3; do
    mkdir -p $O/$arm-run$s
    (cd $O/$arm-run$s && $O/w-$arm 300 $s 8 misocp > $O/$arm$s.txt 2>&1) &
  done
done
wait
for s in 1 2 3; do
  echo "== seed $s"
  python3 $P/pairs.py $O/head$s.txt $O/new$s.txt
done > $O/pairs.txt
echo done > $O/done
