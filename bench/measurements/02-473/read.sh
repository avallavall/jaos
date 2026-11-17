#!/bin/bash
# The readings behind 02-473, one solve at a time: carry.patch applied to
# a copy of ec95542. nodes.txt: five MIPLIB 2017 instances to 20,000 nodes
# without and with J7_DSE=1. Then MIPLIB 3 against bench/miplib.baseline
# and MIPLIB 2017's 30 instances at 1e10 work units with J7_DSE=1. The
# records land in OUT (default ~/read473).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-473
O=${OUT:-$HOME/read473}
rm -rf $O; mkdir -p $O/tree
cd $R
git archive ec95542 src include cli bench/run.c | tar x -C $O/tree
(cd $O/tree && patch -s -p1 < $H/carry.patch) || exit 1
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
(cd $O/tree && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
 ar rcs libjaos.a *.o
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
for f in pk1 binkar10_1 gen-ip002 mas76 assign1-5-8; do
    for mode in off on; do
        if [ $mode = on ]; then export J7_DSE=1; else unset J7_DSE; fi
        r=$($O/tree/jaos solve bench/instances-miplib2017/$f.mps --node-limit 20000 2>&1 \
            | grep -E "^iterations|^work_units|^nodes|^bound|^incumbent|DSEAPPLIED" | tr '\n' ' ')
        echo "$f $mode $r"
    done
done > $O/nodes.txt
export J7_DSE=1
$O/tree/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-carry.txt > $O/miplib3-carry.log 2>&1
$O/tree/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017-carry.txt > $O/miplib2017-carry.log 2>&1
python3 bench/measurements/02-328/m17sum.py before=$H/full/before.txt \
    carry=$O/miplib2017-carry.txt > $O/sum.txt
echo done > $O/done
