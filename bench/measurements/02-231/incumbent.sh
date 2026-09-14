#!/usr/bin/env bash
# The reading behind 02-231. Builds `incumbent.c` against the library in
# the tree and runs it over six seeds; every model plants an integer point
# inside its boxes, so every model has something for the callback to
# announce. `incumbent.c` exits non-zero when any of its nine properties
# broke.
#
#   incumbent.sh            the reading, six seeds of 2000
#   incumbent.sh control    two one-line edits to src/mip.c, one at a
#                           time, 2000 models, seed 1
#
# `control` edits `src/mip.c` in place, builds, runs, and puts the file
# back from a copy it took first: the announced bound is pushed one past
# the objective, and the announced objective is pushed by 1e-3. It rebuilds
# the whole library twice and leaves the tree built under the current
# source. Needs `make all` first. Run from anywhere; it finds the
# repository from its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-231
OUT=${2:-/tmp/incumbent-02-231}
mkdir -p "$OUT" build

compile() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/incumbent" \
        "$HERE/incumbent.c" build/release/libjaos.a -lm
}

rebuild() {
    make clean >/dev/null 2>&1
    make all >/dev/null 2>&1 || { echo "build failed"; return 1; }
    rm -f "$OUT/incumbent"
    compile
}

mode=${1:-}

if [ -z "$mode" ]; then
    compile || exit 1
    for seed in 1 2 3 4 5 6; do
        echo "== seed $seed"
        "$OUT/incumbent" 2000 "$seed" | tail -8
    done
    exit 0
fi

if [ "$mode" = control ]; then
    cp src/mip.c "$OUT/mip.c.current"
    compile || exit 1
    echo "== as it is"
    "$OUT/incumbent" 2000 1 | tail -2
    for edit in bound objective; do
        cp "$OUT/mip.c.current" src/mip.c
        case $edit in
        bound) sed -i 's/^        \.bound = bound,$/        .bound = bound + (m->sense == JAOS_MAXIMIZE ? -1.0 : 1.0),/' src/mip.c ;;
        objective) sed -i 's/^        \.objective = inc->obj,$/        .objective = inc->obj + 1e-3,/' src/mip.c ;;
        esac
        echo "== $edit: $(diff "$OUT/mip.c.current" src/mip.c | grep -c '^[<>]') lines changed"
        rebuild || { cp "$OUT/mip.c.current" src/mip.c; exit 1; }
        "$OUT/incumbent" 2000 1 | tail -2
    done
    cp "$OUT/mip.c.current" src/mip.c
    rebuild
    exit 0
fi

echo "usage: incumbent.sh [control]"
exit 1
