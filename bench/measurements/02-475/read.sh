#!/bin/bash
# The readings behind 02-475, one solve at a time, from a copy of 4dc8e97
# with prototype.patch, whose three parts the environment switches:
# JAOS_NOGRID set to anything drops the continuous columns from the
# objective step,
# JAOS_NCUT=1 stops a node's dual simplex at the prune bound and learns the
# pseudocost from the bound it stopped at (2 stops without learning), and
# JAOS_NRC=1 fixes integer bounds by reduced costs at the nodes.
# HiGHS and SCIP on pk1 and mas76 (SCIP_PYTHON names a Python with
# pyscipopt; without it SCIP is left out), SCIP's ablation on mas76 with
# 02-474's scip_tree.py, JAOS's arms on pk1 and mas76 at 5e10 work units,
# the census of objective steps, the arms on MIPLIB 3 against
# bench/miplib.baseline and the landing arm on MIPLIB 2017 at 1e10. The
# records land in OUT (default ~/read475).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-475
T=$R/bench/measurements/02-474
O=${OUT:-$HOME/read475}
D=$R/bench/instances-miplib2017
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
mkdir -p $O/proto
git archive 4dc8e97 src include cli bench/run.c | tar x -C $O/proto
(cd $O/proto && patch -s -p1 < $H/prototype.patch) || exit 1
(cd $O/proto && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
 ar rcs libjaos.a *.o
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
J=$O/proto/jaos
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
for f in pk1 mas76; do
    [ -z "$HI" ] || $HI --options_file bench/compare/highs-mip.opt --time_limit 900 $D/$f.mps > $O/highs-$f.txt 2>&1
    [ -z "$SCIP_PYTHON" ] || $SCIP_PYTHON $T/scip_stats.py $D/$f.mps 900 > $O/scip-$f.txt 2>&1
done
if [ -n "$SCIP_PYTHON" ]; then
    for a in default no-redcost no-node-sepa no-conflict no-restart pscost-branching \
             no-node-sepa-no-conflict-no-restart-no-redcost bare; do
        $SCIP_PYTHON $T/scip_tree.py $D/mas76.mps $a 600
    done > $O/scip-tree-mas76.txt 2>&1
fi
arm() {
    label=$1; f=$2; shift 2
    out=$(env "$@" $J solve $D/$f.mps --work-limit 50000000000 --log summary 2>&1)
    printf '%-6s %-10s %-9s nodes %7s work %12s bound %-20s %s %s\n' $f $label \
        "$(echo "$out" | sed -n 's/^status //p')" "$(echo "$out" | sed -n 's/^nodes //p')" \
        "$(echo "$out" | sed -n 's/^work_units //p')" "$(echo "$out" | sed -n 's/^bound //p')" \
        "$(echo "$out" | grep -o '[0-9]* node relaxations stopped' | sed 's/ node.*/ stopped/')" \
        "$(echo "$out" | grep -o '[0-9]* column bounds tightened' | sed 's/ column.*/ tightened/')"
}
{
    for f in pk1 mas76; do
        arm head $f JAOS_NOGRID=1
        arm grid $f
        arm cut1 $f JAOS_NOGRID=1 JAOS_NCUT=1
        arm grid-cut1 $f JAOS_NCUT=1
        arm grid-cut2 $f JAOS_NCUT=2
        arm rc $f JAOS_NOGRID=1 JAOS_NRC=1
        arm grid-rc $f JAOS_NRC=1
        arm all $f JAOS_NRC=1 JAOS_NCUT=1
    done
    printf '%-6s %-10s %s\n' pk1 cutoff10 "$(JAOS_NOGRID=1 $J solve $D/pk1.mps --work-limit 50000000000 \
        --cutoff 10.00001 2>&1 | grep -E '^(status|nodes|work_units|bound) ' | tr '\n' ' ')"
} > $O/arms.txt
for f in $(grep -v '^#' bench/miplib.manifest | awk 'NF>0{print $1}'); do
    echo "$f $($J solve bench/instances-miplib/$f.mps --node-limit 1 --log summary 2>&1 |
        grep -E 'multiple of|continuous columns share' | sed 's/branch and bound: //' | tr '\n' ' ')"
done > $O/census.txt
for f in $(grep -v '^#' bench/miplib2017.manifest | awk 'NF>0{print $1}'); do
    echo "$f $($J solve $D/$f.mps --node-limit 1 --log summary 2>&1 |
        grep -E 'multiple of|continuous columns share' | sed 's/branch and bound: //' | tr '\n' ' ')"
done >> $O/census.txt
m3() {
    label=$1; shift
    env "$@" $O/proto/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 -o $O/miplib3-$label.txt > $O/miplib3-$label.log 2>&1
}
m3 off JAOS_NOGRID=1
m3 grid
m3 cut JAOS_NOGRID=1 JAOS_NCUT=1
m3 rc JAOS_NOGRID=1 JAOS_NRC=1
m3 cut2 JAOS_NRC=1 JAOS_NCUT=2
m3 all JAOS_NRC=1 JAOS_NCUT=1
env JAOS_NRC=1 JAOS_NCUT=1 $O/proto/run -j 1 -m bench/miplib2017.manifest -e mip -d $D \
    -L 1e10 -o $O/miplib2017-all.txt > $O/miplib2017-all.log 2>&1
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    all=$O/miplib2017-all.txt > $O/sum.txt
echo done > $O/done
