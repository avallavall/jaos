#!/bin/bash
# What the compensation costs in seconds, which the work counter cannot see.
#
# `jm_work_add` is unchanged, so the same units are billed for about four times
# the arithmetic per nonzero, plus two dense O(nrow) passes that bill nothing.
# CLAUDE.md asks for a time ratio exactly where the units are blind, and D171's
# first version did not have one (`numerics-reviewer`).
#
# **The noise floor here is unusually good.** The whole `ken` family and
# `pds-02` come back BIT-IDENTICAL under D171 — same iterations, same work,
# same digest — so their ratio is the added arithmetic and nothing else, and
# `ken-13` is 747 million work units of it. `pds-20` and `dfl001` moved, so
# theirs carry a trajectory change as well.
#
# The protocol is the skill's: two binaries built the same way in the same
# session, `-j 1`, three alternating rounds, minimum per instance, geometric
# mean of per-instance ratios. This host repeats to 6.27% (D93).
#
# Seconds stay here and never enter bench/results/*.txt or a baseline.
#
# **Both sides come from a git ref**, because the working tree has moved on
# since: D172 landed on top of this and changes the objective's own arithmetic.
# Timing the working tree would measure both changes at once.
#
# Usage: run-timing.sh [parent-ref] [candidate-ref]
#        defaults: 4747f29 and 39a49f6, D171 and its parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-4747f29}
cand=${2:-39a49f6}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -O3 -flto"
P="$P -march=native -mtune=native -g -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before" "$D/after"
for f in src/*.c src/*.h; do
    git show "$ref:$f"  > "$D/before/$(basename "$f")" || exit 2
    git show "$cand:$f" > "$D/after/$(basename "$f")"  || exit 2
done
gcc-14 $P "$D/before"/*.c bench/run.c -o "$D/run-parent" -lm 2>/dev/null || exit 2
gcc-14 $P "$D/after"/*.c  bench/run.c -o "$D/run-cand"   -lm 2>/dev/null || exit 2

KN="ken-07 ken-11 ken-13 pds-02 pds-20"
NL="dfl001"
for r in 1 2 3; do
    for t in parent cand; do
        "$D/run-$t" -j 1 -m bench/netlib-kennington.manifest \
            -d bench/instances-kennington $KN > "$D/$t$r.txt" 2>&1
        "$D/run-$t" -j 1 $NL >> "$D/$t$r.txt" 2>&1
    done
done

{
  echo "$ref against $cand, -j 1, minimum of 3 alternating rounds."
  echo "ken-07, ken-11, ken-13 and pds-02 are BIT-IDENTICAL under D171 and are"
  echo "the noise floor: their ratio is the added arithmetic alone. pds-20 and"
  echo "dfl001 moved and carry a trajectory change with it."
  echo
  python3 - "$D" <<'PY'
import re, sys, os, math
D = sys.argv[1]
BITID = {"ken-07", "ken-11", "ken-13", "pds-02"}
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
p, c = mins("parent"), mins("cand")
print(f"{'instance':12s} {'parent s':>10s} {'cand s':>10s} {'ratio':>9s}   ")
rs, rb = [], []
for n in sorted(p):
    if n not in c:
        continue
    r = c[n] / p[n]
    rs.append(r)
    tag = "bit-identical" if n in BITID else "moved"
    if n in BITID:
        rb.append(r)
    print(f"{n:12s} {p[n]:10.4f} {c[n]:10.4f} {r:8.4f}x   {tag}")
def g(xs):
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")
print()
print(f"geometric mean, all {len(rs)}          : {g(rs):.4f}x")
print(f"geometric mean, the {len(rb)} bit-identical: {g(rb):.4f}x"
      "   <-- the arithmetic alone")
PY
} | tee "$here/timing.txt"
