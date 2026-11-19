#!/bin/bash
# The readings of 02-498: `jaos solve FILE --work-limit WORK --threads 1`
# on each instance LIST names, JOBS at a time, read from
# bench/instances-miplib2017-bench (or DIR). A run's line in OUT/table.txt
# is: instance, status, work units, work units over WORK, JAOS's own `time`
# and the run's wall seconds on CLOCK_MONOTONIC, the read included. BIN is
# the tool to run, so one binary can be kept per state of the tree.
#
#   bash bench/measurements/02-498/run.sh BIN WORK OUT LIST [JOBS] [ARGS...]
#
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
export BIN=$1 WORK=$2 OUT=$3
list=$4
jobs=${5:-2}
shift 4
[ $# -gt 0 ] && shift
export EXTRA="$*"
export DIR=${DIR:-$root/bench/instances-miplib2017-bench}
mkdir -p "$OUT"
one() {
    local n=$1 t0 t1 st tm wk
    t0=$(python3 -c 'import time; print(time.monotonic())')
    ( ulimit -v 7000000
      timeout --foreground -s KILL 1800 \
          "$BIN" solve "$DIR/$n.mps" --work-limit "$WORK" --threads 1 \
          $EXTRA 2>&1 ) > "$OUT/$n.log"
    t1=$(python3 -c 'import time; print(time.monotonic())')
    st=$(awk '$1 == "status" {print $2; exit}' "$OUT/$n.log")
    tm=$(awk '$1 == "time" {print $2; exit}' "$OUT/$n.log")
    wk=$(awk '$1 == "work_units" {print $2; exit}' "$OUT/$n.log")
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "${st:-none}" "${wk:-}" \
        "$(awk -v w="${wk:-0}" -v l="$WORK" 'BEGIN {printf "%.4f", w / l}')" \
        "${tm:-}" \
        "$(awk -v a="$t0" -v b="$t1" 'BEGIN {printf "%.2f", b - a}')" \
        > "$OUT/$n.line"
}
export -f one
xargs -P "$jobs" -n 1 bash -c 'one "$1"' _ < "$list"
while read -r n; do cat "$OUT/$n.line"; done < "$list" > "$OUT/table.txt"
cat "$OUT/table.txt"
