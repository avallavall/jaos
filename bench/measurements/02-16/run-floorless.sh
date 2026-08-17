#!/bin/bash
# S1b (D109): what does a floor-less implied-free window take at 8 ulps?
# A copy of the tree gets ps_implied_free_margin's outer max(1, scale)
# removed; the repository is not modified. The copy proves itself first by
# reproducing maros-r7's committed margin-0 reading from 02-12's sweep, then
# runs the standard set at floor-less 8; the result is diffed against the
# committed record.
#
# The prediction stated before the first run, and confirmed by it: maros-r7
# stays at 980 rows. Its 4 exact-equality candidates sit on zero bounds,
# where no margin is ever absorbed, so only margin zero takes them.
#
# Usage (inside WSL, from the repository root):
#   bash bench/measurements/02-16/run-floorless.sh [COPYDIR] [OUTDIR]
set -u
here=$(cd "$(dirname "$0")" && pwd)
MAIN=$(cd "$here/../../.." && pwd)
COPY=${1:-/tmp/jaos-floorless}
SCR=${2:-$here}
mkdir -p "$SCR"

rm -rf "$COPY"
mkdir -p "$COPY/bench" "$COPY/build/diag"
cp -r "$MAIN/src" "$MAIN/include" "$COPY/"
cp "$MAIN/bench/run.c" "$MAIN/bench/netlib.manifest" "$COPY/bench/"

python3 - "$COPY/src/presolve.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """    return PRESOLVE_IMPLIED_FREE_ULPS * DBL_EPSILON *
           (scale > 1.0 ? scale : 1.0);"""
new = """    return PRESOLVE_IMPLIED_FREE_ULPS * DBL_EPSILON * scale;"""
if s.count(old) != 1:
    sys.exit("patch anchor found %d times, want 1" % s.count(old))
open(p, "w").write(s.replace(old, new))
print("patched ps_implied_free_margin")
EOF
[ $? -eq 0 ] || exit 2

cd "$COPY" || exit 9

want=$(grep '^maros-r7 ' "$MAIN/bench/measurements/02-12/sweep/netlib-0.txt")
wit=$(echo "$want" | grep -o 'iters=[0-9]*' | cut -d= -f2)
wwk=$(echo "$want" | grep -o 'work=[0-9]*' | cut -d= -f2)
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off \
    -DJAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0 -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run-m0 -lm || { echo "BUILD FAILED (copy m0)"; exit 2; }
out=$(./build/diag/run-m0 -j 1 -d "$MAIN/bench/instances" maros-r7 2>/dev/null)
mit=$(echo "$out" | grep -o 'iters=[0-9]*' | head -1 | cut -d= -f2)
mwk=$(echo "$out" | grep -o 'work=[0-9]*' | head -1 | cut -d= -f2)
if [ "$mit" != "$wit" ] || [ "$mwk" != "$wwk" ]; then
    echo "COPY SELF-PROOF FAILED: got iters=$mit work=$mwk, want iters=$wit work=$wwk"
    exit 1
fi
echo "copy self-proof ok: margin-0 maros-r7 iters=$mit work=$mwk"

gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run-fl8 -lm || { echo "BUILD FAILED (floorless 8)"; exit 2; }
./build/diag/run-fl8 -j 12 -d "$MAIN/bench/instances" -o "$SCR/netlib-floorless8.txt" \
    > "$SCR/netlib-floorless8.log" 2>&1
echo "standard set done, exit $?"

echo "instance lines differing from the committed record:"
diff <(grep -E '^[a-z0-9]' "$MAIN/bench/results/netlib.txt") \
     <(grep -E '^[a-z0-9]' "$SCR/netlib-floorless8.txt") \
    && echo "NONE - all instance lines identical"
echo "S1B_FLOORLESS_DONE"
