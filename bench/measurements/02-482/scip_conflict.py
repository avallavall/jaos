# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
nodes = int(sys.argv[2])
limit = float(sys.argv[3])
arms = {
    "default": {},
    "no-conflict": {"conflict/enable": False},
    "boundlp-off": {"conflict/useboundlp": "o"},
    "boundlp-graph": {"conflict/useboundlp": "c"},
    "boundlp-dual": {"conflict/useboundlp": "d"},
    "inflp-off": {"conflict/useinflp": "o"},
}
for name in sys.argv[4:]:
    m = Model()
    m.hideOutput()
    m.readProblem(f)
    m.setParam("limits/time", limit)
    m.setParam("limits/nodes", nodes)
    m.setParam("limits/gap", 1e-6)
    m.setParam("parallel/maxnthreads", 1)
    for k, v in arms[name].items():
        m.setParam(k, v)
    m.optimize()
    print(f"{name:16s} {m.getStatus():10s} nodes {m.getNNodes():8d} bound {m.getDualbound():.4f} primal {m.getPrimalbound():.4f} lpiter {m.getNLPIterations():9d} time {m.getSolvingTime():.1f}", flush=True)
