# SPDX-License-Identifier: Apache-2.0
"""Bodies above degree two through Pyomo: each model is written by Pyomo's
own .nl writer, solved by SolverFactory("asl:jaos"), and read back from
JAOS's .sol. Prints what Pyomo reads and the optimum worked out by hand.

usage: pyomo_nl.py JAOS
"""
import math
import sys

import pyomo.environ as pe
import pyomo.kernel as pmo

exe = sys.argv[1]
worst = 0.0


def run(name, m, want, duals=False):
    global worst
    if duals:
        m.dual = pe.Suffix(direction=pe.Suffix.IMPORT)
    opt = pe.SolverFactory("asl:jaos", executable=exe)
    r = opt.solve(m, load_solutions=False)
    tc = r.solver.termination_condition
    line = f"{name}: {tc}"
    if want is None:
        print(f"{line}; {r.solver.message}")
        return
    m.solutions.load_from(r)
    obj = [pe.value(o) for o in m.component_data_objects(pe.Objective)][0]
    gap = abs(obj - want)
    worst = max(worst, gap)
    vals = ", ".join(f"{v.name} {v.value:.6g}"
                     for v in m.component_data_objects(pe.Var))
    print(f"{line}, objective {obj:.12g}, expected {want:.12g}, "
          f"off by {gap:.1e}")
    print(f"   {vals}")
    if duals:
        ds = ", ".join(f"{c.name} {m.dual.get(c):.6g}"
                       for c in m.component_data_objects(pe.Constraint))
        print(f"   duals {ds}")


m = pe.ConcreteModel()
m.x = pe.Var()
m.y = pe.Var()
m.t = pe.Var()
m.c = pe.Constraint(expr=pe.sqrt(m.x ** 2 + m.y ** 2) <= m.t)
m.l = pe.Constraint(expr=m.x + m.y >= 2)
m.o = pe.Objective(expr=m.t)
run("sqrt(x^2 + y^2) <= t", m, math.sqrt(2), duals=True)

m = pe.ConcreteModel()
m.x = pe.Var()
m.y = pe.Var()
m.t = pe.Var(bounds=(0, None))
m.c = pe.Constraint(expr=m.x ** 2 + m.y ** 2 <= m.t ** 2)
m.l = pe.Constraint(expr=m.x + m.y >= 2)
m.o = pe.Objective(expr=m.t)
run("x^2 + y^2 <= t^2, t >= 0", m, math.sqrt(2), duals=True)

m = pe.ConcreteModel()
m.x = pe.Var()
m.y = pe.Var(bounds=(0, None))
m.z = pe.Var(bounds=(0, None))
m.c = pe.Constraint(expr=m.x ** 2 <= m.y * m.z)
m.l = pe.Constraint(expr=m.x >= 2)
m.k = pe.Constraint(expr=m.y == 1)
m.o = pe.Objective(expr=m.z)
run("x^2 <= y z, y, z >= 0", m, 4.0, duals=True)

m = pe.ConcreteModel()
m.x = pe.Var()
m.y = pe.Var(bounds=(0, 4))
m.t = pe.Var()
m.c = pe.Constraint(expr=(m.x - 1) ** 2 / m.y <= m.t)
m.l = pe.Constraint(expr=m.x >= 3)
m.o = pe.Objective(expr=m.t)
run("(x - 1)^2 / y <= t", m, 1.0, duals=True)

m = pe.ConcreteModel()
m.x = pe.Var(bounds=(0, 4))
m.y = pe.Var(bounds=(0, 9))
m.t = pe.Var()
m.c = pe.Constraint(expr=pe.sqrt(m.x * m.y) >= m.t)
m.o = pe.Objective(expr=m.t, sense=pe.maximize)
run("max t, sqrt(x y) >= t", m, 6.0, duals=True)

m = pe.ConcreteModel()
m.x = pe.Var(bounds=(0, 9))
m.o = pe.Objective(expr=sum(pe.sqrt(m.x + k) for k in range(3)) - m.x / 2,
                   sense=pe.maximize)
lo, hi = 1e-12, 9.0
for _ in range(200):
    xs = (lo + hi) / 2
    if sum(1 / math.sqrt(xs + k) for k in range(3)) > 1:
        lo = xs
    else:
        hi = xs
run("max sum sqrt(x + k) - x/2", m,
    sum(math.sqrt(xs + k) for k in range(3)) - xs / 2)

m = pe.ConcreteModel()
m.x = pe.Var(bounds=(-3, 3))
m.y = pe.Var(bounds=(-3, 3))
m.o = pe.Objective(expr=pe.sqrt((m.x - 1) ** 2 + 2 * (m.y + 2) ** 2 + 4)
                   + abs(m.x + m.y))
run("min sqrt((x-1)^2 + 2(y+2)^2 + 4) + |x + y|", m, math.sqrt(14 / 3))

ret = [0.12, 0.10, 0.07, 0.03]
sig = [0.20, 0.15, 0.10, 0.02]


def portfolio(root):
    m = pe.ConcreteModel()
    m.I = pe.RangeSet(0, 3)
    m.w = pe.Var(m.I, bounds=(0, 1))
    m.budget = pe.Constraint(expr=sum(m.w[i] for i in m.I) == 1)
    risk = sum((sig[i] * m.w[i]) ** 2 for i in m.I)
    m.risk = pe.Constraint(expr=pe.sqrt(risk) <= 0.08 if root
                           else risk <= 0.08 ** 2)
    m.o = pe.Objective(expr=sum(ret[i] * m.w[i] for i in m.I),
                       sense=pe.maximize)
    return m


q = portfolio(False)
pe.SolverFactory("asl:jaos", executable=exe).solve(q)
run("max return, sqrt(sum (s_i w_i)^2) <= 0.08, against the quadratic row",
    portfolio(True), pe.value(q.o))

m = pe.ConcreteModel()
m.x = pe.Var(within=pe.Integers, bounds=(-5, 5))
m.y = pe.Var(bounds=(-5, 5))
m.b = pe.Var(within=pe.Binary)
m.z = pe.Var(within=pe.Integers, bounds=(0, 9))
m.c = pe.Constraint(expr=m.y ** 2 + m.z <= 7 + 3 * m.b)
m.o = pe.Objective(expr=(m.x - 1.4) ** 2 + (m.y - 0.3) ** 2 - m.z - m.b)
run("MIQP, integer x inside the square", m, -9.84)

m = pe.ConcreteModel()
m.x = pe.Var(within=pe.Integers, bounds=(0, 10))
m.y = pe.Var(within=pe.Integers, bounds=(0, 10))
m.t = pe.Var()
m.c = pe.Constraint(expr=pe.sqrt((m.x - 3.4) ** 2 + (m.y - 6.6) ** 2) <= m.t)
m.o = pe.Objective(expr=m.t)
run("integer point nearest (3.4, 6.6)", m, math.sqrt(0.4 ** 2 + 0.4 ** 2))

m = pe.ConcreteModel()
m.x = pe.Var(bounds=(0, 2))
m.y = pe.Var(bounds=(0, 2))
m.o = pe.Objective(expr=(m.x + 1) * (m.x - 1) - m.x * m.x + (m.x - m.x) * m.y
                   * m.y + m.y / (m.x - m.x + 4) + 2 ** 3 * m.x ** 0)
run("min (x+1)(x-1) - x^2 + (x-x) y y + y/(x-x+4) + 2^3 x^0", m, 7.0)

k = pmo.block()
k.r = pmo.variable(lb=0)
k.x = pmo.variable_list([pmo.variable(), pmo.variable()])
k.q = pmo.conic.quadratic(k.r, k.x)
k.l = pmo.constraint(k.x[0] + 2 * k.x[1] >= 5)
k.o = pmo.objective(k.r)
opt = pe.SolverFactory("asl:jaos", executable=exe)
r = opt.solve(k, load_solutions=False)
k.load_solution(r.solution(0))
gap = abs(k.r.value - math.sqrt(5))
worst = max(worst, gap)
print(f"kernel conic.quadratic: {r.solver.termination_condition}, "
      f"objective {k.r.value:.12g}, expected {math.sqrt(5):.12g}, "
      f"off by {gap:.1e}")

k = pmo.block()
k.r1 = pmo.variable(lb=0)
k.r2 = pmo.variable(lb=0)
k.x = pmo.variable_list([pmo.variable(), pmo.variable()])
k.q = pmo.conic.rotated_quadratic(k.r1, k.r2, k.x)
k.l = pmo.constraint(k.x[0] + k.x[1] >= 2)
k.f = pmo.constraint(k.r2 == 1)
k.o = pmo.objective(k.r1)
r = opt.solve(k, load_solutions=False)
k.load_solution(r.solution(0))
gap = abs(k.r1.value - 1.0)
worst = max(worst, gap)
print(f"kernel conic.rotated_quadratic: {r.solver.termination_condition}, "
      f"objective {k.r1.value:.12g}, expected 1, off by {gap:.1e}")

for name, expr in [("exp", lambda x: pe.exp(x)),
                   ("x^3", lambda x: x ** 3),
                   ("sqrt(x) in a minimization", lambda x: -pe.sqrt(x) * -1),
                   ("x^1.5", lambda x: x ** 1.5)]:
    m = pe.ConcreteModel()
    m.x = pe.Var(bounds=(0, 3))
    m.o = pe.Objective(expr=expr(m.x))
    run(name, m, None)

print(f"largest gap to the expected optimum: {worst:.1e}")
