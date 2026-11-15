#!/bin/bash
# The reading behind 02-456.
#
# resume: the working tree's sources with dumpnodes.patch run the tree of
# tests/data/mip_noinc_propagate.mps under 3e8 work units and write its
# first 400 node LPs, each with the basis it starts from. resume-many.c
# stops each of them at 40 points of its walk, from the basis, and resumes,
# with the library of BASE (default 00e975c) and with the working tree's
# (build/release). Then 02-455's resume-warm.sh on the working tree's.
#
# cone: conemem.c on tests/data/conic_tree_deep_dive.mps (02-455's
# s2-c01303.mps) under four work limits, each under /usr/bin/time -f %M,
# with both libraries; BASE's only under the three small limits, since at
# 2e9 it reaches 11.3 GB. Each solve may take MEM kilobytes of address
# space (default 4,000,000).
#
# Records land in OUT (default ~/read456).
#
#   BASE=rev OUT=dir MEM=kb read.sh   needs gcc-14 and build/release/libjaos.a
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-456
O=${OUT:-$HOME/read456}
rm -rf $O; mkdir -p $O/dump $O/base $O/nodes
CC="gcc-14 -std=c23 -O2 -ffp-contract=off -pthread"

lib() {
    (cd $1 && for f in src/*.c; do
        $CC -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude -c $f \
            -o $(basename $f .c).o &
    done; wait; ar rcs libjaos.a *.o)
}

(cd $R && tar c src include) | tar x -C $O/dump
(cd $O/dump && patch -s -p1 < $H/dumpnodes.patch) || exit 1
lib $O/dump
git -C $R archive ${BASE:-00e975c} src include | tar x -C $O/base
lib $O/base
L=$R/build/release/libjaos.a
$CC -I$R/include -o $O/dumpnodes $H/dumpnodes.c $O/dump/libjaos.a -lm || exit 1
for t in base change; do
    l=$L; [ $t = base ] && l=$O/base/libjaos.a
    $CC -Wall -Wextra -I$R/include -o $O/resume-many-$t $H/resume-many.c $l -lm || exit 1
    $CC -Wall -Wextra -I$R/include -o $O/conemem-$t $H/conemem.c $l -lm || exit 1
done

(cd $O && JAOS_DUMPNODES=$O/nodes JAOS_DUMPMAX=400 ./dumpnodes \
    $R/tests/data/mip_noinc_propagate.mps 300000000) > $O/dump.txt
$O/resume-many-base 40 $O/nodes/*.mps > $O/resume-base.txt &
$O/resume-many-change 40 $O/nodes/*.mps > $O/resume-change.txt &
wait
bash $H/../02-455/resume-warm.sh > $O/resume-warm.txt

ulimit -v ${MEM:-4000000}
for t in base change; do
    for w in 25000000 50000000 100000000 2000000000; do
        [ $t = base ] && [ $w = 2000000000 ] && continue
        /usr/bin/time -f "max resident %M kB" $O/conemem-$t \
            $R/tests/data/conic_tree_deep_dive.mps $w 2>&1 | sed "s/^/$t $w: /"
    done
done > $O/cone.txt
echo done > $O/done
