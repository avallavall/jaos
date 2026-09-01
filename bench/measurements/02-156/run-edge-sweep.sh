#!/bin/bash
# PRIMAL_EDGE_DRIFT, swept on both sides, on the forced-primal campaign.
#
# The rule is derived in docs/research/approximate-edge-pricing.md and its
# one constant is the drift ratio past which every weight is thrown away.
#
# THE COMPARISON IS PAIRED, and the first version of this script was not.
# The campaign bounds the primal at ten times the dual's work per instance,
# and this rule spends work of its own, so a setting that costs more work
# finishes a different set of instances. Comparing two geometric means over
# two different populations says nothing. Every figure below is over the
# instances that come back `ok` under BOTH the setting and Dantzig, and the
# size of that set is printed beside it.
#
# THE LOW END IS THE CONTROL. At a ratio just above 1 the weights are reset
# at the end of nearly every pivot, so they are all 1 whenever pricing reads
# them, so `gain^2 / 1` orders the candidates exactly as `gain` does and the
# rule must choose what Dantzig chooses. Paired against Dantzig it has to
# read 1.0000 on both means. If it does not, the implementation is wrong and
# no other row is worth reading.
#
# The high end is the other side: a ratio no drift can reach, so the weights
# are never thrown away.
#
# The binary's md5 is printed per setting. Editing a constant in a source
# file is tracked by make, unlike EXTRA_CFLAGS, but a sweep that measured
# one binary N times is a failure this project has had before.
#
# No `trap`: bash runs an EXIT trap inside command substitution too, which
# eats the backup and leaves the source patched (02-152).
#
# Run from anywhere; writes edge-sweep.txt beside this script. About twelve
# minutes.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-156
OUT="$HERE/edge-sweep.txt"
KEEP="$HERE/simplex.c.keep"
RUNS="$HERE/runs"

cp src/simplex.c "$KEEP" || exit 2
mkdir -p "$RUNS"
git show HEAD:bench/results/primal.txt > "$RUNS/dantzig.txt" || exit 2

one() {   # one <label> <value>
    local label="$1" val="$2"
    python3 - "$val" <<'PY'
import re, sys
val = sys.argv[1]
s = open('src/simplex.c').read()
new, n = re.subn(r'constexpr double PRIMAL_EDGE_DRIFT = [^;]+;',
                 f'constexpr double PRIMAL_EDGE_DRIFT = {val};', s, count=1)
assert n == 1, 'the constant is not where this script expects'
open('src/simplex.c', 'w').write(new)
PY
    make primal J=12 > /dev/null 2>&1
    cp bench/results/primal.txt "$RUNS/$label.txt"
    md5sum build/bench/primal | cut -c1-8 > "$RUNS/$label.md5"
}

one d1    1.0000001
one d1p5  1.5
one d2    2.0
one d4    4.0
one d16   16.0
one dinf  1e300

cp "$KEEP" src/simplex.c
rm -f "$KEEP"
git checkout -- bench/results/primal.txt
make build/bench/primal >/dev/null 2>&1 || true

python3 - "$RUNS" <<'PY' | tee "$OUT"
import math, os, re, sys

runs = sys.argv[1]

def load(path):
    """instance -> (verdict, primal_iters, primal_work)."""
    out = {}
    for line in open(path):
        f = line.split()
        if len(f) < 3 or f[1] not in ('ok', 'DISAGREE', 'overrun',
                                      'unreached', 'ERROR'):
            continue
        m = re.search(r'primal=(\d+)/(\d+)', line)
        out[f[0]] = (f[1], int(m.group(1)), int(m.group(2))) if m else (f[1], 0, 0)
    return out

base = load(os.path.join(runs, 'dantzig.txt'))
rows = [('1.0000001', 'd1'), ('1.5', 'd1p5'), ('2.0', 'd2'),
        ('4.0', 'd4'), ('16.0', 'd16'), ('1e300', 'dinf')]

def counts(d):
    c = {}
    for v, _, _ in d.values():
        c[v] = c.get(v, 0) + 1
    return c

bc = counts(base)
print(f"Dantzig, as committed: ok {bc.get('ok',0)}, "
      f"disagree {bc.get('DISAGREE',0)}, overrun {bc.get('overrun',0)}, "
      f"{len(base)} instances")
print()
print("Paired against Dantzig over the instances ok under BOTH.")
print("A ratio below 1 means the edge rule used less.")
print()
print("| DRIFT      | binary   | paired | iters  | work   | ok | ds | ov |")
print("|------------|----------|--------|--------|--------|----|----|----|")

for label, key in rows:
    d = load(os.path.join(runs, key + '.txt'))
    md5 = open(os.path.join(runs, key + '.md5')).read().strip()
    common = [k for k in base
              if base[k][0] == 'ok' and d.get(k, ('x',))[0] == 'ok']
    if common:
        li = sum(math.log((d[k][1] + 1) / (base[k][1] + 1)) for k in common)
        lw = sum(math.log(max(d[k][2], 1) / max(base[k][2], 1)) for k in common)
        it = math.exp(li / len(common))
        wk = math.exp(lw / len(common))
    else:
        it = wk = float('nan')
    c = counts(d)
    print(f"| {label:<10} | {md5:<8} | {len(common):>6} | {it:6.4f} | "
          f"{wk:6.4f} | {c.get('ok',0):>2} | {c.get('DISAGREE',0):>2} | "
          f"{c.get('overrun',0):>2} |")

print()
print("The control is the first row. Anything but 1.0000 on both means")
print("the weights are reaching pricing when they should all be 1.")
PY

echo
echo "restored: $(git status --short src/simplex.c bench/results/primal.txt | wc -l) file(s) dirty"
