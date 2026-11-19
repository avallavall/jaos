#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2

RUN=${RUN:-build/bench/run}
OUT=${OUT:-bench/measurements/02-495/matrix-$(hostname)}
BATCHES=${BATCHES:-"8 3"}
THREADS=${THREADS:-"1 2 4 12"}
LOCK=${LOCK:-$HOME/mipgate.lock}

[ -x "$RUN" ] || { echo "matrix.sh: build $RUN first (make build/bench/run)"; exit 2; }
mkdir -p "$OUT"
for b in $BATCHES; do
    for t in $THREADS; do
        base=""
        [ "$t" = 1 ] && base="-b bench/miplib.baseline"
        flock "$LOCK" "$RUN" -j 2 -O mip_tree_batch="$b" -O threads="$t" \
            -m bench/miplib.manifest -e mip -d bench/instances-miplib $base \
            -o "$OUT/m3-b$b-t$t.txt" > "$OUT/m3-b$b-t$t.log" 2>&1
        flock "$LOCK" "$RUN" -j 2 -O mip_tree_batch="$b" -O threads="$t" \
            -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
            -L 2e9 -o "$OUT/m17-2e9-b$b-t$t.txt" > "$OUT/m17-2e9-b$b-t$t.log" 2>&1
    done
done
{
    echo "# bench/run -j 2 at -O mip_tree_batch=B -O threads=T; cmp of each record against the one-thread record"
    for b in $BATCHES; do
        for t in $THREADS; do
            [ "$t" = 1 ] && continue
            for s in m3 m17-2e9; do
                if cmp -s "$OUT/$s-b$b-t1.txt" "$OUT/$s-b$b-t$t.txt"; then
                    echo "$s b$b t1 vs t$t IDENTICAL"
                else
                    echo "$s b$b t1 vs t$t DIFFERENT"
                fi
            done
        done
    done
} > "$OUT/determinism.txt"
cat "$OUT/determinism.txt"
