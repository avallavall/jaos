# 02-478: at every EVERY-th node of the reduced tree's dump, four searches
# from the mapped point over the columns, the incumbent's objective as a
# cutoff: a dive that fixes the column nearest an integer and solves again,
# the same with the point's zero columns fixed, a RENS sub-MIP over the
# point's floor and ceiling, and a sub-MIP of 500 nodes with the zero columns
# fixed. SCIP solves the LPs and sub-MIPs.
#
#   python3 dives.py MODEL.mps LATDUMP NODEDUMP DIR EVERY
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
from pyscipopt import Model, quicksum
mps, lat, nodef, sdir = sys.argv[1:5]
every = int(sys.argv[5]) if len(sys.argv) > 5 else 200
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
U, V, xinc = A.read_lat(lat)
n = len(cols)
zinc = c @ xinc
tol = 1e-7

def feas(x):
    return (x >= -tol).all() and (M @ x <= b + tol).all()

def build(vtype='C'):
    m = Model(); m.hideOutput()
    x = [m.addVar(lb=0, vtype=vtype) for j in range(n)]
    for i in range(M.shape[0]):
        m.addCons(quicksum(M[i, j] * x[j] for j in range(n) if M[i, j] != 0) <= b[i])
    m.addCons(quicksum(c[j] * x[j] for j in range(n)) <= zinc - 1e-6)
    m.setObjective(quicksum(c[j] * x[j] for j in range(n)))
    return m, x

def dive(xs, fixzeros):
    lo = np.zeros(n); hi = np.full(n, 1e20)
    if fixzeros:
        for j in range(n):
            if abs(xs[j]) < 1e-9:
                hi[j] = 0
    cur = xs.copy()
    for it in range(60):
        fr = [j for j in range(n) if abs(cur[j] - round(cur[j])) > 1e-6 and lo[j] < hi[j]]
        if not fr:
            x = np.round(cur)
            return x if feas(x) and c @ x < zinc - 1e-6 else None
        j = min(fr, key=lambda j: abs(cur[j] - round(cur[j])))
        v = round(cur[j]); lo[j] = hi[j] = v
        m, x = build()
        for k in range(n):
            m.chgVarLb(x[k], lo[k]); m.chgVarUb(x[k], hi[k] if hi[k] < 1e19 else m.infinity())
        m.optimize()
        if m.getStatus() != 'optimal':
            return None
        cur = np.array([m.getVal(v) for v in x])
    return None

def rens(xs, nodelim=500):
    m, x = build('I')
    for j in range(n):
        m.chgVarLb(x[j], np.floor(xs[j] + 1e-9)); m.chgVarUb(x[j], np.ceil(xs[j] - 1e-9))
    m.setParam('limits/nodes', nodelim)
    m.optimize()
    if m.getNSols() > 0:
        return np.array([m.getVal(v) for v in x])
    return None

def zerofix(xs, nodelim=500):
    m, x = build('I')
    for j in range(n):
        if abs(xs[j]) < 1e-9:
            m.chgVarUb(x[j], 0)
    m.setParam('limits/nodes', nodelim)
    m.optimize()
    if m.getNSols() > 0:
        return np.array([m.getVal(v) for v in x]), m.getNNodes()
    return None, m.getNNodes()

res = {'dive': [], 'divez': [], 'rens': [], 'zerofix': []}
k = 0
for line in open(nodef):
    k += 1
    if k % every:
        continue
    t = line.split()
    y = np.array([float(v) for v in t[3:]])
    xs = V @ y
    nz0 = int((np.abs(xs) < 1e-9).sum())
    out = []
    for name, f in (('dive', lambda: dive(xs, False)), ('divez', lambda: dive(xs, True)), ('rens', lambda: rens(xs))):
        p = f()
        z = c @ p if p is not None else None
        if p is not None:
            res[name].append(z)
        out.append('%s=%s' % (name, 'none' if z is None else '%.3f' % z))
    p, nn = zerofix(xs)
    if p is not None:
        res['zerofix'].append(c @ p)
    out.append('zerofix=%s(%d nodes)' % ('none' if p is None else '%.3f' % (c @ p), nn))
    print(t[0], 'depth', t[1], 'zeros', nz0, ' '.join(out), flush=True)
for kk, v in res.items():
    print(kk, len(v), 'hits; best', min(v) if v else None)
