#!/usr/bin/env bash
# The reading behind 02-232. Builds `progress.c` against the library in
# the tree and runs it over six seeds; `progress.c` exits non-zero when
# any of its seven properties broke.
#
#   progress.sh            the reading, six seeds of 2000
#   progress.sh control    two one-line edits, one at a time, 2000 models,
#                          seed 1: the tree's relay dropped, so a node
#                          reports its own work; and the dual's reported
#                          infeasibility replaced by -1
#
# `control` edits `src/mip.c` and then `src/simplex.c` in place, builds,
# runs, and puts each file back from a copy it took first. It rebuilds the
# whole library twice and leaves the tree built under the current source.
# Needs `make all` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-232
OUT=${2:-/tmp/progress-02-232}
mkdir -p "$OUT" build

compile() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/progress" \
        "$HERE/progress.c" build/release/libjaos.a -lm
}

rebuild() {
    make clean >/dev/null 2>&1
    make all >/dev/null 2>&1 || { echo "build failed"; return 1; }
    rm -f "$OUT/progress"
    compile
}

mode=${1:-}

if [ -z "$mode" ]; then
    compile || exit 1
    for seed in 1 2 3 4 5 6; do
        echo "== seed $seed"
        "$OUT/progress" 2000 "$seed" | tail -8
    done
    exit 0
fi

if [ "$mode" = control ]; then
    cp src/mip.c "$OUT/mip.c.current"
    cp src/simplex.c "$OUT/simplex.c.current"
    compile || exit 1
    echo "== as it is"
    "$OUT/progress" 2000 1 | tail -2
    sed -i 's/^        \.work_units = \*r->work + p->work_units,$/        .work_units = p->work_units,/' src/mip.c
    echo "== relay dropped: $(diff "$OUT/mip.c.current" src/mip.c | grep -c '^[<>]') lines changed"
    rebuild || { cp "$OUT/mip.c.current" src/mip.c; exit 1; }
    "$OUT/progress" 2000 1 | tail -2
    cp "$OUT/mip.c.current" src/mip.c
    sed -i 's/^                \.primal_infeasibility = s->infeas_best,$/                .primal_infeasibility = -1.0,/' src/simplex.c
    echo "== infeasibility -1: $(diff "$OUT/simplex.c.current" src/simplex.c | grep -c '^[<>]') lines changed"
    rebuild || { cp "$OUT/simplex.c.current" src/simplex.c; exit 1; }
    "$OUT/progress" 2000 1 | tail -2
    cp "$OUT/simplex.c.current" src/simplex.c
    rebuild
    exit 0
fi

echo "usage: progress.sh [control]"
exit 1
