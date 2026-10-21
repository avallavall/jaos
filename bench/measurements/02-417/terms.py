# 02-417: reads one of 02-365's circulations (an MPS file with a diagonal
# QUADOBJ) and the point dump.py prints, and lists the four columns with
# the largest reduced cost: the column's value, bounds and slacks, its
# cost, its curvature term, its row term and the reduced cost.
#
#   python3 terms.py MODEL.mps DUMP.txt
#
# SPDX-License-Identifier: Apache-2.0
import sys

mps, pts = sys.argv[1], sys.argv[2]
cost, lo, up, A, Q, rows = {}, {}, {}, {}, {}, []
sec = None
for line in open(mps):
    if not line.strip():
        continue
    if not line.startswith(' '):
        sec = line.split()[0]
        continue
    f = line.split()
    if sec == 'ROWS':
        if f[0] != 'N':
            rows.append(f[1])
    elif sec == 'COLUMNS':
        c = f[0]
        for k in range(1, len(f), 2):
            if f[k] == 'obj':
                cost[c] = float(f[k + 1])
            else:
                A.setdefault(c, []).append((f[k], float(f[k + 1])))
    elif sec == 'BOUNDS':
        kind, c = f[0], f[2]
        v = float(f[3]) if len(f) > 3 else None
        if kind == 'UP':
            up[c] = v
        elif kind == 'LO':
            lo[c] = v
        elif kind == 'MI':
            lo[c] = float('-inf')
        elif kind == 'FR':
            lo[c] = float('-inf'); up[c] = float('inf')
        elif kind == 'FX':
            lo[c] = up[c] = v
    elif sec in ('QUADOBJ', 'QMATRIX'):
        Q[(f[0], f[1])] = float(f[2])
cols = sorted(set(cost) | set(A), key=lambda c: int(c[1:]))
x, y = {}, {}
for line in open(pts):
    if not line.startswith('PT '):
        continue
    _, k, i, v = line.split()
    (x if k == 'x' else y)[int(i)] = float(v)
xv = {c: x[k] for k, c in enumerate(cols)}
yv = {r: y[k] for k, r in enumerate(rows)}
out = []
for k, c in enumerate(cols):
    qx = 0.0
    for (a, b), v in Q.items():
        if a == c:
            qx += v * xv[b]
        elif b == c and sec == 'QUADOBJ':
            qx += v * xv[a]
    g = cost.get(c, 0.0) + qx
    ay = sum(v * yv[r] for r, v in A.get(c, []))
    d = g - ay
    l, u = lo.get(c, 0.0), up.get(c, float('inf'))
    out.append((abs(d), c, xv[c], l, u, cost.get(c, 0.0), qx, ay, d))
out.sort(reverse=True)
for t in out[:4]:
    _, c, xx, l, u, cc, qx, ay, d = t
    print('%s x=%.6g lo=%.6g up=%.6g slack_lo=%.3g slack_up=%.3g c=%.4g Qx=%.4g Ay=%.4g d=%.4g' %
          (c, xx, l, u, xx - l, u - xx, cc, qx, ay, d))
