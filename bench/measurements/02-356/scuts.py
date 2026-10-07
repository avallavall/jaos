import sys, os, collections
from pyscipopt import Model, SCIP_PARAMSETTING
# scuts.py MPS : SCIP root with c-MIR + flow cover only; print the cuts left in the LP
f = sys.argv[1]
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 12
SEPAS = ['aggregation', 'gomory', 'impliedbounds', 'mixing', 'zerohalf', 'clique', 'mcf', 'flower', 'rlt', 'cgmip', 'disjunctive', 'oddcycle', 'closecuts', 'eccuts', 'gauge', 'convexproj', 'intobj', 'interminor', 'minor', 'lagromory']
m = Model()
m.hideOutput()
m.readProblem(f)
m.setParam('limits/nodes', 1)
m.setParam('limits/time', 60)
m.setPresolve(SCIP_PARAMSETTING.OFF)
m.setHeuristics(SCIP_PARAMSETTING.OFF)
m.setParam('branching/mostinf/priority', 1000000)
m.setParam('propagating/maxroundsroot', 0)
for s in SEPAS:
    try:
        m.setParam(f'separating/{s}/freq', 0 if s == 'aggregation' else -1)
    except Exception:
        pass
m.setParam('separating/knapsackcover/freq', -1)
m.setParam('separating/cmir/freq', -1) if False else None
m.optimize()
print('dual bound', m.getDualbound())
rows = m.getLPRowsData()
names = collections.Counter()
shown = 0
for r in rows:
    nm = r.name
    if nm.startswith('R0'):
        continue
    names[nm.rstrip('0123456789_')] += 1
    if shown < lim:
        cols = r.getCols(); vals = r.getVals()
        terms = ' + '.join(f'{v:.4g}*{c.getVar().name}' for c, v in zip(cols, vals))
        print(nm, r.getLhs(), '<=', terms, '<=', r.getRhs())
        shown += 1
print(names.most_common())
