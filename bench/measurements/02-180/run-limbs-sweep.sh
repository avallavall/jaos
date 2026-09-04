#!/usr/bin/env bash
# D275. How much of `jaos_verify`'s refusal is the constant and how much is
# the mathematics.
#
# D274 proves 30 of 110 and refuses 74, on a Hadamard bound that is an upper
# bound and a loose one. Nobody has measured how loose. `JM_EXACT_LIMBS`
# exists to be swept -- `docs/tolerances.md` says so and says the sweep that
# means something is over the models a verifier can prove -- so this is that
# sweep: 128, 256, 512 and 1024 limbs, over the standard set.
#
# **`make clean` between settings, and a canary that must move.** Without the
# clean, `make` does not notice a change in EXTRA_CFLAGS and the sweep
# measures one binary four times (D154, and the same shape at D-10). The
# canary is free here: the instrument prints `capacity_bits`, which is
# 32 * JM_EXACT_LIMBS, so a setting whose capacity did not change did not
# rebuild and the run says so and stops.
#
#   bash bench/measurements/02-180/run-limbs-sweep.sh
#
# Not a gate tool.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
out="$here/limbs.txt"
STD="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g"
SHIP="-DNDEBUG -O3 -flto -march=native -mtune=native"

{
    echo "# instrument: bench/measurements/02-179/proofs.c, over the standard set"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# objects: release, rebuilt clean per setting"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "#"
    echo "# limbs  capacity  proved  refused  broken  seconds"
} > "$out"

seen_cap=""
for n in 128 256 512 1024; do
    echo "=== JM_EXACT_LIMBS=$n ==="
    make clean >/dev/null 2>&1
    make build/release/libjaos.a EXTRA_CFLAGS="-DJM_EXACT_LIMBS=$n" \
        >/dev/null 2>&1 || { echo "build failed at $n"; exit 1; }

    $CC $STD $SHIP -DJM_EXACT_LIMBS=$n -Iinclude -Isrc \
        bench/measurements/02-179/proofs.c $(ls build/release/*.o) \
        -o build/proofs-sweep -lm 2>/dev/null || { echo "link failed at $n"; exit 1; }

    t0=$(date +%s)
    ./build/proofs-sweep bench/instances/*.mps > "$here/raw-$n.txt" 2>&1
    t1=$(date +%s)

    # The whole line, not a field: "# capacity: JM_EXACT_LIMBS=128, 4096 bits".
    cap=$(grep -m1 '^# capacity:' "$here/raw-$n.txt")
    bits=$(echo "$cap" | sed 's/.*, \([0-9]*\) bits/\1/')
    # The canary. capacity_bits is 32 * limbs, so it must differ every time.
    if [ -n "$cap" ] && [ "$cap" = "$seen_cap" ]; then
        echo "CANARY: capacity did not move from $seen_cap at limbs=$n --"
        echo "the rebuild did not happen and the sweep is measuring one binary"
        exit 2
    fi
    seen_cap="$cap"

    p=$(grep -c ' PROVED$'  "$here/raw-$n.txt" || true)
    b=$(grep -c ' BROKEN$'  "$here/raw-$n.txt" || true)
    r=$(grep -c ' refused$' "$here/raw-$n.txt" || true)
    printf '%7d %9s %7d %8d %7d %8d\n' \
        "$n" "${bits:-?}" "$p" "$r" "$b" "$((t1-t0))" >> "$out"
    tail -1 "$out"
done

echo
cat "$out"
