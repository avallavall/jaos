#!/bin/bash
# S1c: generate the recovery-error model, build the driver against HEAD src,
# run it, and put prediction and measurement side by side.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN="$JAOS_ROOT"
SCR=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f9716367-518e-4a13-aa7b-60c013ebb796/scratchpad/s1c
cd "$SCR" || exit 9

python3 gen.py ifrecov.mps > gen.out || { cat gen.out; exit 2; }
cat gen.out
V=$(grep '^V=' gen.out | cut -d= -f2)
B=$(grep '^b=' gen.out | cut -d= -f2)
L=$(grep '^L=' gen.out | cut -d= -f2)

gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off \
    -I"$MAIN/include" -I"$MAIN/src" \
    driver.c "$MAIN"/src/*.c -o driver -lm || { echo "BUILD FAILED"; exit 2; }

./driver ifrecov.mps "$V" "$B" "$L"
echo "S1C_DONE exit=$?"
