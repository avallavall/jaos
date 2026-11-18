#!/bin/bash
# The readings behind 02-479, one solve at a time, from copies of b53a1ff.
# trace.patch prints one line per node relaxation (to stderr, under
# N5TRACE); modes.patch adds N5DSE, the node solves' pricing (1: no
# restart of the steepest-edge set on a drift, 2: exact weights at the
# start, 4: dual Devex), at the root too; census.patch solves every node
# relaxation below the root under 1, 2 and 4 from the same start before
# the tree's own solve, outside the work count. change.patch is the change
# (landing.patch is it with its tests and docs). HiGHS and SCIP (SCIP_PYTHON
# names a Python with pyscipopt) on neos5 to 30,000 nodes; MIPLIB 3 against
# bench/miplib.baseline and MIPLIB 2017 at 1e10 work units; the LP sets.
# The records land in OUT (default ~/read479).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-479
O=${OUT:-$HOME/read479}
I=bench/instances-miplib2017
N5=$I/neos5.mps
rm -rf $O; mkdir -p $O
cd $R
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
build() {
    d=$1; shift
    mkdir -p $O/$d
    git archive b53a1ff src include cli bench/run.c | tar x -C $O/$d
    for p in "$@"; do (cd $O/$d && patch -s -p1 < $H/$p) || exit 1; done
    (cd $O/$d && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
     ar rcs libjaos.a *.o
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm
     gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
}
build trace trace.patch
build modes trace.patch modes.patch
build census trace.patch modes.patch census.patch
build land change.patch
build landtrace change.patch trace.patch
build landpre change.patch
sed -i 's/^constexpr double MIP_NODE_PRESOLVE_ITERS = 20.0;/constexpr double MIP_NODE_PRESOLVE_ITERS = 1e9;/' $O/landpre/src/mip.c
(cd $O/landpre && gcc-14 $F -c src/mip.c -o mip.o && ar rcs libjaos.a *.o &&
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm &&
 gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm) || exit 1
line() {
    printf '%-34s %s\n' "$1" "$(grep -E '^(status|nodes|iterations|work_units|bound|incumbent) ' $2 | tr '\n' ' ')"
}
N5TRACE=1 $O/trace/jaos solve $N5 --work-limit 2000000000 --log summary > $O/t.out 2> $O/t.err
N5TRACE=1 $O/landtrace/jaos solve $N5 --work-limit 2000000000 --log summary > $O/lt.out 2> $O/lt.err
{
    echo "== b53a1ff, neos5 at 2e9"; line b53a1ff $O/t.out; python3 $H/trace_split.py $O/t.err
    echo "== the change, neos5 at 2e9"; line change $O/lt.out; python3 $H/trace_split.py $O/lt.err
} > $O/trace-neos5.txt
{
    for w in 2000000000 10000000000; do
        for mode in 0 1 2 4; do
            N5TRACE=1 N5DSE=$mode $O/modes/jaos solve $N5 --work-limit $w --log summary > $O/m.out 2> $O/m.err
            line "N5DSE=$mode at $w" $O/m.out
            python3 $H/trace_split.py $O/m.err | head -1
        done
        $O/land/jaos solve $N5 --work-limit $w --log summary > $O/m.out 2>&1
        line "the change at $w" $O/m.out
        $O/landpre/jaos solve $N5 --work-limit $w --log summary > $O/m.out 2>&1
        line "the change, node presolve kept" $O/m.out
    done
} > $O/modes-neos5.txt
mkdir -p $O/census
for set in miplib2017 miplib; do
    man=bench/miplib.manifest; [ $set = miplib2017 ] && man=bench/miplib2017.manifest
    for f in $(grep -v '^#' $man | awk 'NF>0{print $1}'); do
        p=bench/instances-$set/$f.mps; [ -f $p ] || p=$p.gz
        N5CENSUS=1 $O/census/jaos solve $p --work-limit 500000000 --log summary > /dev/null 2> $O/census/$f.census
    done
done
python3 $H/census_sum.py $O/census/*.census > $O/census.txt
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
if [ -n "$HI" ]; then
    for w in default 0 1 2; do
        printf 'threads = 1\nmip_rel_gap = 1e-6\nmip_max_nodes = 30000\n' > $O/h.opt
        [ $w = default ] || printf 'simplex_dual_edge_weight_strategy = %s\n' $w >> $O/h.opt
        echo "== simplex_dual_edge_weight_strategy $w"
        $HI --options_file $O/h.opt --time_limit 300 $N5 2>&1 | grep -E "^  (Status|Primal bound|Dual bound|Nodes|LP iterations) |^ +[0-9]+ \((strong|separation|heuristics)"
    done > $O/highs-neos5.txt
fi
if [ -n "$SCIP_PYTHON" ]; then
    for a in default steepest quickstart partial dfs bfs estimate-noplunge; do
        $SCIP_PYTHON $H/scip_lp.py $N5 $a 30000 300 $O/scip-$a.stats
        grep -E "^  dual LP " $O/scip-$a.stats
    done > $O/scip-neos5.txt 2>&1
fi
mkdir -p $O/miplib3 $O/miplib2017
for a in land landpre devex; do
    b=$a; e=""; [ $a = devex ] && b=modes && e="N5DSE=4"
    env $e $O/$b/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
        -b bench/miplib.baseline -L 1e10 -o $O/miplib3/$a.txt > $O/miplib3/$a.log 2>&1
done
$O/land/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017/land.txt > $O/miplib2017/land.log 2>&1
python3 bench/measurements/02-474/m3cmp.py bench/miplib.baseline devex=$O/miplib3/devex.txt \
    land=$O/miplib3/land.txt landpre=$O/miplib3/landpre.txt > $O/m3cmp.txt
python3 bench/measurements/02-474/m17cmp.py before=bench/results/miplib2017.txt \
    land=$O/miplib2017/land.txt > $O/m17cmp.txt
python3 bench/measurements/02-328/m17sum.py mine=$O/miplib2017/land.txt \
    before=bench/results/miplib2017.txt > $O/sum.txt
N=bench/measurements/02-474/miplib2017
python3 $H/nullcmp.py bench/results/miplib2017.txt $O/miplib2017/land.txt $N/null150.txt \
    $N/null300.txt $N/null.txt $N/null600.txt $N/null900.txt > $O/nulls.txt
$O/land/run -j 2 -d bench/instances -b bench/netlib.baseline -o $O/netlib.txt > $O/netlib.log 2>&1
$O/land/run -j 2 -m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas \
    -b bench/netlib-infeas.baseline -o $O/netlib-infeas.txt > $O/netlib-infeas.log 2>&1
$O/land/run -j 2 -m bench/netlib-kennington.manifest -d bench/instances-kennington \
    -b bench/netlib-kennington.baseline -o $O/netlib-kennington.txt > $O/netlib-kennington.log 2>&1
git archive b53a1ff bench/primal.c bench/barrier.c bench/warm.c | tar x -C $O/land
for t in primal barrier warm; do
    gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$O/land/include -I$O/land/src $O/land/bench/$t.c \
        $O/land/libjaos.a -o $O/land/$t -lm || exit 1
done
$O/land/primal -j 2 -o $O/primal.txt > /dev/null 2>&1
$O/land/barrier -j 2 -o $O/barrier.txt > /dev/null 2>&1
$O/land/barrier -a pdlp -j 2 -o $O/pdlp.txt > /dev/null 2>&1
$O/land/barrier -a concurrent -j 2 -o $O/concurrent.txt > /dev/null 2>&1
$O/land/warm -j 2 -o $O/warm.txt > /dev/null 2>&1
{
for f in netlib netlib-infeas netlib-kennington; do
    echo "== $f"; grep -E "instances:|gate:|regressed" $O/$f.log
done
for f in primal barrier pdlp concurrent warm; do
    if diff <(grep -v '^#' bench/results/$f.txt | sort) <(grep -v '^#' $O/$f.txt | sort) > /dev/null; then
        echo "== $f: every line reads as bench/results/$f.txt"
    else
        echo "== $f: differs from bench/results/$f.txt"
    fi
done
} > $O/lp-gates.txt
echo done > $O/done
