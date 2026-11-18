# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
arm = sys.argv[2]
nodes = int(sys.argv[3])
limit = float(sys.argv[4]) if len(sys.argv) > 4 else 300.0
arms = {
    "default": {},
    "dfs": {"nodeselection/dfs/stdpriority": 1000000},
    "bfs": {"nodeselection/bfs/stdpriority": 1000000, "nodeselection/bfs/maxplungedepth": 0},
    "estimate-noplunge": {"nodeselection/estimate/stdpriority": 1000000, "nodeselection/estimate/maxplungedepth": 0},
    "devex": {"lp/pricing": "d"},
    "steepest": {"lp/pricing": "s"},
    "quickstart": {"lp/pricing": "q"},
    "partial": {"lp/pricing": "p"},
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
m.writeStatistics(sys.argv[5] if len(sys.argv) > 5 else "/dev/null")
