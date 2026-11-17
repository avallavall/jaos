# 02-476: SCIP 10 at a node limit, one thread, with one part of its tree
# switched off; prints the dual bound it reaches.
#
#   python3 scip_abl.py MODEL.mps ARM NODES [TIME]
#
# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
arm = sys.argv[2]
nodes = int(sys.argv[3])
tl = float(sys.argv[4]) if len(sys.argv) > 4 else 600.0
nosb = {"branching/relpscost/initcand": 0, "branching/relpscost/maxreliable": 0.0, "branching/relpscost/minreliable": 0.0}
arms = {
    "default": {},
    "no-redcost": {"propagating/redcost/freq": -1},
    "no-rootredcost": {"propagating/rootredcost/freq": -1},
    "no-redcost-both": {"propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1},
    "no-pseudoobj": {"propagating/pseudoobj/freq": -1},
    "no-linprop": {"constraints/linear/propfreq": -1},
    "no-node-sepa": {"separating/maxrounds": 0},
    "no-root-sepa": {"separating/maxroundsroot": 0},
    "no-conflict": {"conflict/enable": False},
    "no-restart": {"presolving/maxrestarts": 0},
    "no-sb": nosb,
    "pscost": {"branching/pscost/priority": 100000000},
    "no-prop-all": {"propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1,
                    "propagating/pseudoobj/freq": -1, "constraints/linear/propfreq": -1},
    "no-gomory": {"separating/gomory/freq": -1},
    "no-cmir": {"separating/cmir/freq": -1, "separating/aggregation/freq": -1},
    "bare": {"separating/maxrounds": 0, "conflict/enable": False, "presolving/maxrestarts": 0,
             "propagating/redcost/freq": -1, "propagating/rootredcost/freq": -1,
             "propagating/pseudoobj/freq": -1, "constraints/linear/propfreq": -1},
}
m = Model()
m.hideOutput()
m.readProblem(f)
m.setParam("limits/time", tl)
m.setParam("limits/gap", 1e-6)
if nodes > 0:
    m.setParam("limits/nodes", nodes)
m.setParam("parallel/maxnthreads", 1)
for k, v in arms[arm].items():
    m.setParam(k, v)
m.optimize()
print(f"{arm:20s} {m.getStatus():10s} nodes {m.getNNodes():8d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} time {m.getSolvingTime():.1f}", flush=True)
