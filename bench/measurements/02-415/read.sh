#!/bin/bash
# The reading behind 02-415: QPLIB's 17 convex MIQPs at 1e10 work units on
# three builds. 9f is the tree of 9f69696, before 02-413's last push; head
# is the tree of 3d9213e, with it; new is the working tree, where the
# barrier hands all its work on when it gives up on a refused point.
# Records land in ~/miqp3.
#
#   read.sh       needs git and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=~/miqp3
rm -rf $O; mkdir -p $O
for arm in 9f head new; do
    T=~/m-$arm
    rm -rf $T; mkdir -p $T
    case $arm in
        9f) (cd $R && git archive 9f69696 src include cli Makefile) | tar -x -C $T ;;
        head) (cd $R && git archive 3d9213e src include cli Makefile) | tar -x -C $T ;;
        new) rsync -a $R/src $R/include $R/cli $R/Makefile $T/ ;;
    esac
    mkdir -p $T/bench; cp $R/bench/*.c $T/bench/
    (cd $T && make -s build/cli/jaos > $O/build-$arm.log 2>&1) || exit 1
done
one() {
    arm=$1; n=$2; o=$O/miqp-$arm-$n.txt
    ulimit -v 4000000
    timeout 1500 ~/m-$arm/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 > $o 2>&1
    get() { sed -n "s/^$1 //p" $o | head -1; }
    printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)" >> $O/miqp-$arm.txt
}
export -f one
export O
for arm in 9f head new; do
    for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
        echo "$arm $n"
    done
done | xargs -P 3 -L 1 bash -c 'one $0 $1'
for arm in 9f head new; do sort -o $O/miqp-$arm.txt $O/miqp-$arm.txt; done
echo done > $O/done
