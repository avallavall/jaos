# 02-478: the root LP point against the incumbent and the optimum, the
# columns on which RINS would fix them, and SCIP's count of the integer
# points at least as good as the incumbent.
#
#   python3 rins.py MODEL.mps LATDUMP OPTIMUM DIR
#
# SPDX-License-Identifier: Apache-2.0
import sys
import numpy as np
from pyscipopt import Model, quicksum

mps, lat, opt = sys.argv[1], sys.argv[2], sys.argv[3]


import importlib.util
spec = importlib.util.spec_from_file_location('anam', sys.argv[4] + '/anamod.py')
A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
M, b, c, cols, rt = A.read_mps(mps)
U, V, xinc = A.read_lat(lat)
xopt = A.read_pt(opt, cols)
n = len(cols)

def lp(extra=None):
    m = Model(); m.hideOutput()
    x = [m.addVar(lb=0, vtype='C') for j in range(n)]
    for i in range(M.shape[0]):
        m.addCons(quicksum(M[i, j] * x[j] for j in range(n) if M[i, j] != 0) <= b[i])
    if extra is not None:
        m.addCons(quicksum(c[j] * x[j] for j in range(n)) <= extra)
    m.setObjective(quicksum(c[j] * x[j] for j in range(n)))
    m.optimize()
    return np.array([m.getVal(v) for v in x]), m.getObjVal()

xl, zl = lp()
print('root lp', zl)
print('x_lp ', np.round(xl, 2))
print('x_inc', xinc.astype(int))
print('x_opt', xopt.astype(int))
agree = [j for j in range(n) if abs(xl[j] - xinc[j]) < 1e-6]
print('lp/inc agree on', len(agree), 'cols; opt matches on', sum(xopt[j] == xinc[j] for j in agree))
print('cols where x_opt != x_inc among agree:', [j for j in agree if xopt[j] != xinc[j]])
# count integer points in improving region via SCIP with nogoods (limit)
m = Model(); m.hideOutput()
x = [m.addVar(lb=0, vtype='I') for j in range(n)]
for i in range(M.shape[0]):
    m.addCons(quicksum(M[i, j] * x[j] for j in range(n) if M[i, j] != 0) <= b[i])
m.addCons(quicksum(c[j] * x[j] for j in range(n)) <= c @ xinc + 1e-6)
m.setObjective(0)
m.setParam('limits/time', 600)
m.setParam('constraints/countsols/collect', True)
try:
    m.count()
    print('count', m.getNCountedSols())
except Exception as e:
    print('count failed', e)
