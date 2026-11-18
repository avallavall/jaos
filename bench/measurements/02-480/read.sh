#!/bin/bash
# The readings behind 02-480, one solve at a time, from copies of b53a1ffd.
# SCIP's components taken away one at a time on assign1-5-8, at its root and
# at 50,000 nodes, on the model as given and with its cost columns declared
# integer (SCIP_PYTHON names a Python with pyscipopt; without it SCIP is left
# out); the relaxations of 02-480's table, built by rlt.py and zonly.py and
# solved by HiGHS; JAOS's arms on assign1-5-8 under prototype.patch (its
# switches: JAOS_IMPL=1 Gomory cuts with the implied integers, 2 the other
# root separators, 4 node MIR; JAOS_NULL adds work at the root as 02-474's
# null readings do; leave JAOS_GISL unset, see the README), the change
# (change.patch; null.patch is it with JAOS_NULL; cutrows.patch adds
# JAOS_CUTROWS) under five null readings, and both MIPLIB sets under the
# prototype's first switch and the change. The records land in OUT (default
# ~/read480).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-480
O=${OUT:-$HOME/read480}
I=bench/instances-miplib2017
A=$I/assign1-5-8.mps
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive b53a1ffd src include cli bench/run.c | tar x -C $O/$1
    [ -z "$2" ] || (cd $O/$1 && patch -s -p1 < $H/$2) || exit 1
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build base
build proto prototype.patch
build land change.patch
build null null.patch
build cutrows cutrows.patch
python3 bench/measurements/02-477/zint.py $A $O/zint.mps
if [ -n "$SCIP_PYTHON" ]; then
    $SCIP_PYTHON $H/scip_tree.py $A 1 600 default no-implint no-symmetry > $O/scip-root.txt 2>&1
    $SCIP_PYTHON $H/scip_root.py $O/zint.mps $A > $O/scip-root-arms.txt 2>&1
    $SCIP_PYTHON $H/scip_tree.py $A 50000 1200 default no-implint no-symmetry \
        no-implint-no-symmetry sym-polyhedral sym-orbital sym-sst no-orbitopes \
        no-dynamic-prop no-node-sepa no-conflict no-propagation pscost-branching bfs \
        no-implint+objlim211.5 default+objlim211.5 no-implint-no-symmetry+objlim211.5 \
        no-implint-no-node-sepa no-gomory no-cmir no-zerohalf no-impliedbounds \
        no-pseudoobj no-redcost > $O/scip-tree.txt 2>&1
    $SCIP_PYTHON $H/scip_tree.py $O/zint.mps 50000 1200 default no-implint >> $O/scip-tree.txt 2>&1
    $SCIP_PYTHON $H/scip_prio.py $O/zint.mps 50000 1200 binary-first general-first >> $O/scip-tree.txt 2>&1
    $SCIP_PYTHON $H/scip_tree.py $O/zint.mps 20000 1200 default >> $O/scip-tree.txt 2>&1
    $SCIP_PYTHON - $A $O/sparse.mps <<'EOF'
import sys
from pyscipopt import Model
m = Model(); m.hideOutput(); m.readProblem(sys.argv[1])
m.setParam("misc/usesymmetry", 0); m.presolve(); m.writeProblem(sys.argv[2], trans=True)
EOF
    for l in lp same full tri; do
        $SCIP_PYTHON $H/rlt.py $A $l $O/rlt-$l.mps > /dev/null
    done
fi
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
python3 $H/zonly.py $A $O/zonly.mps
if [ -n "$HI" ]; then
    {
        for l in lp same full tri; do
            [ -f $O/rlt-$l.mps ] && echo "$l $($HI --solver ipm --run_crossover off $O/rlt-$l.mps 2>&1 | grep 'Objective value')"
        done
        echo "zonly $($HI --time_limit 600 $O/zonly.mps 2>&1 | grep -E 'Primal bound|Dual bound|Nodes' | tr -s ' ' | tr '\n' ' ')"
    } > $O/relaxations.txt
fi
tree() {
    label=$1; b=$2; f=$3; shift 3
    out=$("$@" $O/$b/jaos solve $f --work-limit 10000000000 --log progress 2>&1)
    at() { echo "$out" | grep -E "^node $1: " | sed 's/.*bound \([0-9.]*\).*/\1/' | cut -c1-7; }
    printf '%-26s nodes %6s bound %-8s last %-8s at 20000 %-8s root %s\n' "$label" \
        "$(echo "$out" | sed -n 's/^nodes //p')" "$(echo "$out" | sed -n 's/^bound //p' | cut -c1-8)" \
        "$(echo "$out" | grep -E '^node [0-9]+: [0-9]+ open' | tail -1 | sed 's/.*bound \([0-9.]*\).*/\1/' | cut -c1-7)" \
        "$(at 20000)" "$(echo "$out" | sed -n 's/^root: relaxation \([0-9.]*\) after \([0-9]*\) cuts.*/\1 (\2 cuts)/p' | cut -c1-24)"
}
{
    for b in 0 1 2 4 5 7; do tree "impl$b" proto $A env JAOS_IMPL=$b; done
    tree "zint" base $O/zint.mps env
    [ -f $O/sparse.mps ] && tree "sparse" base $O/sparse.mps env
    [ -f $O/sparse.mps ] && tree "sparse impl1" proto $O/sparse.mps env JAOS_IMPL=1
} > $O/jaos-arms.txt
for a in "--no-orbital" "--node-select bound" "--opt mip_reliability=8" "--cut-depth 10" "--cut-rounds 5"; do
    out=$(JAOS_IMPL=1 $O/proto/jaos solve $A --work-limit 10000000000 --log summary $a 2>&1)
    echo "impl1 $a: nodes $(echo "$out" | sed -n 's/^nodes //p') bound $(echo "$out" | sed -n 's/^bound //p')"
done >> $O/jaos-arms.txt
spread() {
    label=$1; b=$2; shift 2
    line="$label"
    for n in 0 150 300 450 600; do
        out=$(env "$@" JAOS_NULL=$n $O/$b/jaos solve $A --work-limit 10000000000 --log progress $ARGS 2>&1)
        line="$line | $n: $(echo "$out" | grep -E '^node [0-9]+: [0-9]+ open' | tail -1 | sed 's/.*bound \([0-9.]*\).*/\1/' | cut -c1-7) $(echo "$out" | sed -n 's/^nodes //p')"
    done
    echo "$line"
}
{
    spread head proto JAOS_IMPL=0
    spread impl proto JAOS_IMPL=1
    ARGS="--cut-rounds 5" spread impl-r5 proto JAOS_IMPL=1
    spread land null X=1
    ARGS="--cut-rounds 5" spread land-r5 null X=1
    spread cutrows cutrows JAOS_CUTROWS=1
    ARGS="--cut-rounds 5" spread cutrows-r5 cutrows JAOS_CUTROWS=1
} > $O/spread.txt
JAOS_IMPL=1 $O/proto/jaos solve $A --work-limit 100000000000 --log progress 2>&1 |
    grep -E '^node [0-9]+0000: |^nodes|^bound|^work_units' > $O/jaos-impl1-1e11.txt
for f in bench/instances-miplib/misc03.mps bench/instances-miplib/misc07.mps $I/pk1.mps \
         $I/markshare_4_0.mps $I/markshare2.mps $I/ic97_potential.mps $I/csched008.mps \
         $I/csched007.mps; do
    [ -f $f ] || f=$f.gz
    for b in 0 1 5; do
        out=$(JAOS_IMPL=$b $O/proto/jaos solve $f --work-limit 10000000000 --log summary 2>&1)
        echo "$(basename $f .mps) impl$b $(echo "$out" | sed -n 's/^status //p') nodes $(echo "$out" | sed -n 's/^nodes //p') work $(echo "$out" | sed -n 's/^work_units //p') bound $(echo "$out" | sed -n 's/^bound //p') inc $(echo "$out" | sed -n 's/^incumbent //p')"
    done
done > $O/census.txt
JAOS_IMPL=1 $O/proto/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-impl.txt > $O/miplib3-impl.log 2>&1
JAOS_IMPL=1 $O/proto/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017-impl.txt > $O/miplib2017-impl.log 2>&1
$O/land/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3-land.txt > $O/miplib3-land.log 2>&1
$O/land/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017-land.txt > $O/miplib2017-land.log 2>&1
$O/land/run -j 2 -d bench/instances -b bench/netlib.baseline -o $O/netlib.txt > $O/netlib.log 2>&1
$O/land/run -j 2 -m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas \
    -b bench/netlib-infeas.baseline -o $O/netlib-infeas.txt > $O/netlib-infeas.log 2>&1
$O/land/run -j 2 -m bench/netlib-kennington.manifest -d bench/instances-kennington \
    -b bench/netlib-kennington.baseline -o $O/netlib-kennington.txt > $O/netlib-kennington.log 2>&1
for f in netlib netlib-infeas netlib-kennington; do
    echo "== $f"; grep -E "instances:|gate:|regressed" $O/$f.log
done > $O/lp-gates.txt
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline \
    impl=$O/miplib3-impl.txt land=$O/miplib3-land.txt > $O/m3cmp.txt
python3 bench/measurements/02-474/m17cmp.py before=bench/results/miplib2017.txt \
    impl=$O/miplib2017-impl.txt land=$O/miplib2017-land.txt > $O/m17cmp.txt
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    impl=$O/miplib2017-impl.txt land=$O/miplib2017-land.txt > $O/sum.txt 2>&1
echo done > $O/done
