#!/bin/bash
# HEAD with ties.patch: logs every column the start tie rule of e368c847
# places (JAOS_TIE=1), and skips the rule (JAOS_NOTIES=1). Solves p0033,
# gen and misc03 with the rule and without it.
#
#   OUT=dir ties.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-457
d=${OUT:-$HOME/ties457}
rm -rf $d; mkdir -p $d
git -C $R archive HEAD src include | tar x -C $d
(cd $d && patch -s -p1 < $H/ties.patch) || exit 1
(cd $d && for f in src/*.c; do
    gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD \
        -fvisibility=hidden -Iinclude -c $f -o $(basename $f .c).o &
done; wait; ar rcs libjaos.a *.o)
gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$d/include -o $d/solve \
    $H/solve.c $d/libjaos.a -lm || exit 1
M=$R/bench/instances-miplib
echo "== where the rule places a column, p0033"
JAOS_TIE=1 $d/solve $M/p0033.mps 2>&1 | grep TIE | sort | uniq -c | sort -rn
echo "== with the rule"
$d/solve $M/p0033.mps $M/gen.mps $M/misc03.mps
echo "== without it"
JAOS_NOTIES=1 $d/solve $M/p0033.mps $M/gen.mps $M/misc03.mps
