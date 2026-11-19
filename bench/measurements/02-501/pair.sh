#!/bin/bash
# The before/after reading of 02-501: every instance of a list under two
# builds, the two legs of an instance queued next to each other so that
# they run at the same time and under the same load, JOBS solves at once,
# each with --threads 1 --log summary and a time limit. Each leg's record
# line carries the load average read when it starts and when it ends, and
# the wall seconds of CLOCK_MONOTONIC. The status, objective or incumbent,
# nodes and seconds are read as 02-491's run.sh reads them. A leg runs
# under an address-space cap of MEM_GB / JOBS; a rerun with the same -o
# keeps the legs already under lines/.
#
#   bash bench/measurements/02-501/pair.sh -a BEFORE -b AFTER -n LIST
#       [-t SECONDS] [-j JOBS] [-m MEM_GB] [-o DIR]
#
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
limit=300
jobs=3
mem_gb=13
out="$here/pair"
a=""
b=""
list=""
while getopts a:b:n:t:j:m:o: opt; do
    case $opt in
        a) a=$OPTARG ;;
        b) b=$OPTARG ;;
        n) list=$OPTARG ;;
        t) limit=$OPTARG ;;
        j) jobs=$OPTARG ;;
        m) mem_gb=$OPTARG ;;
        o) out=$OPTARG ;;
        *) exit 2 ;;
    esac
done
[ -x "$a" ] && [ -x "$b" ] && [ -r "$list" ] || { echo "need -a, -b and -n" >&2; exit 2; }
mkdir -p "$out/lines" "$out/logs"
export LIMIT=$limit OUT=$out BIN_A=$a BIN_B=$b
export DIR=$root/bench/instances-miplib2017-bench
export CAP_KB=$((mem_gb * 1048576 / jobs))

leg() {
    local name=$1 leg=$2 bin=$BIN_A log t0 t1 l0 l1 o st obj nodes secs work wall peak
    [ "$leg" = after ] && bin=$BIN_B
    log=$OUT/logs/$name.$leg
    l0=$(cut -d' ' -f1 /proc/loadavg)
    t0=$(python3 -c 'import time; print(time.monotonic())')
    ( ulimit -v "$CAP_KB"
      exec /usr/bin/time -v -o "$log.time" \
          timeout --foreground -s KILL $((LIMIT + LIMIT / 2 + 60)) \
          "$bin" solve "$DIR/$name.mps" --threads 1 --time-limit "$LIMIT" \
          --log summary ) > "$log.out" 2>&1
    t1=$(python3 -c 'import time; print(time.monotonic())')
    l1=$(cut -d' ' -f1 /proc/loadavg)
    o=$(cat "$log.out")
    st=$(echo "$o" | awk '$1 == "status" {print $2; exit}')
    obj=$(echo "$o" | awk '$1 == "objective" {print $2; exit}')
    [ -n "$obj" ] || obj=$(echo "$o" | awk '$1 == "incumbent" {print $2; exit}')
    nodes=$(echo "$o" | awk '$1 == "nodes" {print $2; exit}')
    secs=$(echo "$o" | awk '$1 == "time" {print $2; exit}')
    work=$(echo "$o" | awk '$1 == "work_units" {print $2; exit}')
    wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN {printf "%.2f", b - a}')
    peak=$(awk -F': ' '/Maximum resident set size/ {print $2}' "$log.time")
    grep -q 'Exit status: 137\|terminated by signal 9' "$log.time" && st=killed
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$leg" \
        "${st:-fail}" "$obj" "$nodes" "$secs" "$work" "$wall" "$peak" \
        "$l0" "$l1" > "$OUT/lines/$name.$leg"
}
export -f leg

start=$(date +%s)
load_start=$(cut -d' ' -f1-3 /proc/loadavg)
grep -v '^#' "$list" | while read -r n; do
    for l in before after; do
        f="$OUT/lines/$n.$l"
        [ -s "$f" ] && [ "$(cut -f3 "$f")" != fail ] && continue
        echo "$n $l"
    done
done | xargs -P "$jobs" -n 2 bash -c 'leg "$1" "$2"' _

{
    echo "# 02-501 pair.sh: before $(basename "$a"), after $(basename "$b"); ${limit} s, --threads 1, jobs $jobs, cap $((CAP_KB / 1048576)) GB"
    echo "# machine: $(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')"
    echo "# load $load_start at start, $(cut -d' ' -f1-3 /proc/loadavg) at end; $(( $(date +%s) - start )) s"
    echo "# instance leg status objective nodes seconds work_units wall_seconds peak_kb load_at_start load_at_end"
    grep -v '^#' "$list" | while read -r n; do
        for l in before after; do
            [ -s "$out/lines/$n.$l" ] && cat "$out/lines/$n.$l"
        done
    done
} > "$out/record.txt"
echo "record: $out/record.txt"
