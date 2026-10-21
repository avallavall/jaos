#!/bin/bash
# The reading behind 02-419: the 20 Netlib models whose decimal exact
# solve outgrew 128 limbs in 02-418, solved again with --exact
# --exact-decimal by builds with JM_EXACT_LIMBS at 512 and 1024, four at a
# time, 300 seconds each. Records land in ~/wide (wide.txt).
#
#   wide.sh       needs the records of 02-418
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/instances
O=~/wide
rm -rf $O; mkdir -p $O
MODELS=$(grep 'outgrew the limb' $R/bench/measurements/02-418/decimal.txt | awk '{print $1}')
echo $MODELS > $O/models.txt
for L in 512 1024; do
    T=~/wide-$L
    rm -rf $T; mkdir -p $T
    rsync -a $R/src $R/include $R/cli $R/Makefile $T/
    (cd $T && make -s EXTRA_CFLAGS=-DJM_EXACT_LIMBS=$L build/cli/jaos > $O/build-$L.log 2>&1) || { echo "build $L failed" >> $O/status; continue; }
done
run() {
    L=$1; name=$2
    f=$(ls $D | grep "^$name\.")
    t0=$(date +%s)
    res=$(timeout 300 ~/wide-$L/build/cli/jaos solve $D/$f --exact --exact-decimal --log summary 2>&1 | grep -E '^status |exact:|exact solving' | sed -E 's/^jaos: .*ends numerical_error: //' | tr '\n' ' ')
    echo "$L $name $res secs $(( $(date +%s) - t0 ))" >> $O/wide.txt
}
export -f run
export D O
for L in 512 1024; do for n in $MODELS; do echo "$L $n"; done; done | xargs -P 4 -L 1 bash -c 'run $0 $1'
sort -o $O/wide.txt $O/wide.txt
echo done > $O/done
