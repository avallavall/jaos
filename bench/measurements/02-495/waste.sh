#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2

JAOS=${JAOS:-build/cli/jaos}
OUT=${OUT:-bench/measurements/02-495/waste-$(hostname)}
SET=${SET:-miplib}
BATCH=${BATCH:-8}
ARGS=${ARGS:-}
LOCK=${LOCK:-$HOME/mipgate.lock}
MANIFEST=bench/$SET.manifest

[ -x "$JAOS" ] || { echo "waste.sh: build $JAOS first (make cli)"; exit 2; }
mkdir -p "$OUT"
one() {
    f=$(ls "bench/instances-$SET/$1".* 2>/dev/null | head -n 1)
    [ -n "$f" ] || return 0
    "$JAOS" solve "$f" --tree-batch "$BATCH" --threads 1 --log summary $ARGS \
        > "$OUT/$1.out" 2>&1
}
export -f one
export JAOS OUT SET BATCH ARGS
grep -v '^#' "$MANIFEST" | awk 'NF { print $1 }' |
    flock "$LOCK" xargs -P 2 -I{} bash -c 'one {}'
{
    printf '%-20s %8s %8s %8s %12s %8s %8s %12s\n' instance nodes rounds rnodes closed_work closed caught again
    for f in "$OUT"/*.out; do
        n=$(basename "$f" .out)
        awk -v n="$n" '
            /^nodes / { nodes = $2 }
            /rounds of up to/ {
                for (i = 1; i <= NF; i++) {
                    if ($(i + 1) == "rounds") r = $i
                    if ($(i + 1) == "nodes;") rn = $i
                    if ($(i + 1) == "closed") c = $i
                    if ($(i + 1) == "work" && $(i + 2) == "units") cw = $i
                    if ($(i + 1) == "times,") cu = $i
                    if ($(i + 1) == "nodes" && $(i + 2) == "were") a = $i
                }
            }
            END {
                printf "%-20s %8d %8d %8d %12d %8d %8d %12d\n", n, nodes, r, rn, cw, c, cu, a
            }' "$f"
    done
} > "$OUT/table.txt"
awk 'NR > 1 { rn += $4; c += $6; a += $8 }
     END { printf "round nodes %d, closed after their lane %d, solved again %d (%.3f%% of round nodes)\n", rn, c, a, (rn > 0 ? 100.0 * a / rn : 0) }' \
    "$OUT/table.txt" >> "$OUT/table.txt"
cat "$OUT/table.txt"
