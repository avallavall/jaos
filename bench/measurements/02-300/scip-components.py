import sys
from pyscipopt import Model, SCIP_PARAMSETTING
for name in sys.argv[1:]:
    for how in ("default", "no-presolve", "no-cuts", "no-presolve-no-cuts"):
        m = Model(); m.hideOutput()
        m.readProblem(f"../../../bench/instances-miplib/{name}.mps")
        m.setParam("limits/time", 60.0); m.setParam("limits/gap", 1e-6); m.setParam("parallel/maxnthreads", 1)
        if "presolve" in how: m.setPresolve(SCIP_PARAMSETTING.OFF)
        if "cuts" in how: m.setSeparating(SCIP_PARAMSETTING.OFF)
        m.optimize()
        print(f"{name:8s} {how:20s} {m.getStatus():10s} nodes {m.getNNodes():8d} time {m.getSolvingTime():7.2f}", flush=True)
