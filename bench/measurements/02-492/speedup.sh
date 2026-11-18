#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2

JAOS=${JAOS:-build/cli/jaos}
OUT=${OUT:-bench/measurements/02-492/speedup-$(hostname)}
THREADS=${THREADS:-"1 4 8"}
LIMIT=${LIMIT:-1000000000}
SETS=${SETS:-"
miplib misc07
miplib l152lav
miplib stein45
miplib bell5
miplib blend2
miplib2017 binkar10_1
miplib2017 tr12-30
miplib2017 mad
miplib2017 neos-2657525-crna
miplib2017 gen-ip002
"}

[ -x "$JAOS" ] || { echo "speedup.sh: build $JAOS first (make cli)"; exit 2; }
mkdir -p "$OUT"
printf '%-20s %3s %12s %8s %8s %8s %6s %s\n' instance thr work wall cpu cpu/wall vs1 same > "$OUT/table.txt"
echo "$SETS" | while read -r set name; do
    [ -n "$set" ] || continue
    f=bench/instances-$set/$name.mps
    [ -f "$f" ] || { echo "speedup.sh: missing $f"; continue; }
    ref=""
    wall1=""
    for t in $THREADS; do
        log=$OUT/$name-t$t.out
        /usr/bin/time -f '%e %U %S' -o "$OUT/$name-t$t.time" \
            "$JAOS" solve "$f" --threads "$t" --work-limit "$LIMIT" > "$log"
        read -r _ user sys < <(tail -n 1 "$OUT/$name-t$t.time")
        wall=$(awk '/^time / { print $2 }' "$log")
        key=$(grep -E '^(status|objective|work_units|nodes|bound|incumbent) ' "$log" | tr '\n' ' ')
        work=$(awk '/^work_units / { print $2 }' "$log")
        [ -n "$ref" ] || { ref=$key; wall1=$wall; }
        same=yes
        [ "$key" = "$ref" ] || same=NO
        awk -v n="$name" -v t="$t" -v w="$work" -v wl="$wall" -v u="$user" \
            -v s="$sys" -v w1="$wall1" -v same="$same" 'BEGIN {
            cpu = u + s
            busy = 0
            gain = 0
            if (wl > 0) {
                busy = cpu / wl
                gain = w1 / wl
            }
            printf "%-20s %3d %12d %8.2f %8.2f %8.2f %6.2f %s\n", n, t, w,
                   wl, cpu, busy, gain, same
        }' >> "$OUT/table.txt"
    done
done
cat "$OUT/table.txt"
