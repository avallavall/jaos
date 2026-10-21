#!/bin/bash
# The readings behind 02-413. Three builds: base, the tree of 9f69696;
# every, base with every.py's margin of 100 in every push's pin rule; last,
# the working tree, whose last push pins only where the dual is 1000 times
# the slack. Each runs 02-248's push.c on seeds 1 to 6, the 11 QPLIB convex
# QPs of 02-295 and Maros-Meszaros. Then 02-365's and 02-366's circulations
# compare every and last against base. Records land in ~/read413.
#
#   read.sh       needs gcc-14, python3, git and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=$HOME/read413
rm -rf $O; mkdir -p $O
for arm in base every last; do
    T=$HOME/r413-$arm
    rm -rf $T; mkdir -p $T/bench
    if [ $arm = last ]; then
        rsync -a $R/src $R/include $R/cli $R/Makefile $T/
    else
        (cd $R && git archive 9f69696 src include cli Makefile) | tar -x -C $T
    fi
    cp $R/bench/*.c $T/bench/
    if [ $arm = every ]; then (cd $T && python3 -I $M/02-413/every.py 100.0) || exit 1; fi
    (cd $T && make -s build/cli/jaos build/bench/run build/release/libjaos.a > $O/build-$arm.log 2>&1) || exit 1
    gcc-14 -std=c23 -O2 -ffp-contract=off -I$T/include -o $O/push-$arm $M/02-248/push.c $T/build/release/libjaos.a -lm -pthread
    mkdir -p $O/gen-$arm
    for seed in 1 2 3 4 5 6; do echo "== seed $seed"; $O/push-$arm 1000 $seed $O/gen-$arm | tail -8; done > $O/gen-$arm.txt 2>&1
    for n in QPLIB_8495 QPLIB_8515 QPLIB_8616 QPLIB_8785 QPLIB_8790 QPLIB_8792 QPLIB_8845 QPLIB_8906 QPLIB_8938 QPLIB_8991 QPLIB_9002; do
        echo $HOME/qplib/$n.qplib
    done | JB=$T/build/cli/jaos xargs -P 3 -I{} bash $M/02-295/cqp1.sh {} r413$arm 100000000000 | sort > $O/cqp-$arm.txt
    (cd $R && $T/build/bench/run -j 6 -m bench/maros-meszaros.manifest -d bench/instances-maros-meszaros -o $O/mm-$arm.txt > $O/mm-$arm.log 2>&1)
done
B=$HOME/r413-base/build/cli/jaos
for arm in every last; do
    N=$HOME/r413-$arm/build/cli/jaos
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1 400 > $O/$arm-nq-12.txt 2>&1
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 12 30 1001 1800 > $O/$arm-nq-12n.txt 2>&1
    python3 -I $M/02-365/netqp_read.py $B $N $O/nq 40 100 1 120 > $O/$arm-nq-40.txt 2>&1
    python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1 400 > $O/$arm-nqb-12.txt 2>&1
    python3 -I $M/02-366/netqpb_read.py $B $N $O/nb 12 30 1001 1800 > $O/$arm-nqb-12n.txt 2>&1
done
python3 -I $M/02-365/netqp_read.py $B $HOME/r413-last/build/cli/jaos $O/nq 40 100 1001 1200 > $O/last-nq-40n.txt 2>&1
python3 -I $M/02-413/mmcmp.py $O/mm-base.txt $O/mm-every.txt > $O/mm-every-cmp.txt
python3 -I $M/02-413/mmcmp.py $O/mm-base.txt $O/mm-last.txt > $O/mm-last-cmp.txt
echo done > $O/done
