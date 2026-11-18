# 02-478: every point the reduced tree's nodes solve to (JAOS_NODEDUMP),
# mapped to the columns by x = V y, rounded six ways: y to nearest, x to
# nearest, x down, every pattern of up to 12 fractional x or y rounded down
# or up, and four random roundings of x. Counts the nodes that give a point
# better than the incumbent the basis was taken at.
#
#   python3 offline.py MODEL.mps LATDUMP NODEDUMP DIR
#
# SPDX-License-Identifier: Apache-2.0
import sys, itertools
import numpy as np
import importlib.util
spec = importlib.util.spec_from_file_location("anam", sys.argv[4] + "/anamod.py")
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
read_mps, read_lat = A.read_mps, A.read_lat

M, b, c, cols, rt = read_mps(sys.argv[1])
U, V, xinc = read_lat(sys.argv[2])
zinc = c @ xinc
tol = 1e-7
n = len(cols)
nodes = []
for line in open(sys.argv[3]):
    t = line.split()
    nodes.append((int(t[0]), int(t[1]), float(t[2]), np.array([float(v) for v in t[3:]])))
print('nodes', len(nodes), 'zinc', zinc)
rng = np.random.default_rng(1)

def feas(x):
    if (x < -tol).any():
        return False
    return (M @ x <= b + tol).all()

stats = {}
def note(name, node, x):
    if x is None:
        return
    z = c @ x
    if z < zinc - 1e-6 and feas(x):
        s = stats.setdefault(name, [0, np.inf, None, []])
        s[0] += 1
        if z < s[1] - 1e-9:
            s[1] = z
            s[2] = node
            s[3].append((node, round(z, 4)))

for (nd, dep, key, y) in nodes:
    xs = V @ y
    # y rounding
    yr = np.round(y)
    note('y-round', nd, V @ yr)
    xr = np.round(xs)
    xr[xr < 0] = 0
    note('x-round', nd, xr)
    xf = np.floor(xs + 1e-9)
    xf[xf < 0] = 0
    note('x-floor', nd, xf)
    # x floor then greedy fill
    F = [j for j in range(n) if abs(xs[j] - round(xs[j])) > 1e-6]
    if len(F) <= 12:
        best = None
        base = np.floor(xs + 1e-9)
        base[base < 0] = 0
        for bits in itertools.product((0, 1), repeat=len(F)):
            x = base.copy()
            for j, bt in zip(F, bits):
                x[j] += bt
            if c @ x < zinc - 1e-6 and feas(x):
                if best is None or c @ x < c @ best:
                    best = x
        note('x-enum12', nd, best)
    Fy = [k for k in range(n) if abs(y[k] - round(y[k])) > 1e-6]
    if len(Fy) <= 12:
        best = None
        base = np.floor(y + 1e-9)
        for bits in itertools.product((0, 1), repeat=len(Fy)):
            yy = base.copy()
            for k, bt in zip(Fy, bits):
                yy[k] += bt
            x = V @ yy
            if c @ x < zinc - 1e-6 and feas(x):
                if best is None or c @ x < c @ best:
                    best = x
        note('y-enum12', nd, best)
    stats.setdefault('_fy', []).append(len(Fy))
    stats.setdefault('_fx', []).append(len(F))
    # randomized x rounding 4 samples
    for s in range(4):
        fr = xs - np.floor(xs)
        x = np.floor(xs) + (rng.random(n) < fr)
        x[x < 0] = 0
        note('x-rand4', nd, x)

fy = np.array(stats.pop('_fy')); fx = np.array(stats.pop('_fx'))
print('frac y: mean %.1f, <=12 at %d nodes; frac x: mean %.1f, <=12 at %d' % (fy.mean(), (fy <= 12).sum(), fx.mean(), (fx <= 12).sum()))
for k, s in stats.items():
    print(k, 'improving nodes', s[0], 'best', s[1], 'at node', s[2], 'path', s[3][:8])
