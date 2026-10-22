#!/bin/bash
# The reading behind 02-430: 02-255's misocp.c with 02-376's unsure.patch
# and 02-326's discrete.c with 02-390's unsure.patch, each tree solved in
# rounds of four nodes on four threads (batch.py), each answer judged
# against brute force, on seeds 1 to 4 and 30 to 33, 1000 models a seed,
# with the working tree's library. Records land in ~/read430.
#
#   read.sh       needs gcc-14, python3 and the library built in build/release
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-430
O=$HOME/read430
rm -rf $O; mkdir -p $O/mis $O/dis
cp $R/build/release/libjaos.a $O/
cp $R/bench/measurements/02-255/misocp.c $O/mis/misocp.c
(cd $O/mis && patch -s -p4 < $R/bench/measurements/02-376/unsure.patch) || exit 1
cp $R/bench/measurements/02-326/discrete.c $O/dis/discrete.c
(cd $O/dis && patch -s -p4 < $R/bench/measurements/02-390/unsure.patch) || exit 1
python3 $H/batch.py $O/mis/misocp.c || exit 1
python3 $H/batch.py $O/dis/discrete.c || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mis/misocp \
    $O/mis/misocp.c $O/libjaos.a -lm -pthread || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/dis/discrete \
    $O/dis/discrete.c $O/libjaos.a -lm -pthread || exit 1
(
  for s in 1 2 3 4 30 31 32 33; do echo "== seed $s"; $O/mis/misocp 1000 $s $O/mis; done
) > $O/mip.txt 2>&1 &
(
  for s in 1 2 3 4 30 31 32 33; do echo "== seed $s"; $O/dis/discrete 1000 $s $O/dis; done
) > $O/dis.txt 2>&1 &
wait
echo done > $O/done
