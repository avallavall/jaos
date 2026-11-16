#!/bin/bash
# The reading behind 02-465, one solve at a time: J7's seven MIPLIB 2017
# instances at 5e10 work units with HEAD's bench/run; pk1 and mas76 at
# 5e10 with MIP_NODE_PRESOLVE_ITERS at 0, built from git archive HEAD;
# and callgrind's instructions by function for both at 1e9. The records
# land in OUT (default ~/read465).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read465}
rm -rf $O; mkdir -p $O
cd $R
make build/bench/run || exit 1
grep -E "^(#|gen-ip002|gen-ip054|mas76|pk1|neos5|assign1-5-8|binkar10_1) " \
    bench/miplib2017.manifest > $O/seven.manifest
./build/bench/run -j 1 -m $O/seven.manifest -e mip -d bench/instances-miplib2017 \
    -L 5e10 -o $O/seven-5e10.txt > $O/seven-5e10.log 2>&1
F="-std=c23 -O2 -g -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
d=$O/off; mkdir -p $d
git archive HEAD src include cli bench/run.c | tar x -C $d
sed -i 's/^constexpr double MIP_NODE_PRESOLVE_ITERS = 20.0;/constexpr double MIP_NODE_PRESOLVE_ITERS = 0.0;/' $d/src/mip.c
(cd $d && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
 ar rcs libjaos.a *.o
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
grep -E "^(#|mas76|pk1) " bench/miplib2017.manifest > $O/two.manifest
$d/run -j 1 -m $O/two.manifest -e mip -d bench/instances-miplib2017 -L 5e10 \
    -o $O/node-presolve-off.txt > $O/node-presolve-off.log 2>&1
p=$O/prof; mkdir -p $p
git archive HEAD src include cli | tar x -C $p
(cd $p && for f in src/*.c; do gcc-14 $F -fno-inline-functions-called-once -c $f -o $(basename $f .c).o & done; wait
 ar rcs libjaos.a *.o
 gcc-14 -std=c23 -O2 -g -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm) || exit 1
for f in pk1 mas76; do
    valgrind --tool=callgrind --callgrind-out-file=$O/cg.$f.out $p/jaos solve \
        bench/instances-miplib2017/$f.mps --work-limit 1000000000 > /dev/null 2>&1
    callgrind_annotate --inclusive=no $O/cg.$f.out | grep -E "^ *[0-9,]+ " | head -25 \
        > $O/profile-$f.txt
done
echo done > $O/done
