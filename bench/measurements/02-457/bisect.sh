#!/bin/bash
# Bisects the first commit after FROM at which an instance's KEY (nodes,
# iters or work, as solve.c prints them) differs from FROM's. Each commit is
# built from git archive into OUT/<commit>, so the working tree is not
# touched. Prints every commit it reads.
#
#   NAME=p0033 FILE=bench/instances-miplib/p0033.mps FROM=5c51216d KEY=nodes \
#       OUT=dir bisect.sh                                  needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-457
W=${OUT:-$HOME/bisect457}; mkdir -p $W
NAME=${NAME:-p0033}
FILE=${FILE:-$R/bench/instances-miplib/$NAME.mps}
FROM=${FROM:-5c51216d}
KEY=${KEY:-nodes}
probe() {
    c=$1; d=$W/$c
    if [ ! -f $d/solve ]; then
        rm -rf $d; mkdir -p $d
        git -C $R archive $c src include | tar x -C $d
        (cd $d && for f in src/*.c; do
            gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD \
                -fvisibility=hidden -Iinclude -c $f -o $(basename $f .c).o &
        done; wait; ar rcs libjaos.a *.o) > $d/build.log 2>&1
        gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$d/include -o $d/solve \
            $H/solve.c $d/libjaos.a -lm >> $d/build.log 2>&1
    fi
    [ -f $d/out-$NAME.txt ] || $d/solve $FILE > $d/out-$NAME.txt 2>&1
    echo "$c $(git -C $R log -1 --format='%ad %s' --date=short $c | cut -c1-70)"
    sed 's/^/    /' $d/out-$NAME.txt
}
key() { sed "s/.* $KEY=\([0-9]*\).*/\1/" $W/$1/out-$NAME.txt; }
mapfile -t C < <(git -C $R rev-list --reverse $FROM..HEAD)
probe $FROM
want=$(key $FROM)
lo=-1; hi=$((${#C[@]} - 1))
probe ${C[$hi]}
while [ $((hi - lo)) -gt 1 ]; do
    mid=$(((lo + hi) / 2))
    probe ${C[$mid]}
    if [ "$(key ${C[$mid]})" = "$want" ]; then lo=$mid; else hi=$mid; fi
done
echo "first commit with another $KEY: ${C[$hi]}"
