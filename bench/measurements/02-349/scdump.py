import sys, json
from pyscipopt import Model, SCIP_PARAMSETTING
# scdump.py MPS OUT.json : SCIP root (presolve off, c-MIR only); writes the LP rows, cuts included, as JSON
f, out = sys.argv[1], sys.argv[2]
SEPAS = ['gomory', 'impliedbounds', 'mixing', 'zerohalf', 'clique', 'mcf', 'flower', 'rlt', 'cgmip', 'disjunctive', 'oddcycle', 'closecuts', 'eccuts', 'gauge', 'convexproj', 'intobj', 'interminor', 'minor', 'lagromory', 'flowcover', 'knapsackcover']
m = Model(); m.hideOutput(); m.readProblem(f)
m.setParam('limits/nodes', 1); m.setPresolve(SCIP_PARAMSETTING.OFF)
for s in SEPAS:
    try:
        m.setParam(f'separating/{s}/freq', -1)
    except Exception:
        pass
m.setParam('separating/aggregation/freq', 0)
m.setParam('limits/nodes', 1)
m.setParam('branching/relpscost/priority', -1)
m.setParam('nodeselection/dfs/stdpriority', 10**6)
m.optimize()
cuts = []
for row in m.getLPRowsData():
    nm = row.name
    cols = row.getCols(); vals = row.getVals()
    cuts.append({'name': nm, 'lhs': row.getLhs(), 'rhs': row.getRhs(), 'const': row.getConstant(),
                 'coef': {c.getVar().name.replace('t_', '', 1): v for c, v in zip(cols, vals)}})
json.dump({'bound': m.getDualbound(), 'rows': cuts}, open(out, 'w'))
print('bound', m.getDualbound(), 'rows', len(cuts), 'named', sum(1 for c in cuts if 'cmir' in c['name']))
