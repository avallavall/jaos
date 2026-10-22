#!/bin/bash
# The reading behind 02-425: warmcone.c on seeds 1 to 3, 300 models a
# seed and eight edits a model, in both modes, with the working tree's
# library. Records land in ~/read425.
#
#   read.sh       needs gcc-14 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read425
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/warmcone \
    $R/bench/measurements/02-425/warmcone.c $R/build/release/libjaos.a -lm -pthread || exit 1
for mode in socp misocp; do
    for s in 1 2 3; do echo "== $mode seed $s"; $O/warmcone 300 $s 8 $mode; done
done > $O/warmcone.txt 2>&1
echo done > $O/done
