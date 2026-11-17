# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

m = Model()
m.readProblem(sys.argv[1])
m.setParam("limits/time", float(sys.argv[2]))
m.setParam("limits/gap", 1e-6)
m.setParam("parallel/maxnthreads", 1)
m.setParam("display/freq", 200)
m.optimize()
m.printStatistics()
