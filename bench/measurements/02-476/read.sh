#!/bin/bash
# The readings behind 02-476, one solve at a time. HiGHS and SCIP on
# gen-ip002 and gen-ip054 (SCIP_PYTHON names a Python with pyscipopt and
# numpy; without it SCIP and the lattice prototype are left out); JAOS's
# switches on copies of 4dc8e97 and of 4dc8e97 with proto.patch, whose
# JAOS_RCNODE=1 tightens each node's bounds by its reduced costs and whose
# JAOS_SBSEL=1 and JAOS_PSHARE pick the branch from the node's own probes;
# lattice.py's widths and the models it writes in a reduced basis; the
# change on both models, hook.patch's rounding in the original columns,
# and MIPLIB 3 against bench/miplib.baseline and MIPLIB 2017 at 1e10 work
# units. The records land in OUT (default ~/read476).
#
#   OUT=dir SCIP_PYTHON=python read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-476
O=${OUT:-$HOME/read476}
D=bench/instances-miplib2017
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
build base 4dc8e97
build proto 4dc8e97 proto.patch
build lat HEAD
build lat1 HEAD "" 's/^constexpr int64_t MIP_LATTICE_NODES = 20000;/constexpr int64_t MIP_LATTICE_NODES = 1;/'
build lat12000 HEAD "" 's/^constexpr int64_t MIP_LATTICE_NODES = 20000;/constexpr int64_t MIP_LATTICE_NODES = 12000;/'
build lat1000 2cb06169
build hook 4dc8e97 hook.patch
HI=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
for g in gen-ip002 gen-ip054; do
    [ -z "$HI" ] || $HI --options_file bench/compare/highs-mip.opt --time_limit 300 $D/$g.mps > $O/highs-$g.txt 2>&1
    [ -z "$SCIP_PYTHON" ] || $SCIP_PYTHON $H/scip_stats.py $D/$g.mps 300 > $O/scip-$g.txt 2>&1
    $O/base/jaos solve $D/$g.mps --node-limit 1 --log progress > $O/jaos-root-$g.txt 2>&1
done
if [ -n "$SCIP_PYTHON" ]; then
    for g in gen-ip054 gen-ip002; do
        for a in default no-redcost no-redcost-both no-pseudoobj no-linprop no-node-sepa no-conflict no-sb no-restart bare; do
            echo "$g $($SCIP_PYTHON $H/scip_abl.py $D/$g.mps $a 200000 600)"
        done
    done > $O/scip-abl.txt 2>&1
fi
row() {
    printf '%-10s %-16s %-10s nodes %7s work %12s bound %-20s inc %s\n' $1 $2 \
        "$(echo "$3" | sed -n 's/^status //p')" "$(echo "$3" | sed -n 's/^nodes //p')" \
        "$(echo "$3" | sed -n 's/^work_units //p')" "$(echo "$3" | sed -n 's/^bound //p')" \
        "$(echo "$3" | sed -n 's/^incumbent //p')"
}
arms() {
    L=$1; shift
    for g in gen-ip054 gen-ip002; do
        row $g default "$($O/base/jaos solve $D/$g.mps --work-limit $L 2>&1)"
        row $g rel0 "$($O/base/jaos solve $D/$g.mps --work-limit $L --reliability 0 2>&1)"
        row $g rel8 "$($O/base/jaos solve $D/$g.mps --work-limit $L --reliability 8 2>&1)"
        row $g rel16cap0 "$($O/base/jaos solve $D/$g.mps --work-limit $L --reliability 16 --probe-cap 0 2>&1)"
        row $g bound "$($O/base/jaos solve $D/$g.mps --work-limit $L --node-select bound 2>&1)"
        row $g nonodemir "$($O/base/jaos solve $D/$g.mps --work-limit $L --no-node-mir 2>&1)"
        row $g depth0 "$($O/base/jaos solve $D/$g.mps --work-limit $L --cut-depth 0 2>&1)"
        row $g gom-every "$($O/base/jaos solve $D/$g.mps --work-limit $L --cut-depth 100000 2>&1)"
        row $g prop4 "$($O/base/jaos solve $D/$g.mps --work-limit $L --propagate 4 2>&1)"
        row $g noheur "$($O/base/jaos solve $D/$g.mps --work-limit $L --no-heuristics 2>&1)"
        row $g mostfrac "$($O/base/jaos solve $D/$g.mps --work-limit $L --branching most-fractional 2>&1)"
        for k in 1 2 8 16 0; do
            row $g cap$k "$($O/base/jaos solve $D/$g.mps --work-limit $L --node-cut-cap $k 2>&1)"
        done
        row $g rcnode "$(JAOS_RCNODE=1 $O/proto/jaos solve $D/$g.mps --work-limit $L 2>&1)"
    done
}
arms 2000000000 > $O/jaos-arms-2e9.txt
arms 10000000000 > $O/jaos-arms-1e10.txt
sb() {
    N=$1; X=$2
    for g in gen-ip054 gen-ip002; do
        row $g pc "$(JAOS_PSHARE=0.1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X 2>&1)"
        row $g pc-sel "$(JAOS_PSHARE=0.1 JAOS_SBSEL=1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X 2>&1)"
        row $g full "$(JAOS_PSHARE=1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X --reliability 1000000 2>&1)"
        row $g full-sel "$(JAOS_PSHARE=1 JAOS_SBSEL=1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X --reliability 1000000 2>&1)"
        row $g full-sel-cap0 "$(JAOS_PSHARE=1 JAOS_SBSEL=1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X --reliability 1000000 --probe-cap 0 2>&1)"
        row $g gom-every "$(JAOS_PSHARE=0.1 $O/proto/jaos solve $D/$g.mps --node-limit $N $X --cut-depth 100000 2>&1)"
    done
}
sb 30000 "" > $O/jaos-sb-30000.txt
sb 20000 "--node-select bound --no-node-mir" > $O/jaos-sb-20000-bound.txt
$O/base/jaos solve $D/gen-ip054.mps --work-limit 100000000000 --log progress 2>&1 \
    | awk '!/^node [0-9]*: [0-9]* open/ || $2 ~ /00000:$/' > $O/jaos-1e11-gen-ip054.txt
if [ -n "$SCIP_PYTHON" ]; then
    for z in "gen-ip054 6840.9656417899996 cut" "gen-ip054 7065.941373525 z1" \
             "gen-ip054 6866.2306826720005 z2" "gen-ip002 -4783.7333920000001 cut" \
             "gen-ip002 -4217.1585803000007 z1" "gen-ip002 -4766.4447485 z2"; do
        set -- $z
        echo "== $1 cut at $2"
        $SCIP_PYTHON $H/lattice.py $D/$1.mps $2 $O/ref-$1-$3.mps cut
        row ref-$1-$3 "" "$($O/base/jaos solve $O/ref-$1-$3.mps --work-limit 10000000000 2>&1)"
    done > $O/lattice.txt 2>&1
fi
for b in lat1:1 lat1000:1 lat12000:1 lat:1 hook:1 hook:1000; do
    for g in gen-ip054 gen-ip002; do
        out=$(JAOS_LATNODES=${b#*:} $O/${b%:*}/jaos solve $D/$g.mps --work-limit 10000000000 --log progress 2>&1)
        row $g $b "$out"
        echo "$out" | grep -E '^lattice:'
    done
done > $O/jaos-lattice.txt
for a in "--reliability 8" "--reliability 0" "--node-select bound" "--cut-depth 0" "--no-node-mir" "--no-heuristics"; do
    row gen-ip054 "$a" "$($O/lat/jaos solve $D/gen-ip054.mps --work-limit 10000000000 $a 2>&1)"
done > $O/jaos-lattice-switches.txt
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude $H/sweep.c $O/lat1/libjaos.a -o $O/sweep -lm -pthread
for k in 0 1 2; do
    for sc in 1 10; do
        $O/sweep 10 7 $k 20 $sc
    done
done > $O/lattice-sweep.txt
$O/lat/run -j 1 -m bench/miplib.manifest -e mip -d bench/instances-miplib \
    -b bench/miplib.baseline -L 1e10 -o $O/miplib3.txt > $O/miplib3.log 2>&1
$O/lat/run -j 1 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
    -L 1e10 -o $O/miplib2017.txt > $O/miplib2017.log 2>&1
python3 bench/measurements/02-328/m17sum.py before=bench/results/miplib2017.txt \
    lattice=$O/miplib2017.txt > $O/sum.txt
echo done > $O/done
