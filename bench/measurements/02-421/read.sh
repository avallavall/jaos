#!/bin/bash
# The reading behind 02-421: rangecheck.c on seeds 1 to 3, 1000 models a
# seed, with the working tree's library. Records land in ~/read421.
#
#   read.sh       needs gcc-14 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read421
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/rangecheck \
    $R/bench/measurements/02-421/rangecheck.c $R/build/release/libjaos.a -lm -pthread || exit 1
for s in 1 2 3; do echo "== seed $s"; $O/rangecheck 1000 $s; done > $O/rangecheck.txt 2>&1
echo done > $O/done
