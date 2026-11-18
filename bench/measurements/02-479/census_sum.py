# SPDX-License-Identifier: Apache-2.0
# Reads census.patch's C lines: each node relaxation below the root solved
# from the same start under N5DSE 1, 2 and 4 before the tree's own solve
# (0). Prints, per instance, the iterations a node under each, the work of
# each over the tree's own, and how many nodes ended in another status or
# objective.
import math
import os
import sys


def one(path):
    tot, diff, n = {}, {}, 0
    for line in open(path):
        if not line.startswith('C '):
            continue
        parts = line.strip().split('|')
        if len(parts) < 5:
            continue
        recs = {}
        for p in parts[1:]:
            f = p.split()
            recs[int(f[0])] = (int(f[1]), int(f[2]), int(f[3]), int(f[4]), float(f[5]))
        n += 1
        b = recs[0]
        for mode, r in recs.items():
            t = tot.setdefault(mode, [0, 0])
            t[0] += r[0]
            t[1] += r[1]
            if mode != 0 and (r[2] != b[2] or
                              (r[2] == 1 and abs(r[4] - b[4]) > 1e-6 * (1 + abs(b[4])))):
                diff[mode] = diff.get(mode, 0) + 1
    return n, tot, diff


print(f"{'instance':22s} {'nodes':>7s} {'it0':>6s} {'it1':>6s} {'it2':>6s} {'it4':>6s} "
      f"{'w1/w0':>6s} {'w2/w0':>6s} {'w4/w0':>6s} other-ends(1,2,4)")
logs = {1: [], 2: [], 4: []}
for path in sys.argv[1:]:
    n, tot, diff = one(path)
    name = os.path.basename(path).split('.')[0]
    if name.startswith('cen-'):
        name = name[4:]
    if n == 0:
        print(f"{name:22s} {n:7d}")
        continue
    w0 = tot[0][1]
    g = lambda m, k: tot.get(m, [0, 0])[k]
    for m in logs:
        logs[m].append(math.log(g(m, 1) / w0))
    print(f"{name:22s} {n:7d} {g(0,0)/n:6.1f} {g(1,0)/n:6.1f} {g(2,0)/n:6.1f} {g(4,0)/n:6.1f} "
          f"{g(1,1)/w0:6.3f} {g(2,1)/w0:6.3f} {g(4,1)/w0:6.3f} "
          f"{diff.get(1,0)},{diff.get(2,0)},{diff.get(4,0)}")
for m, v in logs.items():
    print(f"N5DSE={m}: geometric mean of the work ratio over {len(v)} instances "
          f"{math.exp(sum(v) / len(v)):.3f}, above 1 on {sum(1 for x in v if x > 0)}")
