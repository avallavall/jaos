#!/bin/bash
# The block trace behind 02-485's work budget: the tree with trace.patch
# applied and VERIFY_BLOCK_WORK lifted, built in ~/j485/trace, prints each
# dense block's size, limb products and bytes when it ends, and every 16
# steps. Eight models, four at a time, 900 seconds each; trace.txt keeps
# each model's largest block, last step and verdict.
#
#   D=instance-dir trace.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=${D:?set D to the Netlib instance directory}
T=~/j485/trace
rm -rf $T; mkdir -p $T ~/j485/tr
rsync -a $R/src $R/include $R/cli $R/Makefile $T/
(cd $T && patch -p1 < $R/bench/measurements/02-485/trace.patch &&
 sed -i 's/^constexpr int64_t VERIFY_BLOCK_WORK = .*/constexpr int64_t VERIFY_BLOCK_WORK = INT64_MAX \/ 4;/' src/verify.c &&
 make -s build/cli/jaos) || exit 1
one() {
    t0=$(date +%s)
    J485_TRACE=1 timeout 900 ~/j485/trace/build/cli/jaos solve $D/$1.mps \
        --exact --exact-decimal --log summary > ~/j485/tr/$1.log 2>&1
    echo "exit $? secs $(( $(date +%s) - t0 ))" >> ~/j485/tr/$1.log
}
export -f one
export D
echo perold pilot-we pilot-ja pilot dfl001 d2q06c pilot87 stair | tr ' ' '\n' |
    xargs -P 4 -I{} bash -c 'one {}'
for f in ~/j485/tr/*.log; do
    n=$(basename $f .log)
    grep '^j485 block' $f | sort -k6 -n | tail -1 | sed "s/^j485 block/$n biggest block/"
    grep '^j485 step' $f | tail -1 | sed "s/^j485 step/$n last step/"
    grep -E 'exact:|exact solving|^exit' $f | sed -E "s/^jaos: .*ends numerical_error: //; s/^/$n /"
done > ~/j485/tr/trace.txt
