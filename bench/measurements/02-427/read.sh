#!/bin/bash
# The reading behind 02-427: warmqc.c on seeds 1 to 8, 1000 models a seed
# and eight edits a model, in both modes, with the working tree's library.
# Records land in ~/read427.
#
#   read.sh       needs gcc-14 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read427
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/warmqc \
    $R/bench/measurements/02-427/warmqc.c $R/build/release/libjaos.a -lm -pthread || exit 1
for mode in qcp miqcp; do
    for s in 1 2 3 4 5 6 7 8; do echo "== $mode seed $s"; $O/warmqc 1000 $s 8 $mode; done
done > $O/warmqc.txt 2>&1
echo done > $O/done
