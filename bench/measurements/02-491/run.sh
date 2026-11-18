#!/bin/bash
# The full run of 02-491: every instance of bench/miplib2017-bench.manifest
# under JAOS and HiGHS 1.15.1, one thread each, HiGHS at gap 0 as the
# MIPLIB script runs it (highs.opt) and JAOS at its default, then
# summarise.py. The instances of heavy.txt run first, HEAVY_JOBS at a time,
# and the rest JOBS at a time, largest file first. Each job runs under a
# cap of MEM_GB divided by the jobs beside it; a run that ends with no
# status (a crash, an allocation the cap refused) runs once more alone
# under the whole of MEM_GB. A run still going at 1.5 times the limit plus
# 60 s is killed and reads `killed`. Wall seconds are CLOCK_MONOTONIC's,
# the clock JAOS times itself by: under WSL the realtime clock steps back
# and /proc/uptime jumps ahead (README). SCIP joins with -s "jaos highs scip"
# when SCIP_PYTHON names a Python with pyscipopt. HIGHS names the binary
# when bench/compare/solvers holds none (a worktree). JAOS_ARGS is passed
# to `jaos solve`. -n FILE runs only the instances it lists. -d DIR reads
# the instances from DIR, fetched there if absent. Run again with the same
# -o, it keeps the runs already under its lines/ and does the rest; remove
# that folder to start over.
#
#   bash bench/measurements/02-491/run.sh [-t SECONDS] [-j JOBS] [-h HEAVY_JOBS]
#       [-m MEM_GB] [-s "jaos highs"] [-n FILE] [-d DIR] [-o DIR]
#
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
limit=300
jobs=5
heavy_jobs=2
mem_gb=13
solvers="jaos highs"
only=""
out="$here/full"
dir=bench/instances-miplib2017-bench
while getopts t:j:h:m:s:n:d:o: opt; do
    case $opt in
        t) limit=$OPTARG ;;
        j) jobs=$OPTARG ;;
        h) heavy_jobs=$OPTARG ;;
        m) mem_gb=$OPTARG ;;
        s) solvers=$OPTARG ;;
        n) only=$(cd "$(dirname "$OPTARG")" && pwd)/$(basename "$OPTARG") ;;
        d) dir=$OPTARG ;;
        o) out=$OPTARG ;;
        *) exit 2 ;;
    esac
done
manifest=bench/miplib2017-bench.manifest
bench/fetch.sh -m "$manifest" -b https://miplib.zib.de/WebData/instances \
    -p mps-gz "$dir" || exit 3
dir=$(cd "$dir" && pwd)
make -s cli > /dev/null || { echo "build failed" >&2; exit 4; }
mkdir -p "$out/lines" "$out/logs"
out=$(cd "$out" && pwd)

export LIMIT=$limit OUT=$out DIR=$dir MANIFEST=$root/$manifest HERE=$here
export ROOT=$root JAOS=$root/build/cli/jaos JAOS_ARGS=${JAOS_ARGS:-}
export HIGHS=${HIGHS:-$(ls "$root"/bench/compare/solvers/highs-* 2>/dev/null | head -1)}
export SCIP_PYTHON=${SCIP_PYTHON:-}
case " $solvers " in *" highs "*) [ -x "$HIGHS" ] || { echo "no HiGHS binary; set HIGHS" >&2; exit 5; } ;; esac
case " $solvers " in *" scip "*) [ -n "$SCIP_PYTHON" ] || { echo "set SCIP_PYTHON" >&2; exit 5; } ;; esac

one() {
    local name=$1 solver=$2 cap_kb=$3
    local mps=$DIR/$name.mps log=$OUT/logs/$name.$solver
    local kill_after=$((LIMIT + LIMIT / 2 + 60))
    local ref source st="" obj="" nodes="" secs="" wall="" peak="" o cmd t0 t1
    read -r ref source < <(awk -v n="$name" '$1 == n {print $5, $6}' "$MANIFEST")
    case $solver in
        jaos)  cmd=("$JAOS" solve "$mps" --time-limit "$LIMIT") ;;
        highs) cmd=("$HIGHS" --options_file "$HERE/highs.opt" --time_limit "$LIMIT"
                    --model_file "$mps") ;;
        scip)  cmd=("$SCIP_PYTHON" "$ROOT/bench/compare/scip_solve.py" "$mps" "$LIMIT") ;;
    esac
    [ "$solver" = jaos ] && [ -n "$JAOS_ARGS" ] && cmd+=($JAOS_ARGS)
    t0=$(python3 -c 'import time; print(time.monotonic())')
    ( ulimit -v "$cap_kb"
      exec /usr/bin/time -v -o "$log.time" \
          timeout --foreground -s KILL "$kill_after" "${cmd[@]}" ) > "$log.out" 2>&1
    t1=$(python3 -c 'import time; print(time.monotonic())')
    o=$(cat "$log.out")
    case $solver in
        jaos)
            st=$(echo "$o" | awk '$1 == "status" {print $2; exit}')
            obj=$(echo "$o" | awk '$1 == "objective" {print $2; exit}')
            [ -n "$obj" ] || obj=$(echo "$o" | awk '$1 == "incumbent" {print $2; exit}')
            nodes=$(echo "$o" | awk '$1 == "nodes" {print $2; exit}')
            secs=$(echo "$o" | awk '$1 == "time" {print $2; exit}') ;;
        highs)
            st=$(echo "$o" | awk '$1 == "Status" {print $2; exit}')
            obj=$(echo "$o" | awk '$1 == "Primal" && $2 == "bound" {print $3; exit}')
            nodes=$(echo "$o" | awk '$1 == "Nodes" && $2 ~ /^[0-9]+$/ {n = $2} END {print n}')
            secs=$(echo "$o" | awk '$1 == "Timing" {print $2; exit}') ;;
        scip)
            IFS=$'\t' read -r st obj nodes secs <<< "$(tail -1 "$log.out")" ;;
    esac
    wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN {printf "%.2f", b - a}')
    peak=$(awk -F': ' '/Maximum resident set size/ {print $2}' "$log.time")
    grep -qi 'out of memory\|bad_alloc' "$log.out" && st=fail
    grep -q 'Exit status: 137\|terminated by signal 9' "$log.time" && st=killed
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$solver" \
        "${st:-fail}" "$obj" "$nodes" "$secs" "$ref" "$source" "$wall" "$peak" \
        > "$OUT/lines/$name.$solver"
}
export -f one

names() {
    awk '!/^#/ && NF >= 7 {print $1}' "$MANIFEST" |
        { if [ -n "$only" ]; then grep -Fxf "$only"; else cat; fi; }
}
heavy=$(names | grep -Fxf "$here/heavy.txt")
rest=$(names | grep -Fvxf "$here/heavy.txt" |
    while read -r n; do echo "$(stat -c %s "$DIR/$n.mps") $n"; done |
    sort -k1,1nr | awk '{print $2}')
cap_all=$((mem_gb * 1048576))
tasks() {
    local cap=$1 n s f; shift
    for n in "$@"; do
        for s in $solvers; do
            f="$OUT/lines/$n.$s"
            [ -s "$f" ] && [ "$(cut -f3 "$f")" != fail ] && continue
            echo "$n $s $cap"
        done
    done
}
count=$(echo $heavy $rest | wc -w)
start=$(date +%s)
load_start=$(cut -d' ' -f1-3 /proc/loadavg)
echo "$count instances x [$solvers], ${limit} s, jobs $jobs (heavy $heavy_jobs), cap ${mem_gb} GB"
[ -z "$heavy" ] || tasks $((cap_all / heavy_jobs)) $heavy |
    xargs -P "$heavy_jobs" -n 3 bash -c 'one "$1" "$2" "$3"' _
[ -z "$rest" ] || tasks $((cap_all / jobs)) $rest |
    xargs -P "$jobs" -n 3 bash -c 'one "$1" "$2" "$3"' _
for n in $heavy $rest; do
    for s in $solvers; do
        f="$OUT/lines/$n.$s"
        if [ ! -s "$f" ] || [ "$(cut -f3 "$f")" = fail ]; then
            echo "again alone: $n $s"
            one "$n" "$s" "$cap_all"
        fi
    done
done

record="$out/record.txt"
tree=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
dirty=$(git status --porcelain src include 2>/dev/null | head -1)
hv=$([ -x "$HIGHS" ] && "$HIGHS" --version 2>/dev/null | head -1)
{
    echo "# JAOS against HiGHS on miplib2017-bench.manifest, ${limit} s each, one thread, gap: HiGHS 0 (the MIPLIB script's), JAOS 1e-6 (its default), SCIP 1e-6"
    echo "# machine: $(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')$(grep -qi microsoft /proc/version && echo ' under WSL')"
    echo "# tree: ${tree}${dirty:+ WITH UNCOMMITTED CHANGES}${JAOS_ARGS:+, jaos $JAOS_ARGS}"
    echo "# solvers: [$solvers] ${hv:-HiGHS not found}"
    echo "# jobs $jobs, heavy $heavy_jobs, cap ${mem_gb} GB; load $load_start at start, $(cut -d' ' -f1-3 /proc/loadavg) at end; $(( $(date +%s) - start )) s"
    echo "# instance solver status objective nodes seconds reference source wall_seconds peak_kb"
    for n in $heavy $rest; do for s in $solvers; do cat "$out/lines/$n.$s"; done; done
} > "$record"
python3 "$here/summarise.py" "$record"
echo "record: $record"
