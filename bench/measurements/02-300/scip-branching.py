import sys
from pyscipopt import Model, SCIP_PARAMSETTING
for name in sys.argv[1:]:
    for how in ("bare", "bare+pscost", "bare+noprop", "bare+pscost+noprop"):
        m = Model(); m.hideOutput()
        m.readProblem(f"../../../bench/instances-miplib/{name}.mps")
        m.setParam("limits/time", 120.0); m.setParam("limits/gap", 1e-6); m.setParam("parallel/maxnthreads", 1)
        m.setPresolve(SCIP_PARAMSETTING.OFF); m.setSeparating(SCIP_PARAMSETTING.OFF); m.setHeuristics(SCIP_PARAMSETTING.OFF)
        if "pscost" in how: m.setParam("branching/pscost/priority", 100000)
        if "noprop" in how:
            m.setParam("propagating/maxrounds", 0); m.setParam("propagating/maxroundsroot", 0)
            m.setParam("conflict/enable", False)
        m.optimize()
        print(f"{name:8s} {how:20s} {m.getStatus():10s} nodes {m.getNNodes():8d} time {m.getSolvingTime():7.2f}", flush=True)
