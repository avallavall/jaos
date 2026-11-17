# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

f = sys.argv[1]
m = Model()
m.hideOutput()
m.readProblem(f)
m.setParam("limits/nodes", 1)
m.setParam("presolving/maxrestarts", 0)
m.setParam("parallel/maxnthreads", 1)
m.setParam("presolving/maxrounds", 0)
m.setParam("separating/aggregation/maxaggrsroot", 0)
m.setParam("separating/flowcover/freq", -1)
m.setParam("separating/knapsackcover/freq", -1)
m.setParam("separating/gomory/freq", -1)
m.setParam("separating/zerohalf/freq", -1)
m.setParam("branching/relpscost/initcand", 0)
m.optimize()
print("bound", m.getDualbound())
rows = m.getLPRowsData()
ncut = 0
for r in rows:
    name = r.name
    cols = r.getCols()
    vals = r.getVals()
    if name.startswith("R"):
        continue
    ncut += 1
    act = m.getRowLPActivity(r)
    terms = []
    for c, v in zip(cols, vals):
        var = c.getVar()
        terms.append(f"{v:+.4g}*{var.name}[{m.getVal(var) if False else c.getPrimsol():.3g}]")
    print(name, f"lhs={r.getLhs():.6g} rhs={r.getRhs():.6g} act={act:.6g} n={len(cols)} const={r.getConstant():.4g}", " ".join(terms[:40]))
print("cuts in LP", ncut)
