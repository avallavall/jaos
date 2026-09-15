#!/usr/bin/env bash
# The reading behind 02-242. Builds `limits.c` against the library in the
# tree and runs it over six seeds; `limits.c` exits non-zero when any of
# its six properties broke. Every 50th model is written out and the tool
# runs `solve --work-limit L` on it twice: both exit 3, print `status
# work_limit`, and agree byte for byte above the `time` line.
#
#   limits.sh           the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-242
OUT=${1:-/tmp/limits-02-242}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/limits" \
    "$HERE/limits.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/limits" "${RUNS:-1000}" "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 ok=0 past=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        L=$(head -1 "$b.limit")
        cli=$((cli + 1))
        "$JAOS" solve "$mps" --work-limit "$L" > "$b.out1" 2>/dev/null; rc1=$?
        "$JAOS" solve "$mps" --work-limit "$L" > "$b.out2" 2>/dev/null; rc2=$?
        work=$(sed -n 's/^work_units //p' "$b.out1")
        if [ "$rc1" = 3 ] && [ "$rc2" = 3 ] && grep -q '^status work_limit$' "$b.out1" &&
           cmp -s <(grep -v '^time ' "$b.out1") <(grep -v '^time ' "$b.out2"); then
            ok=$((ok + 1))
        elif [ "$rc1" = 0 ] && [ "$rc2" = 0 ] && grep -q MARKER "$mps" &&
             [ -n "$work" ] && [ "$work" -ge "$L" ] &&
             cmp -s <(grep -v '^time ' "$b.out1") <(grep -v '^time ' "$b.out2"); then
            ok=$((ok + 1)); past=$((past + 1))
        else
            echo "CLI $b: rc $rc1 $rc2"; head -3 "$b.out1"
        fi
    done
    echo "cli $cli ok $ok mip_finished_past_limit $past"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
