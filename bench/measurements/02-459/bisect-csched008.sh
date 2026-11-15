#!/bin/bash
# Solves csched008 (or NAME) under 1e10 work units with the library of each
# commit in COMMITS, each built from git archive into OUT/<commit>, and
# prints its nodes, iterations and work units (bisect-csched008.txt).
#
#   COMMITS="32f28633 ... 1183f038" NAME=csched008 OUT=dir bisect-csched008.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
W=${OUT:-$HOME/bisect459}; mkdir -p $W
NAME=${NAME:-csched008}
FILE=$R/bench/instances-miplib2017/$NAME.mps
[ -f $FILE ] || FILE=$FILE.gz
sed 's/        const jaos_status st = jaos_solve(m);/        if (getenv("WORK_LIMIT") != nullptr \&\& jaos_set_work_limit(m, atoll(getenv("WORK_LIMIT"))) != JAOS_OK)\n            return 1;\n        const jaos_status st = jaos_solve(m);/' \
    $R/bench/measurements/02-457/solve.c |
    sed 's/#include <string.h>/#include <stdlib.h>\n#include <string.h>/' > $W/solve.c
for c in $COMMITS; do
    d=$W/$c
    if [ ! -f $d/solve ]; then
        rm -rf $d; mkdir -p $d
        git -C $R archive $c src include | tar x -C $d
        (cd $d && for f in src/*.c; do
            gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD \
                -fvisibility=hidden -Iinclude -c $f -o $(basename $f .c).o &
        done; wait; ar rcs libjaos.a *.o) > $d/build.log 2>&1
        gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -I$d/include -o $d/solve \
            $W/solve.c $d/libjaos.a -lm >> $d/build.log 2>&1 || exit 1
    fi
    printf '%s %s\n   ' $c "$(git -C $R log -1 --format=%s $c | cut -c1-60)"
    WORK_LIMIT=10000000000 $d/solve $FILE
done
