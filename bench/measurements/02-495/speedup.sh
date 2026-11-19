#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2

JAOS=${JAOS:-build/cli/jaos}
OUT=${OUT:-bench/measurements/02-495/speedup-$(hostname)}
THREADS=${THREADS:-"1 2 4 8"}
LIMIT=${LIMIT:-1000000000}
ARGS=${ARGS:---tree-batch 8}
B=${ARGS##* }
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
printf '%-20s %5s %3s %12s %8s %8s %8s %6s %9s %s\n' instance batch thr work wall cpu cpu/wall vs1 rss_kb same > "$OUT/table.txt"
echo "$SETS" | while read -r set name; do
    [ -n "$set" ] || continue
    f=bench/instances-$set/$name.mps
    [ -f "$f" ] || { echo "speedup.sh: missing $f"; continue; }
    log=$OUT/$name-b1-t1.out
    /usr/bin/time -f '%e %U %S %M' -o "$OUT/$name-b1-t1.time" \
        "$JAOS" solve "$f" --threads 1 --work-limit "$LIMIT" > "$log"
    read -r _ u1 s1 m1 < <(tail -n 1 "$OUT/$name-b1-t1.time")
    w1=$(awk '/^time / { print $2 }' "$log")
    work1=$(awk '/^work_units / { print $2 }' "$log")
    awk -v n="$name" -v w="$work1" -v wl="$w1" -v u="$u1" -v s="$s1" -v m="$m1" 'BEGIN {
        cpu = u + s
        printf "%-20s %5d %3d %12d %8.2f %8.2f %8.2f %6s %9d %s\n", n, 1, 1, w, wl, cpu, (wl > 0 ? cpu / wl : 0), "-", m, "-"
    }' >> "$OUT/table.txt"
    ref=""
    wall1=""
    for t in $THREADS; do
        log=$OUT/$name-t$t.out
        /usr/bin/time -f '%e %U %S %M' -o "$OUT/$name-t$t.time" \
            "$JAOS" solve "$f" --threads "$t" --work-limit "$LIMIT" $ARGS > "$log"
        read -r _ user sys rss < <(tail -n 1 "$OUT/$name-t$t.time")
        wall=$(awk '/^time / { print $2 }' "$log")
        key=$(grep -E '^(status|objective|work_units|nodes|bound|incumbent) ' "$log" | tr '\n' ' ')
        work=$(awk '/^work_units / { print $2 }' "$log")
        [ -n "$ref" ] || { ref=$key; wall1=$wall; }
        same=yes
        [ "$key" = "$ref" ] || same=NO
        awk -v n="$name" -v t="$t" -v w="$work" -v wl="$wall" -v u="$user" \
            -v s="$sys" -v w1="$wall1" -v same="$same" -v m="$rss" -v b="$B" 'BEGIN {
            cpu = u + s
            busy = 0
            gain = 0
            if (wl > 0) {
                busy = cpu / wl
                gain = w1 / wl
            }
            printf "%-20s %5s %3d %12d %8.2f %8.2f %8.2f %6.2f %9d %s\n", n, b, t, w,
                   wl, cpu, busy, gain, m, same
        }' >> "$OUT/table.txt"
    done
done
uptime >> "$OUT/table.txt"
cat "$OUT/table.txt"
