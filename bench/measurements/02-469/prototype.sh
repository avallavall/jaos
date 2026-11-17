#!/bin/bash
# The prototype's readings behind 02-469: prototype.py on neos-911970 and
# mad, 100,000 moves, seeds 1 to 3, each rule with trades and threshold
# accepting with geometric cooling without them. Prints prototype.txt.
#
#   prototype.sh > prototype.txt
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-469
for f in neos-911970 mad; do
    for run in "anneal 1" "linear 1" "geometric 1" "geometric 0"; do
        set -- $run
        for s in 1 2 3; do
            printf '%-12s %-9s swaps=%s seed=%s ' $f $1 $2 $s
            python3 -I $H/prototype.py $R/bench/instances-miplib2017/$f.mps 100000 $s $1 $2
        done
    done
done
