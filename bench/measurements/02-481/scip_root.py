# SPDX-License-Identifier: Apache-2.0
# SCIP on one model to a node limit, one thread, gap 1e-6: the root's
# bound (the dual bound after node 1) and the tree's at the limit.
#   scip_root.py MODEL NODES TIME [name=value ...]
import sys
from pyscipopt import Model

m = Model()
m.hideOutput()
m.readProblem(sys.argv[1])
m.setParam("limits/time", float(sys.argv[3]))
m.setParam("limits/nodes", int(sys.argv[2]))
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
for kv in sys.argv[4:]:
    k, v = kv.split("=", 1)
    try:
        v = int(v)
    except ValueError:
        try:
            v = float(v)
        except ValueError:
            pass
    m.setParam(k, v)
m.optimize()
print(f"{' '.join(sys.argv[4:]) or 'default':40s} {m.getStatus():10s} nodes {m.getNNodes():8d} "
      f"bound {m.getDualbound():.6f} primal {m.getPrimalbound():.4f} "
      f"lpiter {m.getNLPIterations():9d} time {m.getSolvingTime():.1f}", flush=True)
