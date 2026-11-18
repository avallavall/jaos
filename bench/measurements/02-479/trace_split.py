# SPDX-License-Identifier: Apache-2.0
# Reads trace.patch's N lines (one per node relaxation) and prints the
# averages over all nodes after the presolve trial, by whether the node is
# a child of the node solved just before it, by branch direction, and by
# how many nodes ago its parent was solved.
import collections
import sys

rows = []
for line in open(sys.argv[1]):
    if not line.startswith('N '):
        continue
    a, b = line[2:].split('|')
    rows.append(list(map(int, a.split())) + list(map(int, b.split())))
skip = int(sys.argv[2]) if len(sys.argv) > 2 else 51
rows = [r for r in rows if r[0] > skip]


def summ(name, rs):
    n = len(rs)
    if n == 0:
        print(f"{name:22s} n=     0")
        return
    f = lambda k: sum(r[k] for r in rs) / n
    print(f"{name:22s} n={n:6d} iters={f(6):6.1f} weight-restarts={f(9):5.2f} "
          f"flips={f(10):5.2f} refactors={f(8):4.2f} factor-work={f(15):7.0f} "
          f"before-run={f(14):6.0f} work={f(16):8.0f} presolved={f(17):4.2f} depth={f(4):5.1f}")


summ('all', rows)
summ('child of the last', [r for r in rows if r[5] == 1])
summ('not its child', [r for r in rows if r[5] == 0])
for c in (1, 0):
    for u in (0, 1):
        summ(f"{'child' if c else 'not child'}, {'up' if u else 'down'}",
             [r for r in rows if r[5] == c and r[21] == u])
for lo, hi in ((1, 1), (2, 2), (3, 5), (6, 20), (21, 100), (101, 1000), (1001, 10**12)):
    summ(f"parent {lo}-{hi} ago", [r for r in rows if lo <= r[22] <= hi])
h = collections.Counter(min(r[9], 10) for r in rows)
print('weight restarts a solve (10 = 10 or more):', sorted(h.items()))
h = collections.Counter(min(r[13], 10) for r in rows)
print('rows infeasible at the start:', sorted(h.items()))
