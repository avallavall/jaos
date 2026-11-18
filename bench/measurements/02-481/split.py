# SPDX-License-Identifier: Apache-2.0
# Reads trace.patch's N and F lines (one pair per node relaxation) and
# prints the averages over the nodes after the presolve trial: by branch
# direction and whether the node follows its parent, by the branched
# column's step to its new bound, by the columns the node's bounds fix at
# 1, and how the solves of the far branches end.
#   split.py TRACE [FIRST]
import sys

rows = []
fl = {}
for line in open(sys.argv[1]):
    if line.startswith('N '):
        a, b = line[2:].split('|')
        rows.append(list(map(int, a.split())) + list(map(int, b.split())))
    elif line.startswith('F '):
        t = line.split()
        fl[int(t[1])] = (float(t[2]), int(t[3]), int(t[4]), int(t[5]),
                         int(t[6]), float(t[7]))
skip = int(sys.argv[2]) if len(sys.argv) > 2 else 51
rows = [r + list(fl.get(r[0], (-1, 0, 0, 0, 0, 0))) for r in rows if r[0] > skip]
total = sum(r[16] for r in rows)
# 0 node, 4 depth, 5 child of the node before, 6 iterations, 8 refactors,
# 10 bound flips, 15 factor work, 16 work, 19 cutoff hit, 20 status,
# 21 up, 23 step, 24 columns at 1, 25 columns at 0, 26 rows and 27 columns
# after presolve, 28 objective


def summ(name, rs):
    n = len(rs)
    if n == 0:
        print(f"{name:26s} n=     0")
        return
    f = lambda k: sum(r[k] for r in rs) / n
    w = sum(r[16] for r in rs)
    print(f"{name:26s} n={n:6d} iters={f(6):6.1f} flips={f(10):5.2f} "
          f"refactors={f(8):4.2f} factor={f(15):6.0f} work={f(16):7.0f} "
          f"share={w / total:5.3f} rows={f(26):5.1f} cols={f(27):5.1f} "
          f"ones={f(24):4.1f} zeros={f(25):4.1f} obj={f(28):6.3f} depth={f(4):5.1f}")


print(f"nodes {len(rows)}, work {total}, work a node {total / max(1, len(rows)):.0f}")
summ('all', rows)
for c in (1, 0):
    for u in (0, 1):
        summ(f"{'child' if c else 'not child'}, {'up' if u else 'down'}",
             [r for r in rows if r[5] == c and r[21] == u])
for u in (0, 1):
    for lo, hi in ((0, 0.2), (0.2, 0.4), (0.4, 0.6), (0.6, 0.8), (0.8, 1.01)):
        summ(f"{'up' if u else 'down'}, step {lo:.1f}-{hi:.1f}",
             [r for r in rows if r[21] == u and lo <= r[23] < hi])
for lo, hi in ((0, 2), (3, 4), (5, 6), (7, 8), (9, 99)):
    summ(f"ones {lo}-{hi}", [r for r in rows if lo <= r[24] <= hi])
for u in (0, 1):
    for lo, hi in ((0, 0.6), (0.6, 1.01)):
        sel = [r for r in rows if r[21] == u and lo <= r[23] < hi]
        summ(f"{'up' if u else 'down'} {lo}-{hi}: all", sel)
        summ("  stopped at the cutoff", [r for r in sel if r[19] == 1])
        summ("  other end", [r for r in sel if r[19] == 0 and r[20] != 1])
        summ("  optimal, at 15", [r for r in sel if r[19] == 0 and r[20] == 1 and r[28] >= 14.9999])
        summ("  optimal, under 15", [r for r in sel if r[19] == 0 and r[20] == 1 and r[28] < 14.9999])
