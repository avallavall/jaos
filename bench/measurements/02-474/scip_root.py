# SPDX-License-Identifier: Apache-2.0
import sys
from pyscipopt import Model

def root(f, params, nodes=1):
    m = Model()
    m.hideOutput()
    m.readProblem(f)
    m.setParam("limits/nodes", nodes)
    m.setParam("presolving/maxrestarts", 0)
    m.setParam("parallel/maxnthreads", 1)
    for k, v in params.items():
        m.setParam(k, v)
    m.optimize()
    return m.getDualbound()

f = sys.argv[1]
nosb = {"branching/relpscost/initcand": 0, "branching/relpscost/maxreliable": 0.0, "branching/relpscost/minreliable": 0.0}
arms = [
    ("default", {}),
    ("default-nosb", nosb),
    ("maxaggrsroot0", {"separating/aggregation/maxaggrsroot": 0}),
    ("maxaggrsroot1", {"separating/aggregation/maxaggrsroot": 1}),
    ("maxaggrsroot2", {"separating/aggregation/maxaggrsroot": 2}),
    ("no-cmir", {"separating/cmir/freq": -1}),
    ("no-flowcover", {"separating/flowcover/freq": -1}),
    ("no-knapsackcover", {"separating/knapsackcover/freq": -1}),
    ("cmir-only", {"separating/flowcover/freq": -1, "separating/knapsackcover/freq": -1}),
    ("flowcover-only", {"separating/cmir/freq": -1, "separating/knapsackcover/freq": -1}),
    ("knapsackcover-only", {"separating/cmir/freq": -1, "separating/flowcover/freq": -1}),
    ("cmir-only-agg0", {"separating/flowcover/freq": -1, "separating/knapsackcover/freq": -1, "separating/aggregation/maxaggrsroot": 0}),
    ("nopresolve-agg0", {"presolving/maxrounds": 0, "separating/aggregation/maxaggrsroot": 0}),
    ("nopresolve-cmironly-agg0", {"presolving/maxrounds": 0, "separating/aggregation/maxaggrsroot": 0, "separating/flowcover/freq": -1, "separating/knapsackcover/freq": -1}),
    ("nopresolve-cmironly", {"presolving/maxrounds": 0, "separating/flowcover/freq": -1, "separating/knapsackcover/freq": -1}),
]
for name, p in arms:
    try:
        b = root(f, p)
        print(f"{name:26s} {b:.4f}", flush=True)
    except Exception as e:
        print(name, "error", e, flush=True)
