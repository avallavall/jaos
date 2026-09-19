#!/usr/bin/env bash
# The reading behind 02-260: the harnesses of 02-253 and 02-255 over
# seeds 1 to 3, against two libraries, and whether each output differs.
#
#   ab.sh OLD_LIB NEW_LIB [OUT]      each a libjaos.a; needs `make all`
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
OLD=$1
NEW=$2
OUT=${3:-$HOME/ab-02-260}
mkdir -p "$OUT"
for h in 02-253/conic.sh 02-255/misocp.sh; do
    b=$(basename "$h" .sh)
    for seed in 1 2 3; do
        LIB=$OLD bash "bench/measurements/$h" 1000 "$seed" "$OUT/old-$b-$seed" \
            > "$OUT/old-$b-$seed.txt" 2>&1
        LIB=$NEW bash "bench/measurements/$h" 1000 "$seed" "$OUT/new-$b-$seed" \
            > "$OUT/new-$b-$seed.txt" 2>&1
        if cmp -s "$OUT/old-$b-$seed.txt" "$OUT/new-$b-$seed.txt"; then
            echo "$h seed $seed: same"
        else
            echo "$h seed $seed: differs"
            diff "$OUT/old-$b-$seed.txt" "$OUT/new-$b-$seed.txt"
        fi
    done
done
