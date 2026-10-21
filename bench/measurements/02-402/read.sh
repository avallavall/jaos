#!/bin/bash
# The readings behind 02-402: each set at 1 and at 4 threads, compared
# instance by instance on status, work units and the digest of the answer's
# bits. Maros-Meszaros and CBLIB as their gates run them; Netlib under the
# barrier and under the concurrent solve; MIPLIB 3 with tree batches of 4.
# Records land in ~/read402.
#
#   read.sh       needs gcc-14 and the instance folders of the gates
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/measurements/02-402
O=$HOME/read402
rm -rf $O; mkdir -p $O
cd $R
make -s build/bench/run > $O/build.log 2>&1 || { echo "build failed" > $O/done; exit 1; }
RUN=$R/build/bench/run
for t in 1 4; do
  $RUN -j 3 -m bench/maros-meszaros.manifest -d bench/instances-maros-meszaros -O threads=$t -o $O/mm-$t.txt > $O/mm-$t.log 2>&1
  $RUN -j 3 -m bench/cblib.manifest -x cbf.gz -d bench/instances-cblib -O threads=$t -o $O/cblib-$t.txt > $O/cblib-$t.log 2>&1
  $RUN -j 3 -O algorithm=barrier -O threads=$t -o $O/barrier-$t.txt > $O/barrier-$t.log 2>&1
  $RUN -j 3 -O algorithm=concurrent -O threads=$t -o $O/concurrent-$t.txt > $O/concurrent-$t.log 2>&1
  $RUN -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -O mip_tree_batch=4 -O threads=$t -o $O/batch-$t.txt > $O/batch-$t.log 2>&1
done
for s in mm cblib barrier concurrent batch; do
  python3 $D/compare.py $O/$s-1.txt $O/$s-4.txt > $O/$s.cmp
done
echo done > $O/done
