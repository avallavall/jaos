#!/bin/bash
# The readings behind 02-319, the working tree against HEAD: the 3000
# generated conic models of 02-253, QPLIB's 13 continuous QCQPs (02-316's
# qcqp.sh) and its 17 convex MIQPs at 1e10, and `make cblib` on the
# working tree. HEAD is built in ~/jaos-head from git archive; records land
# in ~/conread.
#
#   conread.sh       needs gcc-14 and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/conread
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make all cli > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make all cli build/bench/run > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
for arm in head new; do
  T=$([ $arm = head ] && echo $HOME/jaos-head || echo $R)
  for seed in 1 2 3; do
    echo "== seed $seed"
    (cd $R && LIB=$T/build/release/libjaos.a bash bench/measurements/02-253/conic.sh 1000 $seed $OUT/gen-$arm-$seed | tail -12)
  done > $OUT/gen-$arm.txt 2>&1
  (cd $R && JB=$T/build/cli/jaos OUT=$OUT/qcqp-$arm.txt bash bench/measurements/02-316/qcqp.sh)
  for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do echo $n; done | \
  xargs -P 3 -I{} bash -c 'n={}; o='$OUT'/miqp-'$arm'-$n.txt; ulimit -v 4000000; timeout 1500 '$T'/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 > $o 2>&1; get() { sed -n "s/^$1 //p" $o | head -1; }; printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)"' | sort > $OUT/miqp-$arm.txt
done
(cd $R && make cblib J=3 > $OUT/cblib.log 2>&1)
echo done > $OUT/done
