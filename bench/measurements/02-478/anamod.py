# 02-478: readers for the other scripts: an MPS of rows and integer
# columns, a point file, and the basis JAOS_LATDUMP writes (n, U, V, the
# incumbent).
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np

def read_mps(path):
    rows = []
    rtype = {}
    cols = []
    cidx = {}
    A = {}
    cost = {}
    rhs = {}
    sec = None
    obj = None
    for line in open(path):
        if line.startswith('*') or not line.strip():
            continue
        if not line[0].isspace():
            sec = line.split()[0]
            continue
        t = line.split()
        if sec == 'ROWS':
            if t[0] == 'N':
                obj = t[1]
            else:
                rtype[t[1]] = t[0]
                rows.append(t[1])
        elif sec == 'COLUMNS':
            if 'MARKER' in t[1] if len(t) > 1 else False:
                continue
            c = t[0]
            if c not in cidx:
                cidx[c] = len(cols)
                cols.append(c)
            for k in range(1, len(t), 2):
                r, v = t[k], float(t[k + 1])
                if r == obj:
                    cost[c] = v
                else:
                    A[(r, c)] = v
        elif sec == 'RHS':
            for k in range(1, len(t), 2):
                rhs[t[k]] = float(t[k + 1])
    ridx = {r: i for i, r in enumerate(rows)}
    M = np.zeros((len(rows), len(cols)))
    for (r, c), v in A.items():
        M[ridx[r], cidx[c]] = v
    cvec = np.array([cost.get(c, 0.0) for c in cols])
    b = np.array([rhs.get(r, 0.0) for r in rows])
    return M, b, cvec, cols, [rtype[r] for r in rows]

def read_pt(path, cols):
    d = {}
    for line in open(path):
        if line.startswith('#'):
            continue
        t = line.split()
        if len(t) == 2:
            d[t[0]] = float(t[1])
    return np.array([d.get(c, 0.0) for c in cols])

def read_lat(path):
    f = open(path).read().split('\n')
    n = int(f[0])
    U = np.array([[int(x) for x in f[1 + i].split()] for i in range(n)])
    V = np.array([[int(x) for x in f[1 + n + i].split()] for i in range(n)])
    xinc = np.array([float(x) for x in f[1 + 2 * n].split()])
    return U, V, xinc

