import sys, os
from pyscipopt import Model, SCIP_PARAMSETTING
# scip-sepa.py MPS : SCIP's root bound with presolve off and only its c-MIR separator, under its own settings
f = sys.argv[1]
SEPAS = ['aggregation', 'gomory', 'impliedbounds', 'mixing', 'zerohalf', 'clique', 'mcf', 'flower', 'rlt', 'cgmip', 'disjunctive', 'oddcycle', 'closecuts', 'eccuts', 'gauge', 'convexproj', 'intobj', 'interminor', 'minor', 'lagromory']
def run(tag, keep, extra=None):
    m = Model()
    m.hideOutput()
    m.readProblem(f)
    m.setParam('limits/nodes', 1)
    m.setParam('limits/time', 120)
    m.setPresolve(SCIP_PARAMSETTING.OFF)
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
run('cmir only', ['aggregation'], nofc)
run('cmir only, maxaggrsroot 1', ['aggregation'], lambda m: (nofc(m), m.setParam('separating/aggregation/maxaggrsroot', 1)))
run('cmir only, maxtestdelta 0', ['aggregation'], lambda m: (nofc(m), m.setParam('separating/aggregation/maxtestdelta', 0)))
run('cmir only, no fixintegralrhs', ['aggregation'], lambda m: (nofc(m), m.setParam('separating/aggregation/fixintegralrhs', False)))
run('cmir only, no negscaling', ['aggregation'], lambda m: (nofc(m), m.setParam('separating/aggregation/trynegscaling', False)))
run('cmir only, maxslackroot 0', ['aggregation'], lambda m: (nofc(m), m.setParam('separating/aggregation/maxslackroot', 0.0)))
