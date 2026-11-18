#!/bin/bash
# The readings behind 02-482, from copies of 5d774e5a: the six instances of
# TODO J7 at 1e10 work units under each arm of prototype.patch, more arms on
# mas76 and assign1-5-8, neos5 to the budget its tree closes in,
# the arms on MIPLIB 3, and SCIP's conflict analysis taken apart by scip.sh
# (SCIP_PYTHON names a Python with pyscipopt; without it SCIP is left out).
# J processes at a time (default 3). The records land in OUT (default
# ~/read482).
#
# prototype.patch's switch is JAOS_SC, a sum of bits:
#   1    conflicts from node relaxations stopped at the incumbent, over the
#        stopped duals and the path's bounds, kept in a pool and propagated
#   2    only binaries in a conflict        4    no implied bound in one
#   8    the dual proofs themselves kept     16   no conflict over bounds kept
#   32   local rows' multipliers dropped     64   duals of the relaxation
#        instead of refused                       solved on to its optimum
#   128  proofs propagated from their        256  the node's own bounds, not
#        activity over the path                   only the path's, in a proof
#   512  propagation charged no work         1024 proofs prune, tighten nothing
#   2048 after a presolved stop, a basic column's reduced cost under 1e-9
#        of its terms read as zero (without it, as computed)
# JAOS_SCPOOL (1000) caps the conflicts, JAOS_SCPROOFS (75) the proofs,
# JAOS_SCMAX (32) a conflict's bounds, JAOS_SCPASS (4) the passes a node makes.
#
#   OUT=dir J=3 SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-482
O=${OUT:-$HOME/read482}
J=${J:-3}
I=$R/bench/instances-miplib2017
mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    rm -rf $O/$1; mkdir -p $O/$1
    git archive 5d774e5a src include cli bench/run.c | tar x -C $O/$1
    [ -z "$2" ] || (cd $O/$1 && patch -s -p1 < $H/$2) || exit 1
    (cd $O/$1 && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build base
build proto prototype.patch
one() {
    O=$1; b=$2; arm=$3; f=$4; w=$5; envs=$6
    mkdir -p $O/arms/$arm
    env $(echo $envs | tr ',' ' ') $O/$b/jaos solve $I/$f.mps --work-limit $w --log summary \
        > $O/arms/$arm/$f.log 2>&1
}
export -f one
export I
SIX="gen-ip002 neos5 assign1-5-8 mas76 mas74 pk1"
{
    for f in $SIX; do echo "$O base base $f 10000000000 X=0"; done
    for a in lit:JAOS_SC=1 lit-pool100:JAOS_SC=1,JAOS_SCPOOL=100 lit-branched:JAOS_SC=5 \
             both:JAOS_SC=9 proof:JAOS_SC=153 proof-free:JAOS_SC=665 \
             proof-20:JAOS_SC=153,JAOS_SCPROOFS=20,JAOS_SCPASS=1 \
             prune:JAOS_SC=1177 prune-free:JAOS_SC=1689; do
        for f in $SIX; do echo "$O proto ${a%%:*} $f 10000000000 ${a#*:}"; done
    done
    echo "$O proto lit-2e9 mas76 2000000000 JAOS_SC=1"
    echo "$O proto lit-optimal-2e9 mas76 2000000000 JAOS_SC=65"
    echo "$O proto lit-local assign1-5-8 10000000000 JAOS_SC=33"
    echo "$O proto lit-node-bounds assign1-5-8 10000000000 JAOS_SC=257"
    echo "$O proto lit-local-node-bounds assign1-5-8 10000000000 JAOS_SC=289"
    for f in gen-ip002 mas76 pk1 neos5 mas74; do
        echo "$O proto r-proof $f 10000000000 JAOS_SC=2201"
    done
    for f in gen-ip002 mas76 pk1; do
        echo "$O proto r-proof-free $f 10000000000 JAOS_SC=2713"
    done
    echo "$O proto r-lit gen-ip002 10000000000 JAOS_SC=2049"
} | xargs -P $J -L 1 bash -c 'one $0 $1 $2 $3 $4 $5'
{
    for d in $O/arms/*; do
        for g in $d/*.log; do
            printf '%-20s %-12s %-10s nodes %8s work %12s bound %-13s stopped %7s %s | %s\n' \
                $(basename $d) $(basename $g .log) "$(sed -n 's/^status //p' $g)" \
                "$(sed -n 's/^nodes //p' $g)" "$(sed -n 's/^work_units //p' $g)" \
                "$(sed -n 's/^bound //p' $g | cut -c1-13)" \
                "$(sed -n 's/.*: \([0-9]*\) node relaxations stopped.*/\1/p' $g | tail -1)" \
                "$(grep -h '^conflict pool' $g | tail -1)" "$(grep -h '^proof pool' $g | tail -1)"
        done
    done
} > $O/arms.txt
m3() {
    label=$1; shift
    env "$@" $O/proto/run -j $J -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -o $O/miplib3-$label.txt > $O/miplib3-$label.log 2>&1
}
m3 off X=0
m3 lit-pool100 JAOS_SC=1 JAOS_SCPOOL=100
m3 proof JAOS_SC=153
m3 proof-free JAOS_SC=665
m3 prune JAOS_SC=1177
m3 r-proof JAOS_SC=2201
m3 r-proof-free JAOS_SC=2713
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline off=$O/miplib3-off.txt \
    lit-pool100=$O/miplib3-lit-pool100.txt proof=$O/miplib3-proof.txt \
    proof-free=$O/miplib3-proof-free.txt prune=$O/miplib3-prune.txt \
    r-proof=$O/miplib3-r-proof.txt r-proof-free=$O/miplib3-r-proof-free.txt > $O/m3cmp.txt
python3 $H/m3off.py $O/miplib3-off.txt lit-pool100=$O/miplib3-lit-pool100.txt \
    proof=$O/miplib3-proof.txt proof-free=$O/miplib3-proof-free.txt prune=$O/miplib3-prune.txt \
    r-proof=$O/miplib3-r-proof.txt r-proof-free=$O/miplib3-r-proof-free.txt > $O/m3off.txt
for a in lit:JAOS_SC=1 proof:JAOS_SC=153 prune:JAOS_SC=1177 \
         proof-20:JAOS_SC=153,JAOS_SCPROOFS=20,JAOS_SCPASS=1; do
    env $(echo ${a#*:} | tr ',' ' ') $O/proto/jaos solve $I/neos5.mps \
        --work-limit 300000000000 --log summary > $O/neos5-3e11-${a%%:*}.log 2>&1 &
done
wait
[ -z "$SCIP_PYTHON" ] || OUT=$O INST=$I $H/scip.sh
echo done > $O/done
