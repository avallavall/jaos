#!/bin/bash
# klein2's declined pivots, on a throwaway copy of the tree. Never under
# build/ and never left in the repository (jaos-debug).
set -u
REPO=/mnt/c/Users/vall-/Desktop/projectes/jaos
SP=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/measurements/02-215
D=$(mktemp -d)
mkdir -p "$D/src" "$D/include" "$D/bench" "$D/cli"
cp "$REPO"/src/*.c "$REPO"/src/*.h "$D/src/"
cp "$REPO"/include/*.h "$D/include/"
cp "$REPO"/cli/jaos.c "$D/cli/"

python3 "$SP/diag.py" "$D" || exit 2

gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
    -I"$D/include" -I"$D/src" "$D"/src/*.c "$D/cli/jaos.c" -o "$D/jaos" -lm \
    2>"$D/build.err" || { echo "build failed"; head -20 "$D/build.err"; exit 2; }

cd "$REPO" || exit 2
"$D/jaos" solve bench/instances-infeas/klein2.mps --solution "$D/k2.sol" \
    > /dev/null 2>/dev/null
"$D/jaos" solve bench/instances-infeas/klein2.mps --start "$D/k2.sol" \
    > /dev/null 2>"$D/trace.txt"

echo "=== trace lines ==="
wc -l < "$D/trace.txt"
echo "=== declines vs takes ==="
grep -c '^DECL' "$D/trace.txt"
grep -c '^TAKE' "$D/trace.txt"
echo
echo "=== the first 8 declines ==="
grep '^DECL' "$D/trace.txt" | head -8
echo
echo "=== distinct (r, leave, q) triples among the declines ==="
grep '^DECL' "$D/trace.txt" | awk '{print $3, $4, $5}' | sort | uniq -c \
    | sort -rn | head -10
echo "distinct triples: $(grep '^DECL' "$D/trace.txt" | awk '{print $3, $4, $5}' | sort -u | wc -l)"
echo
echo "=== distinct (r, leave, q) triples among the takes ==="
echo "distinct triples: $(grep '^TAKE' "$D/trace.txt" | awk '{print $3, $4, $5}' | sort -u | wc -l)"
grep '^TAKE' "$D/trace.txt" | awk '{print $3, $4, $5}' | sort | uniq -c \
    | sort -rn | head -6
echo
echo "=== the relative disagreement, distribution ==="
grep -oE 'rel=[0-9.e+-]+' "$D/trace.txt" | cut -d= -f2 \
    | awk '{if($1>=1)a++; else if($1>=0.1)b++; else if($1>=0.01)c++; else if($1>=1e-3)d++; else e++}
           END{print "  >=1     :", a+0; print "  0.1-1   :", b+0;
               print "  0.01-0.1:", c+0; print "  1e-3-1e-2:", d+0;
               print "  <1e-3   :", e+0}'
echo
echo "=== n_updates at the decline ==="
grep -oE 'upd=[0-9]+' "$D/trace.txt" | cut -d= -f2 | sort -n | uniq -c \
    | sort -rn | head -6

echo
echo "=== a window of 14 consecutive trace lines inside the cycle ==="
grep -nE '^(DECL|TAKE)' "$D/trace.txt" | sed -n '60000,60013p' | cut -c1-140
echo
echo "=== the primal step on the two repeated takes ==="
grep '^TAKE' "$D/trace.txt" | grep -E 'r=112 leave=5[45] q=5[45]' \
    | grep -oE 'theta=[0-9.e+-]+' | sort | uniq -c | sort -rn | head -5
echo
echo "=== |alpha| of the two repeated declines ==="
grep '^DECL' "$D/trace.txt" | grep -E 'r=143 leave=61 q=77|r=253 leave=66 q=77' \
    | grep -oE 'alpha=[0-9.e+-]+' | sort -u | head -4
