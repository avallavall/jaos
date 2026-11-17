# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
arm = sys.argv[2]
arms = {
    "default": {},
    "no-node-sepa": {"separating/maxrounds": 0},
    "no-conflict": {"conflict/enable": False},
    "no-restart": {"presolving/maxrestarts": 0},
    "no-node-sepa-no-conflict": {"separating/maxrounds": 0, "conflict/enable": False},
    "no-node-sepa-no-conflict-no-restart": {"separating/maxrounds": 0, "conflict/enable": False, "presolving/maxrestarts": 0},
    "pscost-branching": {"branching/pscost/priority": 100000000},
    "no-redcost": {"propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1},
    "no-node-sepa-no-conflict-no-restart-no-redcost": {"separating/maxrounds": 0, "conflict/enable": False, "presolving/maxrestarts": 0, "propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1},
    "bare": {"separating/maxrounds": 0, "conflict/enable": False, "presolving/maxrestarts": 0, "propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1, "propagating/pseudoobj/freq": -1},
}
m = Model()
m.hideOutput()
m.readProblem(f)
m.setParam("limits/time", float(sys.argv[3]) if len(sys.argv) > 3 else 120.0)
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
for k, v in arms[arm].items():
    m.setParam(k, v)
m.optimize()
print(f"{arm:48s} {m.getStatus():10s} nodes {m.getNNodes():7d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} time {m.getSolvingTime():.1f}", flush=True)
