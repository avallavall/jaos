import sys, os, glob
from pyscipopt import Model, SCIP_PARAMSETTING
# scall.py OUT.csv MPS... : SCIP's first LP value and root bound, presolve off, default separators, 60 s a root
out = open(sys.argv[1], 'w')
for f in sys.argv[2:]:
    m = Model(); m.hideOutput(); m.readProblem(f)
    m.setParam('limits/nodes', 1); m.setParam('limits/time', 60)
    m.setPresolve(SCIP_PARAMSETTING.OFF)
    m.setHeuristics(SCIP_PARAMSETTING.OFF)
    m.setParam('branching/mostinf/priority', 1000000)
    m.setParam('propagating/maxroundsroot', 0)
    m.optimize()
    st = os.path.join(os.environ['TEMP'], 'scall.txt')
    m.writeStatistics(st)
    first = root = ''
    for ln in open(st):
        if 'First LP value' in ln:
            first = ln.split(':')[1].strip()
        if 'Final Dual Bound' in ln:
            root = ln.split(':')[1].strip()
    name = os.path.basename(f).replace('.mps', '')
    out.write(f'{name},{first},{root},{m.getObjectiveSense()}\n'); out.flush()
    print(name, first, root, flush=True)
