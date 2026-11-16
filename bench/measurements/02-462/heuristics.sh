#!/bin/bash
# What finds enigma's point: HEAD's heuristics at larger settings, and the
# feasibility jump with MIP_FJ_WORK at 100 and 1000 times its value and
# MIP_FJ_ROOT out of the way, each built from git archive HEAD. One solve
# at a time; the lines land in OUT (default ~/read462h)/heuristics.txt.
#
#   OUT=dir heuristics.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read462h}
rm -rf $O; mkdir -p $O
cd $R
make build/cli/jaos || exit 1
E=bench/instances-miplib/enigma.mps
show() {
    printf '%-46s nodes %6s first incumbent %6s work %12s\n' "$1" \
        "$(echo "$2" | sed -n 's/^nodes //p')" \
        "$(echo "$2" | sed -n 's/^first_incumbent //p')" \
        "$(echo "$2" | sed -n 's/^work_units //p')"
}
{
    for a in "" "--feaspump 200" "--feaspump 200 --pump-obj 0" \
             "--dive-heuristic 500" "--dive-heuristic 50 --dive-heuristic-depth 100" \
             "--dive" "--dive --dive-backtrack 1000" "--local-branching 10"; do
        show "${a:-default}" "$(./build/cli/jaos solve $E $a 2>&1)"
    done
    F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
    for k in 100 1000; do
        d=$O/fj$k; mkdir -p $d
        git archive HEAD src include cli | tar x -C $d
        sed -i "s/^constexpr int64_t MIP_FJ_WORK = 20000;/constexpr int64_t MIP_FJ_WORK = $((20000 * k));/; s/^constexpr double MIP_FJ_ROOT = 0.5;/constexpr double MIP_FJ_ROOT = 1e12;/" $d/src/mip.c
        (cd $d && for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
         ar rcs libjaos.a *.o
         gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm) || exit 1
        show "feasibility jump, ${k}x its work" "$($d/jaos solve $E 2>&1)"
    done
} > $O/heuristics.txt
echo done > $O/done
