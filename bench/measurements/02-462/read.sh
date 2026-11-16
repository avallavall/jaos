#!/bin/bash
# The reading behind 02-462, with HEAD's tool and bench/run, one solve at a
# time: misc03 across reliability and probe cap, the probe cap on the six
# MIPLIB 3 instances that move under probing, enigma's first incumbent
# under three settings, and MIPLIB 3 under reliability 4 with no cap and
# with cap 4. The records land in OUT (default ~/read462).
#
#   OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read462}
mkdir -p $O
cd $R
make build/cli/jaos build/bench/run || exit 1
J=./build/cli/jaos
I=bench/instances-miplib
{
    for a in "--reliability 0" \
             "--reliability 1 --probe-cap 0" "--reliability 1 --probe-cap 1" "--reliability 1 --probe-cap 4" \
             "--reliability 2 --probe-cap 0" "--reliability 2 --probe-cap 1" "--reliability 2 --probe-cap 4" \
             "--reliability 4 --probe-cap 0" "--reliability 4 --probe-cap 1" "--reliability 4 --probe-cap 4" \
             "--reliability 8 --probe-cap 0" "--reliability 8 --probe-cap 1" "--reliability 8 --probe-cap 4" \
             "--reliability 4 --probe-cap 1 --probe-depth 0" "--reliability 4 --probe-cap 1 --probe-depth 2" \
             "--reliability 4 --probe-cap 1 --probe-depth 5"; do
        out=$($J solve $I/misc03.mps $a --log summary 2>&1)
        printf '%-46s nodes %5s work %10s %s\n' "$a" \
            "$(echo "$out" | sed -n 's/^nodes //p')" \
            "$(echo "$out" | sed -n 's/^work_units //p')" \
            "$(echo "$out" | grep -oE '[0-9]+ probes, [0-9]+ of them capped' | head -1)"
    done
} > $O/misc03.txt
{
    for f in misc03 enigma mod008 p0201 rgn misc07; do
        b=$(awk -v n=$f '$1 == n { print $9 }' bench/miplib.baseline)
        line="$f baseline $b:"
        for c in 0.25 0.5 1 1.5 2 3; do
            w=$($J solve $I/$f.mps --reliability 4 --probe-cap $c 2>&1 | sed -n 's/^work_units //p')
            line="$line  cap $c $(awk -v w=$w -v b=$b 'BEGIN { printf "%.2fx", w / b }')"
        done
        echo "$line"
    done
} > $O/caps.txt
{
    for a in "" "--reliability 4 --probe-cap 1" "--reliability 4"; do
        out=$($J solve $I/enigma.mps $a 2>&1)
        printf '%-32s nodes %5s first_incumbent %5s\n' "${a:-default}" \
            "$(echo "$out" | sed -n 's/^nodes //p')" \
            "$(echo "$out" | sed -n 's/^first_incumbent //p')"
    done
} > $O/enigma.txt
for c in nocap:0 cap4:4; do
    t=${c%%:*}; v=${c##*:}
    ./build/bench/run -j 1 -m bench/miplib.manifest -e mip -d $I -b bench/miplib.baseline \
        -L 1e10 -O mip_reliability=4 -O mip_probe_cap=$v -o $O/miplib3-$t.txt \
        > $O/miplib3-$t.log 2>&1
done
echo done > $O/done
