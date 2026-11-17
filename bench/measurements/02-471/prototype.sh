#!/bin/bash
# The prototype's readings behind 02-471: prototype.py on the four models
# of 02-463's family whose continuous columns link rows, from the
# relaxation's start at 500, 1000 and 4000 LPs with seeds 1 to 3, and from
# the greedy start on csched007. Builds the shared library first. Prints
# prototype.txt.
#
#   prototype.sh > prototype.txt
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-471
I=${INST:-$R/bench/instances-miplib2017}
(cd $R && make build/release/libjaos.so > /dev/null 2>&1) || exit 1
run() {
    printf '%-17s lps=%-5s seed=%s ' $1 $2 $3
    python3 $H/prototype.py $I/$1.mps $2 $3 $4
}
for f in glass4 graphdraw-domain assign1-5-8 csched007; do
    for n in 500 1000 4000; do
        for s in 1 2 3; do
            run $f $n $s relax
        done
    done
done
for s in 1 2 3; do
    run csched007 4000 $s greedy
done
