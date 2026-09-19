# SPDX-License-Identifier: Apache-2.0
# JAOS as an AMPL solver under JuMP: small models through
# AmplNLWriter.Optimizer, printing what JuMP reads back from the .sol.
#
# usage: julia jump_asl.jl JAOS
using JuMP, AmplNLWriter

const exe = ARGS[1]

function report(label, m)
    optimize!(m)
    println(label, ": ", termination_status(m), ", primal ",
            primal_status(m), ", dual ", dual_status(m), "; ", raw_status(m))
    if primal_status(m) == FEASIBLE_POINT
        vals = join(["$(name(v)) $(value(v))" for v in all_variables(m)], ", ")
        println("   ", vals, "; objective ", objective_value(m))
    end
    if dual_status(m) == FEASIBLE_POINT
        cons = all_constraints(m; include_variable_in_set_constraints = false)
        println("   duals ", join(["$(name(c)) $(dual(c))" for c in cons], ", "))
    end
end

m = Model(() -> AmplNLWriter.Optimizer(exe))
@variable(m, 0 <= x <= 4)
@variable(m, y >= 0)
@constraint(m, c1, x + 2y <= 8)
@constraint(m, c2, 3x - y >= -2)
@constraint(m, c3, 1 <= x + y <= 5)
@objective(m, Min, -3x - 2y + 7)
report("lp", m)

k = Model(() -> AmplNLWriter.Optimizer(exe))
@variable(k, 0 <= i <= 10, Int)
@variable(k, b, Bin)
@variable(k, z >= 0)
@constraint(k, 2i + 3z <= 13)
@constraint(k, z - 5b <= 0)
@objective(k, Max, 5i + 4z + 2b)
report("mip", k)

f = Model(() -> AmplNLWriter.Optimizer(exe))
@variable(f, 0 <= x <= 1)
@constraint(f, x >= 2)
@objective(f, Min, x)
report("infeasible", f)

u = Model(() -> AmplNLWriter.Optimizer(exe))
@variable(u, x >= 0)
@constraint(u, x >= 1)
@objective(u, Max, x)
report("unbounded", u)

w = [23, 31, 29, 44, 53, 38, 63, 85, 89, 82, 17, 29, 41, 11, 7, 97, 53, 71]
p = [92, 57, 49, 68, 60, 43, 67, 84, 87, 72, 29, 41, 50, 12, 9, 99, 55, 70]
for (label, args) in (("knapsack", String[]),
                      ("knapsack, work_limit 1", ["work_limit=1"]),
                      ("knapsack, an unknown option", ["nosuch=1"]))
    s = Model(() -> AmplNLWriter.Optimizer(exe, args))
    @variable(s, xs[1:length(w)], Bin)
    @constraint(s, sum(w[j] * xs[j] for j in eachindex(w)) <= 300)
    @objective(s, Max, sum(p[j] * xs[j] for j in eachindex(w)))
    report(label, s)
end
