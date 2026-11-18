#!/bin/bash
# The arms behind 02-484: the tree of 1235543 with knobs.patch, each switch
# read from the environment, on the 30 circulations of 02-365's netqp.py
# that end numerical_error at 1235543 (28 at 12 nodes, 2 at 40). Prints one
# line an arm with the seeds it solves; records land in ~/arms484.
#
#   J24KEEP=1       the second leg's last point stands within BARRIER_TOL
#   J24G3=F         a QP's dual growth stops the walk only above a primal F
#   J24G3D=1        ... and its primal growth always does
#   J24FAR=F        the growth stop at F instead of 1e10
#   J24LIFT=K       at most K lifts a walk
#   J24LIFTSTALL=1  a lift is tried when the walk stalls (also at a growth stop)
#   J24SPACE=1      ... once every BARRIER_STALL_ITERS stalled iterations,
#                   with the primal residual above the tolerance
#   J24LIFTNEAR=1   no lift of a column whose slack exceeds its move
#   J24DEFL=1       no lift whose shift raises the largest row dual
#   J24PROG=1       a new 10% low of the primal residual counts as progress
#   J24LIFTDBG=1    prints each round of the lift
#
#   P=4 arms.sh   needs gcc-14, python3 and git; P solves at a time
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/measurements/02-484
O=$HOME/arms484
T=$HOME/r484-knobs
rm -rf $O $T; mkdir -p $O/m $T
(cd $R && git archive 1235543 src include cli Makefile) | tar -x -C $T
(cd $T && patch -s -p1 < $D/knobs.patch && make -s build/cli/jaos > $O/build.log 2>&1) || exit 1
for s in 18 25 64 110 127 195 199 201 287 288 1104 1156 1163 1282 1340 1357 1411 1450 1458 1504 1514 1535 1537 1551 1596 1624 1735 1746; do
    python3 -I $R/bench/measurements/02-365/netqp.py 12 30 $s $O/m/12-$s.mps
done
for s in 95 1076; do
    python3 -I $R/bench/measurements/02-365/netqp.py 40 100 $s $O/m/40-$s.mps
done
one() {
    f=$1; n=$(basename $f .mps)
    out=$(env $ENVS $T/build/cli/jaos solve $f --check 2>/dev/null)
    st=$(echo "$out" | sed -n 's/^status //p' | head -1)
    ck=$(echo "$out" | sed -n 's/^check_ok //p' | head -1)
    echo "$n $st $ck"
}
export -f one
export T
while read -r name envs; do
    export ENVS="X=0 $envs"
    ls $O/m/*.mps | sort -V | xargs -P ${P:-4} -I{} bash -c 'one {}' | sort -V > $O/$name.txt
    echo "$name ($envs): $(grep -c ' optimal yes' $O/$name.txt): $(grep ' optimal yes' $O/$name.txt | cut -d' ' -f1 | tr '\n' ' ')"
done <<'EOF'
base X=0
keep J24KEEP=1
grow J24G3=1e-2 J24G3D=1
lift J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
keep+grow J24KEEP=1 J24G3=1e-2 J24G3D=1
keep+lift J24KEEP=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
grow+lift J24G3=1e-2 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all J24KEEP=1 J24G3=1e-2 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,feas1e-5 J24KEEP=1 J24G3=1e-5 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,feas1e-4 J24KEEP=1 J24G3=1e-4 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,feas1e-3 J24KEEP=1 J24G3=1e-3 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,feas1e-1 J24KEEP=1 J24G3=1e-1 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,growth-off J24KEEP=1 J24FAR=1e30 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,both-growths J24KEEP=1 J24G3=1e-2 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1 J24SPACE=1
all,no-near J24KEEP=1 J24G3=1e-2 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24DEFL=1 J24SPACE=1
all,no-defl J24KEEP=1 J24G3=1e-2 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24SPACE=1
all,every-stalled J24KEEP=1 J24G3=1e-2 J24G3D=1 J24LIFT=1000 J24LIFTSTALL=1 J24LIFTNEAR=1 J24DEFL=1
EOF
echo done > $O/done
