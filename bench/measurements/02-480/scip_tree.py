# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
nodes = int(sys.argv[2])
limit = float(sys.argv[3])
noimpl = {"constraints/linear/dualpresolving": False}
nosym = {"misc/usesymmetry": 0}
nosepa = {"separating/maxrounds": 0}
arms = {
    "default": {},
    "no-implint": noimpl,
    "no-symmetry": nosym,
    "no-implint-no-symmetry": {**noimpl, **nosym},
    "sym-polyhedral": {"misc/usesymmetry": 1},
    "sym-orbital": {"misc/usesymmetry": 2},
    "sym-sst": {"misc/usesymmetry": 4},
    "no-orbitopes": {"propagating/symmetry/detectorbitopes": False},
    "no-dynamic-prop": {"propagating/symmetry/usedynamicprop": False},
    "no-node-sepa": nosepa,
    "no-implint-no-node-sepa": {**noimpl, **nosepa},
    "no-gomory": {"separating/gomory/freq": -1},
    "no-cmir": {"separating/aggregation/freq": -1},
    "no-zerohalf": {"separating/zerohalf/freq": -1},
    "no-impliedbounds": {"separating/impliedbounds/freq": -1},
    "no-conflict": {"conflict/enable": False},
    "no-propagation": {"propagating/maxrounds": 0, "propagating/maxroundsroot": 0},
    "no-pseudoobj": {"propagating/pseudoobj/freq": -1},
    "no-redcost": {"propagating/redcost/freq": -1},
    "pscost-branching": {"branching/pscost/priority": 100000000},
    "bfs": {"nodeselection/bfs/stdpriority": 1000000},
}
for name in sys.argv[4:]:
    arm, _, objlim = name.partition("+objlim")
    m = Model()
    m.hideOutput()
    m.readProblem(f)
    m.setParam("limits/time", limit)
    m.setParam("limits/nodes", nodes)
    m.setParam("limits/gap", 1e-6)
    m.setParam("parallel/maxnthreads", 1)
    for k, v in arms[arm].items():
        m.setParam(k, v)
    if objlim:
        m.setObjlimit(float(objlim))
    m.optimize()
    print(f"{name:28s} {m.getStatus():10s} nodes {m.getNNodes():8d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} lpiter {m.getNLPIterations():9d} time {m.getSolvingTime():.1f}", flush=True)
