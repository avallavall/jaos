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
const _SEMI = Union{MOI.Semicontinuous{Float64},MOI.Semiinteger{Float64}}
const _KIND = Union{MOI.Integer,MOI.ZeroOne,_SEMI}
const _CONE = Union{MOI.SecondOrderCone,MOI.RotatedSecondOrderCone}
const _SOS = Union{MOI.SOS1{Float64},MOI.SOS2{Float64}}
const _BOUND_SETS = (MOI.GreaterThan{Float64}, MOI.LessThan{Float64},
                     MOI.EqualTo{Float64}, MOI.Interval{Float64})
const _CALLBACKS = Union{MOI.LazyConstraintCallback,MOI.UserCutCallback}

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

const _TAG_ORDER = (:greater, :less, :equal, :interval, :zeroone,
                    :semicontinuous, :semiinteger)
const _LOWER_TAGS = (:greater, :equal, :interval, :semicontinuous, :semiinteger)
const _UPPER_TAGS = (:less, :equal, :interval, :semicontinuous, :semiinteger)

_set_type(tag::Symbol) =
    tag === :greater ? MOI.GreaterThan{Float64} :
    tag === :less ? MOI.LessThan{Float64} :
    tag === :equal ? MOI.EqualTo{Float64} :
    tag === :interval ? MOI.Interval{Float64} :
    tag === :semicontinuous ? MOI.Semicontinuous{Float64} :
    MOI.Semiinteger{Float64}

const _Sets = Dict{Symbol,Tuple{Float64,Float64}}

function _merge(sets::_Sets)
    lower, upper, lower_by, upper_by = -Inf, Inf, :none, :none
    for tag in _TAG_ORDER
        haskey(sets, tag) || continue
        l, u = sets[tag]
        if l > -Inf && l >= lower
            lower, lower_by = l, tag
        end
        if u < Inf && u <= upper
            upper, upper_by = u, tag
        end
    end
    return lower, upper, lower_by, upper_by
end

const _Quadratic = Tuple{Vector{Int64},Vector{Int64},Vector{Float64}}

"""
The `callback_data` of a JAOS callback: the node event behind it.
"""
struct CallbackData
    event::NodeEvent
end

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

After a solve, a change to a variable bound, a row's set or constant, a
row coefficient, an objective coefficient or constant, the whole
objective, the sense or a start value goes to the loaded C model, so
the next solve starts from the last basis. Adding or deleting variables
or rows empties the optimizer, and the next solve loads the model anew.

`MOI.LazyConstraintCallback` runs at every integral point of a branch
and bound and `MOI.UserCutCallback` at every fractional node; both
submit rows through `jaos_node_add_row`. The C library has no way to
take a point during the search, so `MOI.HeuristicCallback` is not
supported. With `mip_pool_size` above 1, `MOI.ResultCount` counts the
pool and `MOI.VariablePrimal(k)` reads its entry `k`. Variable,
constraint and model names reach the C model when it is loaded.
`MOI.RawSolver` returns that `JAOS.Model`, for every call the `JAOS.`
functions offer.
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
    col_sets::Vector{_Sets}
    lower::Vector{Float64}
    upper::Vector{Float64}
    lower_by::Vector{Symbol}
    upper_by::Vector{Symbol}
    discrete::Bool
    start::Vector{Float64}
    copy_of::Vector{Int}
    cost::Vector{Float64}
    a_start::Vector{Int64}
    a_index::Vector{Int64}
    a_value::Vector{Float64}
    row_type::Vector{Any}
    row_set::Vector{Tuple{Float64,Float64}}
    row_constant::Vector{Float64}
    row_lower::Vector{Float64}
    row_upper::Vector{Float64}
    offset::Float64
    quad::Union{Nothing,_Quadratic}
    row_quad::Vector{Tuple{Int,_Quadratic}}
    cones::Vector{Vector{Int64}}
    sos::Vector{Vector{Int64}}
    indicators::Vector{Tuple{Int64,Int64}}
    lazy::Union{Nothing,Function}
    user_cut::Union{Nothing,Function}
    in_callback::Symbol
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
    pool::Vector{Tuple{Vector{Float64},Float64}}
    pool_act::Vector{Vector{Float64}}
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
        o.in_callback = :none
        _clear!(o)
        return o
    end
end

function _clear!(o::Optimizer)
    o.inner = nothing
    o.name = ""
    o.sense = MOI.FEASIBILITY_SENSE
    o.nvar = 0
    o.col_sets = _Sets[]
    o.lower = Float64[]
    o.upper = Float64[]
    o.lower_by = Symbol[]
    o.upper_by = Symbol[]
    o.discrete = false
    o.start = Float64[]
    o.copy_of = Int[]
    o.cost = Float64[]
    o.a_start = Int64[0]
    o.a_index = Int64[]
    o.a_value = Float64[]
    o.row_type = Any[]
    o.row_set = Tuple{Float64,Float64}[]
    o.row_constant = Float64[]
    o.row_lower = Float64[]
    o.row_upper = Float64[]
    o.offset = 0.0
    o.quad = nothing
    o.row_quad = Tuple{Int,_Quadratic}[]
    o.cones = Vector{Int64}[]
    o.sos = Vector{Int64}[]
    o.indicators = Tuple{Int64,Int64}[]
    o.lazy = nothing
    o.user_cut = nothing
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
    o.pool = Tuple{Vector{Float64},Float64}[]
    o.pool_act = Vector{Float64}[]
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

function _name!(setter, m::Model, args...)
    try
        setter(m, args...)
    catch e
        e isa JaosError || rethrow()
    end
    return
end

MOI.supports(::Optimizer, ::MOI.Name) = true
MOI.get(o::Optimizer, ::MOI.Name) = o.name
function MOI.set(o::Optimizer, ::MOI.Name, name::String)
    o.name = name
    o.inner === nothing || _name!(set_model_name, o.inner, name)
    return
end

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
    _install_callbacks(o, m)
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

function _objective_parts(f, ncol::Int, col)
    cost, offset, quad = zeros(ncol), 0.0, nothing
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
    return cost, offset, quad
end

function _check_attributes(o::Optimizer, src::MOI.ModelLike)
    for attr in MOI.get(src, MOI.ListOfModelAttributesSet())
        if attr isa MOI.ObjectiveFunction
            MOI.supports(o, attr) || throw(MOI.UnsupportedAttribute(attr))
        elseif !(attr isa Union{MOI.Name,MOI.ObjectiveSense,_CALLBACKS})
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
    for attr in MOI.get(src, MOI.ListOfModelAttributesSet())
        attr isa MOI.LazyConstraintCallback && (o.lazy = MOI.get(src, attr))
        attr isa MOI.UserCutCallback && (o.user_cut = MOI.get(src, attr))
    end
    index = MOI.Utilities.IndexMap()
    vars = MOI.get(src, MOI.ListOfVariableIndices())
    n = length(vars)
    for (j, v) in enumerate(vars)
        index[v] = MOI.VariableIndex(j)
    end
    col(v::MOI.VariableIndex) = index[v].value
    col_sets = [_Sets() for _ in 1:n]
    integer, semi = falses(n), falses(n)
    for S in _BOUND_SETS
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{MOI.VariableIndex,S}())
            j = col(MOI.get(src, MOI.ConstraintFunction(), ci))
            col_sets[j][_tag(S)] = _bounds(MOI.get(src, MOI.ConstraintSet(), ci))
            index[ci] = MOI.ConstraintIndex{MOI.VariableIndex,S}(j)
        end
    end
    for S in (MOI.Integer, MOI.ZeroOne, MOI.Semicontinuous{Float64},
              MOI.Semiinteger{Float64})
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{MOI.VariableIndex,S}())
            j = col(MOI.get(src, MOI.ConstraintFunction(), ci))
            integer[j] |= !(S <: MOI.Semicontinuous)
            if S <: MOI.ZeroOne
                col_sets[j][:zeroone] = (0.0, 1.0)
            elseif S <: _SEMI
                semi[j] = true
                col_sets[j][_tag(S)] = _bounds(MOI.get(src, MOI.ConstraintSet(), ci))
            end
            index[ci] = MOI.ConstraintIndex{MOI.VariableIndex,S}(j)
        end
    end
    lower, upper = fill(-Inf, n), fill(Inf, n)
    lower_by, upper_by = fill(:none, n), fill(:none, n)
    for j in 1:n
        lower[j], upper[j], lower_by[j], upper_by[j] = _merge(col_sets[j])
    end
    copy_of = zeros(Int, n)

    rl, ru, constant = Float64[], Float64[], Float64[]
    row_set, row_type = Tuple{Float64,Float64}[], Any[]
    I, J, V = Int[], Int[], Float64[]
    function add_row!(l, u, c, cols, vals, T)
        push!(rl, l - c)
        push!(ru, u - c)
        push!(constant, c)
        push!(row_set, (l, u))
        push!(row_type, T)
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
            r = add_row!(l, u, f.constant, affine(f.terms)..., Tuple{_SAF,S})
            index[ci] = MOI.ConstraintIndex{_SAF,S}(r)
        end
    end
    row_quad = Tuple{Int,_Quadratic}[]
    for S in (MOI.GreaterThan{Float64}, MOI.LessThan{Float64})
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{_SQF,S}())
            f = MOI.Utilities.canonical(MOI.get(src, MOI.ConstraintFunction(), ci))
            l, u = _bounds(MOI.get(src, MOI.ConstraintSet(), ci))
            r = add_row!(l, u, f.constant, affine(f.affine_terms)..., Tuple{_SQF,S})
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
                    add_row!(0.0, 0.0, 0.0, [k, j], [1.0, -1.0], nothing)
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
            r = add_row!(l, u, g.constant, affine(g.terms)..., nothing)
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
        cost, offset, quad = _objective_parts(MOI.get(src, MOI.ObjectiveFunction{F}()),
                                              ncol, col)
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
    start = zeros(ncol)
    if discrete && MOI.VariablePrimalStart() in
                   MOI.get(src, MOI.ListOfVariableAttributesSet())
        for (j, v) in enumerate(vars)
            s = MOI.get(src, MOI.VariablePrimalStart(), v)
            s === nothing || (start[j] = s)
        end
        for j in n+1:ncol
            start[j] = start[copy_of[j]]
        end
        set_mip_start(m, start)
    end
    if MOI.VariableName() in MOI.get(src, MOI.ListOfVariableAttributesSet())
        for (j, v) in enumerate(vars)
            name = MOI.get(src, MOI.VariableName(), v)
            isempty(name) || _name!(set_col_name, m, j - 1, name)
        end
    end
    for (F, S) in MOI.get(src, MOI.ListOfConstraintTypesPresent())
        (F <: Union{_SAF,_SQF} || (F == _VAF && S <: MOI.Indicator)) || continue
        MOI.ConstraintName() in MOI.get(src, MOI.ListOfConstraintAttributesSet{F,S}()) ||
            continue
        for ci in MOI.get(src, MOI.ListOfConstraintIndices{F,S}())
            k = index[ci].value
            r = F == _VAF ? indicators[k][2] : k - 1
            name = MOI.get(src, MOI.ConstraintName(), ci)
            isempty(name) || _name!(set_row_name, m, r, name)
        end
    end
    name = MOI.get(src, MOI.Name())
    isempty(name) || _name!(set_model_name, m, name)
    _apply_options(o, m)

    o.inner = m
    o.name = name
    o.sense = sense
    o.nvar = n
    o.col_sets = col_sets
    o.lower, o.upper = lower, upper
    o.lower_by, o.upper_by = lower_by, upper_by
    o.discrete = discrete
    o.start, o.copy_of = start, copy_of
    o.cost = cost
    o.a_start, o.a_index, o.a_value = a_start, a_index, a_value
    o.row_type, o.row_set = row_type, row_set
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

function _edit(f, o::Optimizer, err)
    o.inner === nothing && throw(err("the optimizer holds no model"))
    try
        f(o.inner)
    catch e
        e isa JaosError || rethrow()
        throw(err(e.msg))
    end
    _forget!(o)
    return
end

function _col_set(o::Optimizer, ci::MOI.ConstraintIndex{MOI.VariableIndex,S}) where {S}
    j = ci.value
    (1 <= j <= o.nvar && haskey(o.col_sets[j], _tag(S))) || throw(MOI.InvalidIndex(ci))
    return j
end

function _move_bounds(o::Optimizer, j::Int, err)
    l, u, lb, ub = _merge(o.col_sets[j])
    _edit(m -> set_col_bounds(m, j - 1, l, u), o, err)
    o.lower[j], o.upper[j], o.lower_by[j], o.upper_by[j] = l, u, lb, ub
    return
end

function MOI.set(o::Optimizer, attr::MOI.ConstraintSet,
                 ci::MOI.ConstraintIndex{MOI.VariableIndex,S},
                 s::S) where {S<:Union{_BOUND,_SEMI}}
    j = _col_set(o, ci)
    o.col_sets[j][_tag(S)] = _bounds(s)
    _move_bounds(o, j, msg -> MOI.SetAttributeNotAllowed(attr, msg))
    return
end

function MOI.add_constraint(o::Optimizer, v::MOI.VariableIndex, s::S) where {S<:_BOUND}
    j = v.value
    (o.inner !== nothing && 1 <= j <= o.nvar) ||
        throw(MOI.AddConstraintNotAllowed{MOI.VariableIndex,S}())
    sets, tag = o.col_sets[j], _tag(S)
    for t in _LOWER_TAGS
        tag in _LOWER_TAGS && haskey(sets, t) &&
            throw(MOI.LowerBoundAlreadySet{_set_type(t),S}(v))
    end
    for t in _UPPER_TAGS
        tag in _UPPER_TAGS && haskey(sets, t) &&
            throw(MOI.UpperBoundAlreadySet{_set_type(t),S}(v))
    end
    sets[tag] = _bounds(s)
    _move_bounds(o, j, msg -> MOI.AddConstraintNotAllowed{MOI.VariableIndex,S}(msg))
    return MOI.ConstraintIndex{MOI.VariableIndex,S}(j)
end

function MOI.delete(o::Optimizer, ci::MOI.ConstraintIndex{MOI.VariableIndex,S}) where {S<:_BOUND}
    j = _col_set(o, ci)
    delete!(o.col_sets[j], _tag(S))
    _move_bounds(o, j, msg -> MOI.DeleteNotAllowed(ci, msg))
    return
end

function _row(o::Optimizer, ci::MOI.ConstraintIndex{F,S}) where {F,S}
    r = ci.value
    (1 <= r <= length(o.row_type) && o.row_type[r] === Tuple{F,S}) ||
        throw(MOI.InvalidIndex(ci))
    return r
end

function _row_bounds(o::Optimizer, r::Int, sides::Tuple{Float64,Float64},
                     c::Float64, err)
    l, u = sides
    _edit(m -> set_row_bounds(m, r - 1, l - c, u - c), o, err)
    o.row_set[r], o.row_constant[r] = sides, c
    o.row_lower[r], o.row_upper[r] = l - c, u - c
    return
end

function MOI.set(o::Optimizer, attr::MOI.ConstraintSet,
                 ci::MOI.ConstraintIndex{F,S},
                 s::S) where {F<:Union{_SAF,_SQF},S<:_BOUND}
    r = _row(o, ci)
    _row_bounds(o, r, _bounds(s), o.row_constant[r],
                msg -> MOI.SetAttributeNotAllowed(attr, msg))
    return
end

function MOI.set(o::Optimizer, attr::MOI.ConstraintSet,
                 ci::MOI.ConstraintIndex{_VAF,S}, s::S) where {S<:MOI.Indicator}
    1 <= ci.value <= length(o.indicators) || throw(MOI.InvalidIndex(ci))
    r = o.indicators[ci.value][2] + 1
    _row_bounds(o, r, _bounds(s.set), o.row_constant[r],
                msg -> MOI.SetAttributeNotAllowed(attr, msg))
    return
end

function MOI.modify(o::Optimizer, ci::MOI.ConstraintIndex{F,S},
                    change::MOI.ScalarConstantChange{Float64}) where {F<:Union{_SAF,_SQF},S<:_BOUND}
    r = _row(o, ci)
    _row_bounds(o, r, o.row_set[r], change.new_constant,
                msg -> MOI.ModifyConstraintNotAllowed(ci, change, msg))
    return
end

function _set_entry!(o::Optimizer, r::Int, j::Int, v::Float64)
    for p in o.a_start[j]+1:o.a_start[j+1]
        o.a_index[p] == r - 1 || continue
        if iszero(v)
            deleteat!(o.a_index, p)
            deleteat!(o.a_value, p)
            o.a_start[j+1:end] .-= 1
        else
            o.a_value[p] = v
        end
        return
    end
    iszero(v) && return
    p = o.a_start[j+1] + 1
    insert!(o.a_index, p, r - 1)
    insert!(o.a_value, p, v)
    o.a_start[j+1:end] .+= 1
    return
end

function MOI.modify(o::Optimizer, ci::MOI.ConstraintIndex{F,S},
                    change::MOI.ScalarCoefficientChange{Float64}) where {F<:Union{_SAF,_SQF},S<:_BOUND}
    r = _row(o, ci)
    j = change.variable.value
    1 <= j <= o.nvar || throw(MOI.InvalidIndex(change.variable))
    _edit(m -> set_coefficient(m, r - 1, j - 1, change.new_coefficient), o,
          msg -> MOI.ModifyConstraintNotAllowed(ci, change, msg))
    _set_entry!(o, r, j, change.new_coefficient)
    return
end

function _objective!(o::Optimizer, cost::Vector{Float64}, offset::Float64,
                     quad, err)
    _edit(o, err) do m
        for j in eachindex(cost)
            cost[j] == o.cost[j] || set_col_cost(m, j - 1, cost[j])
        end
        offset == o.offset || set_objective_offset(m, offset)
        if quad != o.quad
            set_quadratic(m, something(quad, (Int64[], Int64[], Float64[]))...)
        end
    end
    o.cost, o.offset, o.quad = cost, offset, quad
    return
end

function MOI.set(o::Optimizer, attr::MOI.ObjectiveSense, sense::MOI.OptimizationSense)
    err = msg -> MOI.SetAttributeNotAllowed(attr, msg)
    if sense == MOI.FEASIBILITY_SENSE && o.inner !== nothing
        _objective!(o, zeros(length(o.cost)), 0.0, nothing, err)
    end
    _edit(m -> set_objective_sense(m, sense == MOI.MAX_SENSE ? :max : :min), o, err)
    o.sense = sense
    return
end

function MOI.set(o::Optimizer, attr::MOI.ObjectiveFunction{F},
                 f::F) where {F<:Union{MOI.VariableIndex,_SAF,_SQF}}
    err = msg -> MOI.SetAttributeNotAllowed(attr, msg)
    o.sense == MOI.FEASIBILITY_SENSE &&
        throw(err("the objective of a feasibility problem is not loaded"))
    _objective!(o, _objective_parts(f, length(o.cost), v -> v.value)..., err)
    return
end

function MOI.modify(o::Optimizer, ::MOI.ObjectiveFunction{F},
                    change::MOI.ScalarCoefficientChange{Float64}) where {F<:Union{_SAF,_SQF}}
    err = msg -> MOI.ModifyObjectiveNotAllowed(change, msg)
    o.sense == MOI.FEASIBILITY_SENSE &&
        throw(err("the objective of a feasibility problem is not loaded"))
    j = change.variable.value
    1 <= j <= o.nvar || throw(MOI.InvalidIndex(change.variable))
    _edit(m -> set_col_cost(m, j - 1, change.new_coefficient), o, err)
    o.cost[j] = change.new_coefficient
    return
end

function MOI.modify(o::Optimizer, ::MOI.ObjectiveFunction{F},
                    change::MOI.ScalarConstantChange{Float64}) where {F<:Union{_SAF,_SQF}}
    err = msg -> MOI.ModifyObjectiveNotAllowed(change, msg)
    o.sense == MOI.FEASIBILITY_SENSE &&
        throw(err("the objective of a feasibility problem is not loaded"))
    _edit(m -> set_objective_offset(m, change.new_constant), o, err)
    o.offset = change.new_constant
    return
end

function MOI.set(o::Optimizer, attr::MOI.VariablePrimalStart, v::MOI.VariableIndex,
                 value::Union{Nothing,Real})
    j = v.value
    (o.inner !== nothing && 1 <= j <= o.nvar) ||
        throw(MOI.SetAttributeNotAllowed(attr, "the optimizer holds no such variable"))
    o.start[j] = value === nothing ? 0.0 : Float64(value)
    for k in o.nvar+1:length(o.start)
        o.copy_of[k] == j && (o.start[k] = o.start[j])
    end
    if o.discrete
        try
            set_mip_start(o.inner, o.start)
        catch e
            e isa JaosError || rethrow()
            throw(MOI.SetAttributeNotAllowed(attr, e.msg))
        end
    end
    return
end

function _install_callbacks(o::Optimizer, m::Model)
    on = o.lazy !== nothing || o.user_cut !== nothing
    set_node_callback(m, on ? (ev -> _node(o, ev)) : nothing)
    return
end

function _node(o::Optimizer, ev::NodeEvent)
    f = ev.integral ? o.lazy : o.user_cut
    f === nothing && return
    o.in_callback = ev.integral ? :lazy : :cut
    try
        f(CallbackData(ev))
    finally
        o.in_callback = :none
    end
    return
end

MOI.supports(::Optimizer, ::_CALLBACKS) = true
MOI.get(o::Optimizer, ::MOI.LazyConstraintCallback) = o.lazy
MOI.get(o::Optimizer, ::MOI.UserCutCallback) = o.user_cut

function MOI.set(o::Optimizer, ::MOI.LazyConstraintCallback, f::Union{Nothing,Function})
    o.lazy = f
    o.inner === nothing || _install_callbacks(o, o.inner)
    return
end

function MOI.set(o::Optimizer, ::MOI.UserCutCallback, f::Union{Nothing,Function})
    o.user_cut = f
    o.inner === nothing || _install_callbacks(o, o.inner)
    return
end

MOI.supports(::Optimizer,
             ::Union{MOI.LazyConstraint{CallbackData},MOI.UserCut{CallbackData}}) = true

function MOI.get(o::Optimizer, attr::MOI.CallbackVariablePrimal{CallbackData},
                 x::MOI.VariableIndex)
    return attr.callback_data.event.values[x.value]
end

MOI.get(::Optimizer, attr::MOI.CallbackNodeStatus{CallbackData}) =
    attr.callback_data.event.integral ? MOI.CALLBACK_NODE_STATUS_INTEGER :
    MOI.CALLBACK_NODE_STATUS_FRACTIONAL

function _submit(cb::CallbackData, f::_SAF, s::_SIDES)
    g = MOI.Utilities.canonical(f)
    l, u = _bounds(s)
    node_add_row(cb.event, Int64[t.variable.value - 1 for t in g.terms],
                 Float64[t.coefficient for t in g.terms],
                 l - g.constant, u - g.constant)
    return
end

function MOI.submit(o::Optimizer, sub::MOI.LazyConstraint{CallbackData},
                    f::_SAF, s::_SIDES)
    o.in_callback === :cut &&
        throw(MOI.InvalidCallbackUsage(MOI.UserCutCallback(), sub))
    return _submit(sub.callback_data, f, s)
end

function MOI.submit(o::Optimizer, sub::MOI.UserCut{CallbackData},
                    f::_SAF, s::_SIDES)
    o.in_callback === :lazy &&
        throw(MOI.InvalidCallbackUsage(MOI.LazyConstraintCallback(), sub))
    return _submit(sub.callback_data, f, s)
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
    if o.discrete && o.primal == MOI.FEASIBLE_POINT
        o.pool = try
            mip_pool(m)
        catch e
            e isa JaosError || rethrow()
            Tuple{Vector{Float64},Float64}[]
        end
        o.pool_act = [_activity(o, x, true) for (x, _) in o.pool]
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

function MOI.get(o::Optimizer, ::MOI.ResultCount)
    o.primal != MOI.NO_SOLUTION && return max(1, length(o.pool))
    return o.dual != MOI.NO_SOLUTION ? 1 : 0
end

function MOI.get(o::Optimizer, attr::MOI.PrimalStatus)
    k = attr.result_index
    k == 1 && return o.primal
    return 2 <= k <= length(o.pool) ? MOI.FEASIBLE_POINT : MOI.NO_SOLUTION
end

MOI.get(o::Optimizer, attr::MOI.DualStatus) =
    attr.result_index == 1 ? o.dual : MOI.NO_SOLUTION

function MOI.get(o::Optimizer, attr::MOI.ObjectiveValue)
    o.in_callback === :none || throw(MOI.OptimizeInProgress(attr))
    MOI.check_result_index_bounds(o, attr)
    k = attr.result_index
    return k == 1 ? o.objective : o.pool[k][2]
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

function _point(o::Optimizer, attr)
    o.in_callback === :none || throw(MOI.OptimizeInProgress(attr))
    MOI.check_result_index_bounds(o, attr)
    o.primal == MOI.NO_SOLUTION &&
        throw(MOI.GetAttributeNotAllowed(attr, "the solve left no primal point"))
    k = attr.result_index
    k == 1 && return o.x, o.act, o.primal == MOI.FEASIBLE_POINT
    return o.pool[k][1], o.pool_act[k], true
end

function _dual(o::Optimizer, attr)
    o.in_callback === :none || throw(MOI.OptimizeInProgress(attr))
    MOI.check_result_index_bounds(o, attr)
    (attr.result_index == 1 && o.dual != MOI.NO_SOLUTION) ||
        throw(MOI.GetAttributeNotAllowed(attr, "the solve left no dual point"))
    return
end

function MOI.get(o::Optimizer, attr::MOI.VariablePrimal, v::MOI.VariableIndex)
    x, _, _ = _point(o, attr)
    return x[v.value]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{MOI.VariableIndex})
    x, _, _ = _point(o, attr)
    return x[c.value]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{<:Union{_SAF,_SQF}})
    _, act, point = _point(o, attr)
    return act[c.value] + (point ? o.row_constant[c.value] : 0.0)
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VOV,<:_CONE})
    x, _, _ = _point(o, attr)
    return x[o.cones[c.value].+1]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VOV,<:_SOS})
    x, _, _ = _point(o, attr)
    return x[o.sos[c.value].+1]
end

function MOI.get(o::Optimizer, attr::MOI.ConstraintPrimal,
                 c::MOI.ConstraintIndex{_VAF,<:MOI.Indicator})
    x, act, _ = _point(o, attr)
    zc, r = o.indicators[c.value]
    return [x[zc+1], act[r+1] + o.row_constant[r+1]]
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
