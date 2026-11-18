# 02-478: a depth-first search over the region the incumbent leaves to
# improve on, in the reduced basis (y) or the columns (x): at each node the
# Chebyshev center is rounded, then the coordinate nearest an integer is
# fixed to its nearest and next integers. SCIP solves the LPs.
#
#   python3 center.py MODEL.mps LATDUMP DIR LPS y|x
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
import importlib.util
from pyscipopt import Model, quicksum
mps, lat, sdir = sys.argv[1:4]
budget = int(sys.argv[4]) if len(sys.argv) > 4 else 3000
space = sys.argv[5] if len(sys.argv) > 5 else 'y'
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
U, V, xinc = A.read_lat(lat)
n = len(cols)
zinc = c @ xinc if len(sys.argv) <= 6 else float(sys.argv[6])
if space == 'x':
    V = np.eye(n)
G = np.vstack([M @ V, -V, (c @ V)[None, :]])
h = np.concatenate([b, np.zeros(n), [zinc - 1e-6]])
norms = np.linalg.norm(G, axis=1)
lps = 0

def center(fix):
    global lps
    lps += 1
    m = Model(); m.hideOutput()
    y = [m.addVar(lb=-m.infinity(), ub=m.infinity()) for k in range(n)]
    r = m.addVar(lb=-1e3, ub=1e3)
    for i in range(G.shape[0]):
        m.addCons(quicksum(G[i, k] * y[k] for k in range(n) if G[i, k] != 0) + norms[i] * r <= h[i])
    for k, v in fix.items():
        m.chgVarLb(y[k], v); m.chgVarUb(y[k], v)
    m.setObjective(-r)
    m.optimize()
    if m.getStatus() != 'optimal':
        return None, None
    return np.array([m.getVal(v) for v in y]), m.getVal(r)

found = []
def dfs(fix, depth):
    if lps >= budget:
        return
    yc, r = center(fix)
    if yc is None or r < -1e-9:
        return
    free = [k for k in range(n) if k not in fix]
    if not free:
        x = V @ yc
        x = np.round(x)
        if (x >= -1e-7).all() and (M @ x <= b + 1e-7).all() and c @ x < zinc - 1e-6:
            found.append((lps, c @ x))
            print('found', c @ x, 'after', lps, 'LPs', flush=True)
        return
    # try rounding the center directly
    yr = np.round(yc)
    x = V @ yr
    if (x >= -1e-7).all() and (M @ x <= b + 1e-7).all() and c @ x < zinc - 1e-6:
        found.append((lps, c @ x))
        print('found by rounding the center', c @ x, 'after', lps, 'LPs depth', depth, flush=True)
        return
    k = min(free, key=lambda k: abs(yc[k] - round(yc[k])))
    v0 = round(yc[k])
    order = [v0, v0 + (1 if yc[k] > v0 else -1)]
    for v in order:
        f2 = dict(fix); f2[k] = v
        dfs(f2, depth + 1)
        if found:
            return

dfs({}, 0)
print('space', space, 'zinc', zinc, 'LPs', lps, 'found', found)
