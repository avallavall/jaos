# SPDX-License-Identifier: Apache-2.0
using Test
import MathOptInterface as MOI
import JAOS

const DATA = joinpath(@__DIR__, "..", "..", "..", "tests", "data")

function _g1()
    m = JAOS.Model()
    JAOS.read_file(m, joinpath(DATA, "g1.lp"))
    return m
end

function _lp(cost, cl, cu, rl, ru, a_start, a_index, a_value; sense = :min)
    m = JAOS.Model()
    JAOS.load_lp(m, sense, 0.0, Float64.(cost), Float64.(cl), Float64.(cu),
                 Float64.(rl), Float64.(ru), Int64.(a_start), Int64.(a_index),
                 Float64.(a_value))
    return m
end

_infeasible() = _lp([1], [0], [Inf], [1, -Inf], [Inf, 0], [0, 2], [0, 1], [1, 1])

_unbounded() = _lp([-1, -1], [0, 0], [Inf, Inf], [-Inf], [1], [0, 1, 2], [0, 0],
                   [1, -1])

function _knapsack()
    m = _lp([10, 13, 7, 9, 5], zeros(5), ones(5), [-Inf], [8], 0:5,
            zeros(Int, 5), [3, 5, 2, 4, 2]; sense = :max)
    for j in 0:4
        JAOS.set_col_integer(m, j)
    end
    return m
end

function _caching()
    return MOI.Utilities.CachingOptimizer(
        MOI.Utilities.UniversalFallback(MOI.Utilities.Model{Float64}()),
        JAOS.Optimizer(),
    )
end

function test_the_ccall_layer_solves_a_file()
    m = _g1()
    JAOS.solve(m)
    @test JAOS.status(m) == 1
    @test JAOS.status_string(JAOS.status(m)) == "optimal"
    @test JAOS.objective(m) ≈ -5.0
    x, act, y, d = JAOS.solution(m)
    @test x ≈ [0.0, -1.0, 8.0]
    @test length(act) == length(y) == 3
    @test startswith(JAOS.version(), "0.")
    lines = String[]
    JAOS.set_log(m, 1; sink = line -> push!(lines, line))
    JAOS.solve(m)
    @test !isempty(lines)
    JAOS.set_log(m, 0)
    err = try
        JAOS.read_file(m, joinpath(DATA, "no_such_file.lp"))
        nothing
    catch e
        e
    end
    @test err isa JAOS.JaosError
    @test occursin("i/o error", sprint(showerror, err))
    @test JAOS.status_str(3) == "i/o error"
    @test JAOS.infinity() == Inf
end

function test_the_model_reads_back_and_edits()
    m = _g1()
    @test JAOS.num_nz(m) == 7
    @test JAOS.col_cost(m, 0) == 3.0
    @test JAOS.col_bounds(m, 1) == (-1.0, 8.0)
    @test JAOS.col_bounds(m, 2) == (-Inf, Inf)
    @test JAOS.row_bounds(m, 2) == (7.0, 7.0)
    @test JAOS.objective_sense(m) === :min
    @test JAOS.objective_offset(m) == 5.0
    @test JAOS.col_entries(m, 0) == ([0, 1, 2], [1.0, 2.0, 1.0])
    @test JAOS.row_entries(m, 1) == ([0, 1], [2.0, -1.0])
    @test JAOS.coefficient(m, 1, 2) == 0.0
    JAOS.solve(m)
    JAOS.set_col_bounds(m, 1, 0.0, 8.0)
    @test JAOS.status(m) == 0
    JAOS.solve(m)
    @test JAOS.objective(m) ≈ -2.0
    JAOS.set_objective_offset(m, 0.0)
    JAOS.set_col_cost(m, 2, 1.0)
    JAOS.set_objective_sense(m, :max)
    @test JAOS.objective_sense(m) === :max
    JAOS.solve(m)
    @test JAOS.objective(m) ≈ 27.0
    JAOS.set_objective_sense(m, :min)
    JAOS.set_row_bounds(m, 2, 7.0, 7.0)
    JAOS.set_coefficient(m, 0, 2, 1.0)
    @test JAOS.coefficient(m, 0, 2) == 1.0
    @test JAOS.num_nz(m) == 8
    JAOS.set_coefficient(m, 0, 2, 0.0)
    @test JAOS.num_nz(m) == 7

    c = copy(m)
    JAOS.add_rows(c, [-Inf], [1.0], [0, 1], [0], [1.0])
    @test JAOS.num_row(c) == 4
    @test JAOS.num_row(m) == 3
    JAOS.add_cols(c, [1.0], [0.0], [5.0], [0, 1], [3], [1.0])
    @test JAOS.num_col(c) == 4
    @test JAOS.col_entries(c, 3) == ([3], [1.0])
    JAOS.delete_cols(c, [3])
    JAOS.delete_rows(c, [3])
    @test (JAOS.num_row(c), JAOS.num_col(c)) == (3, 3)
    @test_throws JAOS.JaosError JAOS.delete_rows(c, [0, 0])
    @test_throws ArgumentError JAOS.add_cols(c, [1.0], [0.0], [1.0, 2.0])
end

function test_names_read_set_and_find()
    m = _g1()
    @test JAOS.col_name(m, 1) == "y"
    @test JAOS.row_name(m, 0) == "c1"
    @test JAOS.objective_name(m) == "obj"
    @test JAOS.col_index(m, "z") == 2
    @test JAOS.row_index(m, "c3") == 2
    JAOS.set_col_name(m, 1, "why")
    JAOS.set_row_name(m, 0, "cap")
    JAOS.set_objective_name(m, "cost")
    JAOS.set_model_name(m, "golden")
    @test JAOS.col_index(m, "why") == 1
    @test JAOS.row_name(m, 0) == "cap"
    @test JAOS.objective_name(m) == "cost"
    @test JAOS.model_name(m) == "golden"
    JAOS.set_col_name(m, 1, nothing)
    @test JAOS.col_name(m, 1) == "C2"
    @test_throws JAOS.JaosError JAOS.set_col_name(m, 0, "two words")
    @test_throws JAOS.JaosError JAOS.col_index(m, "nobody")
    c = copy(m)
    @test JAOS.model_name(c) == "golden"
    @test JAOS.row_name(c, 0) == "cap"
end

function test_every_model_writer_reads_back()
    m = _g1()
    JAOS.solve(m)
    want = JAOS.objective(m)
    mktempdir() do dir
        for ext in (".mps", ".lp", ".nl", ".qplib", ".cbf", ".osil", ".mps.gz")
            path = joinpath(dir, "g1" * ext)
            JAOS.write_file(m, path)
            back = JAOS.Model()
            JAOS.read_file(back, path)
            JAOS.solve(back)
            @test JAOS.objective(back) ≈ want
        end
        JAOS.write_lp(m, joinpath(dir, "direct.lp"))
        @test isfile(joinpath(dir, "direct.lp"))
        path = joinpath(dir, "g1.sol")
        JAOS.write_sol_ampl(m, path)
        @test occursin("objno", read(path, String))
        JAOS.write_sol_ampl(m, path, "hello")
        @test startswith(read(path, String), "hello")
    end
    q = JAOS.Model()
    JAOS.read_file(q, joinpath(DATA, "g_sos.lp"))
    mktempdir() do dir
        @test_throws JAOS.JaosError JAOS.write_nl(q, joinpath(dir, "q.nl"))
    end
end

function test_answer_files_round_trip()
    m = _g1()
    JAOS.solve(m)
    x, act, y, d = JAOS.solution(m)
    cs, rs = JAOS.basis(m)
    mktempdir() do dir
        sol = joinpath(dir, "g1.sol")
        JAOS.write_solution(m, sol)
        @test JAOS.solution_file_status(m, sol) == 1
        obj, (x2, act2, y2, d2), (cs2, rs2) = JAOS.read_solution(m, sol)
        @test obj ≈ -5.0
        @test x2 == x && y2 == y && d2 == d && act2 == act
        @test (cs2, rs2) == (cs, rs)
        @test JAOS.read_basis(m, sol) == (cs, rs)
        bas = joinpath(dir, "g1.bas")
        JAOS.write_mps_basis(m, bas)
        @test JAOS.read_mps_basis(m, bas) == (cs, rs)
        pt = joinpath(dir, "g1.pt")
        JAOS.write_point(m, pt)
        @test JAOS.read_point(m, pt) == x
        JAOS.write_point_values(m, pt, [1.0, 2.0, 3.0])
        @test JAOS.read_point(m, pt) == [1.0, 2.0, 3.0]
        du = joinpath(dir, "g1.duals")
        JAOS.write_duals(m, du)
        @test JAOS.read_duals(m, du) == y
        JAOS.write_dual_values(m, du, [0.5, 0.0, -1.0])
        @test JAOS.read_duals(m, du) == [0.5, 0.0, -1.0]
        @test_throws ArgumentError JAOS.write_point_values(m, pt, [1.0])

        f = _infeasible()
        JAOS.solve(f)
        cert = joinpath(dir, "f.sol")
        JAOS.write_solution(f, cert)
        @test JAOS.solution_file_status(f, cert) == 2
        st, ray = JAOS.read_certificate(f, cert)
        @test st == 2
        @test ray == JAOS.certificate(f)
        @test_throws JAOS.JaosError JAOS.read_solution(f, cert)

        c = JAOS.Model()
        JAOS.read_file(c, joinpath(DATA, "g_cone.mps"))
        JAOS.solve(c)
        conic = joinpath(dir, "c.sol")
        JAOS.write_solution(c, conic)
        z = JAOS.read_cone_duals(c, conic)
        @test length(z) == 1
        @test z[1] ≈ [1.0, -0.6, -0.8] atol = 1e-6
    end
end

function test_a_basis_goes_back_in()
    m = JAOS.Model()
    JAOS.read_file(m, joinpath(DATA, "solve1.mps"))
    JAOS.solve(m)
    cs, rs = JAOS.basis(m)
    fresh = JAOS.Model()
    JAOS.read_file(fresh, joinpath(DATA, "solve1.mps"))
    JAOS.set_basis(fresh, cs, rs)
    JAOS.solve(fresh)
    @test JAOS.objective(fresh) ≈ 29.0
    @test JAOS.iterations(fresh) == 0
    JAOS.clear_basis(fresh)
    JAOS.solve(fresh)
    @test JAOS.iterations(fresh) > 0
    @test_throws ArgumentError JAOS.set_basis(fresh, cs[1:1], rs)
end

function test_marks_sos_indicators_and_their_getters()
    s = JAOS.Model()
    JAOS.read_file(s, joinpath(DATA, "g_sos.lp"))
    @test JAOS.num_sos(s) == 2
    @test JAOS.sos(s, 0) == (2, [0, 1, 2], [1.0, 2.0, 3.0])
    @test JAOS.sos(s, 1) == (1, [2, 0], [0.5, 4.0])
    i = JAOS.Model()
    JAOS.read_file(i, joinpath(DATA, "g_ind.lp"))
    @test JAOS.row_indicator(i, 0) == (1, 1)
    @test JAOS.col_integer(i, 1)
    @test !JAOS.col_integer(i, 0)
    JAOS.set_row_indicator(i, 0, -1, 1)
    @test JAOS.row_indicator(i, 0) == (nothing, 0)
    c = JAOS.Model()
    JAOS.read_file(c, joinpath(DATA, "g_semi.lp"))
    @test JAOS.col_semicontinuous(c, 0)
    @test !JAOS.col_semicontinuous(c, 1)
    JAOS.solve(c)
    @test JAOS.objective(c) ≈ 2.0
end

function test_quadratic_and_cone_getters()
    q = JAOS.Model()
    JAOS.read_file(q, joinpath(DATA, "g_quad.lp"))
    @test JAOS.quadratic_nz(q) == 2
    @test JAOS.quadratic(q) == ([0, 1], [0, 1], [2.0, 2.0])
    @test JAOS.col_quadratic(q, 0) == 2.0
    JAOS.set_col_quadratic(q, 0, 4.0)
    @test JAOS.col_quadratic(q, 0) == 4.0
    r = JAOS.Model()
    JAOS.read_file(r, joinpath(DATA, "g_qcp.mps"))
    @test JAOS.row_quadratic_nz(r, 0) == 2
    @test JAOS.row_quadratic(r, 0) == ([0, 1], [0, 1], [2.0, 2.0])
    c = JAOS.Model()
    JAOS.read_file(c, joinpath(DATA, "g_cone.mps"))
    @test JAOS.cone(c, 0) == (:quadratic, [0, 1, 2])
    JAOS.solve(c)
    @test JAOS.cone_dual(c, 0) ≈ [1.0, -0.6, -0.8] atol = 1e-6
    JAOS.delete_cones(c, [0])
    @test JAOS.num_cones(c) == 0
end

function test_the_pool_keeps_the_best_points()
    m = _knapsack()
    JAOS.set_mip_cut_rounds(m, 0)
    JAOS.set_mip_cover_rounds(m, 0)
    JAOS.set_mip_mir_rounds(m, 0)
    JAOS.set_mip_cut_depth(m, 0)
    JAOS.set_mip_pool_size(m, 3)
    @test JAOS.get_option(m, "mip_pool_size") == "3"
    JAOS.solve(m)
    pool = JAOS.mip_pool(m)
    @test 1 <= length(pool) <= 3
    @test length(pool) == JAOS.mip_pool_count(m)
    @test pool[1][2] ≈ 23.0
    objs = [v for (_, v) in pool]
    @test objs == sort(objs; rev = true)
    @test JAOS.mip_pool_solution(m, 0) == pool[1]
    @test_throws JAOS.JaosError JAOS.set_mip_pool_size(m, 0)
    rep = JAOS.mip_report(m)
    @test rep.has_incumbent && rep.incumbent ≈ 23.0
    @test JAOS.has_integer(m) && !JAOS.has_integer(_g1())
    x, v = JAOS.mip_incumbent(m)
    @test v ≈ 23.0 && x == pool[1][1]
end

function test_the_callbacks_see_the_solve_and_can_stop_it()
    m = JAOS.Model()
    JAOS.read_file(m, joinpath(DATA, "solve1.mps"))
    seen = JAOS.Progress[]
    JAOS.set_progress_callback(m, p -> (push!(seen, p); nothing))
    JAOS.solve(m)
    @test !isempty(seen)
    @test seen[1].iterations == 0
    JAOS.set_progress_callback(m, p -> :stop)
    JAOS.solve(m)
    @test JAOS.status(m) == 7
    JAOS.set_progress_callback(m, p -> error("broken callback"))
    @test_throws ErrorException JAOS.solve(m)
    JAOS.set_progress_callback(m, nothing)
    JAOS.solve(m)
    @test JAOS.status(m) == 1

    k = _knapsack()
    incumbents = JAOS.Incumbent[]
    JAOS.set_incumbent_callback(k, inc -> (push!(incumbents, inc); nothing))
    JAOS.solve(k)
    @test !isempty(incumbents)
    @test incumbents[end].objective ≈ 23.0
    @test length(incumbents[end].values) == 5
    JAOS.set_incumbent_callback(k, inc -> :stop)
    JAOS.solve(k)
    @test JAOS.status(k) == 7
    JAOS.set_incumbent_callback(k, nothing)

    p = _lp([2, 2, 1], zeros(3), ones(3), [-Inf], [2], [0, 1, 2, 3], [0, 0, 0],
            [1, 1, 1]; sense = :max)
    for j in 0:2
        JAOS.set_col_integer(p, j)
    end
    events = Tuple{Int,Bool}[]
    JAOS.set_node_callback(p, function (ev)
        push!(events, (ev.node, ev.integral))
        if ev.integral && ev.values[1] + ev.values[2] > 1.5
            JAOS.node_add_row(ev, [0, 1], [1.0, 1.0], -Inf, 1.0)
        end
        return nothing
    end)
    JAOS.solve(p)
    @test JAOS.objective(p) ≈ 3.0
    @test any(last, events)
    held = copy(p)
    JAOS.solve(held)
    @test JAOS.objective(held) ≈ 3.0
    JAOS.set_node_callback(p, ev -> JAOS.node_add_row(ev, [7], [1.0], 0.0, 1.0))
    @test_throws JAOS.JaosError JAOS.solve(p)
    JAOS.set_node_callback(p, ev -> :stop)
    JAOS.solve(p)
    @test JAOS.status(p) == 7
    kept = Ref{Any}(nothing)
    JAOS.set_node_callback(p, ev -> (kept[] = ev; nothing))
    JAOS.solve(p)
    @test_throws JAOS.JaosError JAOS.node_add_row(kept[], [0], [1.0])
    JAOS.set_node_callback(p, nothing)
    JAOS.solve(p)
    @test JAOS.objective(p) ≈ 4.0

    h = _lp([10, 13, 7, 8, 11, 5], zeros(6), ones(6), [-Inf], [12],
            [0, 1, 2, 3, 4, 5, 6], zeros(Int, 6), [4, 6, 3, 4, 5, 2]; sense = :max)
    for j in 0:5
        JAOS.set_col_integer(h, j)
    end
    JAOS.set_mip_cut_rounds(h, 0)
    JAOS.set_mip_cover_rounds(h, 0)
    JAOS.set_mip_mir_rounds(h, 0)
    JAOS.set_mip_heuristics(h, false)
    JAOS.set_mip_node_limit(h, 1)
    short_refused = Ref(false)
    JAOS.set_node_callback(h, function (ev)
        JAOS.node_add_solution(ev, [1.0, 0.0, 1.0, 0.0, 1.0, 0.0])
        try
            JAOS.node_add_solution(ev, [1.0, 0.0, 1.0])
        catch e
            short_refused[] = e isa JAOS.JaosError
        end
        return nothing
    end)
    JAOS.solve(h)
    @test JAOS.status(h) == 8
    rep = JAOS.mip_report(h)
    @test rep.has_incumbent
    @test rep.incumbent ≈ 28.0
    @test short_refused[]

    s = _lp([3, 1, 1], zeros(3), ones(3), [-Inf, -Inf], [3, 3], [0, 2, 3, 4],
            [0, 1, 0, 1], [2, 2, 2, 2]; sense = :max)
    for j in 0:2
        JAOS.set_col_integer(s, j)
    end
    for fn in (JAOS.set_mip_cut_rounds, JAOS.set_mip_cover_rounds,
               JAOS.set_mip_mir_rounds, JAOS.set_mip_clique_rounds,
               JAOS.set_mip_cut_depth, JAOS.set_mip_dive_heuristic,
               JAOS.set_mip_feaspump, JAOS.set_mip_tighten)
        fn(s, 0)
    end
    JAOS.set_mip_heuristics(s, false)
    chosen, deeper = Int[], Float64[]
    JAOS.set_node_callback(s, function (ev)
        if ev.depth == 0 && !ev.integral
            ev.branch_col = ev.branch_col == 1 ? 2 : 1
            push!(chosen, ev.branch_col)
        elseif ev.depth == 1
            push!(deeper, ev.values[chosen[1]+1])
        end
        return nothing
    end)
    JAOS.solve(s)
    @test JAOS.objective(s) ≈ 3.0
    @test !isempty(deeper)
    @test all(v -> v == 0.0 || v == 1.0, deeper)
end

function test_the_checkers_judge_answers()
    m = _g1()
    JAOS.solve(m)
    x, _, y, _ = JAOS.solution(m)
    r = JAOS.check_solution(m, x, y)
    @test r.primal_feasible && r.dual_feasible && r.checked_duals
    @test r.primal_objective ≈ -5.0
    bad = JAOS.check_solution(m, x .+ 100.0)
    @test !bad.primal_feasible
    @test !bad.checked_duals
    @test_throws ArgumentError JAOS.check_solution(m, [1.0])

    f = _infeasible()
    JAOS.solve(f)
    cr = JAOS.check_certificate(f, JAOS.certificate(f))
    @test cr.certified
    @test !JAOS.check_certificate(f, [0.0, 0.0]).certified

    u = _unbounded()
    JAOS.solve(u)
    @test JAOS.status(u) == 3
    rr = JAOS.check_ray(u, JAOS.unbounded_ray(u))
    @test rr.certified
    @test rr.rate < 0.0
    @test !JAOS.check_ray(u, [-1.0, 0.0]).certified

    c = JAOS.Model()
    JAOS.read_file(c, joinpath(DATA, "g_cone.mps"))
    JAOS.solve(c)
    xc, _, yc, _ = JAOS.solution(c)
    cc = JAOS.check_conic_solution(c, xc, yc, [JAOS.cone_dual(c, 0)]; tol = 1e-6)
    @test cc.primal_feasible
    @test cc.max_cone_violation < 1e-6
    b = _lp([0, 0], [-Inf, 2], [1, 2], Float64[], Float64[], [0, 0, 0], Int[],
            Float64[])
    JAOS.add_cone(b, :quadratic, [0, 1])
    JAOS.solve(b)
    @test JAOS.status(b) == 2
    zb = JAOS.cone_dual(b, 0)
    @test JAOS.check_conic_certificate(b, Float64[], [zb]).certified
    @test !JAOS.check_conic_certificate(b, Float64[], [[-1.0, 0.0]]).certified
    @test_throws ArgumentError JAOS.check_conic_certificate(b, Float64[], [])
end

function test_exact_verification_and_proof_files()
    m = _lp([-1, -1], [0, 0], [3, Inf], [-Inf], [4], [0, 1, 2], [0, 0], [1, 1])
    JAOS.solve(m)
    r = JAOS.verify(m)
    @test r.capacity_bits == 4096.0
    @test r.status == 0 && r.stage == 0
    @test JAOS.exact_objective(m) == -4 // 1
    @test JAOS.exact_row_dual(m, 0) == -1 // 1
    vb = JAOS.verify_basis(m, [2, 0], [2])
    @test vb.status == 0
    @test JAOS.exact_col_value(m, 0) == 3 // 1
    vb = JAOS.verify_basis(m, [1, 1], [0])
    @test vb.status == 1 && vb.stage == 3
    @test_throws ArgumentError JAOS.verify_basis(m, [0], [0])

    t = _lp([1], [0], [Inf], [1], [Inf], [0, 1], [0], [3])
    JAOS.solve(t)
    @test JAOS.verify(t).status == 0
    @test JAOS.exact_col_value(t, 0) == 1 // 3
    mktempdir() do dir
        path = joinpath(dir, "t.proof")
        JAOS.write_proof(t, path)
        rep = JAOS.check_proof(t, path)
        @test rep.primal && rep.dual && rep.objective && rep.certified
        @test rep.kind == 0 && rep.bad_row == -1
        write(path, replace(read(path, String), "1/3" => "1/4"))
        @test !JAOS.check_proof(t, path).certified

        f = _lp([1, 1], [0, 0], [Inf, Inf], [-Inf, 2], [1, Inf], [0, 2, 4],
                [0, 1, 0, 1], [1, 1, 1, 1])
        JAOS.solve(f)
        e = JAOS.exact_certificate(f)
        @test e.derived
        @test e.bound_bits <= e.capacity_bits
        @test JAOS.exact_row_multiplier(f, 0) == -1 // 1
        @test JAOS.exact_row_multiplier(f, 1) == 1 // 1
        JAOS.write_proof(f, path)
        @test JAOS.check_proof(f, path).kind == 1

        u = _lp([-1, 0], [0, 0], [Inf, Inf], [-Inf], [1], [0, 1, 2], [0, 0],
                [1, -1])
        JAOS.solve(u)
        @test JAOS.status(u) == 3
        @test JAOS.exact_unbounded_ray(u).derived
        dx, dy = JAOS.exact_col_direction(u, 0), JAOS.exact_col_direction(u, 1)
        @test dx isa Rational{BigInt}
        @test dx == dy > 0
        JAOS.write_proof(u, path)
        @test JAOS.check_proof(u, path).kind == 2
    end
end

function test_the_subsystem_and_the_relaxation()
    f = _infeasible()
    JAOS.solve(f)
    found = JAOS.iis(f)
    @test found[1] == [1, 2]
    @test found[2] == [0]
    @test found[3].members == 2
    sub = JAOS.iis_model(f, found)
    JAOS.solve(sub)
    @test JAOS.status(sub) == 2
    loose = JAOS.iis_model(f, [0, 2], found[2])
    JAOS.solve(loose)
    @test JAOS.status(loose) == 1
    @test_throws ArgumentError JAOS.iis_model(f, [0], found[2])
    rows, cols, rep = JAOS.feasrelax(f, :rows)
    @test rep.total ≈ 1.0
    @test rep.status == 1
    @test sum(abs, rows) ≈ 1.0
    @test cols == [0.0]
    @test JAOS.feasrelax(f)[3].total ≈ 1.0
    @test_throws ArgumentError JAOS.feasrelax(f, :everything)
end

function test_ranging_reads_the_optimal_basis()
    m = _g1()
    @test_throws JAOS.JaosError JAOS.cost_ranging(m)
    JAOS.solve(m)
    lo, hi = JAOS.cost_ranging(m)
    c = [3.0, 2.0, -1.0]
    @test all(lo .<= c .<= hi)
    a, b, cc, d = JAOS.rhs_ranging(m)
    @test length(a) == length(d) == 3
    @test a[3] <= 7.0 <= b[3]
    a, b, cc, d = JAOS.bound_ranging(m)
    @test length(a) == 3
end

function test_statistics_options_and_typed_setters()
    m = _g1()
    st = JAOS.statistics(m)
    @test (st.num_row, st.num_col, st.num_nz) == (3, 3, 7)
    @test st.equality_row == 1 && st.free_col == 1
    @test JAOS.presolve_report(m).rounds == 0
    JAOS.solve(m)
    @test JAOS.presolve_report(m).num_col <= 3
    @test JAOS.solve_time(m) >= 0.0
    names = JAOS.option_names()
    @test length(names) == 57
    @test "mip_gap" in names
    mktempdir() do dir
        path = joinpath(dir, "opts.txt")
        write(path, "# options\nmip_gap = 0.25\nwork_limit 99\n")
        JAOS.read_options(m, path)
    end
    @test parse(Float64, JAOS.get_option(m, "mip_gap")) == 0.25
    @test JAOS.get_option(m, "work_limit") == "99"
    JAOS.set_work_limit(m, 0)
    JAOS.set_algorithm(m, 1)
    @test JAOS.algorithm(m) == 1
    JAOS.set_threads(m, 2)
    @test JAOS.threads(m) == 2
    JAOS.set_primal_tolerance(m, 1e-8)
    JAOS.set_mip_cutoff(m, 100.0)
    JAOS.set_mip_dive(m, true)
    JAOS.set_mip_restart(m, 1)
    @test JAOS.get_option(m, "mip_dive") == "true"
    @test JAOS.get_option(m, "mip_restart") == "true"
    @test parse(Float64, JAOS.get_option(m, "mip_cutoff")) == 100.0
    @test_throws JAOS.JaosError JAOS.set_threads(m, 0)
    JAOS.solve(m)
    @test JAOS.objective(m) ≈ -5.0
    @test JAOS.work_units(m) > 0
    t = _g1()
    wrong = String[]
    special = Dict(:set_mip_node_select => 0, :set_mip_tree_batch => 3,
                   :set_work_limit => 1000, :set_primal_tolerance => 1e-8,
                   :set_dual_tolerance => 1e-8, :set_mip_cutoff => 100.5,
                   :set_mip_probe_cap => 2.0, :set_mip_probing_cap => 2.0)
    for (fn, T) in JAOS._SETTERS
        name = String(fn)[5:end]
        set = getfield(JAOS, fn)
        if T === Bool
            on = JAOS.get_option(t, name) == "true"
            set(t, !on)
            JAOS.get_option(t, name) == string(!on) || push!(wrong, name)
        elseif T === Cint
            set(t, 0)
            a = JAOS.get_option(t, name)
            set(t, 1)
            a != JAOS.get_option(t, name) || push!(wrong, name)
        elseif T === Int64
            v = get(special, fn, 2)
            set(t, v)
            JAOS.get_option(t, name) == string(v) || push!(wrong, name)
        else
            v = get(special, fn, 0.25)
            set(t, v)
            parse(Float64, JAOS.get_option(t, name)) == v || push!(wrong, name)
        end
    end
    JAOS.set_mip_gap(t, 0.5)
    JAOS.set_mip_node_limit(t, 7)
    JAOS.set_time_limit(t, 2.5)
    @test parse(Float64, JAOS.get_option(t, "mip_gap")) == 0.5
    @test JAOS.get_option(t, "mip_node_limit") == "7"
    @test parse(Float64, JAOS.get_option(t, "time_limit")) == 2.5
    @test isempty(wrong)
    @test length(JAOS._SETTERS) == 52
end

function test_moi_conformance()
    model = MOI.Bridges.full_bridge_optimizer(_caching(), Float64)
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

function _two_by_two()
    model = _caching()
    MOI.set(model, MOI.Silent(), true)
    x = MOI.add_variables(model, 2)
    lb = [MOI.add_constraint(model, xi, MOI.GreaterThan(0.0)) for xi in x]
    ub = MOI.add_constraint(model, x[1], MOI.LessThan(3.0))
    f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([1.0, 1.0], x), 0.0)
    c = MOI.add_constraint(model, f, MOI.LessThan(4.0))
    obj = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([-1.0, -2.0], x), 0.0)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MIN_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    return model, x, lb, ub, c
end

function _warm(model, inner)
    @test MOI.Utilities.state(model) == MOI.Utilities.ATTACHED_OPTIMIZER
    @test MOI.get(model.optimizer, MOI.RawSolver()) === inner
    MOI.optimize!(model)
    @test MOI.get(model, MOI.TerminationStatus()) == MOI.OPTIMAL
    return MOI.get(model, MOI.ObjectiveValue())
end

function test_moi_changes_reach_the_loaded_model()
    model, x, lb, ub, c = _two_by_two()
    MOI.optimize!(model)
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ -8.0
    inner = MOI.get(model.optimizer, MOI.RawSolver())
    @test inner isa JAOS.Model
    MOI.set(model, MOI.ConstraintSet(), c, MOI.LessThan(2.0))
    @test MOI.get(model.optimizer, MOI.TerminationStatus()) == MOI.OPTIMIZE_NOT_CALLED
    @test _warm(model, inner) ≈ -4.0
    @test JAOS.row_bounds(inner, 0) == (-Inf, 2.0)
    MOI.modify(model, MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Float64}}(),
               MOI.ScalarCoefficientChange(x[1], -3.0))
    @test _warm(model, inner) ≈ -6.0
    @test JAOS.col_cost(inner, 0) == -3.0
    MOI.modify(model, MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Float64}}(),
               MOI.ScalarConstantChange(10.0))
    @test _warm(model, inner) ≈ 4.0
    MOI.set(model, MOI.ConstraintSet(), ub, MOI.LessThan(1.0))
    @test _warm(model, inner) ≈ 10.0 - 3.0 - 2.0
    @test JAOS.col_bounds(inner, 0) == (0.0, 1.0)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    @test _warm(model, inner) ≈ 10.0
    @test JAOS.objective_sense(inner) === :max
    MOI.set(model, MOI.ObjectiveSense(), MOI.MIN_SENSE)
    MOI.modify(model, c, MOI.ScalarCoefficientChange(x[2], 2.0))
    @test JAOS.coefficient(inner, 0, 1) == 2.0
    @test _warm(model, inner) ≈ 6.0
    @test MOI.get(model, MOI.ConstraintPrimal(), c) ≈ 2.0
    MOI.modify(model, c, MOI.ScalarConstantChange(1.0))
    @test _warm(model, inner) ≈ 7.0
    @test MOI.get(model, MOI.ConstraintPrimal(), c) ≈ 2.0
    @test JAOS.row_bounds(inner, 0) == (-Inf, 1.0)
    MOI.delete(model, ub)
    @test _warm(model, inner) ≈ 7.0
    @test JAOS.col_bounds(inner, 0) == (0.0, Inf)
    ub = MOI.add_constraint(model, x[1], MOI.LessThan(0.5))
    @test _warm(model, inner) ≈ 8.0
    @test JAOS.col_bounds(inner, 0) == (0.0, 0.5)
    MOI.delete(model, lb[2])
    fixed = MOI.add_constraint(model, x[2], MOI.EqualTo(0.25))
    @test _warm(model, inner) ≈ 8.0
    @test JAOS.col_bounds(inner, 1) == (0.25, 0.25)
    @test_throws MOI.LowerBoundAlreadySet MOI.add_constraint(
        model, x[2], MOI.GreaterThan(0.0))
    obj = MOI.ScalarAffineFunction([MOI.ScalarAffineTerm(1.0, x[1])], 0.0)
    MOI.set(model, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    @test _warm(model, inner) ≈ 0.0
    MOI.set(model, MOI.ObjectiveSense(), MOI.FEASIBILITY_SENSE)
    @test _warm(model, inner) ≈ 0.0
    @test JAOS.col_cost(inner, 0) == 0.0

    cold, y, _, _, _ = _two_by_two()
    MOI.optimize!(cold)
    before = MOI.get(cold.optimizer, MOI.RawSolver())
    MOI.add_variable(cold)
    MOI.optimize!(cold)
    @test MOI.get(cold.optimizer, MOI.RawSolver()) !== before
end

function test_moi_warm_resolve_agrees_with_a_cold_one()
    model = _caching()
    MOI.set(model, MOI.Silent(), true)
    x = MOI.add_variables(model, 3)
    for xi in x
        MOI.add_constraint(model, xi, MOI.Interval(0.0, 10.0))
    end
    rows = [MOI.add_constraint(model,
                MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(a, x), 0.0),
                MOI.LessThan(b))
            for (a, b) in (([1.0, 2.0, 1.0], 14.0), ([3.0, 1.0, 2.0], 18.0),
                           ([1.0, 1.0, 3.0], 12.0))]
    obj = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([-2.0, -3.0, -4.0], x), 0.0)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MIN_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    MOI.optimize!(model)
    cold_iterations = MOI.get(model, MOI.SimplexIterations())
    MOI.set(model, MOI.ConstraintSet(), rows[2], MOI.LessThan(17.0))
    MOI.optimize!(model)
    warm = MOI.get(model, MOI.ObjectiveValue())
    @test MOI.get(model, MOI.SimplexIterations()) < cold_iterations
    fresh = MOI.Utilities.Model{Float64}()
    MOI.copy_to(fresh, model)
    other = JAOS.Optimizer()
    MOI.set(other, MOI.Silent(), true)
    MOI.optimize!(other, fresh)
    @test MOI.get(other, MOI.ObjectiveValue()) ≈ warm
end

function _mip(n)
    model = _caching()
    MOI.set(model, MOI.Silent(), true)
    x = MOI.add_variables(model, n)
    for xi in x
        MOI.add_constraint(model, xi, MOI.ZeroOne())
    end
    return model, x
end

function test_moi_lazy_constraints_and_user_cuts()
    model, x = _mip(3)
    f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, x), 0.0)
    MOI.add_constraint(model, f, MOI.LessThan(2.0))
    obj = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([2.0, 2.0, 1.0], x), 0.0)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    @test MOI.supports(JAOS.Optimizer(), MOI.LazyConstraintCallback())
    @test !MOI.supports(JAOS.Optimizer(), MOI.HeuristicCallback())
    statuses = MOI.CallbackNodeStatusCode[]
    MOI.set(model, MOI.LazyConstraintCallback(), function (cb)
        push!(statuses, MOI.get(model, MOI.CallbackNodeStatus(cb)))
        v = MOI.get(model, MOI.CallbackVariablePrimal(cb), x)
        if v[1] + v[2] > 1.5
            g = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, x[1:2]), 0.0)
            MOI.submit(model, MOI.LazyConstraint(cb), g, MOI.LessThan(1.0))
        end
    end)
    MOI.optimize!(model)
    @test MOI.get(model, MOI.TerminationStatus()) == MOI.OPTIMAL
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ 3.0
    v = MOI.get(model, MOI.VariablePrimal(), x)
    @test v[1] + v[2] <= 1.0 + 1e-9
    @test !isempty(statuses)
    @test all(==(MOI.CALLBACK_NODE_STATUS_INTEGER), statuses)

    MOI.set(model, MOI.LazyConstraintCallback(), function (cb)
        MOI.get(model, MOI.VariablePrimal(), x[1])
    end)
    @test_throws MOI.OptimizeInProgress MOI.optimize!(model)
    MOI.set(model, MOI.LazyConstraintCallback(), nothing)
    MOI.optimize!(model)
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ 4.0

    cuts, y = _mip(3)
    g = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(2.0, y), 0.0)
    MOI.add_constraint(cuts, g, MOI.LessThan(3.0))
    obj = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([3.0, 2.5, 2.0], y), 0.0)
    MOI.set(cuts, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    MOI.set(cuts, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    for name in ("mip_cut_rounds", "mip_cover_rounds", "mip_mir_rounds",
                 "mip_clique_rounds", "mip_cut_depth", "mip_dive_heuristic",
                 "mip_feaspump", "mip_tighten")
        MOI.set(cuts, MOI.RawOptimizerAttribute(name), 0)
    end
    MOI.set(cuts, MOI.RawOptimizerAttribute("mip_heuristics"), false)
    MOI.optimize!(cuts)
    plain = MOI.get(cuts, MOI.NodeCount())
    @test plain > 1
    added = Ref(0)
    MOI.set(cuts, MOI.UserCutCallback(), function (cb)
        @test MOI.get(cuts, MOI.CallbackNodeStatus(cb)) ==
              MOI.CALLBACK_NODE_STATUS_FRACTIONAL
        v = MOI.get(cuts, MOI.CallbackVariablePrimal(cb), y)
        if sum(v) > 1.0 + 1e-9
            h = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, y), 0.0)
            MOI.submit(cuts, MOI.UserCut(cb), h, MOI.LessThan(1.0))
            added[] += 1
        end
    end)
    MOI.optimize!(cuts)
    @test MOI.get(cuts, MOI.ObjectiveValue()) ≈ 3.0
    @test added[] >= 1
    @test MOI.get(cuts, MOI.NodeCount()) < plain

    MOI.set(cuts, MOI.UserCutCallback(), function (cb)
        h = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, y), 0.0)
        MOI.submit(cuts, MOI.LazyConstraint(cb), h, MOI.LessThan(1.0))
    end)
    @test_throws MOI.InvalidCallbackUsage MOI.optimize!(cuts)
end

function test_moi_result_count_reads_the_pool()
    model, x = _mip(5)
    f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([3.0, 5.0, 2.0, 4.0, 2.0], x), 0.0)
    c = MOI.add_constraint(model, f, MOI.LessThan(8.0))
    obj = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([10.0, 13.0, 7.0, 9.0, 5.0], x), 0.0)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(obj)}(), obj)
    MOI.optimize!(model)
    @test MOI.get(model, MOI.ResultCount()) == 1
    MOI.set(model, MOI.RawOptimizerAttribute("mip_pool_size"), 4)
    for name in ("mip_cut_rounds", "mip_cover_rounds", "mip_mir_rounds", "mip_cut_depth")
        MOI.set(model, MOI.RawOptimizerAttribute(name), 0)
    end
    MOI.optimize!(model)
    n = MOI.get(model, MOI.ResultCount())
    @test 2 <= n <= 4
    @test MOI.get(model, MOI.ObjectiveValue(1)) ≈ 23.0
    objs = [MOI.get(model, MOI.ObjectiveValue(k)) for k in 1:n]
    @test objs == sort(objs; rev = true)
    for k in 2:n
        @test MOI.get(model, MOI.PrimalStatus(k)) == MOI.FEASIBLE_POINT
        @test MOI.get(model, MOI.DualStatus(k)) == MOI.NO_SOLUTION
        v = MOI.get(model, MOI.VariablePrimal(k), x)
        @test sum(obj.terms[j].coefficient * v[j] for j in 1:5) ≈ objs[k]
        @test MOI.get(model, MOI.ConstraintPrimal(k), c) ≈
              sum(f.terms[j].coefficient * v[j] for j in 1:5)
    end
    @test MOI.get(model, MOI.PrimalStatus(n + 1)) == MOI.NO_SOLUTION
    @test_throws MOI.ResultIndexBoundsError MOI.get(model, MOI.VariablePrimal(n + 1), x[1])
end

function test_moi_names_reach_the_library()
    model = _caching()
    MOI.set(model, MOI.Silent(), true)
    x = MOI.add_variables(model, 3)
    MOI.set(model, MOI.VariableName(), x[1], "apples")
    MOI.set(model, MOI.VariableName(), x[2], "two words")
    for xi in x
        MOI.add_constraint(model, xi, MOI.GreaterThan(0.0))
    end
    f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, x), 0.0)
    c = MOI.add_constraint(model, f, MOI.GreaterThan(1.0))
    MOI.set(model, MOI.ConstraintName(), c, "demand")
    MOI.set(model, MOI.Name(), "fruit")
    MOI.set(model, MOI.ObjectiveSense(), MOI.MIN_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(f)}(), f)
    MOI.optimize!(model)
    inner = MOI.get(model.optimizer, MOI.RawSolver())
    @test JAOS.col_name(inner, 0) == "apples"
    @test JAOS.col_name(inner, 1) == "C2"
    @test JAOS.col_name(inner, 2) == "C3"
    @test JAOS.row_name(inner, 0) == "demand"
    @test JAOS.model_name(inner) == "fruit"
    @test JAOS.col_index(inner, "apples") == 0
    MOI.set(model.optimizer, MOI.Name(), "veg")
    @test JAOS.model_name(inner) == "veg"
end

function test_moi_start_values_reach_the_loaded_model()
    model, x = _mip(2)
    f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(1.0, x), 0.0)
    MOI.add_constraint(model, f, MOI.LessThan(1.0))
    MOI.set(model, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    MOI.set(model, MOI.ObjectiveFunction{typeof(f)}(), f)
    MOI.optimize!(model)
    inner = MOI.get(model.optimizer, MOI.RawSolver())
    MOI.set(model, MOI.VariablePrimalStart(), x[2], 1.0)
    @test MOI.get(model.optimizer, MOI.RawSolver()) === inner
    MOI.optimize!(model)
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ 1.0
end

for name in names(@__MODULE__; all = true)
    if startswith("$name", "test_")
        @testset "$name" begin
            getfield(@__MODULE__, name)()
        end
    end
end
