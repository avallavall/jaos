#!/bin/bash
# D97 derivation, step 1: reproduce the minimal failing design's refusals.
# 7c7375c is feat(02-04) "a bound that is reasoned with rather than
# published". Expected from D97's table: pilot and pilot87 INFEASIBLE,
# most of the set solved.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN="$JAOS_ROOT"
SCRATCH=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f9716367-518e-4a13-aa7b-60c013ebb796/scratchpad
COPY=$SCRATCH/jaos-d97
S=$SCRATCH/d97
mkdir -p "$S"

rm -rf "$COPY"; mkdir -p "$COPY"
(cd "$MAIN" && git archive 7c7375c) | tar -x -C "$COPY"
cd "$COPY" || exit 9
mkdir -p build/diag
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run -lm \
    || { echo "BUILD FAILED"; exit 2; }
echo "built 7c7375c"

for inst in afiro pilot pilot87 agg maros; do
    ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$inst" 2>/dev/null \
        | grep -E "^\[" | sed 's/(.*//'
done
echo "D97_STEP1_DONE"
