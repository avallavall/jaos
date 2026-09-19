# SPDX-License-Identifier: Apache-2.0
"""JAOS as an AMPL solver under Pyomo: small models through
SolverFactory("asl:jaos"), printing what Pyomo reads back from the .sol.

usage: pyomo_asl.py JAOS
"""
import sys

import pyomo.environ as pe

exe = sys.argv[1]


def run(name, m, options=None, duals=False):
    if duals:
        m.dual = pe.Suffix(direction=pe.Suffix.IMPORT)
    opt = pe.SolverFactory("asl:jaos", executable=exe)
    for k, v in (options or {}).items():
        opt.options[k] = v
    r = opt.solve(m, load_solutions=False)
    print(f"{name}: status {r.solver.status}, termination "
          f"{r.solver.termination_condition}")
    if len(r.solution) > 0 and len(r.solution[0].variable) > 0:
        m.solutions.load_from(r)
        vals = ", ".join(f"{v.name} {v.value:g}"
                         for v in m.component_data_objects(pe.Var))
        objs = [pe.value(o) for o in m.component_data_objects(pe.Objective)]
        print(f"   {vals}; objective {objs[0]:g}")
        if duals:
            ds = ", ".join(f"{c.name} {m.dual.get(c):g}"
                           for c in m.component_data_objects(pe.Constraint))
            print(f"   duals {ds}")
    else:
        print("   no point")


m = pe.ConcreteModel()
m.x = pe.Var(bounds=(0, 4))
m.y = pe.Var(bounds=(0, None))
m.c1 = pe.Constraint(expr=m.x + 2 * m.y <= 8)
m.c2 = pe.Constraint(expr=3 * m.x - m.y >= -2)
m.c3 = pe.Constraint(expr=(1, m.x + m.y, 5))
m.obj = pe.Objective(expr=-3 * m.x - 2 * m.y + 7)
run("lp", m, duals=True)

k = pe.ConcreteModel()
k.i = pe.Var(domain=pe.Integers, bounds=(0, 10))
k.b = pe.Var(domain=pe.Binary)
k.z = pe.Var(bounds=(0, None))
k.c1 = pe.Constraint(expr=2 * k.i + 3 * k.z <= 13)
k.c2 = pe.Constraint(expr=k.z - 5 * k.b <= 0)
k.obj = pe.Objective(expr=5 * k.i + 4 * k.z + 2 * k.b, sense=pe.maximize)
run("mip", k)

f = pe.ConcreteModel()
f.x = pe.Var(bounds=(0, 1))
f.c = pe.Constraint(expr=f.x >= 2)
f.obj = pe.Objective(expr=f.x)
run("infeasible", f)

u = pe.ConcreteModel()
u.x = pe.Var(bounds=(0, None))
u.c = pe.Constraint(expr=u.x >= 1)
u.obj = pe.Objective(expr=u.x, sense=pe.maximize)
run("unbounded", u)

w = [23, 31, 29, 44, 53, 38, 63, 85, 89, 82, 17, 29, 41, 11, 7, 97, 53, 71]
p = [92, 57, 49, 68, 60, 43, 67, 84, 87, 72, 29, 41, 50, 12, 9, 99, 55, 70]
s = pe.ConcreteModel()
s.I = pe.RangeSet(0, len(w) - 1)
s.x = pe.Var(s.I, domain=pe.Binary)
s.cap = pe.Constraint(expr=sum(w[i] * s.x[i] for i in s.I) <= 300)
s.obj = pe.Objective(expr=sum(p[i] * s.x[i] for i in s.I), sense=pe.maximize)
run("knapsack", s)
run("knapsack, work_limit 1", s, {"work_limit": 1})
run("knapsack, an unknown option", s, {"nosuch": 1})

q = pe.ConcreteModel()
q.x = pe.Var(bounds=(0, 3))
q.obj = pe.Objective(expr=(q.x - 1) ** 2)
run("quadratic", q)
