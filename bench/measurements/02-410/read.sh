#!/bin/bash
# The readings behind 02-410: edge.c on seeds 1 to 3, 3000 models each, on
# the working tree and on c1ecd23's parent, the tree before 02-409's
# change, built in ~/jaos-pre410 from git archive. Records land in
# ~/read410.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read410
rm -rf $O; mkdir -p $O
rm -rf $HOME/jaos-pre410; mkdir -p $HOME/jaos-pre410
(cd $R && git archive c1ecd23^) | tar -x -C $HOME/jaos-pre410
(cd $HOME/jaos-pre410 && make -s build/release/libjaos.a > $O/pre-build.log 2>&1) || { echo "pre build failed" > $O/done; exit 1; }
(cd $R && make -s build/release/libjaos.a > $O/new-build.log 2>&1) || { echo "new build failed" > $O/done; exit 1; }
for arm in pre new; do
  L=$([ $arm = pre ] && echo $HOME/jaos-pre410/build/release/libjaos.a || echo $R/build/release/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/edge-$arm $R/bench/measurements/02-410/edge.c $L -lm -pthread || exit 1
  for s in 1 2 3; do echo "== seed $s"; $O/edge-$arm 3000 $s; done > $O/edge-$arm.txt 2>&1
  gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/edgeqp-$arm $R/bench/measurements/02-410/edgeqp.c $L -lm -pthread || exit 1
  for s in 1 2 3; do echo "== seed $s"; $O/edgeqp-$arm 3000 $s; done > $O/edgeqp-$arm.txt 2>&1
done
echo done > $O/done
