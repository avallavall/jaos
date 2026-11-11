#!/bin/bash
# The table behind 02-442: the four directions of the tie rule and the
# lower-first rule, on pilot87 (dual and primal) and on qual, refinery and
# vol1 (dual). Builds a copy of HEAD with variants.patch in OUT (default
# ~/var442) and prints one line a solve. J28V picks the rule: 0 as landed,
# bit 1 the bound with less infeasibility, bit 2 the bound with more room,
# 9 the lower bound.
#
#   OUT=dir variants.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=${OUT:-$HOME/var442}
rm -rf $D; mkdir -p $D/var
cd $R && git archive HEAD | tar -x -C $D
cd $D && patch -p1 -s < $R/bench/measurements/02-442/variants.patch || exit 1
make -s build/cli/jaos > /dev/null 2>&1 || { echo BUILD FAILED; exit 1; }
run() {
    v=$1; name=$2; file=$3; alg=$4
    J28V=$v $D/build/cli/jaos solve $file --algorithm $alg --opt work_limit=60000000000 2> /dev/null |
        grep -E '^(status|iterations|work_units)' | tr '\n' ' ' > $D/var/$v-$name-$alg.txt
}
export -f run; export D
for v in 0 1 2 3 9; do
    echo "$v pilot87 $R/bench/instances/pilot87.mps dual"
    echo "$v pilot87 $R/bench/instances/pilot87.mps primal"
    for f in qual refinery vol1; do echo "$v $f $R/bench/instances-infeas/$f.mps dual"; done
done | xargs -P 5 -L 1 bash -c 'run "$@"' _
for f in $D/var/*.txt; do echo "$(basename $f .txt): $(cat $f)"; done
