#!/bin/bash
# 02-413's sweep of QP_PUSH_MARGIN on seven circulations that end
# numerical_error on the tree of 9f69696: 02-366's bounded circulation seed
# 1420 and 02-365's seeds 44, 252, 1420, 1450, 1458 and 1504, all at 12
# nodes. Each margin is a copy of the working tree with the constant
# replaced. Records land in ~/sweep413.
#
#   sweep.sh      needs python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=$HOME/sweep413
mkdir -p $O
python3 -I $M/02-366/netqpb.py 12 30 1420 $O/b1420.mps
for s in 44 252 1420 1450 1458 1504; do python3 -I $M/02-365/netqp.py 12 30 $s $O/q$s.mps; done
for mg in 10.0 100.0 1000.0 10000.0 1e5; do
    T=$HOME/sweep413-$mg
    mkdir -p $T
    rsync -a --delete $R/src $R/include $R/cli $R/Makefile $T/
    sed -i "s/QP_PUSH_MARGIN = 1000.0;/QP_PUSH_MARGIN = $mg;/" $T/src/barrier.c
    (cd $T && make -s build/cli/jaos > /dev/null 2>&1)
    for f in b1420 q44 q252 q1420 q1450 q1458 q1504; do
        echo "$mg $f $($T/build/cli/jaos solve $O/$f.mps 2>&1 | grep '^status\|^work_units' | tr '\n' ' ')"
    done
done
