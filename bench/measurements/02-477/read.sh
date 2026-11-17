#!/bin/bash
# The readings behind 02-477, one solve at a time, from copies of 4dc8e97.
# HiGHS and SCIP on neos5 and assign1-5-8 (SCIP_PYTHON names a Python with
# pyscipopt; without it SCIP is left out); JAOS's root and tree on both;
# the symmetry search's spend on both sets at three caps (symonly.patch);
# assign1-5-8's tree with the whole group and five switches, and with its
# cost columns declared integer (zint.py); sym.patch, the root's cap raised
# to 1000, and change.patch, the change (landing.patch is it with its tests
# and docs), on MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017 at
# 1e10 work units. The records land in OUT (default ~/read477).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-477
O=${OUT:-$HOME/read477}
I=bench/instances-miplib2017
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive 4dc8e97 src include cli bench/run.c | tar x -C $O/$1
    [ -z "$2" ] || (cd $O/$1 && patch -s -p1 < $H/$2) || exit 1
    [ -z "$3" ] || sed -i "s/^constexpr int64_t MIP_SYMMETRY_WORK = .*;/constexpr int64_t MIP_SYMMETRY_WORK = $3;/" $O/$1/src/mip.c
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build base
build sym sym.patch
build land change.patch
for w in 250 1000 4000; do build symonly$w symonly.patch $w; done
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
for f in neos5 assign1-5-8; do
    [ -z "$HI" ] || $HI --options_file bench/compare/highs-mip.opt --time_limit 600 $I/$f.mps > $O/highs-$f.txt 2>&1
    [ -z "$SCIP_PYTHON" ] || $SCIP_PYTHON bench/measurements/02-474/scip_stats.py $I/$f.mps 600 > $O/scip-$f.txt 2>&1
    $O/base/jaos solve $I/$f.mps --node-limit 1 --log progress > $O/jaos-root-$f.txt 2>&1
    $O/base/jaos solve $I/$f.mps --work-limit 10000000000 --log progress > $O/jaos-tree-$f.txt 2>&1
done
if [ -n "$SCIP_PYTHON" ]; then
    $SCIP_PYTHON bench/measurements/02-474/scip_root.py $I/assign1-5-8.mps > $O/scip-root-assign1-5-8.txt 2>&1
    for a in default no-symmetry no-node-sepa no-conflict no-restart no-propagation bare; do
        $SCIP_PYTHON $H/scip_tree.py $I/assign1-5-8.mps $a 20000 600
    done > $O/scip-tree-assign1-5-8.txt 2>&1
fi
for set in miplib miplib2017; do
    man=bench/miplib.manifest; [ $set = miplib2017 ] && man=bench/miplib2017.manifest
    for f in $(grep -v '^#' $man | awk 'NF>0{print $1}'); do
        p=bench/instances-$set/$f.mps; [ -f $p ] || p=$p.gz
        line="$f"
        for w in 250 1000 4000; do
            line="$line | $w: $(JAOS_SYMONLY=1 $O/symonly$w/jaos solve $p --node-limit 1 2>&1 | grep '^SYM' | tr '\n' ' ')"
        done
        echo "$line"
    done
done > $O/symsearch.txt
tree() {
    label=$1; b=$2; f=$3; shift 3
    out=$($O/$b/jaos solve $f --work-limit 10000000000 --log summary "$@" 2>&1)
    printf '%-11s %-5s %-10s nodes %6s work %12s bound %s orbit-branchings %s\n' $label $b \
        "$(echo "$out" | sed -n 's/^status //p')" "$(echo "$out" | sed -n 's/^nodes //p')" \
        "$(echo "$out" | sed -n 's/^work_units //p')" "$(echo "$out" | sed -n 's/^bound //p')" \
        "$(echo "$out" | grep -o '[0-9]* branchings widened' | cut -d' ' -f1)"
}
python3 $H/zint.py $I/assign1-5-8.mps $O/assign1-5-8-zint.mps
A=$I/assign1-5-8.mps
{
    tree default base $A; tree default sym $A; tree default land $A
    tree no-orbit sym $A --no-orbital; tree bound sym $A --node-select bound
    tree cut-all sym $A --cut-depth 100000; tree rel8 sym $A --opt mip_reliability=8
    tree zero-half sym $A --opt mip_zero_half_rounds=4
    tree zint sym $O/assign1-5-8-zint.mps
    tree neos-911970 base $I/neos-911970.mps; tree neos-911970 sym $I/neos-911970.mps
    tree neos-911970 land $I/neos-911970.mps
} > $O/jaos-arms.txt
for a in sym land; do
    $O/$a/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 -o $O/miplib3-$a.txt > $O/miplib3-$a.log 2>&1
    $O/$a/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
        -L 1e10 -o $O/miplib2017-$a.txt > $O/miplib2017-$a.log 2>&1
done
$O/land/run -j 2 -d bench/instances -b bench/netlib.baseline -o $O/netlib.txt > $O/netlib.log 2>&1
$O/land/run -j 2 -m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas \
    -b bench/netlib-infeas.baseline -o $O/netlib-infeas.txt > $O/netlib-infeas.log 2>&1
$O/land/run -j 2 -m bench/netlib-kennington.manifest -d bench/instances-kennington \
    -b bench/netlib-kennington.baseline -o $O/netlib-kennington.txt > $O/netlib-kennington.log 2>&1
for f in netlib netlib-infeas netlib-kennington; do
    echo "== $f"; grep -E "instances:|gate:|regressed" $O/$f.log
done > $O/lp-gates.txt
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline \
    sym=$O/miplib3-sym.txt land=$O/miplib3-land.txt > $O/m3cmp.txt
python3 bench/measurements/02-474/m17cmp.py before=bench/results/miplib2017.txt \
    sym=$O/miplib2017-sym.txt land=$O/miplib2017-land.txt > $O/m17cmp.txt
echo done > $O/done
