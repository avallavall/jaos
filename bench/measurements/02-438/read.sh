#!/bin/bash
# The reading behind 02-438: the tree of 8458cbd (base) against the working
# tree (walk), whose push frees one pin a round once a release round is
# undone at once (2ba1ad3). 02-365's and 02-366's circulations, 02-248's
# generated QPs, Maros-Meszaros, QPLIB's 11 convex QPs and its 17 convex
# MIQPs at 1e10 work units. Records land in ~/read438.
#
#   read.sh       needs gcc-14, python3, git and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=$HOME/read438
rm -rf $O; mkdir -p $O
for arm in base walk; do
    T=$HOME/r438-$arm
    rm -rf $T; mkdir -p $T/bench
    if [ $arm = walk ]; then
        rsync -a $R/src $R/include $R/cli $R/Makefile $T/
    else
        (cd $R && git archive 8458cbd src include cli Makefile) | tar -x -C $T
    fi
    cp $R/bench/*.c $T/bench/
    (cd $T && make -s build/cli/jaos build/bench/run build/release/libjaos.a > $O/build-$arm.log 2>&1) || exit 1
done
B=$HOME/r438-base/build/cli/jaos
N=$HOME/r438-walk/build/cli/jaos
python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1 400 > $O/nq-12.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1001 1800 > $O/nq-12n.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $O/nq 40 100 1 120 > $O/nq-40.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $O/nq 40 100 1001 1200 > $O/nq-40n.txt 2>&1
python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1 400 > $O/nqb-12.txt 2>&1
python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1001 1800 > $O/nqb-12n.txt 2>&1
for arm in base walk; do
    T=$HOME/r438-$arm
    gcc-14 -std=c23 -O2 -ffp-contract=off -I$T/include -o $O/push-$arm $M/02-248/push.c $T/build/release/libjaos.a -lm -pthread
    mkdir -p $O/gen-$arm
    for seed in 1 2 3 4 5 6; do echo "== seed $seed"; $O/push-$arm 1000 $seed $O/gen-$arm | tail -8; done > $O/gen-$arm.txt 2>&1
    (cd $R && $T/build/bench/run -j 3 -m bench/maros-meszaros.manifest -d bench/instances-maros-meszaros -o $O/mm-$arm.txt > $O/mm-$arm.log 2>&1)
    for n in QPLIB_8495 QPLIB_8515 QPLIB_8616 QPLIB_8785 QPLIB_8790 QPLIB_8792 QPLIB_8845 QPLIB_8906 QPLIB_8938 QPLIB_8991 QPLIB_9002; do
        echo $HOME/qplib/$n.qplib
    done | JB=$T/build/cli/jaos xargs -P 3 -I{} bash $M/02-295/cqp1.sh {} r438$arm 100000000000 | sort > $O/cqp-$arm.txt
done
python3 -I $M/02-413/mmcmp.py $O/mm-base.txt $O/mm-walk.txt > $O/mm-cmp.txt
one() {
    arm=$1; n=$2; o=$O/miqp-$arm-$n.txt
    ulimit -v 4000000
    timeout 1500 $HOME/r438-$arm/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 > $o 2>&1
    get() { sed -n "s/^$1 //p" $o | head -1; }
    printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)" >> $O/miqp-$arm.txt
}
export -f one
export O
for arm in base walk; do
    for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
        echo "$arm $n"
    done
done | xargs -P 3 -L 1 bash -c 'one $0 $1'
for arm in base walk; do sort -o $O/miqp-$arm.txt $O/miqp-$arm.txt; done
echo done > $O/done
