# SPDX-License-Identifier: Apache-2.0

const _SAF = MOI.ScalarAffineFunction{Float64}
const _SQF = MOI.ScalarQuadraticFunction{Float64}
const _VAF = MOI.VectorAffineFunction{Float64}
const _VOV = MOI.VectorOfVariables
const _BOUND = Union{MOI.GreaterThan{Float64},MOI.LessThan{Float64},
                     MOI.EqualTo{Float64},MOI.Interval{Float64}}
const _SIDE = Union{MOI.GreaterThan{Float64},MOI.LessThan{Float64}}
const _SIDES = Union{MOI.GreaterThan{Float64},MOI.LessThan{Float64},
                     MOI.EqualTo{Float64}}
const _KIND = Union{MOI.Integer,MOI.ZeroOne,MOI.Semicontinuous{Float64},
                    MOI.Semiinteger{Float64}}
const _CONE = Union{MOI.SecondOrderCone,MOI.RotatedSecondOrderCone}
const _SOS = Union{MOI.SOS1{Float64},MOI.SOS2{Float64}}
const _BOUND_SETS = (MOI.GreaterThan{Float64}, MOI.LessThan{Float64},
                     MOI.EqualTo{Float64}, MOI.Interval{Float64})

_bounds(s::MOI.GreaterThan) = (s.lower, Inf)
_bounds(s::MOI.LessThan) = (-Inf, s.upper)
_bounds(s::MOI.EqualTo) = (s.value, s.value)
_bounds(s::MOI.Interval) = (s.lower, s.upper)
_bounds(s::Union{MOI.Semicontinuous,MOI.Semiinteger}) = (s.lower, s.upper)

_fires_at(::Type{MOI.Indicator{A,S}}) where {A,S} =
    A == MOI.ACTIVATE_ON_ONE ? 1 : 0

_tag(::Type{<:MOI.GreaterThan}) = :greater
_tag(::Type{<:MOI.LessThan}) = :less
_tag(::Type{<:MOI.EqualTo}) = :equal
_tag(::Type{<:MOI.Interval}) = :interval
_tag(::Type{MOI.ZeroOne}) = :zeroone
_tag(::Type{<:MOI.Semicontinuous}) = :semicontinuous
_tag(::Type{<:MOI.Semiinteger}) = :semiinteger

const _Quadratic = Tuple{Vector{Int64},Vector{Int64},Vector{Float64}}

"""
    Optimizer()

JAOS as a MathOptInterface optimizer. It takes a whole model at once
through `MOI.copy_to`, so JuMP keeps its own copy of the model in front
of it:

    model = JuMP.Model(JAOS.Optimizer)

Variables may carry bounds and be integer, binary, semi-continuous or
semi-integer. Constraints may be affine, convex quadratic with one side,
second-order and rotated second-order cones over variables, SOS1 and
SOS2 sets and indicator rows, as far as the C library solves each
combination. `MOI.RawOptimizerAttribute` reaches every option of
`jaos_set_option`.
"""
mutable struct Optimizer <: MOI.AbstractOptimizer
    inner::Union{Nothing,Model}
    silent::Bool
    time_limit::Union{Nothing,Float64}
    threads::Union{Nothing,Int}
    gap::Union{Nothing,Float64}
    node_limit::Union{Nothing,Int}
    raw::Dict{String,Any}
    name::String
    sense::MOI.OptimizationSense
    nvar::Int
    lower::Vector{Float64}
    upper::Vector{Float64}
    lower_by::Vector{Symbol}
    upper_by::Vector{Symbol}
    discrete::Bool
    cost::Vector{Float64}
    a_start::Vector{Int64}
    a_index::Vector{Int64}
    a_value::Vector{Float64}
    row_constant::Vector{Float64}
    row_lower::Vector{Float64}
    row_upper::Vector{Float64}
    offset::Float64
    quad::Union{Nothing,_Quadratic}
    row_quad::Vector{Tuple{Int,_Quadratic}}
    cones::Vector{Vector{Int64}}
    sos::Vector{Vector{Int64}}
    indicators::Vector{Tuple{Int64,Int64}}
    solved::Bool
    error::Union{Nothing,JaosError}
    code::Int
    primal::MOI.ResultStatusCode
    dual::MOI.ResultStatusCode
    x::Vector{Float64}
    act::Vector{Float64}
    y::Vector{Float64}
    d::Vector{Float64}
    z::Vector{Vector{Float64}}
    objective::Float64
    bound::Float64
    nodes::Int64
    iterations::Int64
    time::Float64
    basis::Union{Nothing,Tuple{Vector{Cint},Vector{Cint}}}
    conflict::MOI.ConflictStatusCode
    row_side::Vector{Cint}
    col_side::Vector{Cint}
    function Optimizer()
        o = new()
        o.silent = false
        o.time_limit = nothing
        o.threads = nothing
        o.gap = nothing
        o.node_limit = nothing
        o.raw = Dict{String,Any}()
        _clear!(o)
        return o
    end
end

function _clear!(o::Optimizer)
    o.inner = nothing
    o.name = ""
    o.sense = MOI.FEASIBILITY_SENSE
    o.nvar = 0
    o.lower = Float64[]
    o.upper = Float64[]
    o.lower_by = Symbol[]
    o.upper_by = Symbol[]
    o.discrete = false
    o.cost = Float64[]
    o.a_start = Int64[0]
    o.a_index = Int64[]
    o.a_value = Float64[]
    o.row_constant = Float64[]
    o.row_lower = Float64[]
    o.row_upper = Float64[]
    o.offset = 0.0
    o.quad = nothing
    o.row_quad = Tuple{Int,_Quadratic}[]
    o.cones = Vector{Int64}[]
    o.sos = Vector{Int64}[]
    o.indicators = Tuple{Int64,Int64}[]
    _forget!(o)
    return
end

function _forget!(o::Optimizer)
    o.solved = false
    o.error = nothing
    o.code = 0
    o.primal = MOI.NO_SOLUTION
    o.dual = MOI.NO_SOLUTION
    o.x = Float64[]
    o.act = Float64[]
    o.y = Float64[]
    o.d = Float64[]
    o.z = Vector{Float64}[]
    o.objective = NaN
    o.bound = NaN
    o.nodes = 0
    o.iterations = 0
    o.time = 0.0
    o.basis = nothing
    o.conflict = MOI.COMPUTE_CONFLICT_NOT_CALLED
    o.row_side = Cint[]
    o.col_side = Cint[]
    return
end

Base.show(io::IO, o::Optimizer) =
    print(io, "JAOS.Optimizer with ", o.nvar, " variables")

MOI.get(::Optimizer, ::MOI.SolverName) = "JAOS"
MOI.get(::Optimizer, ::MOI.SolverVersion) = version()
MOI.get(o::Optimizer, ::MOI.RawSolver) = o.inner
MOI.is_empty(o::Optimizer) = o.inner === nothing
MOI.empty!(o::Optimizer) = _clear!(o)
MOI.get(o::Optimizer, ::MOI.NumberOfVariables) = o.nvar

MOI.supports(::Optimizer, ::MOI.Name) = true
MOI.get(o::Optimizer, ::MOI.Name) = o.name
MOI.set(o::Optimizer, ::MOI.Name, name::String) = (o.name = name; nothing)

function _log(o::Optimizer, m::Model)
    set_log(m, o.silent ? 0 : 1)
    if !o.silent && haskey(o.raw, "log_level")
        set_option(m, "log_level", o.raw["log_level"])
    end
    return
end

MOI.supports(::Optimizer, ::MOI.Silent) = true
MOI.get(o::Optimizer, ::MOI.Silent) = o.silent
function MOI.set(o::Optimizer, ::MOI.Silent, value::Bool)
    o.silent = value
    o.inner === nothing || _log(o, o.inner)
    return
end

MOI.supports(::Optimizer, ::MOI.TimeLimitSec) = true
MOI.get(o::Optimizer, ::MOI.TimeLimitSec) = o.time_limit
function MOI.set(o::Optimizer, ::MOI.TimeLimitSec, value::Union{Nothing,Real})
    o.time_limit = value === nothing ? nothing : Float64(value)
    o.inner === nothing || set_time_limit(o.inner, something(o.time_limit, 0.0))
    return
end

MOI.supports(::Optimizer, ::MOI.NumberOfThreads) = true
MOI.get(o::Optimizer, ::MOI.NumberOfThreads) = o.threads
function MOI.set(o::Optimizer, ::MOI.NumberOfThreads, value::Union{Nothing,Integer})
    o.threads = value === nothing ? nothing : Int(value)
    o.inner === nothing || set_threads(o.inner, something(o.threads, 1))
    return
end

MOI.supports(::Optimizer, ::MOI.RelativeGapTolerance) = true
MOI.get(o::Optimizer, ::MOI.RelativeGapTolerance) = o.gap
function MOI.set(o::Optimizer, ::MOI.RelativeGapTolerance, value::Union{Nothing,Real})
    o.gap = value === nothing ? nothing : Float64(value)
    o.inner === nothing || set_mip_gap(o.inner, something(o.gap, 0.0))
    return
end

MOI.supports(::Optimizer, ::MOI.NodeLimit) = true
MOI.get(o::Optimizer, ::MOI.NodeLimit) = o.node_limit
function MOI.set(o::Optimizer, ::MOI.NodeLimit, value::Union{Nothing,Integer})
    o.node_limit = value === nothing ? nothing : Int(value)
    o.inner === nothing || set_mip_node_limit(o.inner, something(o.node_limit, 0))
    return
end

function _apply_options(o::Optimizer, m::Model)
    _log(o, m)
    o.time_limit === nothing || set_time_limit(m, o.time_limit)
    o.threads === nothing || set_threads(m, o.threads)
    o.gap === nothing || set_mip_gap(m, o.gap)
    o.node_limit === nothing || set_mip_node_limit(m, o.node_limit)
    for key in sort!(collect(keys(o.raw)))
        set_option(m, key, o.raw[key])
    end
    return
end

MOI.supports(::Optimizer, ::MOI.RawOptimizerAttribute) = true
function MOI.set(o::Optimizer, attr::MOI.RawOptimizerAttribute, value)
    target = o.inner === nothing ? Model() : o.inner
    try
        set_option(target, attr.name, value)
    catch e
        e isa JaosError || rethrow()
        throw(MOI.UnsupportedAttribute(attr, e.msg))
    end
    o.raw[attr.name] = value
    return
end
function MOI.get(o::Optimizer, attr::MOI.RawOptimizerAttribute)
    haskey(o.raw, attr.name) && return o.raw[attr.name]
    try
        return get_option(o.inner === nothing ? Model() : o.inner, attr.name)
    catch e
        e isa JaosError || rethrow()
        throw(MOI.GetAttributeNotAllowed(attr, e.msg))
    end
end

MOI.supports(::Optimizer, ::MOI.ObjectiveSense) = true
MOI.supports(::Optimizer,
             ::MOI.ObjectiveFunction{<:Union{MOI.VariableIndex,_SAF,_SQF}}) = true
MOI.supports(::Optimizer, ::MOI.VariablePrimalStart, ::Type{MOI.VariableIndex}) = true

MOI.supports_constraint(::Optimizer, ::Type{MOI.VariableIndex},
                        ::Type{<:Union{_BOUND,_KIND}}) = true
MOI.supports_constraint(::Optimizer, ::Type{_SAF}, ::Type{<:_BOUND}) = true
MOI.supports_constraint(::Optimizer, ::Type{_SQF}, ::Type{<:_SIDE}) = true
MOI.supports_constraint(::Optimizer, ::Type{_VOV},
                        ::Type{<:Union{_CONE,_SOS}}) = true
MOI.supports_constraint(::Optimizer, ::Type{_VAF},
                        ::Type{MOI.Indicator{A,S}}) where {A,S<:_SIDES} = true

function _pairs(terms, col)
    acc = Dict{Tuple{Int64,Int64},Float64}()
    for t in terms
        a, b = col(t.variable_1) - 1, col(t.variable_2) - 1
        key = a >= b ? (a, b) : (b, a)
        acc[key] = get(acc, key, 0.0) + t.coefficient
    end
    keys_ = sort!([k for (k, v) in acc if v != 0.0])
    return (Int64[k[1] for k in keys_], Int64[k[2] for k in keys_],
            Float64[acc[k] for k in keys_])
end

function _check_attributes(o::Optimizer, src::MOI.ModelLike)
    for attr in MOI.get(src, MOI.ListOfModelAttributesSet())
        if attr isa MOI.ObjectiveFunction
            MOI.supports(o, attr) || throw(MOI.UnsupportedAttribute(attr))
        elseif !(attr isa Union{MOI.Name,MOI.ObjectiveSense})
            throw(MOI.UnsupportedAttribute(attr))
        end
    end
    for attr in MOI.get(src, MOI.ListOfVariableAttributesSet())
        attr isa Union{MOI.VariableName,MOI.VariablePrimalStart} ||
            throw(MOI.UnsupportedAttribute(attr))
    end
    for (F, S) in MOI.get(src, MOI.ListOfConstraintTypesPresent())
        MOI.supports_constraint(o, F, S) ||
            throw(MOI.UnsupportedConstraint{F,S}())
        for attr in MOI.get(src, MOI.ListOfConstraintAttributesSet{F,S}())
            attr isa MOI.ConstraintName || throw(MOI.UnsupportedAttribute(attr))
        end
    end
    return
end

function MOI.copy_to(o::Optimizer, src::MOI.ModelLike)
    MOI.empty!(o)
    _check_attributes(o, src)
    index = MOI.Utilities.IndexMap()
    vars = MOI.get(src, MOI.ListOfVariableIndices())
    n = length(vars)
    for (j, v) in enumerate(vars)
        index[v] = MOI.VariableIndex(j)
    end
    col(v::MOI.VariableIndex) = index[v].value
    lower, upper = fill(-Inf, n), fill(Inf, n)
    lower_by, upper_by = fill(:none, n), fill(:none, n)
    integer, semi = falses(n), falses(n)
    copy_of = zeros(Int, n)
    function tighten!(j, l, u, S)
        if l > -Inf && l >= lower[j]
            lower[j], lower_by[j] = l, _tag(S)
        end
        if u < Inf && u <= upper[j]
            upper[j], upper_by[j] = u, _tag(S)
        end
        return
    end
    for S in _BOUND_SETS
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{MOI.VariableIndex,S}())
            j = col(MOI.get(src, MOI.ConstraintFunction(), ci))
            tighten!(j, _bounds(MOI.get(src, MOI.ConstraintSet(), ci))..., S)
            index[ci] = MOI.ConstraintIndex{MOI.VariableIndex,S}(j)
        end
    end
    for S in (MOI.Integer, MOI.ZeroOne, MOI.Semicontinuous{Float64},
              MOI.Semiinteger{Float64})
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{MOI.VariableIndex,S}())
            j = col(MOI.get(src, MOI.ConstraintFunction(), ci))
            integer[j] |= !(S <: MOI.Semicontinuous)
            if S <: MOI.ZeroOne
                tighten!(j, 0.0, 1.0, S)
            elseif S <: Union{MOI.Semicontinuous,MOI.Semiinteger}
                semi[j] = true
                tighten!(j, _bounds(MOI.get(src, MOI.ConstraintSet(), ci))..., S)
            end
            index[ci] = MOI.ConstraintIndex{MOI.VariableIndex,S}(j)
        end
    end

    rl, ru, constant = Float64[], Float64[], Float64[]
    I, J, V = Int[], Int[], Float64[]
    function add_row!(l, u, c, cols, vals)
        push!(rl, l - c)
        push!(ru, u - c)
        push!(constant, c)
        r = length(rl)
        append!(I, fill(r, length(cols)))
        append!(J, cols)
        append!(V, vals)
        return r
    end
    affine(terms) = ([col(t.variable) for t in terms],
                     [t.coefficient for t in terms])
    for S in _BOUND_SETS
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{_SAF,S}())
            f = MOI.Utilities.canonical(MOI.get(src, MOI.ConstraintFunction(), ci))
            l, u = _bounds(MOI.get(src, MOI.ConstraintSet(), ci))
            r = add_row!(l, u, f.constant, affine(f.terms)...)
            index[ci] = MOI.ConstraintIndex{_SAF,S}(r)
        end
    end
    row_quad = Tuple{Int,_Quadratic}[]
    for S in (MOI.GreaterThan{Float64}, MOI.LessThan{Float64})
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{_SQF,S}())
            f = MOI.Utilities.canonical(MOI.get(src, MOI.ConstraintFunction(), ci))
            l, u = _bounds(MOI.get(src, MOI.ConstraintSet(), ci))
            r = add_row!(l, u, f.constant, affine(f.affine_terms)...)
            push!(row_quad, (r, _pairs(f.quadratic_terms, col)))
            index[ci] = MOI.ConstraintIndex{_SQF,S}(r)
        end
    end
    cones, kinds = Vector{Int64}[], Symbol[]
    taken = falses(n)
    for S in (MOI.SecondOrderCone, MOI.RotatedSecondOrderCone)
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{_VOV,S}())
            members = Int64[]
            for v in MOI.get(src, MOI.ConstraintFunction(), ci).variables
                j = col(v)
                if taken[j]
                    push!(lower, -Inf)
                    push!(upper, Inf)
                    push!(lower_by, :none)
                    push!(upper_by, :none)
                    push!(integer, false)
                    push!(semi, false)
                    push!(taken, false)
                    push!(copy_of, j)
                    k = length(lower)
                    add_row!(0.0, 0.0, 0.0, [k, j], [1.0, -1.0])
                    j = k
                end
                taken[j] = true
                push!(members, j - 1)
            end
            push!(cones, members)
            push!(kinds, S == MOI.RotatedSecondOrderCone ? :rotated : :quadratic)
            index[ci] = MOI.ConstraintIndex{_VOV,S}(length(cones))
        end
    end
    sos, sos_type, sos_weight = Vector{Int64}[], Int[], Vector{Float64}[]
    for S in (MOI.SOS1{Float64}, MOI.SOS2{Float64})
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{_VOV,S}())
            f = MOI.get(src, MOI.ConstraintFunction(), ci)
            push!(sos, Int64[col(v) - 1 for v in f.variables])
            push!(sos_type, S <: MOI.SOS1 ? 1 : 2)
            push!(sos_weight, copy(MOI.get(src, MOI.ConstraintSet(), ci).weights))
            index[ci] = MOI.ConstraintIndex{_VOV,S}(length(sos))
        end
    end
    indicators, fires = Tuple{Int64,Int64}[], Int[]
    for (F, S) in MOI.get(src, MOI.ListOfConstraintTypesPresent())
        (F == _VAF && S <: MOI.Indicator) || continue
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{F,S}())
            rows = MOI.Utilities.eachscalar(MOI.get(src, MOI.ConstraintFunction(), ci))
            z = MOI.Utilities.canonical(rows[1])
            g = MOI.Utilities.canonical(rows[2])
            if length(z.terms) != 1 || z.terms[1].coefficient != 1.0 ||
               !iszero(z.constant)
                throw(MOI.UnsupportedConstraint{F,S}(
                    "the first row of an indicator must be one variable alone"))
            end
            l, u = _bounds(MOI.get(src, MOI.ConstraintSet(), ci).set)
            r = add_row!(l, u, g.constant, affine(g.terms)...)
            push!(indicators, (col(z.terms[1].variable) - 1, r - 1))
            push!(fires, _fires_at(S))
            index[ci] = MOI.ConstraintIndex{F,S}(length(indicators))
        end
    end

    ncol, nrow = length(lower), length(rl)
    sense = MOI.get(src, MOI.ObjectiveSense())
    cost, offset, quad = zeros(ncol), 0.0, nothing
    if sense != MOI.FEASIBILITY_SENSE
        F = MOI.get(src, MOI.ObjectiveFunctionType())
        f = MOI.get(src, MOI.ObjectiveFunction{F}())
        if f isa MOI.VariableIndex
            cost[col(f)] += 1.0
        else
            f = MOI.Utilities.canonical(f)
            offset = f.constant
            for t in (f isa _SQF ? f.affine_terms : f.terms)
                cost[col(t.variable)] += t.coefficient
            end
            f isa _SQF && (quad = _pairs(f.quadratic_terms, col))
        end
    end

    a_start = zeros(Int64, ncol + 1)
    for j in J
        a_start[j+1] += 1
    end
    cumsum!(a_start, a_start)
    a_index, a_value = zeros(Int64, length(J)), zeros(length(J))
    fill_at = a_start[1:ncol]
    for k in eachindex(J)
        p = fill_at[J[k]] += 1
        a_index[p] = I[k] - 1
        a_value[p] = V[k]
    end

    m = Model()
    load_lp(m, sense == MOI.MAX_SENSE ? :max : :min, offset, cost, lower,
            upper, rl, ru, a_start, a_index, a_value)
    for j in 1:ncol
        integer[j] && set_col_integer(m, j - 1)
        semi[j] && set_col_semicontinuous(m, j - 1)
    end
    quad === nothing || isempty(quad[3]) || set_quadratic(m, quad...)
    for (r, q) in row_quad
        set_row_quadratic(m, r - 1, q...)
    end
    for (k, members) in enumerate(cones)
        add_cone(m, kinds[k], members)
    end
    for (k, members) in enumerate(sos)
        add_sos(m, sos_type[k], members, sos_weight[k])
    end
    for (k, (c, r)) in enumerate(indicators)
        set_row_indicator(m, r, c, fires[k])
    end
    discrete = any(integer) || any(semi) || !isempty(sos) || !isempty(indicators)
    if discrete && MOI.VariablePrimalStart() in
                   MOI.get(src, MOI.ListOfVariableAttributesSet())
        start = zeros(ncol)
        for (j, v) in enumerate(vars)
            s = MOI.get(src, MOI.VariablePrimalStart(), v)
            s === nothing || (start[j] = s)
        end
        for j in n+1:ncol
            start[j] = start[copy_of[j]]
        end
        set_mip_start(m, start)
    end
    _apply_options(o, m)

    o.inner = m
    o.name = MOI.get(src, MOI.Name())
    o.sense = sense
    o.nvar = n
    o.lower, o.upper = lower, upper
    o.lower_by, o.upper_by = lower_by, upper_by
    o.discrete = discrete
    o.cost = cost
    o.a_start, o.a_index, o.a_value = a_start, a_index, a_value
    o.row_constant = constant
    o.row_lower, o.row_upper = rl, ru
    o.offset = offset
    o.quad = quad
    o.row_quad = row_quad
    o.cones = cones
    o.sos = sos
    o.indicators = indicators
    return index
end

function _activity(o::Optimizer, x::Vector{Float64}, quadratic::Bool)
    act = zeros(length(o.row_constant))
    for j in 1:length(o.lower), p in o.a_start[j]+1:o.a_start[j+1]
        act[o.a_index[p]+1] += o.a_value[p] * x[j]
    end
    quadratic || return act
    for (r, (rows, cols, vals)) in o.row_quad
        for k in eachindex(vals)
            a, b = rows[k] + 1, cols[k] + 1
            act[r] += a == b ? 0.5 * vals[k] * x[a]^2 : vals[k] * x[a] * x[b]
        end
    end
    return act
end

function _bound_part(o::Optimizer, y::Vector{Float64}, z::Vector{Vector{Float64}})
    a = zeros(length(o.lower))
    for j in 1:length(o.lower), p in o.a_start[j]+1:o.a_start[j+1]
        a[j] += o.a_value[p] * y[o.a_index[p]+1]
    end
    for (k, members) in enumerate(o.cones), (t, j) in enumerate(members)
        a[j+1] += z[k][t]
    end
    return -a
end

function _empty_model!(o::Optimizer)
    m = Model()
    load_lp(m, :min, 0.0, Float64[], Float64[], Float64[], Float64[],
            Float64[], Int64[0], Int64[], Float64[])
    _apply_options(o, m)
    o.inner = m
    return
end

function MOI.optimize!(o::Optimizer)
    o.inner === nothing && _empty_model!(o)
    _forget!(o)
    m = o.inner
    try
        solve(m)
    catch e
        e isa JaosError || rethrow()
        o.error = e
    end
    o.solved = true
    o.time = solve_time(m)
    o.iterations = iterations(m)
    o.error === nothing || return
    o.code = status(m)
    sigma = o.sense == MOI.MAX_SENSE ? -1.0 : 1.0
    if o.discrete
        report = try
            mip_report(m)
        catch e
            e isa JaosError || rethrow()
            nothing
        end
        if report !== nothing
            o.nodes = report.nodes
            o.bound = report.bound
        end
    end
    if o.code == 1
        o.x, o.act, y, d = solution(m)
        o.objective = objective(m)
        o.primal = MOI.FEASIBLE_POINT
        if !o.discrete
            z = [cone_dual(m, k - 1, length(c)) for (k, c) in enumerate(o.cones)]
            o.bound = o.objective
            if all(!isnothing, z)
                o.y, o.d = sigma .* y, sigma .* d
                o.z = Vector{Float64}[sigma .* zk for zk in z]
                o.dual = MOI.FEASIBLE_POINT
            end
        end
    elseif o.code == 2 && !o.discrete
        y = certificate(m)
        z = [cone_dual(m, k - 1, length(c)) for (k, c) in enumerate(o.cones)]
        if y !== nothing && all(!isnothing, z)
            o.y, o.z = y, Vector{Float64}[zk for zk in z]
            o.d = _bound_part(o, y, o.z)
            o.dual = MOI.INFEASIBILITY_CERTIFICATE
        end
    elseif o.code == 3 && !o.discrete
        ray = unbounded_ray(m)
        if ray !== nothing
            o.x = ray
            o.act = _activity(o, ray, false)
            o.objective = sum(o.cost .* ray; init = 0.0)
            o.primal = MOI.INFEASIBILITY_CERTIFICATE
        end
    elseif o.discrete && o.code in (4, 5, 6, 7, 8)
        found = try
            mip_incumbent(m)
        catch e
            e isa JaosError || rethrow()
            nothing
        end
        if found !== nothing
            o.x, o.objective = found
            o.act = _activity(o, o.x, true)
            o.primal = MOI.FEASIBLE_POINT
        end
    end
    return
end

const _TERMINATION = (MOI.OPTIMIZE_NOT_CALLED, MOI.OPTIMAL, MOI.INFEASIBLE,
                      MOI.DUAL_INFEASIBLE, MOI.OTHER_LIMIT, MOI.TIME_LIMIT,
                      MOI.NUMERICAL_ERROR, MOI.INTERRUPTED, MOI.NODE_LIMIT)

function MOI.get(o::Optimizer, ::MOI.TerminationStatus)
    o.solved || return MOI.OPTIMIZE_NOT_CALLED
    if o.error !== nothing
        s = o.error.status
        return s == 1 ? MOI.INVALID_MODEL : s == 2 ? MOI.MEMORY_LIMIT :
               s == 4 ? MOI.NUMERICAL_ERROR : MOI.OTHER_ERROR
    end
    return _TERMINATION[o.code+1]
end

function MOI.get(o::Optimizer, ::MOI.RawStatusString)
    o.solved || return "optimize not called"
    o.error === nothing || return o.error.msg
    return status_string(o.code)
end

MOI.get(o::Optimizer, ::MOI.ResultCount) =
    o.primal != MOI.NO_SOLUTION || o.dual != MOI.NO_SOLUTION ? 1 : 0

MOI.get(o::Optimizer, attr::MOI.PrimalStatus) =
    attr.result_index == 1 ? o.primal : MOI.NO_SOLUTION
MOI.get(o::Optimizer, attr::MOI.DualStatus) =
    attr.result_index == 1 ? o.dual : MOI.NO_SOLUTION

function MOI.get(o::Optimizer, attr::MOI.ObjectiveValue)
    MOI.check_result_index_bounds(o, attr)
    return o.objective
end

function _sided(y::Vector{Float64}, lower::Vector{Float64}, upper::Vector{Float64})
    total = 0.0
    for (k, v) in enumerate(y)
        side = v > 0.0 ? lower[k] : upper[k]
        v != 0.0 && isfinite(side) && (total += side * v)
    end
    return total
end

function MOI.get(o::Optimizer, attr::MOI.DualObjectiveValue)
    _dual(o, attr)
    isempty(o.row_quad) || throw(MOI.GetAttributeNotAllowed(attr,
        "JAOS gives no dual objective for a model with quadratic rows"))
    sigma = o.sense == MOI.MAX_SENSE ? -1.0 : 1.0
    total = sigma * (_sided(o.y, o.row_lower, o.row_upper) +
                     _sided(o.d, o.lower, o.upper))
    o.dual == MOI.INFEASIBILITY_CERTIFICATE && return total
    value = o.offset + total
    if o.quad !== nothing
        rows, cols, vals = o.quad
        for k in eachindex(vals)
            a, b = rows[k] + 1, cols[k] + 1
            value -= a == b ? 0.5 * vals[k] * o.x[a]^2 : vals[k] * o.x[a] * o.x[b]
        end
    end
    return value
end

function MOI.get(o::Optimizer, attr::MOI.ObjectiveBound)
    isnan(o.bound) || return o.bound
    return o.sense == MOI.MAX_SENSE ? Inf : -Inf
end

function MOI.get(o::Optimizer, attr::MOI.RelativeGap)
    MOI.check_result_index_bounds(o, attr)
    return abs(MOI.get(o, MOI.ObjectiveBound()) - o.objective) / abs(o.objective)
end

MOI.get(o::Optimizer, ::MOI.SolveTimeSec) = o.time
MOI.get(o::Optimizer, ::MOI.NodeCount) = o.nodes
MOI.get(o::Optimizer, ::MOI.SimplexIterations) = o.iterations

function _primal(o::Optimizer, attr)
    MOI.check_result_index_bounds(o, attr)
    o.primal == MOI.NO_SOLUTION &&
        throw(MOI.GetAttributeNotAllowed(attr, "the solve left no primal point"))
    return
end

function _dual(o::Optimizer, attr)
    MOI.check_result_index_bounds(o, attr)
    o.dual == MOI.NO_SOLUTION &&
        throw(MOI.GetAttributeNotAllowed(attr, "the solve left no dual point"))
    return
end

function MOI.get(o::Optimizer, attr::MOI.VariablePrimal, v::MOI.VariableIndex)
    _primal(o, attr)
    return o.x[v.value]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{MOI.VariableIndex})
    _primal(o, attr)
    return o.x[c.value]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{<:Union{_SAF,_SQF}})
    _primal(o, attr)
    point = o.primal == MOI.FEASIBLE_POINT
    return o.act[c.value] + (point ? o.row_constant[c.value] : 0.0)
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VOV,<:_CONE})
    _primal(o, attr)
    return o.x[o.cones[c.value].+1]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VOV,<:_SOS})
    _primal(o, attr)
    return o.x[o.sos[c.value].+1]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VAF,<:MOI.Indicator})
    _primal(o, attr)
    zc, r = o.indicators[c.value]
    return [o.x[zc+1], o.act[r+1] + o.row_constant[r+1]]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintDual,
                 c::MOI.ConstraintIndex{<:Union{_SAF,_SQF}})
    _dual(o, attr)
    return o.y[c.value]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintDual,
                 c::MOI.ConstraintIndex{MOI.VariableIndex,S}) where {S<:_BOUND}
    _dual(o, attr)
    v = o.d[c.value]
    S <: MOI.GreaterThan && return max(v, 0.0)
    S <: MOI.LessThan && return min(v, 0.0)
    return v
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintDual,
                 c::MOI.ConstraintIndex{_VOV,<:_CONE})
    _dual(o, attr)
    return o.z[c.value]
end

function _basis(o::Optimizer, attr)
    MOI.check_result_index_bounds(o, attr)
    if o.basis === nothing && o.inner !== nothing && o.code == 1
        o.basis = basis(o.inner)
    end
    o.basis === nothing &&
        throw(MOI.GetAttributeNotAllowed(attr, "the solve left no basis"))
    return o.basis
end

const _NONBASIC = (MOI.BASIC, MOI.NONBASIC_AT_LOWER, MOI.NONBASIC_AT_UPPER,
                   MOI.SUPER_BASIC)

function MOI.get(o::Optimizer, attr::MOI.VariableBasisStatus, v::MOI.VariableIndex)
    s = _basis(o, attr)[1][v.value]
    fixed = o.lower[v.value] == o.upper[v.value]
    return (s == 1 || s == 2) && fixed ? MOI.NONBASIC : _NONBASIC[s+1]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintBasisStatus,
                 c::MOI.ConstraintIndex{_SAF,S}) where {S<:_BOUND}
    s = _basis(o, attr)[2][c.value]
    (s == 1 || s == 2) && !(S <: MOI.Interval) && return MOI.NONBASIC
    return _NONBASIC[s+1]
end

function MOI.compute_conflict!(o::Optimizer)
    o.solved || throw(MOI.OptimizeNotCalled())
    o.conflict = MOI.NO_CONFLICT_FOUND
    o.error === nothing || return
    if o.code == 1
        o.conflict = MOI.NO_CONFLICT_EXISTS
    elseif o.code == 2
        found = iis(o.inner)
        if found !== nothing
            o.row_side, o.col_side, _ = found
            o.conflict = MOI.CONFLICT_FOUND
        end
    end
    return
end

MOI.get(o::Optimizer, ::MOI.ConflictStatus) = o.conflict

function _in_conflict(o::Optimizer)
    o.conflict == MOI.CONFLICT_FOUND ||
        throw(MOI.GetAttributeNotAllowed(MOI.ConstraintConflictStatus(),
                                         "no conflict was found"))
    return
end

function MOI.get(o::Optimizer, ::MOI.ConstraintConflictStatus,
                 c::MOI.ConstraintIndex{MOI.VariableIndex,S}) where {S}
    _in_conflict(o)
    j = c.value
    side = o.col_side[j]
    S <: MOI.Integer && return MOI.NOT_IN_CONFLICT
    if S <: Union{MOI.Semicontinuous,MOI.Semiinteger}
        return side == 0 ? MOI.NOT_IN_CONFLICT : MOI.MAYBE_IN_CONFLICT
    end
    at_lower = (side == 1 || side == 3) && o.lower_by[j] === _tag(S)
    at_upper = (side == 2 || side == 3) && o.upper_by[j] === _tag(S)
    return at_lower || at_upper ? MOI.IN_CONFLICT : MOI.NOT_IN_CONFLICT
end

function MOI.get(o::Optimizer, ::MOI.ConstraintConflictStatus,
                 c::MOI.ConstraintIndex{<:Union{_SAF,_SQF}})
    _in_conflict(o)
    return o.row_side[c.value] == 0 ? MOI.NOT_IN_CONFLICT : MOI.IN_CONFLICT
end

function MOI.get(o::Optimizer, ::MOI.ConstraintConflictStatus,
                 c::MOI.ConstraintIndex{<:Union{_VOV,_VAF}})
    _in_conflict(o)
    return MOI.NOT_IN_CONFLICT
end
