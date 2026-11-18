# 02-478: the same roundings on the columns' tree's node points
# (JAOS_NODECOLS=1): nearest, nearest then 1-opt, every pattern of up to 10
# fractional columns, eight random roundings, down then 1-opt. Prints the
# first node at which each reaches -4766.44, -4769.74, -4771.636 and the
# optimum -4783.733.
#
#   python3 coff.py MODEL.mps NODEDUMP DIR
#
# SPDX-License-Identifier: Apache-2.0
import sys, itertools
import numpy as np
import importlib.util
mps, nodef, sdir = sys.argv[1:4]
spec = importlib.util.spec_from_file_location('anam', sdir + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
n = len(cols)
tol = 1e-7
targets = [-4766.44, -4769.74, -4771.636, -4783.733]
rng = np.random.default_rng(7)
first = {}
fcount = []
combos = {}
def combo(F):
    if F not in combos:
        combos[F] = np.array(list(itertools.product((0.0, 1.0), repeat=F)))
    return combos[F]

def hit(name, nd, z):
    for t in targets:
        if z <= t + 1e-6:
            first.setdefault((name, t), nd)

def feasv(X):
    return (X >= -tol).all(axis=1) & ((X @ M.T) <= b + tol).all(axis=1)

def oneopt(x):
    r = M @ x
    z = c @ x
    improved = True
    while improved:
        improved = False
        for j in np.argsort(c):
            for d in (1.0, -1.0):
                if c[j] * d >= 0:
                    continue
                if x[j] + d < 0:
                    continue
                if (r + d * M[:, j] <= b + tol).all():
                    x[j] += d; r = r + d * M[:, j]; z += c[j] * d; improved = True
                    break
    return x

for line in open(nodef):
    t = line.split()
    nd = int(t[0])
    xs = np.array([float(v) for v in t[3:]])
    fl = np.floor(xs + 1e-9)
    fr = xs - fl
    F = np.where((fr > 1e-6) & (fr < 1 - 1e-6))[0]
    fcount.append(len(F))
    xr = np.round(xs)
    if feasv(xr[None, :])[0]:
        hit('nearest', nd, c @ xr)
        hit('nearest+1opt', nd, c @ oneopt(xr.copy()))
    if len(F) <= 10:
        C = combo(len(F))
        X = np.repeat(fl[None, :], len(C), axis=0)
        X[:, F] += C
        ok = feasv(X)
        if ok.any():
            z = (X[ok] @ c).min()
            hit('enum10', nd, z)
    R = np.repeat(fl[None, :], 8, axis=0) + (rng.random((8, n)) < fr[None, :])
    ok = feasv(R)
    if ok.any():
        hit('rand8', nd, (R[ok] @ c).min())
    xf = fl.copy()
    if feasv(xf[None, :])[0]:
        hit('floor+1opt', nd, c @ oneopt(xf))
fc = np.array(fcount)
print('nodes', len(fc), 'frac mean %.1f median %d, <=10 at %d' % (fc.mean(), np.median(fc), (fc <= 10).sum()))
for name in ('nearest', 'nearest+1opt', 'enum10', 'rand8', 'floor+1opt'):
    print('%-14s' % name, ' '.join('%s:%s' % (t, first.get((name, t), '-')) for t in targets))
