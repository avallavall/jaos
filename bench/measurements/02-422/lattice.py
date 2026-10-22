# 02-422: a first point by lattice rounding, tried from outside the tool.
# For each model of 02-394 that JAOS left at 2e9 work units, solve the LP
# relaxation, write the integer solutions of the equality rows whose
# columns are all integer as x0 + L z (column operations to a Hermite
# form), reduce L by LLL, round the relaxation's point to the nearest
# lattice point (Babai), fix the integer columns there and solve the LP
# over the continuous ones; when that fails, step along lattice vectors
# and try again, up to 60 LPs a model.
#
#   python3 lattice.py JAOS WORKDIR LIST
#
# LIST is 02-394's dive.sh list; the models are read.sh's, in ~/mip394.
#
# SPDX-License-Identifier: Apache-2.0
import os
import subprocess
import sys

import numpy as np

JAOS = sys.argv[1]
WORK = sys.argv[2]


def read_mps(path):
    rows, kind, cols, cost, lo, up, integer, rhs = [], {}, {}, {}, {}, {}, {}, {}
    sense = 'min'
    sec, inint = None, False
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if not line.startswith(' '):
            sec = t[0]
            continue
        if sec == 'OBJSENSE':
            sense = t[0].lower()
        elif sec == 'ROWS':
            if t[0] != 'N':
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
                if t[k] == 'obj':
                    cost[c] = float(t[k + 1])
                else:
                    cols[c][t[k]] = float(t[k + 1])
        elif sec == 'RHS':
            for k in range(1, len(t), 2):
                rhs[t[k]] = float(t[k + 1])
        elif sec == 'BOUNDS':
            c, v = t[2], float(t[3]) if len(t) > 3 else None
            if t[0] == 'UP':
                up[c] = v
            elif t[0] == 'LO':
                lo[c] = v
            elif t[0] == 'FX':
                lo[c] = up[c] = v
    return dict(rows=rows, kind=kind, cols=cols, cost=cost, lo=lo, up=up,
                integer=integer, rhs=rhs, sense=sense)


def write_mps(m, path, fix=None, relax=False):
    with open(path, 'w') as f:
        f.write('NAME p\n')
        if m['sense'] == 'max':
            f.write('OBJSENSE\n    MAX\n')
        f.write('ROWS\n N obj\n')
        for r in m['rows']:
            f.write(' %s %s\n' % (m['kind'][r], r))
        f.write('COLUMNS\n')
        inint = False
        for c in m['cols']:
            want = m['integer'][c] and not relax and fix is None
            if want and not inint:
                f.write("    M1 'MARKER' 'INTORG'\n")
                inint = True
            if not want and inint:
                f.write("    M2 'MARKER' 'INTEND'\n")
                inint = False
            f.write('    %s obj %.17g\n' % (c, m['cost'][c]))
            for r, a in m['cols'][c].items():
                f.write('    %s %s %.17g\n' % (c, r, a))
        if inint:
            f.write("    M2 'MARKER' 'INTEND'\n")
        f.write('RHS\n')
        for r, b in m['rhs'].items():
            f.write('    rhs %s %.17g\n' % (r, b))
        f.write('BOUNDS\n')
        for c in m['cols']:
            lo, up = m['lo'][c], m['up'][c]
            if fix is not None and c in fix:
                lo = up = fix[c]
            if lo == up:
                f.write(' FX bnd %s %.17g\n' % (c, lo))
                continue
            f.write(' LO bnd %s %.17g\n' % (c, lo))
            if up != float('inf'):
                f.write(' UP bnd %s %.17g\n' % (c, up))
        f.write('ENDATA\n')


def solve(path):
    pt = path + '.pt'
    out = subprocess.run([JAOS, 'solve', path, '--write-point', pt],
                         capture_output=True, text=True).stdout
    st, obj, x = None, None, {}
    for line in out.splitlines():
        if line.startswith('status '):
            st = line.split()[1]
        elif line.startswith('objective '):
            obj = float(line.split()[1])
    if st == 'optimal' and os.path.exists(pt):
        for line in open(pt):
            t = line.split()
            if len(t) == 2:
                x[t[0]] = float(t[1])
        os.remove(pt)
    return st, obj, x


def kernel(A, b):
    k, n = A.shape
    A = [list(map(int, r)) for r in A]
    U = [[1 if i == j else 0 for j in range(n)] for i in range(n)]

    def colop(j1, j2, a, bb, c, d):
        for row in A:
            u, v = row[j1], row[j2]
            row[j1], row[j2] = a * u + bb * v, c * u + d * v
        for row in U:
            u, v = row[j1], row[j2]
            row[j1], row[j2] = a * u + bb * v, c * u + d * v

    piv = 0
    pivcols = []
    for i in range(k):
        if piv >= n:
            break
        for j in range(piv + 1, n):
            while A[i][j] != 0:
                if A[i][piv] == 0:
                    colop(piv, j, 0, 1, 1, 0)
                    continue
                q = A[i][j] // A[i][piv]
                colop(piv, j, 1, 0, -q, 1)
                if A[i][j] != 0:
                    colop(piv, j, 0, 1, 1, 0)
        if A[i][piv] != 0:
            pivcols.append(i)
            piv += 1
    y = [0] * n
    for p, i in enumerate(pivcols):
        s = int(b[i]) - sum(A[i][q] * y[q] for q in range(p))
        if s % A[i][p] != 0:
            return None, None
        y[p] = s // A[i][p]
    for i in range(k):
        if i not in pivcols:
            if sum(A[i][q] * y[q] for q in range(piv)) != int(b[i]):
                return None, None
    Um = np.array(U, dtype=object)
    x0 = Um.dot(np.array(y, dtype=object))
    L = Um[:, piv:]
    return np.array(x0, dtype=float), np.array(L, dtype=float)


def lll(B, delta=0.75):
    B = B.copy()
    n = B.shape[1]

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
    while k < n and it < 20000:
        it += 1
        for j in range(k - 1, -1, -1):
            q = round(mu[k, j])
            if q != 0:
                B[:, k] -= q * B[:, j]
                Q, mu = gs(B)
        if Q[:, k].dot(Q[:, k]) >= (delta - mu[k, k - 1] ** 2) * Q[:, k - 1].dot(Q[:, k - 1]):
            k += 1
        else:
            B[:, [k, k - 1]] = B[:, [k - 1, k]]
            Q, mu = gs(B)
            k = max(k - 1, 1)
    return B


def babai(B, t):
    n = B.shape[1]
    Q = np.zeros_like(B)
    for i in range(n):
        v = B[:, i].copy()
        for j in range(i):
            v -= B[:, i].dot(Q[:, j]) / Q[:, j].dot(Q[:, j]) * Q[:, j]
        Q[:, i] = v
    z = np.zeros(n)
    r = t.copy()
    for i in range(n - 1, -1, -1):
        c = round(r.dot(Q[:, i]) / Q[:, i].dot(Q[:, i]))
        z[i] = c
        r -= c * B[:, i]
    return B.dot(z)


def attempt(m, name):
    ints = [c for c in m['cols'] if m['integer'][c]]
    pure = [r for r in m['rows'] if m['kind'][r] == 'E' and
            all(m['integer'][c] for c in m['cols'] if r in m['cols'][c])]
    lp = os.path.join(WORK, name + '-lp.mps')
    write_mps(m, lp, relax=True)
    st, obj, x = solve(lp)
    if st != 'optimal':
        return 'lp ' + str(st)
    xs = np.array([x.get(c, 0.0) for c in ints])
    lo = np.array([m['lo'][c] for c in ints])
    up = np.array([m['up'][c] for c in ints])
    if pure:
        A = np.array([[m['cols'][c].get(r, 0.0) for c in ints] for r in pure])
        b = np.array([m['rhs'].get(r, 0.0) for r in pure])
        x0, L = kernel(A, b)
        if x0 is None:
            return 'no integer point on the pure rows'
        if L.shape[1] > 0:
            L = lll(L)
            cand = x0 + babai(L, xs - x0)
        else:
            cand = x0
    else:
        L = np.zeros((len(ints), 0))
        cand = np.round(xs)
    tries = 0
    best = None
    seen = set()
    queue = [cand]
    rng = np.random.default_rng(1)
    while queue and tries < 60:
        v = np.rint(queue.pop(0))
        key = tuple(v)
        if key in seen:
            continue
        seen.add(key)
        if np.all(v >= lo - 1e-9) and np.all(v <= up + 1e-9):
            tries += 1
            fix = {c: float(v[k]) for k, c in enumerate(ints)}
            fp = os.path.join(WORK, name + '-fix.mps')
            write_mps(m, fp, fix=fix)
            st2, obj2, _ = solve(fp)
            if st2 == 'optimal':
                if best is None or obj2 < best:
                    best = obj2
                return 'found %.6g after %d LPs (pure rows %d, lattice %d)' % (
                    obj2, tries, len(pure), L.shape[1])
        for _ in range(4):
            if L.shape[1] == 0:
                break
            j = rng.integers(L.shape[1])
            s = rng.choice([-1, 1])
            w = v + s * L[:, j]
            viol = np.maximum(lo - w, 0).sum() + np.maximum(w - up, 0).sum()
            vv = np.maximum(lo - v, 0).sum() + np.maximum(v - up, 0).sum()
            if viol <= vv + 2:
                queue.append(w)
    return 'none after %d LPs (pure rows %d, lattice %d)' % (
        tries, len(pure), L.shape[1])


def main():
    names = [l.split()[0] for l in open(sys.argv[3]) if l.strip()]
    for name in names:
        m = read_mps(os.path.expanduser('~/mip394/%s.mps' % name))
        print(name, attempt(m, name), flush=True)


main()
