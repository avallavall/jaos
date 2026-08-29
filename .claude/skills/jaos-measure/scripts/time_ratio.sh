#!/bin/bash
# time_ratio: a same-instance wall-clock ratio between the working tree and a
# git ref, taken the one way that means anything here.
#
#   scripts/time_ratio.sh -r REF [-n ROUNDS] INSTANCE...
#   scripts/time_ratio.sh -r HEAD~1 -n 5 maros-r7 pilot87 dfl001
#
# Output is `name ref candidate` rows, ready for
#
#   scripts/time_ratio.sh -r REF INSTANCE... | scripts/geomean.py --pairs -
#
# and every other line is a `#` comment, which `--pairs` skips.
#
# Why this is a script. The protocol in SKILL.md has five clauses and each one
# has a reason; four of them are easy to leave out and none of the omissions
# shows up in the number:
#
#   -j 1              a parallel run's seconds are twelve solves contending
#                     for one cache, which is not the workload (D57)
#   two binaries      built from the same compiler in the same session, so
#                     the comparison is of code and not of environment
#   alternate         a machine that gets busy halfway through otherwise
#                     charges the whole drift to whichever tree ran second
#   the MINIMUM       not the mean: the mean measures what else the machine
#                     was doing, the minimum measures the least interrupted
#                     run. This is what bench/compare does
#   three rounds up   one round has no minimum to take
#
# What it does NOT do. It never writes a record: seconds never enter
# `bench/results/*.txt` or a baseline, because a baseline that changes every
# run cannot detect a regression. It prints to stdout and nowhere else.
#
# And read the floor before the ratio. This host repeats to 6.27% (D93), so
# any reading inside that band is a statement about the machine. Where the
# change is on the solve path at all, `tools/icount.sh` resolves what seconds
# cannot, exactly, and should be run first (D206); a time ratio is the fourth
# metric and is for what an instruction count cannot see.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2

# This host's own repeatability, measured the way D81 measured its 1.4%.
# Printed beside every result so a ratio is never read without it.
HOST_FLOOR_PCT=6.27
ref=""
rounds=3
while getopts "r:n:" o; do
    case $o in
        r) ref=$OPTARG;;
        n) rounds=$OPTARG;;
        *) exit 2;;
    esac
done
shift $((OPTIND - 1))
[ -n "$ref" ] && [ $# -ge 1 ] || {
    echo "usage: time_ratio.sh -r REF [-n ROUNDS] INSTANCE..." >&2; exit 2; }
case $rounds in
    ''|*[!0-9]*) echo "rounds must be a number" >&2; exit 2;;
esac
[ "$rounds" -ge 2 ] || { echo "rounds must be at least 2: one round has no minimum" >&2; exit 2; }

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

make build/bench/run > /dev/null 2>&1 || { echo "build of the working tree failed" >&2; exit 2; }
git worktree add --detach "$D/wt" "$ref" > /dev/null 2>&1 || {
    echo "cannot check out $ref" >&2; exit 2; }
ln -s "$JAOS_ROOT/bench/instances" "$D/wt/bench/instances" 2>/dev/null
[ -d "$JAOS_ROOT/bench/instances-kennington" ] && \
    ln -s "$JAOS_ROOT/bench/instances-kennington" "$D/wt/bench/instances-kennington" 2>/dev/null
( cd "$D/wt" && make build/bench/run > /dev/null 2>&1 ) || {
    echo "build of $ref failed" >&2; exit 2; }

# One run of one tree over every named instance, at -j 1. The runner prefixes
# each record line with its own timing bracket, `[  0.123456s] name ...`, on
# stdout and never in the record; that bracket is the reading.
run_once() {   # $1 = tree dir -> "name secs" per line
    ( cd "$1" && ./build/bench/run -j 1 -o "$D/rec.txt" "${@:2}" 2>/dev/null ) \
        | sed -nE 's/^\[[[:space:]]*([0-9.]+)s\][[:space:]]+([A-Za-z0-9][A-Za-z0-9_.-]*).*/\2 \1/p'
}

declare -A best_ref best_new
for r in $(seq 1 "$rounds"); do
    # Alternating, and the ref first on odd rounds, the working tree first on
    # even ones, so neither position is always the cold one.
    if [ $((r % 2)) -eq 1 ]; then order="ref new"; else order="new ref"; fi
    for who in $order; do
        [ "$who" = ref ] && tree="$D/wt" || tree="$JAOS_ROOT"
        while read -r name secs; do
            [ -n "$name" ] || continue
            if [ "$who" = ref ]; then
                cur=${best_ref[$name]:-}
                if [ -z "$cur" ] || awk -v a="$secs" -v b="$cur" 'BEGIN{exit !(a<b)}'; then
                    best_ref[$name]=$secs
                fi
            else
                cur=${best_new[$name]:-}
                if [ -z "$cur" ] || awk -v a="$secs" -v b="$cur" 'BEGIN{exit !(a<b)}'; then
                    best_new[$name]=$secs
                fi
            fi
        done < <(run_once "$tree" "$@")
    done
done

echo "# time_ratio: $(git rev-parse --short "$ref") -> working tree, -j 1,"
echo "# minimum over $rounds alternating rounds. Seconds, never recorded."
echo "# This host repeats to ${HOST_FLOOR_PCT}% (D93): a ratio inside"
echo "# $(awk -v f=$HOST_FLOOR_PCT 'BEGIN{printf "%.4f..%.4f", 1-f/100, 1+f/100}') is a statement about the machine, not the change."
echo "# name  $(git rev-parse --short "$ref")  working-tree"
n=0
same=0
for inst in "$@"; do
    a=${best_ref[$inst]:-}
    b=${best_new[$inst]:-}
    if [ -z "$a" ] || [ -z "$b" ]; then
        echo "# $inst NO READING (ref=${a:-none} working=${b:-none})"
        continue
    fi
    printf '%s %s %s\n' "$inst" "$a" "$b"
    n=$((n + 1))
    [ "$a" = "$b" ] && same=$((same + 1))
done

[ "$n" -gt 0 ] || { echo "no instance produced a reading on both trees" >&2; exit 2; }

# The canary, D82's. Two trees that time identically to the microsecond on
# every instance have not been distinguished: one binary was measured twice,
# or the change is not on the solve path. A six-decimal clock makes an
# accidental exact tie on several instances close to impossible.
if [ "$same" -eq "$n" ]; then
    echo "STOP: every instance timed identically on both trees to the microsecond;" >&2
    echo "      one binary was measured twice, or nothing changed on the solve path" >&2
    exit 2
fi
echo "# pipe this into geomean.py --pairs - for the geometric mean"
exit 0
