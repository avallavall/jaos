#!/bin/bash
# The readings behind 02-500. Builds a copy of the tree it sits in (the
# change) and of 97541e88 (head), then of 02-482's prototype.patch on head
# with prototype/colprop.py and colprop2.py applied in that order (the
# worklist arms; rules.py, rules2_tail.py, prealways.py and nullw.py give
# the other prototype arms). Then: neos5, pk1 and mas76 to their close under
# the change at 300, 600 and 1000 proofs and with the proofs off, the null
# readings (JAOS_NULL from nullw.py on both), the symmetry shares, and MIPLIB
# 3 under the change, the share rule alone and cut depth 0. J readings at a
# time (default 3), records in OUT (default ~/read500).
#
#   OUT=dir J=3 bench/measurements/02-500/read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-500
O=${OUT:-$HOME/read500}
J=${J:-3}
I17=$R/bench/instances-miplib2017
I3=$R/bench/instances-miplib
mkdir -p $O/out
build() {
    rm -rf $O/$1; mkdir -p $O/$1
    if [ "$2" = tree ]; then
        (cd $R && tar cf - --exclude=build src include cli bench/run.c Makefile) | tar x -C $O/$1
    else
        git -C $R archive $2 src include cli bench/run.c Makefile | tar x -C $O/$1
    fi
    shift 2
    (cd $O/$1 && for e in "$@"; do
        case $e in
            *.patch) patch -s -p1 < $e ;;
            *.py) python3 $e src/mip.c ;;
        esac
    done && make -j4 build/cli/jaos build/bench/run > /dev/null 2>&1) || exit 1
}
build change tree
build change-null tree $H/prototype/nullw.py
build head 97541e88
build head-null 97541e88 $H/prototype/nullw.py
build proto 97541e88 $R/bench/measurements/02-482/prototype.patch \
    $H/prototype/colprop.py $H/prototype/colprop2.py
one() {
    O=$1; name=$2; b=$3; f=$4; w=$5; envs=$6; shift 6
    env X=0 $(echo $envs | tr ',' ' ') $O/$b/build/cli/jaos solve $f --threads 1 \
        --work-limit $w --log summary "$@" > $O/out/$name.log 2>&1
    echo "$name $(grep -E '^(status|nodes|work_units|bound|incumbent) ' \
        $O/out/$name.log | tr '\n' ' ')" >> $O/readings.txt
}
export -f one
{
    for p in 300 600 1000; do
        echo "$O n5-$p change $I17/neos5.mps 300000000000 X=0 --dual-proofs $p"
        echo "$O pk1-$p change $I17/pk1.mps 20000000000 X=0 --dual-proofs $p"
    done
    echo "$O n5-0 change $I17/neos5.mps 300000000000 X=0 --dual-proofs 0"
    echo "$O pk1-0 change $I17/pk1.mps 20000000000 X=0 --dual-proofs 0"
    echo "$O mas76-600 change $I17/mas76.mps 20000000000 X=0"
    for k in 100 200 300 400; do
        for m in neos5 pk1 mas76; do
            echo "$O $m-null$k change-null $I17/$m.mps 300000000000 JAOS_NULL=$k"
            echo "$O $m-head-null$k head-null $I17/$m.mps 300000000000 JAOS_NULL=$k"
        done
    done
    for a in 300:4249,JAOS_SCPROOFS=300 1000:4249,JAOS_SCPROOFS=1000; do
        echo "$O proto-${a%%:*}-cd0 proto $I17/neos5.mps 300000000000 JAOS_SC=${a#*:} --cut-depth 0"
    done
    echo "$O proto-300-cd3 proto $I17/neos5.mps 300000000000 JAOS_SC=4249,JAOS_SCPROOFS=300"
} | xargs -P $J -I{} bash -c 'one {}'
for f in $I3/*.mps $I17/*.mps; do
    s=$($O/change/build/cli/jaos solve $f --threads 1 --node-limit 2 --log summary 2>&1 |
        grep -o '[0-9.]* of the integer columns in orbits' | tail -1)
    [ -n "$s" ] && echo "$(basename $f .mps) $s"
done > $O/shares.txt
cd $R
for a in change:: rule::-O,mip_dual_proofs=0 cd0:head:-O,mip_cut_depth=0 head:head:; do
    n=${a%%:*}; rest=${a#*:}; b=${rest%%:*}; args=${rest#*:}
    [ -z "$b" ] && b=change
    $O/$b/build/bench/run -j 2 -m bench/miplib.manifest -e mip -d $I3 \
        $(echo $args | tr ',' ' ') -o $O/m3-$n.txt > /dev/null 2>&1
done
python3 bench/measurements/02-482/m3off.py $O/m3-head.txt change=$O/m3-change.txt \
    rule=$O/m3-rule.txt cd0=$O/m3-cd0.txt > $O/m3cmp.txt
