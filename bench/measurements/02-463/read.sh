#!/bin/bash
# The reading behind 02-463, with HEAD's tool and bench/run, one solve at a
# time: MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017's 30
# instances by default at 1e10 work units, enigma under four settings, and
# census.py over both sets. The records land in OUT (default ~/read463).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-463
O=${OUT:-$HOME/read463}
mkdir -p $O
cd $R
make build/cli/jaos build/bench/run || exit 1
./build/bench/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-default.txt \
    > $O/miplib3-default.log 2>&1
./build/bench/run -j 1 -m bench/miplib2017.manifest -e mip \
    -d bench/instances-miplib2017 -L 1e10 -o $O/miplib2017-default.txt \
    > $O/miplib2017-default.log 2>&1
python3 bench/measurements/02-328/m17sum.py default=$O/miplib2017-default.txt > $O/sum.txt
{
    for a in "" "--reliability 4 --probe-cap 1" "--reliability 4" "--reliability 4 --probe-cap 4"; do
        out=$(./build/cli/jaos solve bench/instances-miplib/enigma.mps $a --log progress 2>&1)
        printf '%-32s nodes %5s first_incumbent %5s work %10s | %s\n' "${a:-default}" \
            "$(echo "$out" | sed -n 's/^nodes //p')" \
            "$(echo "$out" | sed -n 's/^first_incumbent //p')" \
            "$(echo "$out" | sed -n 's/^work_units //p')" \
            "$(echo "$out" | grep -m1 'root: incumbent')"
    done
} > $O/enigma.txt
python3 -I $H/census.py bench/miplib.manifest bench/instances-miplib \
    bench/results/miplib.txt > $O/census-miplib3.txt
python3 -I $H/census.py bench/miplib2017.manifest bench/instances-miplib2017 \
    bench/results/miplib2017.txt > $O/census-miplib2017.txt
echo done > $O/done
