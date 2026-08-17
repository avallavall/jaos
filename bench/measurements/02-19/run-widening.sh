#!/bin/bash
# S2 (D112): the distribution of row widenings the cost-0 bounded singleton
# column family produces across the standard set. A patched copy prints one
# line per firing; the repository is not modified.
#
# Usage (inside WSL, from the repository root):
#   bash bench/measurements/02-19/run-widening.sh [COPYDIR] [WORKDIR]
set -u
here=$(cd "$(dirname "$0")" && pwd)
MAIN=$(cd "$here/../../.." && pwd)
COPY=${1:-/tmp/jaos-scw}
SCR=${2:-/tmp/jaos-scw-work}
mkdir -p "$SCR/fire"

rm -rf "$COPY"; mkdir -p "$COPY/bench" "$COPY/build/diag"
cp -r "$MAIN/src" "$MAIN/include" "$COPY/"
cp "$MAIN/bench/run.c" "$MAIN/bench/netlib.manifest" "$COPY/bench/"

python3 - "$COPY/src/presolve.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """                    const double c1 = a * cur_cl[j], c2 = a * cur_cu[j];
                    const double cmin = c1 < c2 ? c1 : c2;
                    const double cmax = c1 > c2 ? c1 : c2;"""
new = old + """
                    fprintf(stderr, "SCW rl=%.17g ru=%.17g cmin=%.17g cmax=%.17g\\n",
                            cur_rl[i], cur_ru[i], cmin, cmax);"""
if s.count(old) != 1:
    sys.exit("anchor found %d times, want 1" % s.count(old))
s = "#include <stdio.h>\n" + s.replace(old, new)
open(p, "w").write(s)
print("patched")
EOF
[ $? -eq 0 ] || exit 2

cd "$COPY" || exit 9
gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
    src/*.c bench/run.c -o build/diag/run -lm || { echo "BUILD FAILED"; exit 2; }

# Calibration: grow22 must report exactly 20 firings (02-11's committed
# count), and its iters/work must match the committed record.
./build/diag/run -j 1 -d "$MAIN/bench/instances" grow22 \
    2> "$SCR/fire/grow22.txt" | grep -o 'iters=[0-9]* work=[0-9]*' | head -1
# The runner solves every instance twice (the determinism check), and each
# solve presolves, so the trace holds two passes. They must be identical,
# and one pass of grow22 must be the committed 20.
n=$(grep -c '^SCW' "$SCR/fire/grow22.txt")
if [ "$n" != "40" ]; then
    echo "CALIBRATION FAILED: grow22 firing lines=$n, want 40 (20 per solve)"
    exit 1
fi
echo "calibration ok: grow22 fires 20 per solve, twice"

for f in "$MAIN"/bench/instances/*.mps; do
    inst=$(basename "$f" .mps)
    [ "$inst" = grow22 ] && continue
    ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$inst" \
        2> "$SCR/fire/$inst.txt" > /dev/null
done
echo "all instances done"

python3 - "$SCR/fire" <<'EOF'
import glob, os, sys, math
rows = []
for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.txt"))):
    inst = os.path.basename(path)[:-4]
    lines = [l for l in open(path) if l.startswith("SCW ")]
    # two identical passes per instance: the runner's determinism check
    # solves twice, and both presolves must fire identically
    if len(lines) % 2 != 0 or lines[: len(lines) // 2] != lines[len(lines) // 2 :]:
        sys.exit("%s: the two presolve passes differ, instrument invalid" % inst)
    for line in lines[: len(lines) // 2]:
        d = dict(kv.split("=") for kv in line.split()[1:])
        rl, ru = float(d["rl"]), float(d["ru"])
        add = float(d["cmax"]) - float(d["cmin"])
        w0 = ru - rl if (math.isfinite(rl) and math.isfinite(ru)) else float("inf")
        scale = max(1.0, abs(rl) if math.isfinite(rl) else 0.0,
                         abs(ru) if math.isfinite(ru) else 0.0)
        rows.append((inst, w0, add, add / scale))
insts = {}
for inst, w0, add, rel in rows:
    e = insts.setdefault(inst, [0, 0, 0.0])
    e[0] += 1
    if w0 == 0.0:
        e[1] += 1
    e[2] = max(e[2], rel)
print("%-12s %8s %8s %12s" % ("instance", "firings", "on_eq", "max_rel_widen"))
for inst in sorted(insts):
    n, eq, mx = insts[inst]
    print("%-12s %8d %8d %12.4g" % (inst, n, eq, mx))
tot = len(rows); toteq = sum(1 for r in rows if r[1] == 0.0)
big = sum(1 for r in rows if r[3] > 1.0)
huge = sum(1 for r in rows if r[3] > 100.0)
print("TOTAL firings=%d on_equality_rows=%d rel>1=%d rel>100=%d instances=%d"
      % (tot, toteq, big, huge, len(insts)))
EOF
echo "S2_WIDENING_DONE"
