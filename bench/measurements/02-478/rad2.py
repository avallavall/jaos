# 02-478: rad.py's radius one on the columns' first nodes with a running
# incumbent, split into a rounded point already feasible and one repaired
# by a single move, and a descent of single improving moves.
#
#   python3 rad2.py MODEL.mps DIR NODEDUMP INCUMBENT NODES
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
mps, sdir, nodef, zinc0 = sys.argv[1:5]
maxn = int(sys.argv[5]) if len(sys.argv) > 5 else 10**9
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
n = len(cols)
tol = 1e-7
zinc = float(zinc0)
D1 = np.vstack([np.eye(n), -np.eye(n)])
MD1 = D1 @ M.T
cD1 = D1 @ c
log = []
first = {}
def hit(name, nd, z, extra):
    global zinc
    log.append((name, nd, round(z, 4), extra))
# simulate a running incumbent: a strategy's hit updates its own incumbent
inc = {'r0': zinc, 'rep1': zinc, 'r1': zinc, 'r1desc': zinc}
for line in open(nodef):
    t = line.split()
    nd = int(t[0])
    if nd > maxn:
        break
    xs = np.array([float(v) for v in t[3:]])
    xr = np.round(xs)
    xr[xr < 0] = 0
    r0 = M @ xr
    z0 = c @ xr
    viol = r0 - b > tol
    nv = int(viol.sum())
    f0 = nv == 0
    if f0 and z0 < inc['r0'] - 1e-6:
        inc['r0'] = z0; hit('r0', nd, z0, 'feasible')
    R = r0[None, :] + MD1
    ok1 = (R <= b + tol).all(axis=1) & ((xr[None, :] + D1) >= 0).all(axis=1)
    cand = []
    if f0:
        cand.append(z0)
    if ok1.any():
        cand.append((z0 + cD1[ok1]).min())
    if cand:
        z = min(cand)
        if z < inc['r1'] - 1e-6:
            inc['r1'] = z; hit('r1', nd, z, 'x_r feasible' if f0 else 'repair of %d rows' % nv)
        if not f0 and ok1.any():
            z = (z0 + cD1[ok1]).min()
            if z < inc['rep1'] - 1e-6:
                inc['rep1'] = z; hit('rep1', nd, z, 'repair of %d rows' % nv)
    # descent: repeat best radius-1 move from best feasible
    if cand:
        if f0:
            x = xr.copy()
        else:
            k = np.argmin(np.where(ok1, z0 + cD1, np.inf))
            x = xr + D1[k]
        r = M @ x
        while True:
            Rr = r[None, :] + MD1
            ok = (Rr <= b + tol).all(axis=1) & ((x[None, :] + D1) >= 0).all(axis=1) & (cD1 < -1e-9)
            if not ok.any():
                break
            k = np.argmin(np.where(ok, cD1, np.inf))
            x = x + D1[k]; r = r + MD1[k]
        z = c @ x
        if z < inc['r1desc'] - 1e-6:
            inc['r1desc'] = z; hit('r1desc', nd, z, '')
for name in ('r0', 'r1', 'rep1', 'r1desc'):
    print(name, [(l[1], l[2], l[3]) for l in log if l[0] == name])
