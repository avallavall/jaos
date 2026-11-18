#!/bin/bash
# The readings of 02-494: `jaos solve FILE --time-limit LIMIT --log progress`
# on each instance LIST names, JOBS at a time, read from
# bench/instances-miplib2017-bench (bench/miplib2017-bench.manifest). Each
# log line is stamped with the seconds since its run started, on
# CLOCK_MONOTONIC. A run's line in OUT/table.txt is: instance, status, JAOS's
# own `time`, the run's wall seconds, the stamp of the first log line (the
# read and whatever runs before the first line is logged) and the work units.
# BIN is the tool to run, so one binary can be kept per state of the tree.
#
#   bash bench/measurements/02-494/run.sh BIN LIMIT OUT LIST [JOBS] [ARGS...]
#
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
export BIN=$1 LIMIT=$2 OUT=$3
list=$4
jobs=${5:-2}
shift 4
[ $# -gt 0 ] && shift
export EXTRA="$*"
export DIR=${DIR:-$root/bench/instances-miplib2017-bench}
mkdir -p "$OUT"
one() {
    local n=$1 t0 t1 st tm wk first
    t0=$(python3 -c 'import time; print(time.monotonic())')
    ( ulimit -v 7000000
      timeout --foreground -s KILL $((LIMIT * 3 + 300)) \
          "$BIN" solve "$DIR/$n.mps" --time-limit "$LIMIT" --log progress \
          $EXTRA 2>&1 ) |
        python3 -u -c '
import sys, time
t0 = time.monotonic()
for line in sys.stdin:
    sys.stdout.write("%8.2f %s" % (time.monotonic() - t0, line[:600].rstrip("\n") + "\n"))
    sys.stdout.flush()' > "$OUT/$n.log"
    t1=$(python3 -c 'import time; print(time.monotonic())')
    st=$(awk '$2 == "status" {print $3; exit}' "$OUT/$n.log")
    tm=$(awk '$2 == "time" {print $3; exit}' "$OUT/$n.log")
    wk=$(awk '$2 == "work_units" {print $3; exit}' "$OUT/$n.log")
    first=$(head -1 "$OUT/$n.log" | awk '{print $1}')
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "${st:-none}" "${tm:-}" \
        "$(awk -v a="$t0" -v b="$t1" 'BEGIN {printf "%.2f", b - a}')" \
        "$first" "${wk:-}" > "$OUT/$n.line"
}
export -f one
xargs -P "$jobs" -n 1 bash -c 'one "$1"' _ < "$list"
while read -r n; do cat "$OUT/$n.line"; done < "$list" > "$OUT/table.txt"
cat "$OUT/table.txt"
