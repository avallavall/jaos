#!/bin/bash
# The reading behind 02-424: resume2.c on seeds 1 to 9, 1000 models a
# seed, with the working tree's library, then 02-247's resume.c on seeds
# 1 to 8. Records land in ~/read424.
#
#   read.sh       needs gcc-14 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read424
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/resume2 \
    $R/bench/measurements/02-424/resume2.c $R/build/release/libjaos.a -lm -pthread || exit 1
(cd $O && for s in 1 2 3 4 5 6 7 8 9; do echo "== seed $s"; ./resume2 1000 $s; done) > $O/resume2.txt 2>&1
(cd $R && SEEDS="1 2 3 4 5 6 7 8" bash bench/measurements/02-247/resume.sh $O/r247) > $O/resume.txt 2>&1
echo done > $O/done
