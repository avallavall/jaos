#!/bin/bash
# The readings behind 02-400, the working tree against HEAD: 02-318's
# qpread.sh (the 6000 generated QPs of 02-248, QPLIB's continuous convex
# QPs and its 17 convex MIQPs), 02-365's and 02-366's circulation
# generators on their first seeds and on seeds 1001 to 1400, then
# `make maros-meszaros` on the working tree. Records land in ~/qpread and
# ~/read400.
#
#   read.sh       needs gcc-14, python3 and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/read400
rm -rf $OUT; mkdir -p $OUT
bash $R/bench/measurements/02-318/qpread.sh
B=$HOME/jaos-base/build/cli/jaos
N=$R/build/cli/jaos
M=$R/bench/measurements
python3 -I $M/02-365/netqp_read.py $B $N $OUT/nq 12 30 1 400 > $OUT/netqp-12-30.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $OUT/nq 40 100 1 120 > $OUT/netqp-40-100.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $OUT/nq 12 30 1001 1400 > $OUT/netqp-12-30-new.txt 2>&1
python3 -I $M/02-365/netqp_read.py $B $N $OUT/nq 40 100 1001 1200 > $OUT/netqp-40-100-new.txt 2>&1
python3 -I $M/02-366/netqpb_read.py $B $N $OUT/nb 12 30 1 400 > $OUT/netqpb-12-30.txt 2>&1
python3 -I $M/02-366/netqpb_read.py $B $N $OUT/nb 12 30 1001 1400 > $OUT/netqpb-12-30-new.txt 2>&1
(cd $R && make maros-meszaros J=6 > $OUT/mm.log 2>&1)
echo done > $OUT/done
