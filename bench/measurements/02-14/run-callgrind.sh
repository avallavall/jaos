#!/bin/bash
# S1d (D108): split the post-D106 overcost of greenbeb/scfxm3/forplan by
# function. Two trees, one diagnostic build each (-O2, no LTO, same flags),
# calibrated against the committed records before anything is profiled.
#
# Usage (inside WSL, from the repository root):
#   git worktree add /tmp/jaos-pre b40fe74
#   bash bench/measurements/02-14/run-callgrind.sh /tmp/jaos-pre [OUTDIR]
#
# b40fe74 is the commit whose tree produced the pre-D106 record; the
# calibration below fails on any other tree. OUTDIR defaults to
# bench/measurements/02-14/profiles.
set -u
[ $# -ge 1 ] || { echo "usage: run-callgrind.sh PRE_TREE [OUTDIR]"; exit 2; }
PRE=$1
here=$(cd "$(dirname "$0")" && pwd)
MAIN=$(cd "$here/../../.." && pwd)
SCR=${2:-$here/profiles}
mkdir -p "$SCR"

build() {
    cd "$1" || exit 9
    mkdir -p build/diag
    gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
        src/*.c bench/run.c -o build/diag/run -lm \
        || { echo "BUILD FAILED in $1"; exit 2; }
    echo "built $1/build/diag/run"
}

calib() { # tree tag inst want_iters want_work
    cd "$1" || exit 9
    out=$(./build/diag/run -j 1 -d "$MAIN/bench/instances" "$3" 2>/dev/null)
    it=$(echo "$out" | grep -o 'iters=[0-9]*' | head -1 | cut -d= -f2)
    wk=$(echo "$out" | grep -o 'work=[0-9]*' | head -1 | cut -d= -f2)
    if [ "$it" != "$4" ] || [ "$wk" != "$5" ]; then
        echo "CALIBRATION FAILED $2/$3: got iters=$it work=$wk, want iters=$4 work=$5"
        exit 1
    fi
    echo "calibration ok: $2 $3 iters=$it work=$wk"
}

build "$MAIN"
build "$PRE"

calib "$MAIN" post greenbeb 11194 573519868
calib "$MAIN" post scfxm3   1427  11414560
calib "$MAIN" post forplan  194   1246118
calib "$PRE"  pre  greenbeb 8124  379164967
calib "$PRE"  pre  scfxm3   1354  8419409
calib "$PRE"  pre  forplan  182   1069794

for side in post pre; do
    if [ "$side" = post ]; then tree=$MAIN; else tree=$PRE; fi
    cd "$tree" || exit 9
    for inst in greenbeb scfxm3 forplan; do
        valgrind --tool=callgrind \
            --callgrind-out-file="$SCR/cg-$side-$inst.out" \
            ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$inst" \
            > /dev/null 2>&1 \
            || { echo "CALLGRIND FAILED $side/$inst"; exit 2; }
        callgrind_annotate --threshold=99 "$SCR/cg-$side-$inst.out" \
            > "$SCR/ann-$side-$inst.txt"
        echo "profiled $side/$inst"
    done
done
echo "S1D_DONE"
