# SPDX-License-Identifier: Apache-2.0
import sys
from collections import defaultdict
from pyscipopt import Model, quicksum

src, level = sys.argv[1], sys.argv[2]
rows, rhs, coef = {}, {}, defaultdict(dict)
sec = None
for line in open(src):
    if not line.startswith(' '):
        sec = line.split()[0]
        continue
    t = line.split()
    if sec == 'ROWS':
        rows[t[1]] = t[0]
    elif sec == 'COLUMNS' and "'MARKER'" not in line:
        for k in range(1, len(t), 2):
            coef[t[k]][t[0]] = float(t[k + 1])
    elif sec == 'RHS':
        for k in range(1, len(t), 2):
            rhs[t[k]] = float(t[k + 1])
n, G = 26, 5
col = lambda i, g: 'C%04d' % (n + 1 + G * i + g)
size = [int(rhs['R%04d' % (n * G + n + 1 + g)]) for g in range(G)]
a = [[0.0] * n for _ in range(n)]
for i in range(n):
    r = coef['R%04d' % (G * i + 1)]
    for k in range(n):
        if k != i:
            a[i][k] = -r.get(col(k, 0), 0.0)
assert all(a[i][k] == a[k][i] for i in range(n) for k in range(n))
assert all(-coef['R%04d' % (G * i + g + 1)][col(i, g)] == 5.0 for i in range(n) for g in range(G))
m = Model()
m.hideOutput()
x = {(i, g): m.addVar(lb=0, ub=1) for i in range(n) for g in range(G)}
z = [m.addVar(lb=0, ub=20) for i in range(n)]
for i in range(n):
    m.addCons(quicksum(x[i, g] for g in range(G)) == 1)
    for g in range(G):
        m.addCons(z[i] - 5 * x[i, g] - quicksum(a[i][k] * x[k, g] for k in range(n) if k != i) >= -5)
for g in range(G):
    m.addCons(quicksum(x[i, g] for i in range(n)) == size[g])
if level != 'lp':
    P = {}
    def p(u, w):
        key = (u, w) if u < w else (w, u)
        if key not in P:
            v = m.addVar(lb=0, ub=1)
            P[key] = v
            m.addCons(v <= x[u])
            m.addCons(v <= x[w])
            m.addCons(v >= x[u] + x[w] - 1)
        return P[key]
    same = level == 'same'
    hs = lambda g: [g] if same else range(G)
    v = {(i, g): m.addVar(lb=0, ub=20) for i in range(n) for g in range(G)}
    for i in range(n):
        m.addCons(quicksum(v[i, g] for g in range(G)) == z[i])
        for g in range(G):
            m.addCons(v[i, g] <= 20 * x[i, g])
            m.addCons(z[i] - v[i, g] <= 20 * (1 - x[i, g]))
            for h in range(G):
                if h == g:
                    m.addCons(quicksum(p((i, g), (k, g)) for k in range(n) if k != i) == (size[g] - 1) * x[i, g])
                elif not same:
                    m.addCons(quicksum(p((i, g), (k, h)) for k in range(n) if k != i) == size[h] * x[i, g])
            if not same:
                for k in range(n):
                    if k != i:
                        m.addCons(quicksum(p((i, g), (k, h)) for h in range(G)) == x[i, g])
            for h in hs(g):
                m.addCons(v[i, g] - 5 * (1 if h == g else 0) * x[i, g]
                          - quicksum(a[i][k] * p((i, g), (k, h)) for k in range(n) if k != i) + 5 * x[i, g] >= 0)
                m.addCons((z[i] - v[i, g]) - 5 * (x[i, h] - (x[i, g] if h == g else 0))
                          - quicksum(a[i][k] * (x[k, h] - p((i, g), (k, h))) for k in range(n) if k != i)
                          + 5 * (1 - x[i, g]) >= 0)
    if level == 'tri':
        y = {(i, k): quicksum(p((i, g), (k, g)) for g in range(G))
             for i in range(n) for k in range(n) if i < k}
        yy = lambda i, k: y[(i, k) if i < k else (k, i)]
        for i in range(n):
            for k in range(i + 1, n):
                for l in range(k + 1, n):
                    m.addCons(yy(i, k) + yy(k, l) - yy(i, l) <= 1)
                    m.addCons(yy(i, k) - yy(k, l) + yy(i, l) <= 1)
                    m.addCons(-yy(i, k) + yy(k, l) + yy(i, l) <= 1)
m.setObjective(quicksum(z), 'minimize')
if len(sys.argv) > 3:
    m.writeProblem(sys.argv[3])
    print(f"{level:6s} vars {m.getNVars()} conss {m.getNConss()} written to {sys.argv[3]}")
    sys.exit(0)
m.setParam('presolving/maxrounds', 0)
m.optimize()
print(f"{level:6s} vars {m.getNVars()} conss {m.getNConss()} status {m.getStatus()} bound {m.getObjVal():.4f}")
