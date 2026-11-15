#!/bin/bash
# The reading behind 02-461, one solve at a time with HEAD's bench/run:
# MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017's 30 instances,
# both at 1e10 work units. kept is --reliability 4 --probe-cap 1, whose
# strong-branching children run on the node's kept factors; base is the
# default. The records land in OUT (default ~/read461), and 02-328's
# m17sum.py sums the MIPLIB 2017 ones into sum.txt.
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read461}
K="-O mip_reliability=4 -O mip_probe_cap=1"
mkdir -p $O
cd $R
make build/bench/run || exit 1
./build/bench/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 $K -o $O/miplib3-kept.txt \
    > $O/miplib3-kept.log 2>&1
for c in base kept; do
    args=""
    [ $c = kept ] && args="$K"
    ./build/bench/run -j 1 -m bench/miplib2017.manifest -e mip \
        -d bench/instances-miplib2017 -L 1e10 $args -o $O/$c.txt \
        > $O/$c.log 2>&1
done
for c in base kept; do
    python3 bench/measurements/02-328/m17sum.py $c=$O/$c.txt
done > $O/sum.txt
echo done > $O/done
