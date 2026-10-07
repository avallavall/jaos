import sys, os
from pyscipopt import Model, SCIP_PARAMSETTING
# scvar2.py MPS : SCIP root with presolve off and one separator family at a time
f = sys.argv[1]
SEPAS = ['aggregation', 'gomory', 'impliedbounds', 'mixing', 'zerohalf', 'clique', 'mcf', 'flower', 'rlt', 'cgmip', 'disjunctive', 'oddcycle', 'closecuts', 'eccuts', 'gauge', 'convexproj', 'intobj', 'interminor', 'minor', 'lagromory']
def run(tag, keep, extra=None):
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
            m.setParam(f'separating/{s}/freq', 0 if s in keep else -1)
        except Exception:
            pass
    if extra:
        extra(m)
    m.optimize()
    st = os.path.join(os.environ['TEMP'], 'scv2.txt')
    m.writeStatistics(st)
    final = '?'
    for ln in open(st):
        if 'Final Dual Bound' in ln:
            final = ln.split(':')[1].strip()
    print(f'{tag:34s} root {final:>22s}', flush=True)
def nofc(m):
    m.setParam('separating/flowcover/freq', -1); m.setParam('separating/knapsackcover/freq', -1)
def only_fc(m):
    m.setParam('separating/knapsackcover/freq', -1)
run('all', SEPAS)
run('cmir', ['aggregation'], nofc)
run('agg (cmir+flow+knap)', ['aggregation'])
run('gomory', ['gomory'])
run('impliedbounds', ['impliedbounds'])
run('none', [])
