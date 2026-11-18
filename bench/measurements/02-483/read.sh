#!/bin/bash
# The readings behind 02-483, from copies of 1235543a with prototype.patch
# (its switches below), change.patch (the change; landing.patch is it with
# its tests and docs) and null.patch (the change with 02-474's null
# readings). The records land in OUT (default ~/read483); J solves run at
# once (default 3).
#
# prototype.patch's switches:
#   JAOS_GE   bits: 1 the Gomory rounds read as integer the slack of every
#             all-integer equality row with whole coefficients, on a model
#             with implied integers; 2 the same on its inequality rows;
#             4 on the cut rows; 8 on every model; 16 an equality row's
#             slack takes no term in the cut; 32 every equality row's slack
#             takes the integer coefficient
#   JAOS_GM   the root's Gomory rounds go on, up to this many, on a model
#             with implied integers (JAOS_GMALL: on every model), while the
#             round before lifted the bound by JAOS_GS (default 1e-4)
#   JAOS_GNZ  they stop once all the root's cuts hold this many times the
#             model's nonzeros (the change counts a round's cuts alone)
#   JAOS_GLOG a line a round with its lift and nonzeros
#   JAOS_NULL adds this many times the model's size in work at the root
#
#   OUT=dir J=3 read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-483
O=${OUT:-$HOME/read483}
P=${J:-3}
I3=$R/bench/instances-miplib
I17=$R/bench/instances-miplib2017
A=$I17/assign1-5-8.mps
B5=$I3/bell5.mps
rm -rf $O; mkdir -p $O/out $O/miplib3 $O/miplib2017
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive 1235543a src include cli bench/run.c | tar x -C $O/$1
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
one() {
    label=$1; b=$2; f=$3; shift 3
    envs=()
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
    [ "$1" = "--" ] && shift
    out=$(env "${envs[@]}" $O/$b/jaos solve $f --work-limit ${WL:-10000000000} --log progress "$@" 2>&1)
    echo "$out" > $O/out/$label.log
    printf '%-24s nodes %7s bound %-10s last %-9s root %-28s status %s work %s\n' "$label" \
        "$(echo "$out" | sed -n 's/^nodes //p')" "$(echo "$out" | sed -n 's/^bound //p' | cut -c1-10)" \
        "$(echo "$out" | grep -E '^node [0-9]+: [0-9]+ open' | tail -1 | sed 's/.*bound \([0-9.]*\).*/\1/' | cut -c1-9)" \
        "$(echo "$out" | sed -n 's/^root: relaxation \([0-9.]*\) after \([0-9]*\) cuts.*/\1 (\2 cuts)/p' | cut -c1-28)" \
        "$(echo "$out" | sed -n 's/^status //p')" "$(echo "$out" | sed -n 's/^work_units //p')"
}
export -f one
export O WL
spread() {
    f=$1; shift
    for arm in "$@"; do
        IFS='|' read -r label b envs args <<< "$arm"
        for n in ${NULLS:-0 150 300 450 600}; do
            echo "one $label-n$n $b $f $envs JAOS_NULL=$n -- $args"
        done
    done
}
{
    spread $A "head|proto||" "ge1|proto|JAOS_GE=1|" "ge7|proto|JAOS_GE=7|" "ge16|proto|JAOS_GE=16|" \
        "r3-ge1|proto|JAOS_GE=1|--cut-rounds 3" "r5|proto||--cut-rounds 5" "r5-ge1|proto|JAOS_GE=1|--cut-rounds 5" \
        "r5-ge7|proto|JAOS_GE=7|--cut-rounds 5" "r5-ge16|proto|JAOS_GE=16|--cut-rounds 5" \
        "r5-ge32|proto|JAOS_GE=32|--cut-rounds 5" "r10-ge1|proto|JAOS_GE=1|--cut-rounds 10" \
        "gm20|proto|JAOS_GM=20|" "gm20-ge1|proto|JAOS_GE=1 JAOS_GM=20|" \
        "gm20-all10|proto|JAOS_GM=20 JAOS_GNZ=10|" "gm20-all15|proto|JAOS_GM=20 JAOS_GNZ=15|" \
        "change|null||"
} | xargs -P $P -I{} bash -c "{}" > $O/assign.txt
{
    spread $B5 "head|proto||" "ge10|proto|JAOS_GE=10|" "ge11|proto|JAOS_GE=11|" "ge15|proto|JAOS_GE=15|"
    echo "one r2 proto $B5 -- --cut-rounds 2"
    echo "one r2-nomir proto $B5 -- --cut-rounds 2 --opt mip_node_mir=false"
    echo "one r5 proto $B5 -- --cut-rounds 5"
    echo "one r5-nomir proto $B5 -- --cut-rounds 5 --opt mip_node_mir=false"
    echo "one head-nomir proto $B5 -- --opt mip_node_mir=false"
    echo "one head-cutoff proto $B5 -- --opt mip_cutoff=8966420"
    echo "one r2-cutoff proto $B5 -- --cut-rounds 2 --opt mip_cutoff=8966420"
    echo "one change base $B5"
} | xargs -P $P -I{} bash -c "{}" > $O/bell5.txt
NULLS="0 150 300 450 600" spread $I3/misc07.mps "head|proto||" "ge1|proto|JAOS_GE=1|" |
    xargs -P $P -I{} bash -c "{}" > $O/misc07.txt
{
    for f in $I3/misc03 $I3/misc07 $I17/pk1 $I17/csched007 $I17/csched008 $I17/ic97_potential \
             $I17/markshare2 $I17/markshare_4_0; do
        b=$(basename $f)
        echo "one $b-head proto $f.mps"
        echo "one $b-ge1 proto $f.mps JAOS_GE=1"
        echo "one $b-gm5-ge1 proto $f.mps JAOS_GE=1 JAOS_GM=5"
        echo "one $b-gm20-ge1 proto $f.mps JAOS_GE=1 JAOS_GM=20"
        echo "one $b-change land $f.mps"
    done
    echo "one csched007-gm5 proto $I17/csched007.mps JAOS_GM=5"
    echo "one csched007-gm20 proto $I17/csched007.mps JAOS_GM=20"
} | xargs -P $P -I{} bash -c "{}" > $O/census.txt
for f in $I3/*.mps $I17/*.mps; do
    out=$($O/base/jaos solve $f --node-limit 1 --work-limit 2000000000 --log summary 2>&1)
    eq=$(awk '/^ROWS/{f=1;next}/^COLUMNS/{f=0}f&&$1=="E"{n++}END{print n+0}' $f)
    echo "$(basename $f .mps) grid[$(echo "$out" | grep -o 'a multiple of [0-9.e+-]*' | head -1)]" \
         "implied[$(echo "$out" | grep -o '[0-9]* continuous columns share' | head -1)] E-rows $eq" \
         "root $(echo "$out" | sed -n 's/^root: relaxation \([0-9.e+-]*\) after \([0-9]*\) cuts.*/\1 \2/p')"
done > $O/grid.txt
$O/land/jaos solve $A --work-limit 100000000000 --log progress 2>&1 |
    grep -E '^node [0-9]+0000: |^nodes|^bound|^work_units|^root: relax' > $O/change-1e11.txt
for f in $I3/bell5 $I3/misc03 $I3/misc07 $I3/mod008 $I3/p0201 $I3/p0033 $I17/assign1-5-8 \
         $I17/csched007 $I17/csched008 $I17/ic97_potential; do
    for arm in "head|1|" "r5|5|" "r5-ge1|5|JAOS_GE=1" "gm20|-1|JAOS_GM=20"; do
        IFS='|' read -r label r envs <<< "$arm"
        rr=""; [ "$r" -ge 0 ] && rr="--cut-rounds $r"
        out=$(env JAOS_GLOG=1 $envs $O/proto/jaos solve $f.mps --node-limit 1 $rr --log detail 2>&1)
        echo "$(basename $f) $label: $(echo "$out" | grep -E 'gomory more' | sed 's/gomory more: round \([0-9]*\) lift \([0-9.e+-]*\) nz \([0-9]*\) model nz \([0-9]*\) rows \([0-9]*\).*/r\1 \2 nz \3\/\4 rows \5/' | tr '\n' ';')"
        echo "    $(echo "$out" | grep -E '^root: relaxation' | cut -c1-80)"
    done
done > $O/roots.txt
m3() {
    label=$1; b=$2; shift 2
    envs=()
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
    [ "$1" = "--" ] && shift
    (cd $O/$b && env "${envs[@]}" ./run -j 2 -m $R/bench/miplib.manifest -e mip -d $I3 \
        -b $R/bench/miplib.baseline "$@" -o $O/miplib3/$label.txt > $O/miplib3/$label.log 2>&1)
}
m3 head proto
m3 r2 proto -- -O mip_cut_rounds=2 -L 2e10
m3 r5 proto -- -O mip_cut_rounds=5 -L 2e10
m3 ge9 proto JAOS_GE=9 -- -L 2e10
m3 ge11 proto JAOS_GE=11 -- -L 2e10
m3 ge16 proto JAOS_GE=16 -- -L 2e10
m3 ge32 proto JAOS_GE=32 -- -L 2e10
m3 change land
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline head=$O/miplib3/head.txt \
    r2=$O/miplib3/r2.txt r5=$O/miplib3/r5.txt ge9=$O/miplib3/ge9.txt ge11=$O/miplib3/ge11.txt \
    ge16=$O/miplib3/ge16.txt ge32=$O/miplib3/ge32.txt change=$O/miplib3/change.txt > $O/m3cmp.txt
(cd $O/land && ./run -j 2 -m $R/bench/miplib2017.manifest -e mip -d $I17 -L 1e10 \
    -o $O/miplib2017/change.txt > $O/miplib2017/change.log 2>&1)
python3 bench/measurements/02-474/m17cmp.py before=bench/results/miplib2017.txt \
    change=$O/miplib2017/change.txt > $O/m17cmp.txt
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    change=$O/miplib2017/change.txt > $O/sum.txt 2>&1
(cd $O/land && ./run -j 6 -d $R/bench/instances -b $R/bench/netlib.baseline -o $O/netlib.txt > $O/netlib.log 2>&1
 ./run -j 6 -m $R/bench/netlib-infeas.manifest -e infeasible -d $R/bench/instances-infeas \
     -b $R/bench/netlib-infeas.baseline -o $O/netlib-infeas.txt > $O/netlib-infeas.log 2>&1
 ./run -j 2 -m $R/bench/netlib-kennington.manifest -d $R/bench/instances-kennington \
     -b $R/bench/netlib-kennington.baseline -o $O/netlib-kennington.txt > $O/netlib-kennington.log 2>&1)
for f in netlib netlib-infeas netlib-kennington; do
    echo "== $f"; grep -E "instances:|gate:|baseline:" $O/$f.log
done > $O/lp-gates.txt
echo done > $O/done
