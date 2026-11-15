#!/bin/bash
# The reading behind 02-459: MIPLIB 2017's 30 instances at 1e10 work
# units, one solve at a time, with HEAD's library under probe-bound.patch.
# The patch publishes, at a strong-branching probe stopped by its cap in
# the dual simplex, the Lagrangian bound of its last basis
# (J7_PROBE_BOUND=1), learns the pseudocost from it, and can hold the
# probes' work to a share of the tree's (J7_PROBE_SHARE). Each
# configuration's record lands in OUT (default ~/read459) as CONFIG.txt,
# and 02-328's m17sum.py sums them. base is the library without the
# patch's switches, which is HEAD.
#
#   CONFIGS="base rel4cap1b c1s10 c1d5" MANIFEST=file OUT=dir read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-459
O=${OUT:-$HOME/read459}
M=${MANIFEST:-$R/bench/miplib2017.manifest}
rm -rf $O; mkdir -p $O/tree
git -C $R archive HEAD src include bench/run.c | tar x -C $O/tree
(cd $O/tree && patch -s -p1 < $H/probe-bound.patch) || exit 1
(cd $O/tree && for f in src/*.c; do
    gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD \
        -fvisibility=hidden -Iinclude -c $f -o $(basename $f .c).o &
done; wait; ar rcs libjaos.a *.o)
gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$O/tree/include -I$O/tree/src \
    -o $O/run $O/tree/bench/run.c $O/tree/libjaos.a -lm || exit 1
cd $R
for c in ${CONFIGS:-base rel4cap1b c1s10 c1d5}; do
    args=""; envs=""
    case $c in
        base) ;;
        rel4) args="-O mip_reliability=4" ;;
        rel4cap1) args="-O mip_reliability=4 -O mip_probe_cap=1" ;;
        rel4cap1b) args="-O mip_reliability=4 -O mip_probe_cap=1"; envs="J7_PROBE_BOUND=1" ;;
        c1s10) args="-O mip_reliability=4 -O mip_probe_cap=1"; envs="J7_PROBE_BOUND=1 J7_PROBE_SHARE=0.1" ;;
        c1s20) args="-O mip_reliability=4 -O mip_probe_cap=1"; envs="J7_PROBE_BOUND=1 J7_PROBE_SHARE=0.2" ;;
        c05s20) args="-O mip_reliability=4 -O mip_probe_cap=0.5"; envs="J7_PROBE_BOUND=1 J7_PROBE_SHARE=0.2" ;;
        r8c05s20) args="-O mip_reliability=8 -O mip_probe_cap=0.5"; envs="J7_PROBE_BOUND=1 J7_PROBE_SHARE=0.2" ;;
        c1d5) args="-O mip_reliability=4 -O mip_probe_cap=1 -O mip_probe_depth=5"; envs="J7_PROBE_BOUND=1" ;;
        *) echo "unknown configuration $c"; exit 1 ;;
    esac
    env $envs $O/run -j 1 -m $M -e mip -d bench/instances-miplib2017 -L 1e10 $args \
        -o $O/$c.txt > $O/$c.log 2>&1
done
for c in ${CONFIGS:-base rel4cap1b c1s10 c1d5}; do
    python3 bench/measurements/02-328/m17sum.py $c=$O/$c.txt
done > $O/sum.txt
echo done > $O/done
