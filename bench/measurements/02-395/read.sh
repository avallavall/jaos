#!/bin/bash
# The sweep behind 02-395 on 2e8ba6e: 02-226's formats.c on seeds 7 to 10
# (3000 models each) and 02-252's writers.sh on seeds 3 and 4 (1000 models
# each). Records land in ~/fmt396.
#
#   read.sh       needs gcc-14, `make all` and HiGHS from
#                 bench/compare/fetch-solvers.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/fmt396; rm -rf $O; mkdir -p $O
cd $R
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o $O/formats bench/measurements/02-226/formats.c build/release/libjaos.a -lm -pthread || exit 1
(
  for s in 7 8 9 10; do echo "== formats $s"; $O/formats 3000 $s x | tail -3; done
) > $O/formats.txt 2>&1 &
(
  for s in 3 4; do echo "== writers $s"; bash bench/measurements/02-252/writers.sh 1000 $s $O/w$s | tail -12; done
) > $O/writers.txt 2>&1 &
wait
echo done > $O/done
