#!/bin/bash
# S1b blocker (recorded in 02-15/README.md): attribute d2q06c's 2.2163x at
# margin zero. One tree, two diagnostic builds — the default margin 8, and
# JAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0 — each calibrated against its
# 02-12 sweep record before profiling.
#
# Usage (inside WSL, from the repository root):
#   bash bench/measurements/02-15/run-callgrind.sh [OUTDIR]
# OUTDIR defaults to bench/measurements/02-15/profiles.
set -u
here=$(cd "$(dirname "$0")" && pwd)
MAIN=$(cd "$here/../../.." && pwd)
SCR=${1:-$here/profiles}
mkdir -p "$SCR"
cd "$MAIN" || exit 9
mkdir -p build/diag

gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run8 -lm \
    || { echo "BUILD FAILED (margin 8)"; exit 2; }
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off \
    -DJAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0 -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run0 -lm \
    || { echo "BUILD FAILED (margin 0)"; exit 2; }
echo "built run8 and run0"

calib() { # binary tag want_iters want_work
    out=$(./build/diag/"$1" -j 1 -d bench/instances d2q06c 2>/dev/null)
    it=$(echo "$out" | grep -o 'iters=[0-9]*' | head -1 | cut -d= -f2)
    wk=$(echo "$out" | grep -o 'work=[0-9]*' | head -1 | cut -d= -f2)
    if [ "$it" != "$3" ] || [ "$wk" != "$4" ]; then
        echo "CALIBRATION FAILED $2: got iters=$it work=$wk, want iters=$3 work=$4"
        exit 1
    fi
    echo "calibration ok: $2 d2q06c iters=$it work=$wk"
}

calib run8 margin8 6740  428597453
calib run0 margin0 11812 949907413

for side in 8 0; do
    valgrind --tool=callgrind \
        --callgrind-out-file="$SCR/cg-m$side-d2q06c.out" \
        ./build/diag/run$side -j 1 -d bench/instances d2q06c \
        > /dev/null 2>&1 \
        || { echo "CALLGRIND FAILED margin $side"; exit 2; }
    callgrind_annotate --threshold=99 "$SCR/cg-m$side-d2q06c.out" \
        > "$SCR/ann-m$side-d2q06c.txt"
    echo "profiled margin $side"
done
echo "S1B_DONE"
