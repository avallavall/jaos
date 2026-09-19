# SPDX-License-Identifier: Apache-2.0
using Test
import MathOptInterface as MOI
import JAOS

const DATA = joinpath(@__DIR__, "..", "..", "..", "tests", "data")

function test_the_ccall_layer_solves_a_file()
    m = JAOS.Model()
    JAOS.read_file(m, joinpath(DATA, "g1.lp"))
    JAOS.solve(m)
    @test JAOS.status(m) == 1
    @test JAOS.status_string(JAOS.status(m)) == "optimal"
    @test JAOS.objective(m) ≈ -5.0
    x, act, y, d = JAOS.solution(m)
    @test x ≈ [0.0, -1.0, 8.0]
    @test length(act) == length(y) == 3
    @test startswith(JAOS.version(), "0.")
    err = try
        JAOS.read_file(m, joinpath(DATA, "no_such_file.lp"))
        nothing
    catch e
        e
    end
    @test err isa JAOS.JaosError
end

function test_moi_conformance()
    model = MOI.Bridges.full_bridge_optimizer(
        MOI.Utilities.CachingOptimizer(
            MOI.Utilities.UniversalFallback(MOI.Utilities.Model{Float64}()),
            JAOS.Optimizer(),
        ),
        Float64,
    )
    MOI.set(model, MOI.Silent(), true)
    config = MOI.Test.Config(atol = 1e-6, rtol = 1e-6)
    MOI.Test.runtests(
        model,
        config;
        exclude = [
            "test_quadratic_SecondOrderCone_basic",
            "test_quadratic_nonconvex_constraint_basic",
            "test_quadratic_nonconvex_constraint_integration",
            "test_solve_conflict_zeroone_2",
        ],
    )
    return
end

function test_raw_options_and_names()
    o = JAOS.Optimizer()
    @test MOI.get(o, MOI.SolverName()) == "JAOS"
    MOI.set(o, MOI.RawOptimizerAttribute("mip_gap"), 1e-4)
    @test MOI.get(o, MOI.RawOptimizerAttribute("mip_gap")) == 1e-4
    @test_throws MOI.UnsupportedAttribute MOI.set(
        o, MOI.RawOptimizerAttribute("no_such_option"), 1)
    return
end

for name in names(@__MODULE__; all = true)
    if startswith("$name", "test_")
        @testset "$name" begin
            getfield(@__MODULE__, name)()
        end
    end
end
