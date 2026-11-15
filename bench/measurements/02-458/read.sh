#!/bin/bash
# The reading behind 02-458: 02-255's misocp.c (with 02-376's unsure.patch)
# on seeds 1 to 4, 13, 16, 17 and 29, 02-326's discrete.c (with 02-390's
# unsure.patch) on seeds 1 to 3, 22, 24 and 25, and 02-253's conic.c on
# seeds 1 to 3, 1000 models each, with the library of BASE (default HEAD)
# and with the working tree's under finish-reg.patch at FINISH_REG = 1e-9,
# 1e-11 and 1e-13. Records land in OUT (default ~/read458) as SET-VALUE.txt;
# summary.py compares them.
#
#   BASE=rev OUT=dir read.sh       needs gcc-14, python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-458
O=${OUT:-$HOME/read458}
rm -rf $O; mkdir -p $O/lib $O/head $O/src $HOME/uns
(cd $R && tar c src include) | tar x -C $O/lib
(cd $O/lib && patch -s -p1 < $H/finish-reg.patch) || exit 1
git -C $R archive ${BASE:-HEAD} src include | tar x -C $O/head
for d in $O/lib $O/head; do
    (cd $d && for f in src/*.c; do
        gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD \
            -fvisibility=hidden -Iinclude -c $f -o $(basename $f .c).o &
    done; wait; ar rcs libjaos.a *.o)
done
git -C $R show HEAD:bench/measurements/02-255/misocp.c > $O/src/misocp.c
(cd $O/src && patch -s -p4 < $R/bench/measurements/02-376/unsure.patch)
git -C $R show HEAD:bench/measurements/02-326/discrete.c > $O/src/discrete.c
(cd $O/src && patch -s -p4 < $R/bench/measurements/02-390/unsure.patch)
CC="gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$R/include"
for t in lib head; do
    $CC -o $O/misocp-$t $O/src/misocp.c $O/$t/libjaos.a -lm 2>/dev/null || exit 1
    $CC -o $O/discrete-$t $O/src/discrete.c $O/$t/libjaos.a -lm 2>/dev/null || exit 1
    $CC -o $O/conic-$t $R/bench/measurements/02-253/conic.c $O/$t/libjaos.a -lm || exit 1
done
for v in head 1e-9 1e-11 1e-13; do
    (
        mkdir -p $O/v$v/m $O/v$v/d $O/v$v/c
        t=lib; [ $v = head ] && t=head
        [ $v != head ] && export FINISH_REG=$v
        for s in 1 2 3 4 13 16 17 29; do
            echo "== seed $s"; $O/misocp-$t 1000 $s $O/v$v/m | grep -vE '^UNSURE file'
        done > $O/misocp-$v.txt 2>&1
        for s in 1 2 3 22 24 25; do
            echo "== seed $s"; $O/discrete-$t 1000 $s $O/v$v/d
        done > $O/discrete-$v.txt 2>&1
        for s in 1 2 3; do
            echo "== seed $s"; $O/conic-$t 1000 $s $O/v$v/c
        done > $O/conic-$v.txt 2>&1
    ) &
done
wait
python3 $H/summary.py $O > $O/sweep.txt
echo done > $O/done
