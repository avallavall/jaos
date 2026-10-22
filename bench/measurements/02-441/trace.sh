#!/bin/bash
# The trace behind 02-441: builds the CLI of REV (default 9b14bd0) in
# ~/t441 with every jm_work_add printing its file, line and amount, and
# compute_primal printing each nonbasic column it moves off zero. It then
# solves afiro.mps and its mirror (02-440's pilot.sh builds the
# mirrordump it uses in ~/p440, seed 1) and prints where the two traces
# part before the first pivot.
#
#   REV=rev trace.sh     needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
W=~/t441
~/p440/mirrordump $R/bench/instances/afiro.mps 1 ~/p440/afiro-m.mps || exit 1
rm -rf $W; mkdir -p $W
git -C $R archive ${REV:-9b14bd0} src include cli Makefile | tar -x -C $W
python3 $R/bench/measurements/02-441/trace.py $W/src/jaos_internal.h $W/src/simplex.c || exit 1
cd $W
make -j12 LTO=0 build/cli/jaos > build.log 2>&1 || { echo BUILD FAILED; exit 1; }
for x in a b; do
  if [ $x = a ]; then f=$R/bench/instances/afiro.mps; else f=~/p440/afiro-m.mps; fi
  J28=1 ./build/cli/jaos solve $f --log progress > $x.out 2> $x.tr
  grep -E '^work_units' $x.out
  awk '/^iter 0:/{print; exit} {print}' $x.tr > $x.pre
done
echo "== where the work parts (file:line amount)"
diff <(awk '$1=="W"{print $2, $3}' a.pre) <(awk '$1=="W"{print $2, $3}' b.pre)
for x in a b; do echo "== nonbasic columns off zero, $x"; grep '^NB ' $x.pre; done
