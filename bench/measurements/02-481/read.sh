#!/bin/bash
# The readings behind 02-481, from copies of 1235543 built by build.sh:
# head (the record), pre (node presolve on every node,
# MIP_NODE_PRESOLVE_ITERS out of reach), trace (trace.patch: one N and one
# F line per node relaxation under N5TRACE, an X line every 10,000 nodes
# with the open nodes and the explored share; N5PRE keeps node presolve on)
# and prof (profile.patch: the node relaxations' work units by source line
# under N5PROF). HiGHS 1.15.1 and SCIP (SCIP_PYTHON names a Python with
# pyscipopt) on neos5, one thread, gap 1e-6. Two solves at a time.
#
#   OUT=dir INST=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-481
O=${OUT:-$HOME/read481}
I=${INST:-$R/bench/instances-miplib2017}
N5=$I/neos5.mps
HI=$(ls -d $R/bench/compare/solvers/highs-* 2>/dev/null | head -1)
export OUT=$O REV=1235543
rm -rf $O; mkdir -p $O
bash $H/build.sh head || exit 1
bash $H/build.sh trace $H/trace.patch || exit 1
bash $H/build.sh prof $H/profile.patch || exit 1
# pre keeps node presolve on every node, skip drops it after the trial
# whatever the trial reads
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
for v in "pre 1e9" "skip 0.0"; do
    set -- $v
    bash $H/build.sh $1 || exit 1
    sed -i "s/^constexpr double MIP_NODE_PRESOLVE_ITERS = 20.0;/constexpr double MIP_NODE_PRESOLVE_ITERS = $2;/" $O/$1/src/mip.c
    (cd $O/$1 && gcc-14 $F -c src/mip.c -o mip.o && ar rcs libjaos.a *.o &&
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm) || exit 1
done
J=$O/jaos; mkdir -p $J
solve() {
    name=$1 bin=$2 w=$3; shift 3
    $O/$bin/jaos solve $N5 --work-limit $w --log summary "$@" > $J/$name.out 2> $J/$name.log
}
solvep() {
    name=$1 bin=$2 w=$3; shift 3
    $O/$bin/jaos solve $N5 --work-limit $w --log progress "$@" 2>&1 > $J/$name.out |
        grep -E "^node [0-9]*0000: .* open|^branch|^root|^node [0-9]*: the first" > $J/$name.log
}
line() {
    printf '%-22s %s\n' "$1" "$(grep -E '^(status|nodes|work_units|bound|incumbent) ' $J/$1.out | tr '\n' ' ')"
}
# the record and node presolve on every node, at 1e10 and 1e11
solve head-1e10 head 10000000000 & solve pre-1e10 pre 10000000000 & wait
solve head-1e11 head 100000000000 & solve pre-1e11 pre 100000000000 & wait
# to 5e11, the open nodes every 10,000: the record, node presolve on every
# node, no cuts below the root (its trial keeps node presolve), the same
# with node presolve dropped after the trial, and with RINS off (on pre)
solvep head-5e11 head 500000000000 & solvep pre-5e11 pre 500000000000 & wait
solvep depth0-5e11 head 500000000000 --cut-depth 0 &
solvep skip-depth0-5e11 skip 500000000000 --cut-depth 0 & wait
solvep pre-depth0-rins0-5e11 pre 500000000000 --cut-depth 0 --rins 0
# the tree's switches at 1e10
for a in "sel-bound --node-select bound" "cut5 --cut-rounds 5" "noheur --no-heuristics" \
         "rins0 --rins 0" "depth0 --cut-depth 0" "depth10 --cut-depth 10" "rel0 --reliability 0" \
         "rel8 --reliability 8" "restart --restart" "noorb --no-orbital" \
         "mostfrac --branching most-fractional"; do
    set -- $a; n=$1; shift
    solve arm-$n head 10000000000 "$@" &
    [ $(jobs -r | wc -l) -ge 2 ] && wait -n
done
wait
{
    for n in head-1e10 pre-1e10 head-1e11 pre-1e11 head-5e11 pre-5e11 depth0-5e11 \
             skip-depth0-5e11 pre-depth0-rins0-5e11; do line $n; done
    echo
    for f in $J/arm-*.out; do line $(basename $f .out); done
    echo
    grep -H -E '^branch and bound work' $J/head-1e10.log $J/depth0-5e11.log | sed "s|$J/||"
} > $O/jaos.txt
# the root under more cuts (node limit 1)
for a in "default" "g5 --cut-rounds 5" "g10 --cut-rounds 10" "g20 --cut-rounds 20" \
         "g50 --cut-rounds 50" "zh5 --zero-half-rounds 5" "mir5 --mir-rounds 5" \
         "cov5 --cover-rounds 5" \
         "g20zh5mir5 --cut-rounds 20 --zero-half-rounds 5 --mir-rounds 5 --cover-rounds 5"; do
    set -- $a; n=$1; shift
    $O/head/jaos solve $N5 --node-limit 1 --log summary "$@" > $J/root-$n.out 2> $J/root-$n.log
    printf '%-10s %s | %s\n' $n "$(grep -h '^root: relaxation' $J/root-$n.log | cut -d, -f1)" \
        "$(grep -E '^work_units' $J/root-$n.out)"
done > $O/root.txt
# the node relaxations at 2e9, without and with node presolve
N5TRACE=1 $O/trace/jaos solve $N5 --work-limit 2000000000 --log summary > $O/tr-def.out 2> $O/tr-def.err &
N5TRACE=1 N5PRE=1 $O/trace/jaos solve $N5 --work-limit 2000000000 --log summary > $O/tr-pre.out 2> $O/tr-pre.err &
wait
{
    echo "== the record at 2e9"; python3 $H/split.py $O/tr-def.err
    echo "== node presolve on every node at 2e9"; python3 $H/split.py $O/tr-pre.err
} > $O/trace-neos5.txt
N5PROF=1 $O/prof/jaos solve $N5 --work-limit 2000000000 --log summary > /dev/null 2> $O/pf-def.err
python3 $H/profile_sum.py $O/pf-def.err $O/prof > $O/profile-neos5.txt
# the explored share every 10,000 nodes at 1e10
mkdir -p $O/expl
for a in "def" "depth0 --cut-depth 0" "depth60 --cut-depth 60" "depth60cap0 --cut-depth 60 --node-cut-cap 0" \
         "selbound --node-select bound" "g10 --cut-rounds 10" "noorb --no-orbital" \
         "mostfrac --branching most-fractional"; do
    set -- $a; n=$1; shift
    (N5TRACE=1 $O/trace/jaos solve $N5 --work-limit 10000000000 --log summary "$@" 2>&1 > $O/expl/$n.out |
        grep '^X ' > $O/expl/$n.x) &
    [ $(jobs -r | wc -l) -ge 2 ] && wait -n
done
wait
python3 $H/explored.py 10000 $O/expl/*.x > $O/explored.txt
# HiGHS: four seeds (0 to 3) and its tree's parts taken away
if [ -n "$HI" ]; then
    mkdir -p $O/highs
    hrun() {
        name=$1; shift
        printf 'threads = 1\nmip_rel_gap = 1e-6\n' > $O/highs/$name.opt
        for kv in "$@"; do echo "$kv" >> $O/highs/$name.opt; done
        $HI --options_file $O/highs/$name.opt --time_limit 1800 $N5 2>&1 | grep -v HighsProfiling > $O/highs/$name.txt
    }
    NC="mip_allow_cut_separation_at_nodes = false"; NR="mip_allow_restart = false"
    NS="mip_detect_symmetry = false"; NH="mip_heuristic_effort = 0"
    P1="mip_pool_soft_limit = 1"; P2="mip_pool_age_limit = 0"; R0="mip_pscost_minreliable = 0"
    hrun default & hrun seed1 "random_seed = 1" & wait
    hrun seed2 "random_seed = 2" & hrun seed3 "random_seed = 3" & wait
    hrun norestart "$NR" & hrun nosym "$NS" & wait
    hrun nodecuts "$NC" & hrun noheur "$NH" & wait
    hrun rel0 "$R0" & hrun pool0 "$P1" "$P2" & wait
    hrun alloff "$NC" "$NR" "$NS" "$NH" & hrun bare "$NC" "$NR" "$NS" "$NH" "$P1" "$P2" & wait
    hrun bare-rel0 "$NC" "$NR" "$NS" "$NH" "$P1" "$P2" "$R0"
    hrun2() { name=$1; shift; printf 'threads = 1\nmip_rel_gap = 1e-6\n' > $O/highs/$name.opt
        for kv in "$@"; do echo "$kv" >> $O/highs/$name.opt; done
        $HI --options_file $O/highs/$name.opt --time_limit 60 $N5 2>&1 | grep -v HighsProfiling > $O/highs/$name.txt; }
    hrun2 probe-soft1 "$NC" "$NR" "$NS" "$NH" "$P1" & hrun2 probe-age0 "$NC" "$NR" "$NS" "$NH" "$P2" & wait
    bash $H/highs_sum.sh $O/highs/*.txt > $O/highs.txt
fi
if [ -n "$SCIP_PYTHON" ]; then
    $SCIP_PYTHON $H/scip_root.py $N5 1 60 > $O/scip-root.txt 2>&1
    $SCIP_PYTHON $H/scip_params.py conflict/use > $O/scip-params.txt 2>&1
fi
echo done > $O/done
