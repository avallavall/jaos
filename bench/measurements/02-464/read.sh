#!/bin/bash
# The reading behind 02-464, with HEAD's bench/run, one solve at a time:
# MIPLIB 3 against bench/miplib.baseline under reliability 4 at five probe
# caps and by default, and MIPLIB 2017's 30 instances by default, all at
# 1e10 work units. The records land in OUT (default ~/read464), and
# 02-328's m17sum.py sums the MIPLIB 2017 one into sum.txt.
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read464}
mkdir -p $O
cd $R
make build/bench/run || exit 1
m3() {
    t=$1; shift
    ./build/bench/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 "$@" -o $O/miplib3-$t.txt \
        > $O/miplib3-$t.log 2>&1
}
for c in 0 0.5 1 1.5 4; do
    m3 cap$c -O mip_reliability=4 -O mip_probe_cap=$c
done
m3 default
./build/bench/run -j 1 -m bench/miplib2017.manifest -e mip \
    -d bench/instances-miplib2017 -L 1e10 -o $O/miplib2017-default.txt \
    > $O/miplib2017-default.log 2>&1
python3 bench/measurements/02-328/m17sum.py default=$O/miplib2017-default.txt > $O/sum.txt
echo done > $O/done
