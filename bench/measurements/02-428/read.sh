#!/bin/bash
# The reading behind 02-428: 02-255's misocp.c with 02-376's unsure.patch
# and 02-326's discrete.c with 02-390's unsure.patch, each answer judged
# against brute force, on seeds 30 to 45, 1000 models a seed, with the
# working tree's library. Records land in ~/read428.
#
#   read.sh       needs gcc-14 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/read428
rm -rf $O; mkdir -p $O/mis $O/dis
cp $R/build/release/libjaos.a $O/
cp $R/bench/measurements/02-255/misocp.c $O/mis/misocp.c
(cd $O/mis && patch -s -p4 < $R/bench/measurements/02-376/unsure.patch) || exit 1
cp $R/bench/measurements/02-326/discrete.c $O/dis/discrete.c
(cd $O/dis && patch -s -p4 < $R/bench/measurements/02-390/unsure.patch) || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mis/misocp \
    $O/mis/misocp.c $O/libjaos.a -lm -pthread || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/dis/discrete \
    $O/dis/discrete.c $O/libjaos.a -lm -pthread || exit 1
(
  for s in $(seq 30 45); do echo "== seed $s"; $O/mis/misocp 1000 $s $O/mis; done
) > $O/mip.txt 2>&1 &
(
  for s in $(seq 30 45); do echo "== seed $s"; $O/dis/discrete 1000 $s $O/dis; done
) > $O/dis.txt 2>&1 &
wait
echo done > $O/done
