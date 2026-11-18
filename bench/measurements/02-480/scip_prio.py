# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
nodes = int(sys.argv[2])
limit = float(sys.argv[3])
for arm in sys.argv[4:]:
    m = Model()
    m.hideOutput()
    m.readProblem(f)
    m.setParam("limits/time", limit)
    m.setParam("limits/nodes", nodes)
    m.setParam("limits/gap", 1e-6)
    m.setParam("parallel/maxnthreads", 1)
    for v in m.getVars():
        general = v.vtype() == "INTEGER" and v.getUbOriginal() > 1.5
        if arm == "binary-first" and not general:
            m.chgVarBranchPriority(v, 10)
        if arm == "general-first" and general:
            m.chgVarBranchPriority(v, 10)
    m.optimize()
    print(f"{arm:16s} {m.getStatus():10s} nodes {m.getNNodes():8d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} lpiter {m.getNLPIterations():9d} time {m.getSolvingTime():.1f}", flush=True)
