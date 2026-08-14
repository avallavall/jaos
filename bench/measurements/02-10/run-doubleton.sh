#!/usr/bin/env bash
# Build the doubleton counter into a COPY of the tree and run it.
#
# Same recipe as bench/measurements/02-07/README.md's, and the same rule: the
# repository is never modified. The counter is called at presolve's one exit,
# on whatever model presolve publishes -- the reduced one where it reduced,
# the original where it did not.
#
# It refuses to report until the calibration model passes. An instrument that
# finds nothing is worth nothing until it has been shown able to find
# something, and this one had two predecessors in this directory that produced
# plausible totals while reading the wrong field.
set -u
REPO=/mnt/c/Users/vall-/Desktop/projectes/jaos
HERE=$REPO/bench/measurements/02-10
W=${1:?usage: run-doubleton.sh <scratch-dir>}

rm -rf "$W"; mkdir -p "$W/src" "$W/out"
cp "$REPO"/src/*.c "$REPO"/src/*.h "$W/src/"
cp "$HERE/diag_doubleton.inc" "$W/src/"

python3 - "$W/src/presolve.c" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1])
t = p.read_text(encoding="utf-8")

anchor = '#include <float.h>'
assert t.count(anchor) == 1, "float.h anchor is not unique"
t = t.replace(anchor, '#include <stdio.h>\n' + anchor + '\n#include "diag_doubleton.inc"')

# The one exit every path reaches. `done:` is past every outcome assignment
# and before the frees, and p->reduced is populated on the REDUCED and SOLVED
# paths only -- on NONE the published model is the original.
call = '''done:
    diag_doubleton((p->outcome == JM_PRESOLVE_REDUCED ||
                    p->outcome == JM_PRESOLVE_SOLVED) ? &p->reduced : m);
'''
assert t.count('\ndone:\n') == 1, "done: label is not unique"
t = t.replace('\ndone:\n', '\n' + call, 1)
p.write_text(t, encoding="utf-8")
print("patched")
PY
[ $? -eq 0 ] || exit 91

FLAGS="-std=c23 -ffp-contract=off -O2 -DNDEBUG -I$REPO/include -I$W/src"
gcc-14 $FLAGS "$W"/src/*.c "$HERE/validate_doubleton.c" -o "$W/validate" -lm \
    2>"$W/build.err" || { echo "VALIDATE BUILD FAILED"; tail -20 "$W/build.err"; exit 92; }
gcc-14 $FLAGS "$W"/src/*.c "$REPO/bench/measurements/02-07/diag.c" -o "$W/run" -lm \
    2>>"$W/build.err" || { echo "RUNNER BUILD FAILED"; tail -20 "$W/build.err"; exit 92; }

echo "=== calibration ==="
"$W/validate" 2>&1 | tee "$W/out/validate.txt"
got=$(grep -o "eqrows=3 dbl=3 dblfree=1 subnz=6" "$W/out/validate.txt" | head -1)
if [ "$got" != "eqrows=3 dbl=3 dblfree=1 subnz=6" ]; then
  echo
  echo "CALIBRATION FAILED. The counter does not reproduce the hand answer."
  echo "Nothing below would mean anything, so nothing below ran."
  exit 93
fi
echo "calibration ok"

for set in "instances netlib" "instances-kennington kennington"; do
  set -- $set
  echo
  echo "=== $2 ==="
  : > "$W/out/$2.txt"
  for mps in "$REPO/bench/$1"/*.mps; do
    name=$(basename "$mps" .mps)
    line=$("$W/run" "$mps" 2>&1 | grep "^DOUBLETON " | head -1)
    echo "$name ${line:-DOUBLETON no-line}" >> "$W/out/$2.txt"
  done
  awk '{ for (i=2;i<=NF;i++) { split($i,kv,"=");
           if (kv[1]=="dbl") d+=kv[2]; else if (kv[1]=="dblfree") f+=kv[2];
           else if (kv[1]=="subnz") s+=kv[2]; else if (kv[1]=="liverows") lr+=kv[2];
           else if (kv[1]=="totalnz") tz+=kv[2] } n++ }
       END { printf "  instances                       %d\n", n;
             printf "  surviving doubleton equalities  %d\n", d;
             printf "  of those with a free endpoint   %d\n", f;
             printf "  live rows after presolve        %d\n", lr;
             printf "  share of live rows              %.2f%%\n", 100.0*d/lr;
             printf "  nonzeros one pass could remove  %d of %d  (%.2f%%)\n",
                    s, tz, 100.0*s/tz }' "$W/out/$2.txt"
  echo "  worst five:"
  sort -t= -k4 -n -r "$W/out/$2.txt" 2>/dev/null | \
    awk '{ for (i=2;i<=NF;i++) { split($i,kv,"="); if (kv[1]=="dbl") d=kv[2] }
           if (d+0>0) printf "    %-12s dbl=%d\n", $1, d }' | \
    sort -t= -k2 -n -r | head -5
done
echo
echo "raw: $W/out/"
