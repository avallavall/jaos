#!/bin/bash
# The readings behind 02-318, the tree in the working copy against HEAD:
# the 6000 generated QPs of 02-248, QPLIB's continuous convex QPs that
# finish under 1e11 work units, and its 17 convex MIQPs at 1e10. HEAD is
# built in ~/jaos-base from git archive; the records land in ~/qpread and
# ~/cqp-base, ~/cqp-new.
#
#   qpread.sh        needs gcc-14 and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/qpread
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-base; mkdir -p $HOME/jaos-base
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-base
(cd $HOME/jaos-base && make all cli > $OUT/base-build.log 2>&1) || { echo "base build failed" > $OUT/done; exit 1; }
(cd $R && make all cli > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
for arm in base new; do
  T=$([ $arm = base ] && echo $HOME/jaos-base || echo $R)
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$T/include -o $OUT/push-$arm $R/bench/measurements/02-248/push.c $T/build/release/libjaos.a -lm -pthread
  mkdir -p $OUT/gen-$arm
  for seed in 1 2 3 4 5 6; do echo "== seed $seed"; $OUT/push-$arm 1000 $seed $OUT/gen-$arm | tail -8; done > $OUT/gen-$arm.txt 2>&1
  for n in QPLIB_8495 QPLIB_8515 QPLIB_8616 QPLIB_8785 QPLIB_8790 QPLIB_8792 QPLIB_8845 QPLIB_8906 QPLIB_8938 QPLIB_8991 QPLIB_9002; do
    echo $HOME/qplib/$n.qplib
  done | JB=$T/build/cli/jaos xargs -P 3 -I{} bash $R/bench/measurements/02-295/cqp1.sh {} $arm 100000000000 | sort > $OUT/cqp-$arm.txt
  for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do echo $n; done | \
  xargs -P 3 -I{} bash -c 'n={}; o='$OUT'/miqp-'$arm'-$n.txt; ulimit -v 4000000; timeout 1500 '$T'/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 > $o 2>&1; get() { sed -n "s/^$1 //p" $o | head -1; }; printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)"' | sort > $OUT/miqp-$arm.txt
done
echo done > $OUT/done
