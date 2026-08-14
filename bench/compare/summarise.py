#!/usr/bin/env python3
"""Recompute a comparison rung's summary from its record, independently.

Two reasons this exists rather than trusting the harness's printed block.
The printed block is console output and this session already saw one line of
it overwritten by a linker warning, so "the clp summary is missing" and "the
clp summary scrolled into another line" look identical. And a second
implementation of the same arithmetic is the cheapest check there is that the
first one is right.

Same rules as run-compare.sh: a ratio counts only when BOTH solvers verified
their answer and BOTH are at or above the 0.05 s floor; geometric mean of
per-instance ratios, never a ratio of totals.
"""
import math
import sys
from pathlib import Path

FLOOR = 0.05


def read(path):
    rows = {}
    for line in Path(path).read_text().split("\n"):
        f = line.split("\t")
        if len(f) < 7 or line.startswith("#"):
            continue
        try:
            secs = float(f[5].rstrip(","))
            iters = int(f[4])
        except ValueError:
            continue
        rows[(f[0], f[1])] = (f[2], secs, iters)
    return rows


rec = read(sys.argv[1])
insts = sorted({i for i, s in rec})
solvers = [s for s in ("highs", "soplex", "clp") if any(s == x for _, x in rec)]

print(f"# {sys.argv[1]}")
for s in solvers:
    pairs = []
    for i in insts:
        j, c = rec.get((i, "jaos")), rec.get((i, s))
        if not j or not c:
            continue
        if not j[0].endswith("/ok") or not c[0].endswith("/ok"):
            continue
        if j[1] < FLOOR or c[1] < FLOOR:
            continue
        # c[2] == 0 happens: Clp reports zero iterations on instances it
        # finishes without a simplex step. Kept in the time row, dropped from
        # the iteration rows, counted and printed.
        pairs.append((i, j[1] / c[1], (j[2] / c[2]) if c[2] else None))
    if not pairs:
        print(f"\nvs {s}: nothing above the floor")
        continue
    withit = [p for p in pairs if p[2] is not None]
    gt = math.exp(sum(math.log(r) for _, r, _ in pairs) / len(pairs))
    faster = sum(1 for _, r, _ in pairs if r < 1)
    worst = max(pairs, key=lambda p: p[1])
    best = min(pairs, key=lambda p: p[1])
    print(f"\nvs {s} over {len(pairs)} instances above the floor:")
    print(f"  time per solve      {gt:.2f}x")
    if withit:
        gi = math.exp(sum(math.log(p[2]) for p in withit) / len(withit))
        gti = math.exp(sum(math.log(p[1]) for p in withit) / len(withit))
        print(f"  iterations          {gi:.2f}x")
        print(f"  time per iteration  {gti/gi:.2f}x")
    dropped = len(pairs) - len(withit)
    if dropped:
        names = ", ".join(p[0] for p in pairs if p[2] is None)
        print(f"  {dropped} of the {len(pairs)} reported zero iterations and are "
              f"left out of the two rows above: {names}")
    print(f"  JAOS faster on {faster} of {len(pairs)};  "
          f"worst {worst[0]} {worst[1]:.1f}x;  best {best[0]} {best[1]:.2f}x")

# Answers the harness cannot count for you: who disagreed with the reference.
print("\n# instances where a solver's answer was rejected")
for i in insts:
    bad = [s for s in ("jaos",) + tuple(solvers)
           if (i, s) in rec and not rec[(i, s)][0].endswith("/ok")]
    if bad:
        print(f"  {i:12} {', '.join(bad)}")
