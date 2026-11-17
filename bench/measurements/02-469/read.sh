#!/bin/bash
# The readings behind 02-469, one solve at a time, from a copy of HEAD:
# MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017's 30 instances at
# 1e10 work units; the budget the items search spends on mad and
# neos-911970 at the root, from a second copy with trace.py's prints; and
# the prototype's readings. The records land in OUT (default ~/read469).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-469
O=${OUT:-$HOME/read469}
rm -rf $O; mkdir -p $O/landed $O/trace
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    (cd $1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm)
}
git archive HEAD src include cli bench/run.c | tar x -C $O/landed
git archive HEAD src include cli bench/run.c | tar x -C $O/trace
python3 -I $H/trace.py $O/trace/src/mip.c || exit 1
build $O/landed || exit 1
build $O/trace || exit 1
$O/landed/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-landed.txt \
    > $O/miplib3-landed.log 2>&1
$O/landed/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017-landed.txt > $O/miplib2017-landed.log 2>&1
python3 bench/measurements/02-328/m17sum.py before=$H/full/before.txt \
    landed=$O/miplib2017-landed.txt > $O/sum.txt
for f in mad neos-911970; do
    $O/trace/jaos solve bench/instances-miplib2017/$f.mps --node-limit 1 2>&1 \
        | grep -E "ANBEST|ANEND|^work_units" | sed "s/^/$f /"
done > $O/budget.txt
bash $H/prototype.sh > $O/prototype.txt
echo done > $O/done
