#!/bin/bash
# The reading behind 02-420: decimals.c on seeds 1 to 3, 1000 models a
# seed, with the working tree's library. Records land in ~/read420.
#
#   read.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read420
rm -rf $O; mkdir -p $O
(cd $R && make -s all > $O/build.log 2>&1) || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/decimals \
    $R/bench/measurements/02-420/decimals.c $R/build/release/libjaos.a -lm -pthread
for s in 1 2 3; do echo "== seed $s"; $O/decimals 1000 $s $O; done > $O/decimals.txt 2>&1
echo done > $O/done
