#!/usr/bin/env bash
# The reading behind 02-233. Builds `prestats.c` against the library in
# the tree and runs it over six seeds; `prestats.c` exits non-zero when
# any of its six properties broke.
#
#   prestats.sh            the reading, six seeds of 2000
#   prestats.sh control    two one-line edits to src/model.c, one at a
#                          time, 2000 models, seed 1: the reported
#                          singleton column count pushed by one, and the
#                          reported row count pushed by one
#
# `control` edits `src/model.c` in place, builds, runs, and puts the file
# back from a copy it took first. It rebuilds the whole library twice and
# leaves the tree built under the current source. Needs `make all` first.
# Run from anywhere; it finds the repository from its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-233
OUT=${2:-/tmp/prestats-02-233}
mkdir -p "$OUT" build

compile() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/prestats" \
        "$HERE/prestats.c" build/release/libjaos.a -lm
}

rebuild() {
    make clean >/dev/null 2>&1
    make all >/dev/null 2>&1 || { echo "build failed"; return 1; }
    rm -f "$OUT/prestats"
    compile
}

mode=${1:-}

if [ -z "$mode" ]; then
    compile || exit 1
    for seed in 1 2 3 4 5 6; do
        echo "== seed $seed"
        "$OUT/prestats" 2000 "$seed" | tail -8
    done
    exit 0
fi

if [ "$mode" = control ]; then
    cp src/model.c "$OUT/model.c.current"
    compile || exit 1
    echo "== as it is"
    "$OUT/prestats" 2000 1 | tail -2
    for edit in singleton rows; do
        cp "$OUT/model.c.current" src/model.c
        case $edit in
        singleton) sed -i 's/^        \.singleton_col = c->singleton_col,$/        .singleton_col = c->singleton_col + 1,/' src/model.c ;;
        rows) sed -i 's/^        \.num_row = m->presolve_num_row,$/        .num_row = m->presolve_num_row + 1,/' src/model.c ;;
        esac
        echo "== $edit: $(diff "$OUT/model.c.current" src/model.c | grep -c '^[<>]') lines changed"
        rebuild || { cp "$OUT/model.c.current" src/model.c; exit 1; }
        "$OUT/prestats" 2000 1 | tail -2
    done
    cp "$OUT/model.c.current" src/model.c
    rebuild
    exit 0
fi

echo "usage: prestats.sh [control]"
exit 1
