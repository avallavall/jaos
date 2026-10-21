#!/bin/bash
# 02-365's circulations, the tree against copies with stall.py at K = 1e-3 and
# 0.1: 12 nodes on seeds 1 to 400 and 1001 to 1400, 40 nodes on seeds 1 to
# 120 and 1001 to 1200. Records land in ~/pr402.
#
#   stall.sh     needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
S=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=~/pr402; rm -rf $O; mkdir -p $O
mkdir -p ~/pr402b; rsync -a --delete $R/src $R/include $R/cli $R/Makefile ~/pr402b/
(cd ~/pr402b && make -s build/cli/jaos > $O/base-build.log 2>&1) || { echo "base build failed" > $O/done; exit 1; }
B=~/pr402b/build/cli/jaos
for k in 1e-3 0.1; do
  T=~/pr402-$k; mkdir -p $T; rsync -a --delete $R/src $R/include $R/cli $R/Makefile $T/
  (cd $T && python3 $S/stall.py $k && make -s build/cli/jaos > $O/build-$k.log 2>&1) || { echo "build $k failed" >> $O/status; continue; }
  N=$T/build/cli/jaos
  python3 -I $M/02-365/netqp_read.py $B $N $O/nq-$k 12 30 1 400 > $O/nq-12-$k.txt 2>&1
  python3 -I $M/02-365/netqp_read.py $B $N $O/nq-$k 12 30 1001 1400 > $O/nq-12n-$k.txt 2>&1
  python3 -I $M/02-365/netqp_read.py $B $N $O/nq-$k 40 100 1 120 > $O/nq-40-$k.txt 2>&1
  python3 -I $M/02-365/netqp_read.py $B $N $O/nq-$k 40 100 1001 1200 > $O/nq-40n-$k.txt 2>&1
  echo "k $k done" >> $O/status
done
echo done > $O/done
