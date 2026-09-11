#!/usr/bin/env bash
# The reading behind 02-229. Builds `postsolve.c` against the library in the
# tree and runs it over six seeds. Every model is an LP that holds the zero
# point, and the generator plants what presolve removes: an empty row, an
# empty column, a fixed column, a singleton row, a copy of another row, and
# a free column.
#
# `postsolve.c` takes: runs, seed, and a dump flag (any non-empty string
# prints the failing model). It exits non-zero when any of the eight
# properties broke.
#
#   postsolve.sh            the reading, six seeds of 4000
#   postsolve.sh control    the two presolve faults, 2000 models, seed 1
#   postsolve.sh patch      the two source patches, 2000 models, seed 1
#
# `control` runs the two presolve faults the Makefile already carries.
# `patch` edits `src/model.c` in place, builds, runs, and puts the file
# back with `git checkout`: it drops the objective offset in the load, adds
# one to the offset the copy passes on, and publishes the point one index
# out of step.
#
# Both rebuild the whole library, so they take a few minutes and they leave
# the tree built under the last one. Run `make clean && make all` after.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make all` first.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-229
OUT=${2:-/tmp/postsolve}
mkdir -p "$OUT" build

compile() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/postsolve" \
        "$HERE/postsolve.c" build/release/libjaos.a -lm
}

mode=${1:-}

if [ -z "$mode" ]; then
    compile || exit 1
    for seed in 1 2 3 4 5 6; do
        echo "seed $seed:"
        "$OUT/postsolve" 4000 "$seed" | tail -8
    done
    exit 0
fi

if [ "$mode" = patch ]; then
    # Two edits to `src/model.c`, one at a time. The first drops the
    # objective offset where `jaos_load_lp` stores it, which every model
    # and every copy then carries wrong. The second adds one to the offset
    # `jaos_model_copy` hands the load, which only the copy carries wrong.
    # Print what each edit changed: a patch that matches nothing looks
    # exactly like a clean pass.
    for edit in none load copy shift; do
        git checkout -- src/model.c || exit 1
        case $edit in
        load) sed -i 's/^    m->obj_offset = obj_offset;$/    m->obj_offset = 0.0;/' src/model.c ;;
        copy) sed -i 's/^                      src->obj_offset, src->col_cost, src->col_lower,$/                      src->obj_offset + 1.0, src->col_cost, src->col_lower,/' src/model.c ;;
        shift) sed -i 's|^        memcpy(col_value, m->sol_col,.*$|        for (int64_t j = 0; j < m->num_col; j++) col_value[j] = m->sol_col[(j + 1) % m->num_col];|' src/model.c ;;
        esac
        echo "== $edit"
        git --no-pager diff --stat src/model.c | tail -1
        make clean >/dev/null 2>&1
        make all >/dev/null 2>&1 || { echo "$edit: build failed"; continue; }
        rm -f "$OUT/postsolve"
        compile || { echo "$edit: harness build failed"; continue; }
        "$OUT/postsolve" 2000 1 | tail -3
    done
    git checkout -- src/model.c
    exit 0
fi

# The controls. `make clean` first every time: a rebuilt library whose
# timestamp lands inside the same second as the old one does not relink,
# and the run then measures the code it was meant to replace.
for fault in none JAOS_PRESOLVE_FAULT_OFFBYONE JAOS_PRESOLVE_FAULT_WRONGDUAL; do
    make clean >/dev/null 2>&1
    if [ "$fault" = none ]; then
        make all >/dev/null 2>&1 || { echo "$fault: build failed"; continue; }
    else
        make all EXTRA_CFLAGS=-D"$fault" >/dev/null 2>&1 ||
            { echo "$fault: build failed"; continue; }
    fi
    rm -f "$OUT/postsolve"
    compile || { echo "$fault: harness build failed"; continue; }
    echo "== $fault"
    "$OUT/postsolve" 2000 1 | tail -3
done
