#!/bin/bash
# The reading behind 02-466, one solve at a time. csched008's root
# heuristics on 02-466's parent (dd33215), from heuristics.py. MIPLIB 3
# against bench/miplib.baseline and MIPLIB 2017's 30 instances at 1e10
# work units: with MIP_HEUR_FLOOR at 0 and MIP_HEUR_ROOT set per run, and
# as landed. Each from a copy of HEAD. The records land in OUT (default
# ~/read466).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-466
O=${OUT:-$HOME/read466}
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    d=$1
    (cd $d && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm)
}
d=$O/before; mkdir -p $d
git archive dd33215 src include cli bench/run.c | tar x -C $d
python3 -I $H/heuristics.py $d > /dev/null || exit 1
build $d || exit 1
$d/jaos solve bench/instances-miplib2017/csched008.mps --work-limit 10000000000 2>&1 \
    | grep -E "HEURW|^nodes|^first_incumbent|^work_units" > $O/csched008-before.txt
for v in 0.25 0.5 1.0 landed; do
    d=$O/share$v; mkdir -p $d
    git archive HEAD src include cli bench/run.c | tar x -C $d
    if [ $v != landed ]; then
        sed -i "s/^constexpr double MIP_HEUR_ROOT = 0.5;/constexpr double MIP_HEUR_ROOT = $v;/; s/^constexpr int64_t MIP_HEUR_FLOOR = 20000;/constexpr int64_t MIP_HEUR_FLOOR = 0;/" $d/src/mip.c
    fi
    build $d || exit 1
    $d/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 -o $O/miplib3-$v.txt > $O/miplib3-$v.log 2>&1
done
for v in 0.5 0.25 landed; do
    $O/share$v/run -j 1 -m bench/miplib2017.manifest -e mip \
        -d bench/instances-miplib2017 -L 1e10 -o $O/miplib2017-$v.txt \
        > $O/miplib2017-$v.log 2>&1
    python3 bench/measurements/02-328/m17sum.py $v=$O/miplib2017-$v.txt
done > $O/sum.txt
echo done > $O/done
