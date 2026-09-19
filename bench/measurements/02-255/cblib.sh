#!/usr/bin/env bash
# The CBLIB reading behind 02-255: every mixed-integer instance of CBLIB
# 2014 under a work limit, one line each with the status, the objective or
# the incumbent, the bound, CBLIB's reference and claim, and the checker.
#
#   cblib.sh [LIMIT [FLAG...]]    default 1e11 work units; the flags go to
#                                 every solve, as --no-heuristics
#
# Needs `make cli`. DIR holds the 80 files of
# https://cblib.zib.de/download/cblib2014/mip/ and STAT is
# cblib/instances/stat.set-cblib2014.csv of
# https://github.com/HFriberg/cblib-base.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
DIR=${DIR:-bench/instances-cblib-mip}
STAT=${STAT:-$DIR/stat.set-cblib2014.csv}
OUT=${OUT:-$HOME/cblib-02-255}
LIM=${1:-100000000000}
[ $# -gt 0 ] && shift
mkdir -p "$OUT"
for f in $(ls -Sr "$DIR"/*.cbf.gz); do
    n=$(basename "$f" .cbf.gz)
    build/cli/jaos solve "$f" --check --work-limit "$LIM" "$@" > "$OUT/$n.txt" 2>&1
    get() { sed -n "s/^$1 //p" "$OUT/$n.txt"; }
    ref=$(awk -F';' -v n="$n" '$2 == n {print $6}' "$STAT")
    claim=$(awk -F';' -v n="$n" '$2 == n {print $10}' "$STAT")
    printf '%-24s %-12s obj=%-22s inc=%-22s bound=%-22s ref=%-22s %-20s check=%-3s nodes=%-7s work=%s\n' \
        "$n" "$(get status)" "$(get objective)" "$(get incumbent)" \
        "$(get bound)" "$ref" "$claim" "$(get check_ok)" "$(get nodes)" \
        "$(get work_units)"
done
