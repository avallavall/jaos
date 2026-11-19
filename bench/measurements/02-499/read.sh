#!/bin/bash
# The readings behind 02-499, from copies of BASE (default 4e9f5c2c, ZI's
# old stop), of LAND (default HEAD) and of LAND with khook.patch (the stop
# read from JAOS_ZI_K) or diag.patch (ZI_DIAG_WORK keeps ZI's work out of
# the tree's count, ZI_DIAG_POOL keeps its points that do not beat the
# incumbent out of the pool). The records land in OUT (default ~/read499);
# two solves run at once. M17=0 skips MIPLIB 2017.
#
#   OUT=dir BASE=commit LAND=commit read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$R/bench/measurements/02-499
O=${OUT:-$HOME/read499}
B=${BASE:-4e9f5c2c}
L=${LAND:-HEAD}
I3=$R/bench/instances-miplib
I17=$R/bench/instances-miplib2017
rm -rf $O; mkdir -p $O/miplib3 $O/logs3 $O/miplib2017
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive $2 src include cli bench/run.c | tar x -C $O/$1
    [ -z "$3" ] || (cd $O/$1 && patch -s -p1 < $P/$3) || exit 1
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build base $B
build land $L
build khook $L khook.patch
build diag $L diag.patch
m3() {
    label=$1; b=$2; shift 2
    (cd $O/$b && env JAOS_NONE=1 "$@" ./run -j 2 -m $R/bench/miplib.manifest -e mip -d $I3 \
        -b $R/bench/miplib.baseline -o $O/miplib3/$label.txt $M3ARGS > $O/miplib3/$label.log 2>&1)
}
m3 base base
M3ARGS="-O mip_zi_round=false" m3 nozi land
m3 final land
for k in 1000 500 300 200 100 50 20; do m3 k$k khook JAOS_ZI_K=$k; done
python3 $P/cmp3.py $O/miplib3/nozi.txt base=$O/miplib3/base.txt \
    $(for k in 1000 500 300 200 100 50 20; do echo k$k=$O/miplib3/k$k.txt; done) > $O/m3cmp.txt
logs3() {
    label=$1; b=$2; envs=$3; shift 3
    mkdir -p $O/logs3/$label
    grep -v '^#' bench/miplib.manifest | awk 'NF>=2{print $1}' | \
        xargs -P 2 -I{} bash -c "env JAOS_NONE=1 $envs $O/$b/jaos solve $I3/{}.mps --threads 1 --log progress $* > $O/logs3/$label/{}.log 2>&1"
    for f in $O/logs3/$label/*.log; do
        n=$(basename $f .log)
        wk=$(sed -n 's/^work_units //p' $f); nd=$(sed -n 's/^nodes //p' $f)
        zi=$(grep -E 'ZI rounding ran' $f | sed -E 's/.*ran ([0-9]+) times for ([0-9]+) incumbents in ([0-9]+) work units/\1 \2 \3/')
        cr=$(grep -E 'crossover over the pool ran' $f | sed -E 's/.*ran ([0-9]+) times for ([0-9]+) incumbents in ([0-9]+) work units/\1 \2 \3/')
        zn=$(grep -E 'incumbent .* by ZI rounding' $f | sed -E 's/^node ([0-9]+):.*/\1/' | tr '\n' ',')
        printf '%-10s work=%-11s nodes=%-6s zi=[%s] zinc=[%s] cross=[%s]\n' $n "$wk" "$nd" "$zi" "$zn" "$cr"
    done > $O/logs3/$label.txt
}
logs3 base base ""
logs3 final land ""
logs3 nozi land "" --no-zi-round
logs3 dpool diag "ZI_DIAG_POOL=1"
logs3 dwork diag "ZI_DIAG_WORK=1"
logs3 dboth diag "ZI_DIAG_POOL=1 ZI_DIAG_WORK=1"
[ "${M17:-1}" = 0 ] && { echo done > $O/done; exit 0; }
m17() {
    label=$1; b=$2; shift 2
    mkdir -p $O/miplib2017/$label
    grep -v '^#' bench/miplib2017.manifest | awk 'NF>=5{print $1}' | \
        xargs -P 2 -I{} bash -c "$O/$b/jaos solve $I17/{}.mps --threads 1 --work-limit 10000000000 --log progress $* > $O/miplib2017/$label/{}.log 2>&1"
    grep -v '^#' bench/miplib2017.manifest | awk 'NF>=5{print $1, $5}' | while read f ref; do
        l=$O/miplib2017/$label/$f.log
        st=$(sed -n 's/^status //p' $l); ob=$(sed -n 's/^objective //p' $l)
        [ -n "$ob" ] || ob=$(sed -n 's/^incumbent //p' $l)
        bd=$(sed -n 's/^bound //p' $l); nd=$(sed -n 's/^nodes //p' $l)
        wk=$(sed -n 's/^work_units //p' $l); fi1=$(sed -n 's/^first_incumbent //p' $l)
        if [ "$st" = optimal ]; then
            echo "$f optimal rows=0 work=$wk nodes=$nd first=$fi1 obj=$ob ref=$ref"
        else
            echo "$f $st rows=0 work=$wk nodes=$nd first=$fi1 inc=${ob:-none} bound=$bd ref=$ref"
        fi
    done > $O/miplib2017/$label.txt
    for f in $O/miplib2017/$label/*.log; do
        last=$(grep -E ': incumbent [-0-9.e+]* (by|from)' $f | tail -1 | sed 's/.*incumbent [-0-9.e+]* //')
        s=$(grep -E 'ran [0-9]+ times' $f | sed -E 's/branch and bound: (.*) ran ([0-9]+) times for ([0-9]+) incumbents in ([0-9]+) work units/\1:\2\/\3\/\4/' | sed 's/the //; s/ of the incumbent//' | tr '\n' ';')
        zn=$(grep -E 'incumbent .* by ZI rounding' $f | sed -E 's/^node ([0-9]+):.*/\1/' | tr '\n' ',')
        printf '%-20s last[%s] zi_nodes[%s]\n    %s\n' $(basename $f .log) "$last" "$zn" "$s"
    done > $O/miplib2017/$label-who.txt
}
m17 final land
m17 nozi land --no-zi-round
git show $B:bench/results/miplib2017.txt > $O/miplib2017/base.txt
python3 bench/measurements/02-328/m17sum.py base=$O/miplib2017/base.txt \
    final=$O/miplib2017/final.txt nozi=$O/miplib2017/nozi.txt > $O/sum.txt
echo done > $O/done
