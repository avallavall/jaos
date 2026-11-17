#!/bin/bash
# The prototype's readings behind 02-472: sched.py on csched008 and csched007
# under each branching and propagation, each point checked by solving the
# LP with its integers fixed (fix.py). The relaxation's point comes from
# JAOS on a copy of the model without its integer markers. Prints
# prototype.txt.
#
#   [JAOS=path/to/jaos] [INST=dir] prototype.sh > prototype.txt
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-472
J=${JAOS:-$R/build/cli/jaos}
I=${INST:-$R/bench/instances-miplib2017}
T=$(mktemp -d)
for f in csched008 csched007; do
    M=$I/$f.mps
    grep -v MARKER $M > $T/$f-lp.mps
    echo "$f relaxation $($J solve $T/$f-lp.mps --write-point $T/$f-lp.pt 2>&1 | grep '^objective')"
    for run in "settimes table 12" "settimes placed 1" "job table 1"; do
        set -- $run
        [ $f = csched007 ] && set -- $1 $2 1
        rm -f $T/$f-*.point
        echo "$f branch=$1 prop=$2"
        BRANCH=$1 PROP=$2 python3 -I $H/sched.py $M $T/$f-lp.pt 5000 $3 $T/$f
        for p in $(ls $T/$f-*.point 2>/dev/null | sort -t- -k3 -n); do
            python3 -I $H/fix.py $M $p $T/fixed.mps
            echo "  $(basename $p .point): $($J solve $T/fixed.mps 2>&1 | head -2 | tr '\n' ' ')"
        done
    done
done
rm -rf $T
