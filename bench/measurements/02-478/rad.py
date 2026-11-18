# 02-478: from each node's point rounded in the columns, the best feasible
# point within one move of one column by one (r1) and within two such moves
# (r2), against an incumbent. RED=1 reads the reduced tree's dump.
#
#   python3 rad.py MODEL.mps LATDUMP DIR NODEDUMP RED [INCUMBENT]
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
mps, lat, sdir, nodef, red = sys.argv[1:6]
red = red == '1'
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
U, V, xinc = A.read_lat(lat)
n = len(cols)
tol = 1e-7
zinc = float(sys.argv[6]) if len(sys.argv) > 6 else c @ xinc
targets = [zinc - 1e-6, -4774.64, -4783.733]
# single moves
D1 = np.vstack([np.eye(n), -np.eye(n)])
pairs = []
for i in range(2 * n):
    for j in range(i + 1, 2 * n):
        if j == i + n:
            continue
        pairs.append(D1[i] + D1[j])
D2 = np.array(pairs)
MD1 = D1 @ M.T
MD2 = D2 @ M.T
cD1 = D1 @ c
cD2 = D2 @ c
first = {}
cnt = {}
def hit(name, nd, z):
    cnt[name] = cnt.get(name, 0) + 1
    for t in targets:
        if z <= t + 1e-6:
            first.setdefault((name, t), nd)

def greedy(x, steps):
    r = M @ x
    for s in range(steps):
        viol = np.maximum(r - b, 0).sum() + np.maximum(-x, 0).sum()
        if viol <= tol:
            return x
        best = None
        for j in range(n):
            for d in (-1.0, 1.0):
                if x[j] + d < 0:
                    continue
                r2 = r + d * M[:, j]
                v2 = np.maximum(r2 - b, 0).sum()
                key = (v2, d * c[j])
                if best is None or key < best[0]:
                    best = (key, j, d)
        _, j, d = best
        x[j] += d
        r = r + d * M[:, j]
    viol = np.maximum(r - b, 0).sum()
    return x if viol <= tol else None

for line in open(nodef):
    t = line.split()
    nd = int(t[0])
    p = np.array([float(v) for v in t[3:]])
    xs = V @ p if red else p
    xr = np.round(xs)
    xr[xr < 0] = 0
    r0 = M @ xr
    z0 = c @ xr
    if (r0 <= b + tol).all() and z0 < zinc - 1e-6:
        hit('r0', nd, z0)
    ok1 = ((r0[None, :] + MD1) <= b + tol).all(axis=1) & ((xr[None, :] + D1) >= 0).all(axis=1)
    if ok1.any():
        z = (z0 + cD1[ok1]).min()
        if z < zinc - 1e-6:
            hit('r1', nd, z)
    ok2 = ((r0[None, :] + MD2) <= b + tol).all(axis=1) & ((xr[None, :] + D2) >= 0).all(axis=1)
    if ok2.any():
        z = (z0 + cD2[ok2]).min()
        if z < zinc - 1e-6:
            hit('r2', nd, z)
for name in ('r0', 'r1', 'r2'):
    print('%-4s nodes-with-improving %6d ' % (name, cnt.get(name, 0)), ' '.join('%.3f:%s' % (t, first.get((name, t), '-')) for t in targets))
