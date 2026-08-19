#!/bin/bash
# Build and run the hostile-basis probe at HEAD. src/ is read, never written.
#
# Prediction, stated before the run: unknown. D145 makes "reproducible at
# HEAD" plausible; every line correct would mean the promise survives these
# 16 deterministic shifts on five instances, recorded as exactly that and
# no more.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out="$here/hostile.txt"
cd "$root" || exit 9
mkdir -p build/diag

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
    -Iinclude src/*.c "$here/probe-hostile.c" -o build/diag/probe-hostile -lm \
    || { echo "build failed"; exit 2; }

d=bench/instances
./build/diag/probe-hostile 16 \
    scsd1=$d/scsd1.mps degen2=$d/degen2.mps modszk1=$d/modszk1.mps \
    cycle=$d/cycle.mps woodw=$d/woodw.mps 2> "$out.raw"

{
echo "# One line per (instance, shift). wrong=1 with status=1 (OPTIMAL) is a"
echo "# wrong optimum from the public API alone; chk=0 is a checker-refused"
echo "# point published optimal. Reference from the same binary's cold solve."
echo
awk '/^HOSTILE /{
    n++
    if ($0 ~ /error=/) { err++; print; next }
    delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
    if (v["wrong"] == 1) { W++; print }
    else if (v["chk"] == 0) { R++; print }
}
END {
    printf "\n%d trials: WRONG-OPTIMAL=%d checker-refused=%d errors=%d\n",
           n, W, R, err
    if (W > 0) print "the jaos.h promise is refuted at HEAD by construction"
    else if (R > 0) print "no wrong objective; the checker refuses some published points"
    else print "all trials correct on these shifts; the promise held here"
}' "$out.raw"
} 2>&1 | tee "$out"
echo "raw lines kept in $out.raw"
