#!/bin/bash
# The reading behind 02-446: 02-427's warmqc.c through warmqc.py, in qcp
# and miqcp modes on seeds 1 to 8, 1000 models a seed and 8 edits a model,
# with LIB (default the working tree's library). Records land in OUT
# (default ~/read446), one line a run in warmqc.txt.
#
#   LIB=libjaos.a OUT=dir read.sh     needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read446}
L=${LIB:-$R/build/release/libjaos.a}
rm -rf $O; mkdir -p $O
python3 $R/bench/measurements/02-446/warmqc.py $R/bench/measurements/02-427/warmqc.c $O/warmqc.c || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/warmqc \
    $O/warmqc.c $L -lm -pthread || exit 1
for mode in qcp miqcp; do
    for s in 1 2 3 4 5 6 7 8; do
        ( echo "== $mode seed $s"; $O/warmqc 1000 $s 8 $mode ) > $O/$mode-$s.txt 2>&1 &
    done
    wait
done
cat $O/qcp-*.txt $O/miqcp-*.txt > $O/warmqc.txt
echo done > $O/done
