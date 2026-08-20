#!/bin/bash
# What the compensation costs in seconds, which work units cannot see.
#
# The Neumaier step is arithmetic that `jm_work_add` does not bill: the same
# number of nonzeros is charged either way, so the work counter reports the
# trajectory and says nothing about the cost of the accumulation itself. Four
# of the six instances below come back bit-identical on the gate — same
# iterations, same work, same digest — so their ratio is the arithmetic alone.
# `pilot87` and `pilotnov` moved, and their ratios carry a trajectory change
# as well as the cost.
#
# The protocol is the skill's: two binaries built the same way in the same
# session, `-j 1`, three alternating rounds, minimum per instance, geometric
# mean of per-instance ratios. This host repeats to 6.27% (D93), so anything
# inside that band is not a reading.
#
# Seconds are development numbers. They stay here and never enter
# bench/results/*.txt or a baseline.
#
# Usage: run-timing.sh [git-ref]      default: f3a7798, D168's parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-f3a7798}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -O3 -flto"
P="$P -march=native -mtune=native -g -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
git show "$ref:src/simplex.c" > "$D/before/simplex.c" || exit 2
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = simplex.c ] || cp "$f" "$D/before/$b"
done
gcc-14 $P "$D/before"/*.c bench/run.c -o "$D/run-parent" -lm || exit 2
gcc-14 $P src/*.c         bench/run.c -o "$D/run-cand"   -lm || exit 2

INST="maros-r7 dfl001 truss degen3 pilot87 pilotnov"
for r in 1 2 3; do
    "$D/run-parent" -j 1 $INST > "$D/p$r.txt" 2>&1
    "$D/run-cand"   -j 1 $INST > "$D/c$r.txt" 2>&1
done

{
  echo "parent $ref versus the compensated accumulation, -j 1, minimum of 3"
  echo "alternating rounds. maros-r7, dfl001, truss and degen3 are"
  echo "bit-identical on the gate and are the noise floor."
  echo
  python3 - "$D" <<'PY'
import re, sys, os, math
D = sys.argv[1]
def mins(pref):
    best = {}
    for r in (1, 2, 3):
        for line in open(os.path.join(D, f"{pref}{r}.txt")):
            m = re.match(r'\[\s*([0-9.]+)s\]\s+(\S+)\s', line)
            if m:
                t, n = float(m.group(1)), m.group(2)
                if n not in best or t < best[n]:
                    best[n] = t
    return best
p, c = mins("p"), mins("c")
print(f"{'instance':12s} {'parent s':>10s} {'cand s':>10s} {'ratio':>9s}")
rs = []
for n in p:
    if n in c:
        r = c[n] / p[n]
        rs.append(r)
        print(f"{n:12s} {p[n]:10.4f} {c[n]:10.4f} {r:8.4f}x")
if rs:
    g = math.exp(sum(math.log(x) for x in rs) / len(rs))
    print()
    print(f"geometric mean of per-instance ratios: {g:.4f}x over {len(rs)}")
PY
} | tee "$here/timing.txt"
