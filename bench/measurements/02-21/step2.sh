#!/bin/bash
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN="$JAOS_ROOT"
SCRATCH=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f9716367-518e-4a13-aa7b-60c013ebb796/scratchpad
COPY=$SCRATCH/jaos-d97
S=$SCRATCH/d97
cd "$COPY" || exit 9
python3 "$S/patch1.py" src/presolve.c || exit 2
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run -lm || { echo "BUILD FAILED"; exit 2; }
for inst in pilot pilot87; do
    echo "== $inst =="
    ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$inst" 2>&1 >/dev/null | grep '^INF' | head -4
done
echo "D97_STEP2_DONE"
