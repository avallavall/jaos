#!/bin/bash
# The reading behind 02-467, one solve at a time: proximity.patch applied
# to a copy of HEAD, MIPLIB 3 against bench/miplib.baseline and MIPLIB
# 2017's 30 instances at 1e10 work units with J32_PROX=1. The records land
# in OUT (default ~/read467).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-467
O=${OUT:-$HOME/read467}
rm -rf $O; mkdir -p $O/tree
cd $R
git archive HEAD src include bench/run.c | tar x -C $O/tree
(cd $O/tree && patch -s -p1 < $H/proximity.patch) || exit 1
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
(cd $O/tree && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
 ar rcs libjaos.a *.o
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
export J32_PROX=1
$O/tree/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-proximity.txt \
    > $O/miplib3-proximity.log 2>&1
$O/tree/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017-proximity.txt > $O/miplib2017-proximity.log 2>&1
python3 bench/measurements/02-328/m17sum.py proximity=$O/miplib2017-proximity.txt > $O/sum.txt
echo done > $O/done
