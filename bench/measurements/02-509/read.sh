#!/bin/bash
# The reading behind 02-509: the tree of BASE (base) against BASE with
# refine.py (ref), on 02-484's sets: 02-365's and 02-366's circulations, 02-248's
# generated QPs, QPLIB's 11 convex QPs, Maros-Meszaros against its
# baseline, and QPLIB's 17 convex MIQPs at 1e10 work units. Records land in
# ~/read509.
#
#   read.sh [qp|mm|miqp|all] [STEPS] [GATE]   needs gcc-14, python3, git
#                                             and QPLIB in ~/qplib
#
# STEPS is the refinement steps of the ref arm (BARRIER_QP_REFINE), 1 by
# default; GATE is all (every QP Newton step) or far (only while the duals
# are past BARRIER_DIVERGE times the data), retry (a second walk with the
# step after a walk with no answer) or refused (a second walk after a walk
# that converged on a refused point). The ref arm's records carry both in
# their name (ref1, ref1far...). BASE is a revision refine.py applies to,
# b08f2c17 by default.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=$HOME/read509
what=${1:-all}
steps=${2:-1}
gate=${3:-all}
J=${J:-4}
BASE=${BASE:-b08f2c17}
MMI=${MMI:-$R/bench/instances-maros-meszaros}
A=ref$steps
[ $gate = all ] || A=ref$steps$gate
build() {
    arm=$1; T=$HOME/r509-$arm
    rm -rf $T; mkdir -p $T/bench
    (cd $R && git archive $BASE src include cli Makefile) | tar -x -C $T
    [ $arm = base ] || python3 $M/02-509/refine.py $T $steps $gate || return 1
    cp $R/bench/*.c $T/bench/
    (cd $T && make -s build/cli/jaos build/bench/run build/release/libjaos.a > $O/build-$arm.log 2>&1)
}
mm() {
    for arm in base $A; do
        T=$HOME/r509-$arm
        (cd $R && $T/build/bench/run -j $J -m bench/maros-meszaros.manifest -d $MMI -b bench/maros-meszaros.baseline -o $O/mm-$arm.txt > $O/mm-$arm.log 2>&1)
    done
    python3 -I $M/02-413/mmcmp.py $O/mm-base.txt $O/mm-$A.txt > $O/mm-cmp-$A.txt
}
mkdir -p $O
B=$HOME/r509-base/build/cli/jaos
N=$HOME/r509-$A/build/cli/jaos
if [ $what = qp ] || [ $what = all ]; then
    [ -x $B ] || build base || exit 1
    build $A || exit 1
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1 400 > $O/nq-12-$A.txt 2>&1 &
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1001 1800 > $O/nq-12n-$A.txt 2>&1 &
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 40 100 1 120 > $O/nq-40-$A.txt 2>&1 &
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 40 100 1001 1200 > $O/nq-40n-$A.txt 2>&1 &
    wait
    python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1 400 > $O/nqb-12-$A.txt 2>&1 &
    python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1001 1800 > $O/nqb-12n-$A.txt 2>&1 &
    wait
    for arm in base $A; do
        [ $arm = base ] && [ -s $O/cqp-base.txt ] && continue
        T=$HOME/r509-$arm
        gcc-14 -std=c23 -O2 -ffp-contract=off -I$T/include -o $O/push-$arm $M/02-248/push.c $T/build/release/libjaos.a -lm -pthread
        mkdir -p $O/gen-$arm
        for seed in 1 2 3 4 5 6; do echo "== seed $seed"; $O/push-$arm 1000 $seed $O/gen-$arm | tail -8; done > $O/gen-$arm.txt 2>&1
        for n in QPLIB_8495 QPLIB_8515 QPLIB_8616 QPLIB_8785 QPLIB_8790 QPLIB_8792 QPLIB_8845 QPLIB_8906 QPLIB_8938 QPLIB_8991 QPLIB_9002; do
            echo $HOME/qplib/$n.qplib
        done | JB=$T/build/cli/jaos xargs -P 3 -I{} bash $M/02-295/cqp1.sh {} r509$arm 100000000000 | sort > $O/cqp-$arm.txt
    done
fi
if [ $what = qp ] || [ $what = mm ] || [ $what = all ]; then
    mm
fi
if [ $what = miqp ] || [ $what = all ]; then
    one() {
        arm=$1; n=$2; o=$O/miqp-$arm-$n.txt
        ulimit -v 4000000
        timeout 1500 $HOME/r509-$arm/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 > $o 2>&1
        get() { sed -n "s/^$1 //p" $o | head -1; }
        printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)" >> $O/miqp-$arm.txt
    }
    export -f one
    export O
    arms="$A"; [ -s $O/miqp-base.txt ] || arms="base $A"
    for arm in $arms; do
        rm -f $O/miqp-$arm.txt
        for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
            echo "$arm $n"
        done
    done | xargs -P 2 -L 1 bash -c 'one $0 $1'
    for arm in base $A; do sort -o $O/miqp-$arm.txt $O/miqp-$arm.txt; done
fi
echo "done $what $A" > $O/done-$what-$A
