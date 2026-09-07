#!/bin/bash
# The limb budget against how many certificates prove (D337 candidate).
#
# D333 left four of the 29 pinned infeasibilities unreached: `pang`,
# `qual`, `refinery` and `vol1` have a basis and the a-priori bound
# refuses them at 11680 to 16158 bits against a capacity of 4096. That
# ceiling is JM_EXACT_LIMBS, 128 limbs of 32 bits. This asks what raising
# it buys and what it costs.
#
# `make` does not track a change in EXTRA_CFLAGS, so a sweep without
# `make clean` between settings measures one binary N times (D154, and
# the memory that records it). The clean is the whole point of this
# script, and the canary is the count itself: 128 must read 25, and two
# settings reading identical numbers mean the rebuild did not happen.
set -u
cd "$(dirname "$0")/../../.." || exit 2
OUT=bench/measurements/02-216

for n in 128 256 512; do
    echo "=== JM_EXACT_LIMBS=$n ==="
    make clean > /dev/null 2>&1
    make -s shared EXTRA_CFLAGS=-DJM_EXACT_LIMBS=$n > /dev/null 2>&1 || {
        echo "  build failed"; continue; }
    /usr/bin/time -f "  wall %e s, peak rss %M KiB" \
        python3 bench/measurements/02-213/ray.py > "$OUT/n$n.txt" 2>"$OUT/t$n.txt"
    grep -E 'certify on what the writer emits|exact derivation fits' \
        "$OUT/n$n.txt"
    cat "$OUT/t$n.txt"
    echo "  still refused: $(grep -c 'derived=False' "$OUT/n$n.txt") of 29"
done

echo
echo "=== the four D333 could not reach ==="
for i in pang qual refinery vol1; do
    printf '%-9s' "$i"
    for n in 128 256 512; do
        printf ' %s' "$(grep "^$i " "$OUT/n$n.txt" | grep -oE 'shipped=[A-Za-z]+')"
    done
    echo
done
echo "(columns: 128, 256, 512)"
make clean > /dev/null 2>&1
