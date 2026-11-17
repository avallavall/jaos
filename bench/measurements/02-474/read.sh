#!/bin/bash
# The readings behind 02-474, one solve at a time, from copies of 003a8f9.
# HiGHS and SCIP on binkar10_1 (SCIP_PYTHON names a Python with pyscipopt;
# without it SCIP is left out); prototype.patch's JAOS_MIRX arms on
# binkar10_1's root and tree, on MIPLIB 3 and MIPLIB 2017, and its census
# of the node MIR's lift against the branching gain; node-mir.patch, the
# gated change, c1.patch, its first form over every row, and null.patch,
# a work-only change of the trial's size, on
# MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017 at 1e10 work
# units. The records land in OUT (default ~/read474).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-474
O=${OUT:-$HOME/read474}
B=bench/instances-miplib2017/binkar10_1.mps
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive 003a8f9 src include cli bench/run.c | tar x -C $O/$1
    [ -z "$2" ] || (cd $O/$1 && patch -s -p1 < $H/$2) || exit 1
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build proto prototype.patch
build c2 node-mir.patch
build c1 c1.patch
build null null.patch
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
[ -z "$HI" ] || $HI --options_file bench/compare/highs-mip.opt --time_limit 120 $B > $O/highs-binkar10_1.txt 2>&1
if [ -n "$SCIP_PYTHON" ]; then
    $SCIP_PYTHON $H/scip_stats.py $B 120 > $O/scip-binkar10_1.txt 2>&1
    $SCIP_PYTHON $H/scip_root.py $B > $O/scip-root.txt 2>&1
    $SCIP_PYTHON $H/scip_cuts.py $B > $O/scip-cuts.txt 2>&1
    for a in default no-node-sepa no-conflict no-node-sepa-no-conflict-no-restart-no-redcost bare pscost-branching; do
        $SCIP_PYTHON $H/scip_tree.py $B $a 150
    done > $O/scip-tree.txt 2>&1
fi
root() {
    printf '%-10s %s | %s\n' "$1" "$(echo "$2" | grep '^root: relaxation' | head -1)" \
        "$(echo "$2" | sed -n 's/^work_units //p')"
}
for v in 0 1 2 3 8 9 11 19 27; do
    root mirx$v "$(JAOS_MIRX=$v $O/proto/jaos solve $B --node-limit 1 --log progress 2>&1)"
done > $O/jaos-root.txt
tree() {
    label=$1; v=$2; shift 2
    out=$(JAOS_MIRX=$v $O/proto/jaos solve $B --work-limit 10000000000 "$@" 2>&1)
    printf '%-12s %-10s nodes %6s work %12s bound %s\n' $label \
        "$(echo "$out" | sed -n 's/^status //p')" "$(echo "$out" | sed -n 's/^nodes //p')" \
        "$(echo "$out" | sed -n 's/^work_units //p')" "$(echo "$out" | sed -n 's/^bound //p')"
}
{
    tree base 0
    tree root3 3; tree root11 11; tree root27 27
    tree d3-27 91 --node-mir
    tree d10-0 0 --cut-depth 10 --node-mir; tree d10-27 91 --cut-depth 10 --node-mir
    for v in 0 65 66 67 75 91; do tree all-$v $v --cut-depth 100000 --node-mir; done
    for v in 32 99 107 123 227; do tree deep-$v $v --node-mir; done
    for a in c1 c2; do
        out=$($O/$a/jaos solve $B --work-limit 10000000000 --log progress 2>&1)
        echo "$a $(echo "$out" | grep -E '^(status|nodes|work_units|bound) ' | tr '\n' ' ')"
        echo "$out" | grep 'MIR cuts'
    done
} > $O/jaos-tree.txt
m3() {
    $2 -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 -o $O/miplib3-$1.txt ${@:3} > $O/miplib3-$1.log 2>&1
}
m17() {
    $2 -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
        -L 1e10 -o $O/miplib2017-$1.txt ${@:3} > $O/miplib2017-$1.log 2>&1
}
JAOS_MIRX=27 m3 x27 $O/proto/run
JAOS_MIRX=0 m3 x0nm $O/proto/run -O mip_node_mir=1 -O mip_cut_depth=100000
JAOS_MIRX=67 m3 x3nm $O/proto/run -O mip_node_mir=1 -O mip_cut_depth=100000
JAOS_MIRX=227 m3 p227nm $O/proto/run -O mip_node_mir=1
JAOS_MIRX=123 m3 p123nm $O/proto/run -O mip_node_mir=1
m3 c2 $O/c2/run
m3 c1 $O/c1/run
m3 null $O/null/run
JAOS_MIRX=27 m17 x27 $O/proto/run
JAOS_MIRX=227 m17 p227nm $O/proto/run -O mip_node_mir=1
m17 c2 $O/c2/run
m17 c1 $O/c1/run
m17 null $O/null/run
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    x27=$O/miplib2017-x27.txt p227nm=$O/miplib2017-p227nm.txt \
    c2=$O/miplib2017-c2.txt c1=$O/miplib2017-c1.txt \
    null=$O/miplib2017-null.txt > $O/sum.txt
for f in pk1 mas74 neos-3046615-murg; do
    printf '%-18s %s\n' $f "$($O/c1/jaos solve bench/instances-miplib2017/$f.mps \
        --work-limit 10000000000 --no-node-mir 2>&1 | grep -E '^(nodes|bound) ' | tr '\n' ' ')"
done > $O/c1-node-mir-off.txt
census() {
    D=bench/instances-miplib; [ -f $D/$1.mps ] || [ -f $D/$1.mps.gz ] || D=bench/instances-miplib2017
    f=$D/$1.mps; [ -f $f ] || f=$f.gz
    printf '%-22s %s\n' $1 "$(JAOS_MIRX=227 $O/proto/jaos solve $f --work-limit 2000000000 \
        --log progress --node-mir 2>&1 | grep '^NMSTAT' | sed 's/NMSTAT //')"
}
for f in $(grep -v '^#' bench/miplib.manifest bench/miplib2017.manifest | awk -F: 'NF>1{print $2}' | awk 'NF>0{print $1}'); do
    census $f
done > $O/census.txt
echo done > $O/done
