#!/bin/bash
# The readings behind 02-489, from copies of 37720d32 with 02-488's
# prototype.patch (JAOS_GH=1 is ZI rounding alone; its switches are listed
# in 02-488's read.sh) and of the landed commit (LAND, default HEAD). The
# records land in OUT (default ~/read489); two solves run
# at once.
#
#   OUT=dir LAND=commit read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$R/bench/measurements/02-488
O=${OUT:-$HOME/read489}
L=${LAND:-HEAD}
I3=$R/bench/instances-miplib
I17=$R/bench/instances-miplib2017
rm -rf $O; mkdir -p $O/miplib3 $O/miplib2017 $O/g394
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
build proto 37720d32 prototype.patch
build land $L
m17() {
    label=$1; b=$2; shift 2
    mkdir -p $O/miplib2017/$label
    grep -v '^#' bench/miplib2017.manifest | awk 'NF>=5{print $1}' | while read f; do
        echo "env $* $O/$b/jaos solve $I17/$f.mps --work-limit 10000000000 --log progress > $O/miplib2017/$label/$f.log 2>&1"
    done | xargs -P 2 -I{} bash -c "{}"
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
        printf '%-20s last[%s]\n    %s\n' $(basename $f .log) "$last" "$s"
    done > $O/miplib2017/$label-who.txt
}
m3() {
    label=$1; b=$2; shift 2
    (cd $O/$b && env "$@" ./run -j 2 -m $R/bench/miplib.manifest -e mip -d $I3 \
        -b $R/bench/miplib.baseline -o $O/miplib3/$label.txt > $O/miplib3/$label.log 2>&1)
}
m3 zi proto JAOS_GH=1
m3 land land
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline \
    zi=$O/miplib3/zi.txt land=$O/miplib3/land.txt > $O/m3cmp.txt
m17 zi proto JAOS_GH=1
m17 land land
python3 bench/measurements/02-474/m17cmp.py before=bench/results/miplib2017.txt \
    zi=$O/miplib2017/zi.txt land=$O/miplib2017/land.txt > $O/m17cmp.txt
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    zi=$O/miplib2017/zi.txt land=$O/miplib2017/land.txt > $O/sum.txt
python3 -I $P/gen394.py bench/measurements/02-394/mipcmp.py $O/g394 \
    bench/measurements/02-394/mipcmp.txt > $O/g394/list.txt
for a in zi=proto land=land; do
    while read f; do
        r=$(JAOS_GH=1 $O/${a#*=}/jaos solve $f --work-limit 500000000 2>&1)
        get() { echo "$r" | sed -n "s/^$1 //p" | head -1; }
        echo "$(basename $f .mps) inc=$(get incumbent)$(get objective) first=$(get first_incumbent) st=$(get status)"
    done < $O/g394/list.txt > $O/g394/${a%%=*}.txt
done
echo done > $O/done
