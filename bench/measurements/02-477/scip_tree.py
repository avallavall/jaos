# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
arm = sys.argv[2]
nodes = int(sys.argv[3])
limit = float(sys.argv[4]) if len(sys.argv) > 4 else 600.0
arms = {
    "default": {},
    "no-symmetry": {"misc/usesymmetry": 0},
    "no-node-sepa": {"separating/maxrounds": 0},
    "no-root-sepa": {"separating/maxroundsroot": 0, "separating/maxrounds": 0},
    "no-conflict": {"conflict/enable": False},
    "no-restart": {"presolving/maxrestarts": 0},
    "no-propagation": {"propagating/maxrounds": 0, "propagating/maxroundsroot": 0},
    "pscost-branching": {"branching/pscost/priority": 100000000},
    "bfs": {"nodeselection/bfs/stdpriority": 1000000},
    "bare": {"misc/usesymmetry": 0, "separating/maxrounds": 0, "conflict/enable": False,
             "presolving/maxrestarts": 0, "propagating/maxrounds": 0},
}
m = Model()
m.hideOutput()
m.readProblem(f)
m.setParam("limits/time", limit)
m.setParam("limits/nodes", nodes)
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
for k, v in arms[arm].items():
    m.setParam(k, v)
m.optimize()
print(f"{arm:20s} {m.getStatus():10s} nodes {m.getNNodes():8d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} lpiter {m.getNLPIterations():9d} time {m.getSolvingTime():.1f}", flush=True)
