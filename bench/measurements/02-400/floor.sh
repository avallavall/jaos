#!/bin/bash
# QP_PUSH_ROUND at 0, 1e-14, 1e-13 and 1e-12 on 02-398's seven
# circulations: the tree is copied to ~/floor400 and the constant edited in
# each copy. Needs 02-398's read.sh models in ~/read398; the lines land in
# ~/floor400/floor.txt.
#
#   floor.sh      needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/floor400
rm -rf $O; mkdir -p $O
for f in 0.0 1e-14 1e-13 1e-12; do
  T=$O/t$f
  mkdir -p $T
  rsync -a $R/src $R/include $R/cli $R/Makefile $T/
  sed -i "s/QP_PUSH_ROUND  = 1e-13;/QP_PUSH_ROUND  = $f;/" $T/src/barrier.c
  (cd $T && make -s build/cli/jaos > build.log 2>&1) || { echo "build $f failed" >> $O/floor.txt; continue; }
  for s in 1104 1156 1163 1282 1340 1357 1308; do
    r=$($T/build/cli/jaos solve $HOME/read398/n$s.mps 2>&1)
    echo "$f $s $(echo "$r" | grep -E '^(status|objective|work_units)' | awk '{print $2}' | tr '\n' ' ')"
  done >> $O/floor.txt
done
echo done > $O/done
