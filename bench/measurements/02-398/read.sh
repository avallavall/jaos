#!/bin/bash
# The readings behind 02-398: the seven 12-node circulations of TODO row
# J24, HiGHS 1.15.1 on each with cycle.py on its point, and the tool built
# with knobs.py under each switch. The models come from 02-365's netqp.py;
# the tool is built in ~/j24x from the working tree; records land in
# ~/read398.
#
#   read.sh       needs gcc-14 and bench/compare/solvers/highs-1.15.1
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/measurements/02-398
H=$R/bench/compare/solvers/highs-1.15.1
OUT=$HOME/read398
rm -rf $OUT; mkdir -p $OUT
mkdir -p $HOME/j24x/tree
rsync -a --delete $R/src $R/include $R/cli $R/Makefile $HOME/j24x/tree/
(cd $HOME/j24x/tree && python3 $D/knobs.py && make -s build/cli/jaos > $OUT/build.log 2>&1) || { echo "build failed" > $OUT/done; exit 1; }
J=$HOME/j24x/tree/build/cli/jaos
SEEDS="1104 1156 1163 1282 1340 1357 1308"
cd $OUT
printf 'log_file = %s/Highs.log\n' $OUT > h.opt
for s in $SEEDS; do
  python3 $R/bench/measurements/02-365/netqp.py 12 30 $s n$s.mps
  $H --options_file h.opt --model_file n$s.mps --solution_file h$s.sol > /dev/null 2>&1
  echo "$s $(python3 $D/cycle.py n$s.mps h$s.sol)"
done > highs.txt
while read -r name env; do
  for s in $SEEDS; do
    r=$(env $env $J solve n$s.mps 2>&1)
    echo "$name $s $(echo "$r" | grep -E '^(status|objective|work_units)' | awk '{print $2}' | tr '\n' ' ')"
  done
done > knobs.txt <<'EOF'
base X=0
cap10 J24CAP=10
cap100 J24CAP=100
cap1000 J24CAP=1000
far1e16 J24FAR=1e16
far1e30 J24FAR=1e30
far1e30+cap1000 J24FAR=1e30 J24CAP=1000
shift1e3 J24SHIFT=1e3
far1e30+shift1e3 J24FAR=1e30 J24SHIFT=1e3
far1e30+shift1e6 J24FAR=1e30 J24SHIFT=1e6
traf1e-12 J24TRAF=1e-12
far1e30+traf1e-12 J24FAR=1e30 J24TRAF=1e-12
far1e30+traf1e-10 J24FAR=1e30 J24TRAF=1e-10
ref1 J24REF=1
far1e30+ref1 J24FAR=1e30 J24REF=1
far1e30+ref3 J24FAR=1e30 J24REF=3
near1e-3 J24NEAR=1e-3
far1e30+near1e-3 J24FAR=1e30 J24NEAR=1e-3
EOF
for s in $SEEDS; do
  echo "== $s"
  J24FAR=1e30 J24NEAR=1e-3 $J solve n$s.mps --log summary 2>&1 |
      grep -E 'J24 stalled|the push|^status'
done > near.txt
J24FAR=1e30 J24DUMP=60 $J solve n1156.mps 2>&1 | grep -E '^(DUMP|ROW|VAR)' > dump1156.txt
J24FAR=1e30 $J solve n1156.mps --log detail > walk1156.txt 2>&1
echo done > done
