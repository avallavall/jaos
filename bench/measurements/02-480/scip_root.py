# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

known = Model().getParams()
nosb = {"branching/relpscost/initcand": 0, "branching/relpscost/maxreliable": 0.0,
        "branching/relpscost/minreliable": 0.0}
seps = sorted(k.split("/")[1] for k in known if k.startswith("separating/") and k.endswith("/freq"))
arms = [("default", {}), ("no-strong-branching", nosb),
        ("no-restart", {"presolving/maxrestarts": 0}),
        ("no-sepa", {"separating/maxroundsroot": 0}),
        ("no-presolve", {"presolving/maxrounds": 0}),
        ("no-symmetry", {"misc/usesymmetry": 0})]
for s in seps:
    if known[f"separating/{s}/freq"] < 0:
        continue
    arms.append(("no-" + s, {f"separating/{s}/freq": -1}))
    arms.append((s + "-only-nosb", {**nosb, **{f"separating/{t}/freq": -1 for t in seps if t != s}}))
for f in sys.argv[1:]:
    for name, p in arms:
        m = Model()
        m.hideOutput()
        m.readProblem(f)
        m.setParam("limits/nodes", 1)
        m.setParam("parallel/maxnthreads", 1)
        for k, v in p.items():
            m.setParam(k, v)
        m.optimize()
        print(f"{f.split('/')[-1]:16s} {name:26s} {m.getDualbound():.4f} cuts {m.getNCutsApplied()}", flush=True)
