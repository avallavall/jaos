# SPDX-License-Identifier: Apache-2.0
import sys

from pyscipopt import Model

m = Model()
m.hideOutput()
m.readProblem(sys.argv[1])
m.setParam("limits/time", float(sys.argv[2]))
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
m.optimize()
obj = m.getObjVal() if m.getNSols() > 0 else float("nan")
print(f"{m.getStatus()}\t{obj!r}\t{m.getNNodes()}\t{m.getSolvingTime():.3f}")
