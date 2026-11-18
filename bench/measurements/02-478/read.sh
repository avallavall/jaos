#!/bin/bash
# The readings behind 02-478, one solve at a time, from copies of b53a1ffd
# (the record), of b53a1ffd with MIP_LATTICE_NODES at 1, 100, 1000 and
# 5000, of 52cc3a45 (before 02-475, whose first 20000 nodes reach
# gen-ip002's optimum), of b53a1ffd with proto.patch, and of the change
# (LANDED, default HEAD), also with MIP_LATTICE_NODES at 5000, 10000 and
# 20000. proto.patch is the change's check, its first phase left at
# b53a1ffd's 20000 nodes, with switches read from
# the environment: JAOS_NOTRIAL=1 keeps the reduced tree whatever its bound,
# JAOS_REPAIR=1 rounds each node's point with one column moved by one when
# plain rounding breaks rows, JAOS_LATBOUND=1 takes the reduced tree's nodes
# by bound, JAOS_LATNOHEUR=1 turns its heuristics off, and JAOS_LATDUMP,
# JAOS_LATMPS and JAOS_NODEDUMP (with JAOS_NODECOLS=1 for the columns' tree)
# write the basis, the reduced model and every node's point. SCIP_PYTHON
# names a Python with numpy and pyscipopt for the readings from outside the
# tool; without it they are left out. Then MIPLIB 3 against
# bench/miplib.baseline and MIPLIB 2017 at 1e10 work units with the change.
# The records land in OUT (default ~/read478).
#
#   OUT=dir SCIP_PYTHON=python LANDED=commit read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-478
O=${OUT:-$HOME/read478}
D=bench/instances-miplib2017
P=$SCIP_PYTHON
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    mkdir -p $O/$1
    git archive $2 src include cli bench/run.c | tar x -C $O/$1
    [ -z "$3" ] || (cd $O/$1 && patch -s -p1 < $H/$3) || exit 1
    [ -z "$4" ] || sed -i "$4" $O/$1/src/mip.c
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build base b53a1ffd
build old 52cc3a45
build proto b53a1ffd proto.patch
build change ${LANDED:-HEAD}
for k in 1 100 1000 5000; do
    build n$k b53a1ffd "" "s/^constexpr int64_t MIP_LATTICE_NODES = 20000;/constexpr int64_t MIP_LATTICE_NODES = $k;/"
done
for k in 5000 10000 20000; do
    build c$k ${LANDED:-HEAD} "" "s/^constexpr int64_t MIP_LATTICE_NODES = 1000;/constexpr int64_t MIP_LATTICE_NODES = $k;/"
done
row() {
    printf '%-10s %-28s %-10s nodes %7s work %12s bound %-20s inc %s\n' $1 "$2" \
        "$(echo "$3" | sed -n 's/^status //p')" "$(echo "$3" | sed -n 's/^nodes //p')" \
        "$(echo "$3" | sed -n 's/^work_units //p')" "$(echo "$3" | sed -n 's/^bound //p')" \
        "$(echo "$3" | sed -n 's/^incumbent //p')"
    echo "$3" | grep -E '^lattice: after [0-9]+ work|: incumbent .* by|: incumbent .*integral' | tail -3 | sed 's/^/    /'
}
solve() {
    $O/$1/jaos solve $D/$2.mps --work-limit ${L:-10000000000} --log progress "${@:3}" 2>&1
}
for g in gen-ip002 gen-ip054; do
    row $g base "$(solve base $g)"
    row $g "base --no-lattice" "$(solve base $g --no-lattice)"
    row $g change "$(solve change $g)"
    row $g "change --node-select bound" "$(solve change $g --node-select bound)"
done > $O/jaos-change.txt
for g in gen-ip002 gen-ip054; do
    for k in 1 100 1000 5000; do
        row $g "first phase $k" "$(solve n$k $g)"
    done
    for k in 5000 10000 20000; do
        row $g "change, first phase $k" "$(solve c$k $g)"
    done
done > $O/jaos-first.txt
for g in gen-ip002 gen-ip054; do
    row $g "no trial" "$(JAOS_NOTRIAL=1 solve proto $g)"
    row $g "no trial, bound" "$(JAOS_NOTRIAL=1 JAOS_LATBOUND=1 solve proto $g)"
    row $g "no trial, bound, no heur" "$(JAOS_NOTRIAL=1 JAOS_LATBOUND=1 JAOS_LATNOHEUR=1 solve proto $g)"
    row $g "no trial, no heur" "$(JAOS_NOTRIAL=1 JAOS_LATNOHEUR=1 solve proto $g)"
    row $g "no trial, repair" "$(JAOS_NOTRIAL=1 JAOS_REPAIR=1 solve proto $g)"
    row $g "no trial, bound, repair" "$(JAOS_NOTRIAL=1 JAOS_LATBOUND=1 JAOS_REPAIR=1 solve proto $g)"
    row $g "trial, repair" "$(JAOS_REPAIR=1 solve proto $g)"
done > $O/jaos-reduced.txt

$O/change/jaos solve $D/gen-ip002.mps --work-limit 10000000000 --pool-out $O/opt > /dev/null 2>&1
$O/old/jaos solve $D/gen-ip002.mps --work-limit 10000000000 --log progress 2>&1 |
    grep -E ': incumbent|^bound|^incumbent' > $O/jaos-52cc3a45.txt
for g in gen-ip002 gen-ip054; do
    JAOS_LATDUMP=$O/lat-$g.txt JAOS_LATMPS=$O/red-$g.mps $O/proto/jaos solve $D/$g.mps --node-limit 20001 > /dev/null 2>&1
done
JAOS_NOTRIAL=1 JAOS_NODEDUMP=$O/nodes-red.txt $O/proto/jaos solve $D/gen-ip002.mps --work-limit 4000000000 > /dev/null 2>&1
JAOS_NODECOLS=1 JAOS_NODEDUMP=$O/nodes-cols.txt $O/proto/jaos solve $D/gen-ip002.mps --no-lattice --node-limit 80000 > /dev/null 2>&1
{
    for a in "" "--node-select bound"; do
        row gen-ip002 "optimum given, lattice $a" "$(solve base gen-ip002 --partial-start $O/opt-0.pt $a)"
        row gen-ip002 "optimum given, columns $a" "$(solve base gen-ip002 --partial-start $O/opt-0.pt --no-lattice $a)"
    done
} > $O/jaos-optstart.txt

[ -z "$P" ] && exit 0
$P $H/incpt.py $O/lat-gen-ip002.txt $O/inc.pt
$P $H/ystart.py $O/lat-gen-ip002.txt $O/ystart.pt
{
    for a in "" "--node-select bound"; do
        row gen-ip002 "columns from -4771.64 $a" "$(L=8800000000 solve base gen-ip002 --partial-start $O/inc.pt --no-lattice $a)"
    done
} > $O/jaos-restart.txt
red() {
    out=$($O/base/jaos solve $O/red-gen-ip002.mps --no-lattice --partial-start $O/ystart.pt --work-limit 5000000000 --log progress "${@:2}" 2>&1)
    row reduced "$1" "$out"
}
{
    red default
    red dive --dive
    red bound --node-select bound
    red "dive heuristic to depth 5" --dive-heuristic-depth 5
    red "rins 50" --rins 50
    red "no heuristics" --no-heuristics
    red "dive, 5 backtracks" --dive --dive-backtrack 5
    red "pump 20, general" --feaspump 20 --pump-general 1
} > $O/jaos-reduced-switches.txt
M=$D/gen-ip002.mps
$P $H/offline.py $M $O/lat-gen-ip002.txt $O/nodes-red.txt $H > $O/offline-reduced.txt 2>&1
$P $H/coff.py $M $O/nodes-cols.txt $H > $O/offline-columns.txt 2>&1
{
    echo "reduced"; $P $H/rad.py $M $O/lat-gen-ip002.txt $H $O/nodes-red.txt 1
    head -20000 $O/nodes-cols.txt > $O/nodes-cols20k.txt
    echo "columns, first 20000 nodes"; $P $H/rad.py $M $O/lat-gen-ip002.txt $H $O/nodes-cols20k.txt 0 -4754.42
    echo "columns, first 20000 nodes, running incumbent"; $P $H/rad2.py $M $H $O/nodes-cols.txt -4754.42 20000
} > $O/offline-radius.txt 2>&1
$P $H/near.py $M $O/lat-gen-ip002.txt $O/opt-0.pt $H $O/nodes-red.txt $O/nodes-cols.txt > $O/offline-near.txt 2>&1
$P $H/dives.py $M $O/lat-gen-ip002.txt $O/nodes-red.txt $H 500 > $O/offline-dives.txt 2>&1
for s in y x; do $P $H/center.py $M $O/lat-gen-ip002.txt $H 3000 $s; done > $O/offline-center.txt 2>&1
$P $H/rins.py $M $O/lat-gen-ip002.txt $O/opt-0.pt $H > $O/offline-count.txt 2>&1

S=$O/sub; mkdir -p $S
$P $H/sub3.py $D/gen-ip002.mps $H $S a 20
$P $H/sub3.py $D/gen-ip054.mps $H $S b 20
for f in $S/*.mps; do
    for t in base change c20000; do
        row $(basename $f .mps) $t "$($O/$t/jaos solve $f --work-limit 2000000000 --log progress 2>&1)"
    done
done > $O/sweep.txt
for i in a_04 a_11 a_14 a_16 a_17 a_19; do
    for t in base change c20000; do
        row $i "$t, 1e10" "$($O/$t/jaos solve $S/$i.mps --work-limit 10000000000 --log progress 2>&1)"
    done
done > $O/sweep-1e10.txt

mkdir -p $O/miplib3 $O/miplib2017
$O/change/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib -b bench/miplib.baseline -L 1e10 -o $O/miplib3/change.txt > $O/miplib3/change.log 2>&1
$O/change/run -j 1 -m bench/miplib2017.manifest -e mip -d $D -L 1e10 -o $O/miplib2017/change.txt > $O/miplib2017/change.log 2>&1
python3 bench/measurements/02-328/m17sum.py mine=$O/miplib2017/change.txt before=bench/results/miplib2017.txt > $O/sum.txt
