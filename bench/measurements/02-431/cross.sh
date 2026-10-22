#!/bin/bash
# After pairs.sh: the pairs it lists, each dumped from both runs by
# 02-426's pairs.patch with dumps.patch, then every dump solved by HEAD's
# CLI and by the working tree's, under the harness's 2e8 work units.
# pairs.py matches edits by their text, which leaves out the drawn
# values, so a pair can be two different models; the dumps say which.
# Records land in ~/cross431.
#
#   cross.sh      needs gcc-14 and ~/pairs431 from pairs.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/pairs431
X=$HOME/cross431
rm -rf $X; mkdir -p $X
cp $O/warmcone.c $X/warmcone.c
(cd $X && patch -s -p1 < $R/bench/measurements/02-431/dumps.patch) || exit 1
(cd $O/head && make cli > cli-build.log 2>&1) || exit 1
(cd $R && make cli > $X/cli-build.log 2>&1) || exit 1
for arm in head new; do
  L=$([ $arm = head ] && echo $O/head/build/release/libjaos.a || echo $O/new-lib/libjaos.a)
  gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $X/w-$arm $X/warmcone.c $L -lm -pthread || exit 1
done
declare -A D
D[1]=" 88-3 88-4 88-5 88-6 "
D[2]=" 270-8 "
D[3]=" 18-4 144-1 208-1 231-1 "
for arm in head new; do
  for s in 1 2 3; do
    mkdir -p $X/$arm$s
    (cd $X/$arm$s && DUMPS="${D[$s]}" $X/w-$arm 300 $s 8 misocp > out.txt 2>&1) &
  done
done
wait
for s in 1 2 3; do
  for f in $X/new$s/pair-*.mps; do
    b=$(basename $f)
    if cmp -s $f $X/head$s/$b; then same="same model"; else same="other model"; fi
    for arm in head new; do
      g=$X/$arm$s/$b
      h=$($O/head/build/cli/jaos solve $g --work-limit 200000000 2>&1 | grep -E '^(status|objective)' | tr '\n' ' ')
      n=$($R/build/cli/jaos solve $g --work-limit 200000000 2>&1 | grep -E '^(status|objective)' | tr '\n' ' ')
      echo "seed $s $b ($same) from $arm run | HEAD lib: $h| new lib: $n"
    done
  done
done > $X/cross.txt
