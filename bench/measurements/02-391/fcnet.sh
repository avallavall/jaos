#!/bin/bash
# 02-328's fixed-charge networks on seeds 2 to 7, taken on de89dcc, with
# fcnet.sh's build flag. Records land in ~/fc392.
#
#   fcnet.sh      needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/fc392
rm -rf $O; mkdir -p $O
cd $R
make -s B="$O/build" EXTRA_CFLAGS=-DJAOS_MIP_NET_MIN_COLS_VALUE=1 "$O/build/release/libjaos.a" > $O/build.log 2>&1 || { echo "build failed" > $O/done; exit 1; }
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -Iinclude -o $O/fcnet bench/measurements/02-328/fcnet.c $O/build/release/libjaos.a -lm -pthread || { echo "cc failed" > $O/done; exit 1; }
for s in 2 3 4; do mkdir -p $O/s$s; ( $O/fcnet 1000 $s $O/s$s > $O/s$s.txt 2>&1 ) & done
wait
for s in 5 6 7; do mkdir -p $O/s$s; ( $O/fcnet 1000 $s $O/s$s > $O/s$s.txt 2>&1 ) & done
wait
echo done > $O/done
