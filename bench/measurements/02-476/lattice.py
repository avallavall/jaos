# 02-476: the lattice widths of gen-ip002's and gen-ip054's improving region,
# from outside the tool. The region is the LP relaxation with the objective
# cut at the optimum, {Ax <= b, x >= 0, cx <= z*}. Its analytic center gives
# the Dikin ellipsoid H; LLL (delta 0.99) on the lattice Z^n under the norm
# of H^-1 gives integer directions d, and two LPs a direction give its width,
# max dx - min dx over the region, beside the unit directions' widths. It
# writes the model in the reduced coordinates y = U^T x as an MPS. Needs
# numpy and pyscipopt; SCIP solves the LPs.
#
#   python3 lattice.py MODEL.mps OPTIMUM OUT.mps [cut|nocut]
#
# SPDX-License-Identifier: Apache-2.0
import sys
from fractions import Fraction

import numpy as np
from pyscipopt import Model, quicksum


def read_mps(path):
    rows, kind, cols, cost, lo, up, integer, rhs = [], {}, {}, {}, {}, {}, {}, {}
    sense = 'min'
    sec, inint = None, False
    objname = None
    for line in open(path):
        t = line.split()
        if not t or line.startswith('*'):
            continue
        if not line.startswith(' '):
            sec = t[0]
            continue
        if sec == 'OBJSENSE':
            sense = t[0].lower()
        elif sec == 'ROWS':
            if t[0] == 'N':
                objname = t[1]
            else:
                rows.append(t[1])
                kind[t[1]] = t[0]
        elif sec == 'COLUMNS':
            if len(t) >= 3 and t[1] == "'MARKER'":
                inint = t[2] == "'INTORG'"
                continue
            c = t[0]
            if c not in cols:
                cols[c] = {}
                cost[c] = 0.0
                lo[c], up[c] = 0.0, float('inf')
                integer[c] = inint
            for k in range(1, len(t), 2):
                if t[k] == objname:
                    cost[c] = float(t[k + 1])
                else:
                    cols[c][t[k]] = float(t[k + 1])
        elif sec == 'RHS':
            for k in range(1, len(t), 2):
                rhs[t[k]] = float(t[k + 1])
        elif sec == 'BOUNDS':
            c, v = t[2], float(t[3]) if len(t) > 3 else None
            if t[0] in ('UP', 'UI'):
                up[c] = v
            elif t[0] in ('LO', 'LI'):
                lo[c] = v
            elif t[0] == 'FX':
                lo[c] = up[c] = v
    return dict(rows=rows, kind=kind, cols=cols, cost=cost, lo=lo, up=up,
                integer=integer, rhs=rhs, sense=sense)


def dense(m):
    cn = list(m['cols'])
    rn = m['rows']
    ri = {r: i for i, r in enumerate(rn)}
    A = np.zeros((len(rn), len(cn)))
    for j, c in enumerate(cn):
        for r, a in m['cols'][c].items():
            A[ri[r], j] = a
    b = np.array([m['rhs'].get(r, 0.0) for r in rn])
    for i, r in enumerate(rn):
        assert m['kind'][r] == 'L'
    c = np.array([m['cost'][x] for x in cn])
    if m['sense'] == 'max':
        c = -c
    for x in cn:
        assert m['lo'][x] == 0.0 and m['up'][x] == float('inf') and m['integer'][x]
    return A, b, c, cn


def polytope(A, b, c, zcut):
    n = A.shape[1]
    G = [A, -np.eye(n)]
    h = [b, np.zeros(n)]
    if zcut is not None:
        G.append(c.reshape(1, -1))
        h.append(np.array([zcut]))
    return np.vstack(G), np.concatenate(h)


def lp(G, h, obj):
    m = Model()
    m.hideOutput()
    n = G.shape[1]
    x = [m.addVar(lb=None, ub=None) for _ in range(n)]
    for i in range(G.shape[0]):
        m.addCons(quicksum(G[i, j] * x[j] for j in range(n) if G[i, j] != 0.0) <= h[i])
    m.setObjective(quicksum(obj[j] * x[j] for j in range(n) if obj[j] != 0.0))
    m.optimize()
    if m.getStatus() != 'optimal':
        return None, None
    return m.getObjVal(), np.array([m.getVal(v) for v in x])


def cheb(G, h):
    m = Model()
    m.hideOutput()
    n = G.shape[1]
    x = [m.addVar(lb=None, ub=None) for _ in range(n)]
    t = m.addVar(lb=0.0, ub=1e3)
    for i in range(G.shape[0]):
        nr = np.linalg.norm(G[i])
        m.addCons(quicksum(G[i, j] * x[j] for j in range(n) if G[i, j] != 0.0) + nr * t <= h[i])
    m.setObjective(-t)
    m.optimize()
    return np.array([m.getVal(v) for v in x]), m.getVal(t)


def analytic_center(G, h, x):
    for it in range(200):
        s = h - G @ x
        assert np.all(s > 0)
        g = G.T @ (1.0 / s)
        H = G.T @ (G / (s * s)[:, None])
        dx = -np.linalg.solve(H, g)
        lam = np.sqrt(-g @ dx)
        if lam < 1e-10:
            break
        t = 1.0
        while True:
            s2 = h - G @ (x + t * dx)
            if np.all(s2 > 0):
                f0 = -np.sum(np.log(s))
                f1 = -np.sum(np.log(s2))
                if f1 <= f0 + 0.25 * t * (g @ dx):
                    break
            t *= 0.5
        x = x + t * dx
    s = h - G @ x
    H = G.T @ (G / (s * s)[:, None])
    return x, H


def lll_int(R, delta=0.99):
    n = R.shape[1]
    B = R.copy()
    U = np.eye(n, dtype=object)
    for i in range(n):
        for j in range(n):
            U[i, j] = int(i == j)

    def gs(B):
        Q = np.zeros_like(B)
        mu = np.zeros((n, n))
        for i in range(n):
            v = B[:, i].copy()
            for j in range(i):
                mu[i, j] = B[:, i].dot(Q[:, j]) / Q[:, j].dot(Q[:, j])
                v -= mu[i, j] * Q[:, j]
            Q[:, i] = v
        return Q, mu

    Q, mu = gs(B)
    k = 1
    it = 0
    while k < n and it < 100000:
        it += 1
        for j in range(k - 1, -1, -1):
            q = int(round(mu[k, j]))
            if q != 0:
                B[:, k] -= q * B[:, j]
                U[:, k] = U[:, k] - q * U[:, j]
                Q, mu = gs(B)
        if Q[:, k].dot(Q[:, k]) >= (delta - mu[k, k - 1] ** 2) * Q[:, k - 1].dot(Q[:, k - 1]):
            k += 1
        else:
            B[:, [k, k - 1]] = B[:, [k - 1, k]]
            U[:, [k, k - 1]] = U[:, [k - 1, k]]
            Q, mu = gs(B)
            k = max(k - 1, 1)
    return B, U


def int_inverse(U):
    n = U.shape[0]
    M = [[Fraction(int(U[i, j])) for j in range(n)] + [Fraction(int(i == j)) for j in range(n)] for i in range(n)]
    for col in range(n):
        p = next(r for r in range(col, n) if M[r][col] != 0)
        M[col], M[p] = M[p], M[col]
        pv = M[col][col]
        M[col] = [v / pv for v in M[col]]
        for r in range(n):
            if r != col and M[r][col] != 0:
                f = M[r][col]
                M[r] = [a - f * b for a, b in zip(M[r], M[col])]
    V = np.zeros((n, n), dtype=object)
    for i in range(n):
        for j in range(n):
            v = M[i][n + j]
            assert v.denominator == 1
            V[i, j] = int(v)
    return V


def width(G, h, d):
    lo, _ = lp(G, h, d)
    hi, _ = lp(G, h, -d)
    return lo, -hi


def write_ref(path, A, b, c, V, sense_max):
    m, n = A.shape
    AV = A @ V.astype(float)
    cV = c @ V.astype(float)
    with open(path, 'w') as f:
        f.write('NAME ref\nROWS\n N OBJ\n')
        for i in range(m):
            f.write(' L R%d\n' % i)
        for j in range(n):
            f.write(' G X%d\n' % j)
        f.write('COLUMNS\n')
        f.write("    M1 'MARKER' 'INTORG'\n")
        for k in range(n):
            f.write('    Y%d OBJ %.17g\n' % (k, cV[k]))
            for i in range(m):
                if AV[i, k] != 0.0:
                    f.write('    Y%d R%d %.17g\n' % (k, i, AV[i, k]))
            for j in range(n):
                if V[j, k] != 0:
                    f.write('    Y%d X%d %d\n' % (k, j, int(V[j, k])))
        f.write("    M2 'MARKER' 'INTEND'\n")
        f.write('RHS\n')
        for i in range(m):
            f.write('    rhs R%d %.17g\n' % (i, b[i]))
        f.write('BOUNDS\n')
        for k in range(n):
            f.write(' FR bnd Y%d\n' % k)
        f.write('ENDATA\n')


def main():
    path, zopt, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    mode = sys.argv[4] if len(sys.argv) > 4 else 'cut'
    m = read_mps(path)
    A, b, c, cn = dense(m)
    n = A.shape[1]
    zc = zopt if m['sense'] == 'min' else -zopt
    zc = zc + 1e-6 * (1 + abs(zc))
    G, h = polytope(A, b, c, zc if mode == 'cut' else None)
    x0, t = cheb(G, h)
    print('chebyshev radius', t)
    xc, H = analytic_center(G, h, x0)
    Ginv = np.linalg.inv(H)
    Ginv = 0.5 * (Ginv + Ginv.T)
    R = np.linalg.cholesky(Ginv).T
    B, U = lll_int(R)
    V = int_inverse(U.T)
    chk = (U.T.dot(V))
    assert all(chk[i, j] == int(i == j) for i in range(n) for j in range(n))
    Gc, hc = polytope(A, b, c, zc)
    wu = []
    for j in range(n):
        e = np.zeros(n)
        e[j] = 1.0
        lo, hi = width(Gc, hc, e)
        wu.append(hi - lo)
    wr = []
    for k in range(n):
        d = np.array([float(U[i, k]) for i in range(n)])
        lo, hi = width(Gc, hc, d)
        wr.append(hi - lo)
    print('unit widths    ', ' '.join('%.2f' % w for w in sorted(wu)))
    print('reduced widths ', ' '.join('%.2f' % w for w in sorted(wr)))
    print('unit sum log2(w+1) %.1f, reduced %.1f' % (sum(np.log2(np.array(wu) + 1)), sum(np.log2(np.array(wr) + 1))))
    print('max |V| %d, max |U| %d' % (max(abs(int(v)) for v in V.flat), max(abs(int(v)) for v in U.flat)))
    write_ref(out, A, b, c, V, m['sense'] == 'max')
    print('wrote', out)


main()
