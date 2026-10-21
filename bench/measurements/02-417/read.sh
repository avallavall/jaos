#!/bin/bash
# The readings behind 02-417. First 02-393's miqp.c and 02-248's push.c on
# seeds 7 to 12 with the working tree's library (miqp.txt, push.txt). Then
# a copy of the tree with dump.py's print solves the converged J24
# circulations that the checker refuses on their duals, and terms.py lists
# each one's largest reduced costs (terms.txt). Records land in ~/read417.
#
#   read.sh       needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
D=$M/02-417
O=$HOME/read417
rm -rf $O; mkdir -p $O/d
(cd $R && make -s all > $O/build.log 2>&1) || exit 1
L=$R/build/release/libjaos.a
cc() { gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/$1 $2 $L -lm -pthread 2>> $O/cc.log; }
cc miqp $M/02-393/miqp.c
cc push $M/02-248/push.c
(for s in 7 8 9 10 11 12; do echo "== miqp $s"; $O/miqp 1000 $s $O/d | tail -14; done > $O/miqp.txt 2>&1) &
(for s in 7 8 9 10 11 12; do echo "== push $s"; $O/push 1000 $s $O/d | tail -8; done > $O/push.txt 2>&1) &
wait
T=$HOME/r417-dump
rm -rf $T; mkdir -p $T
rsync -a $R/src $R/include $R/cli $R/Makefile $T/
(cd $T && python3 -I $D/dump.py && make -s build/cli/jaos > $O/dump-build.log 2>&1) || exit 1
for s in 195 201 1450 1596 1611 1624; do
    python3 -I $M/02-365/netqp.py 12 30 $s $O/q$s.mps
    $T/build/cli/jaos solve $O/q$s.mps > $O/pt-$s.txt 2>&1
    echo "== $s $(grep DBG $O/pt-$s.txt)"
    python3 -I $D/terms.py $O/q$s.mps $O/pt-$s.txt
done > $O/terms.txt
echo done > $O/done
