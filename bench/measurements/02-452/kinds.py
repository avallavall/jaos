"""Sorts 02-452's interior.txt into kinds, a pair of orders at a time.

A pair is the two orders of one model, tNNNa.mps and tNNNb.mps. Its kind:
cone, a cone whose probe ends optimal at t of 1e-6 or less in either
order; row, a quadratic row likewise; failed, a probe ends with no answer
in either order and no piece is flat; interior, every probe ends optimal
with t above 1e-6.

Usage: python3 kinds.py interior.txt

SPDX-License-Identifier: Apache-2.0
"""
import re
import sys
from collections import defaultdict

pairs = defaultdict(list)
for line in open(sys.argv[1]):
    if not line.startswith("s"):
        continue
    seed, name, rest = line.split(None, 2)
    head, _, probes = rest.partition("|")
    key = (seed, name[:-5])
    pairs[key].append((name[-5], head.strip(), probes.split()))

kinds = defaultdict(list)
for key, sides in sorted(pairs.items()):
    cone = row = failed = False
    for _, _, probes in sides:
        for p in probes:
            what, _, res = p.partition("=")
            m = re.match(r"optimal:([-0-9.e+]+)", res)
            if m is None:
                failed = True
            elif float(m.group(1)) <= 1e-6:
                if what.startswith("K"):
                    cone = True
                else:
                    row = True
    kind = "cone" if cone else "row" if row else "failed" if failed else "interior"
    kinds[kind].append(key)
    status = " / ".join(f"{o}: {h}" for o, h, _ in sorted(sides))
    print(f"{kind:9s} {key[0]:>3s} {key[1]:8s} {status}")

print()
for kind in ("cone", "row", "failed", "interior"):
    print(f"{kind:9s} {len(kinds[kind])}")
print(f"{'all':9s} {sum(len(v) for v in kinds.values())}")
