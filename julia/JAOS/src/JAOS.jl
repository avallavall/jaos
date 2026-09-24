# SPDX-License-Identifier: Apache-2.0
"""
JAOS from Julia: the C library through `ccall`, and a MathOptInterface
optimizer, `JAOS.Optimizer`, so JuMP can use it directly.

Every C function has a Julia function of the same name without the
`jaos_` prefix (`jaos_set_mip_cut_rounds` is `JAOS.set_mip_cut_rounds`),
except where Julia has its own word: `Base.copy` for `jaos_model_copy`,
`mip_report` for `jaos_mip_result`, `status` for `jaos_status_of`. Rows,
columns, cones and SOS sets count from 0, as in C.

Finding the library, in order:

1. the directory of the `JAOS_LIBRARY` environment variable, a full path
   to `libjaos.so`;
2. `build/release` of the repository this package sits in;
3. the system loader's search path.

Build it with `make shared`.
"""
module JAOS

import Libdl
import MathOptInterface as MOI

const libjaos = "libjaos"

function __init__()
    path = get(ENV, "JAOS_LIBRARY", "")
    if !isempty(path)
        isfile(path) ||
            error("JAOS_LIBRARY is set to $(repr(path)), which does not exist")
        dir = dirname(abspath(path))
    else
        dir = joinpath(@__DIR__, "..", "..", "..", "build", "release")
    end
    dir = normpath(dir)
    if isfile(joinpath(dir, "libjaos." * Libdl.dlext))
        dir in Libdl.DL_LOAD_PATH || pushfirst!(Libdl.DL_LOAD_PATH, dir)
    end
    return
end

"""What a C call reported when it did not return `JAOS_OK`."""
struct JaosError <: Exception
    status::Cint
    msg::String
end

Base.showerror(io::IO, e::JaosError) =
    print(io, "JAOS error ", e.status, " (", status_str(e.status), "): ", e.msg)

version() = unsafe_string(ccall((:jaos_version, libjaos), Cstring, ()))

"""The C name of a call status: 0 "ok" to 4 "numerical error"."""
status_str(s::Integer) =
    unsafe_string(ccall((:jaos_status_str, libjaos), Cstring, (Cint,), s))

"""The library's infinity; pass it, or its negation, for a missing bound."""
infinity() = ccall((:jaos_infinity, libjaos), Cdouble, ())

"""The longest name a model holds, in bytes."""
const NAME_MAX = 255

"""
A `jaos_model`, freed when Julia collects it. It holds the Julia
functions behind its callbacks, so they live as long as the C side
can call them.
"""
mutable struct Model
    ptr::Ptr{Cvoid}
    log_sink::Any
    progress_fn::Any
    incumbent_fn::Any
    node_fn::Any
    callback_error::Any
    function Model(ptr::Ptr{Cvoid})
        m = new(ptr, nothing, nothing, nothing, nothing, nothing)
        finalizer(_free, m)
        return m
    end
end

function Model()
    r = Ref{Ptr{Cvoid}}(C_NULL)
    st = ccall((:jaos_model_new, libjaos), Cint, (Ref{Ptr{Cvoid}},), r)
    st == 0 || throw(JaosError(st, "jaos_model_new failed"))
    return Model(r[])
end

function _free(m::Model)
    if m.ptr != C_NULL
        ccall((:jaos_model_free, libjaos), Cvoid, (Ptr{Cvoid},), m.ptr)
        m.ptr = C_NULL
    end
    return
end

Base.unsafe_convert(::Type{Ptr{Cvoid}}, m::Model) = m.ptr

function error_message(m::Model)
    p = ccall((:jaos_model_error, libjaos), Cstring, (Ptr{Cvoid},), m)
    return p == C_NULL ? "" : unsafe_string(p)
end

function _rethrow(m::Model)
    e = m.callback_error
    e === nothing && return
    m.callback_error = nothing
    throw(e)
end

function _check(m::Model, st::Integer)
    _rethrow(m)
    st == 0 || throw(JaosError(st, error_message(m)))
    return nothing
end

_cstr(buf::Vector{UInt8}) =
    String(buf[1:something(findfirst(iszero, buf), length(buf) + 1)-1])

function _doubles(v, n::Integer, what::AbstractString)
    length(v) == n ||
        throw(ArgumentError("$what has $(length(v)) entries, expected $n"))
    return Vector{Float64}(v)
end

function _sides(v, n::Integer, what::AbstractString)
    length(v) == n ||
        throw(ArgumentError("$what has $(length(v)) entries, expected $n"))
    return isempty(v) ? Cint[0] : Vector{Cint}(v)
end

const _FORMATS = (:mps, :lp, :nl, :qplib, :cbf, :osil)

for fmt in _FORMATS, verb in (:read, :write)
    fn, cfn = Symbol(verb, :_, fmt), Symbol(:jaos_, verb, :_, fmt)
    @eval $fn(m::Model, path::AbstractString) =
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Cstring), m, path))
end

for fn in (:write_solution, :write_mps_basis, :write_point, :write_duals,
           :write_proof, :read_options)
    @eval $fn(m::Model, path::AbstractString) =
        _check(m, ccall(($(QuoteNode(Symbol(:jaos_, fn))), libjaos), Cint,
                        (Ptr{Cvoid}, Cstring), m, path))
end

function _format(path::AbstractString)
    p = endswith(path, ".gz") ? chop(path; tail = 3) : path
    for fmt in _FORMATS
        endswith(p, "." * String(fmt)) && return fmt
    end
    return :mps
end

"""
    read_file(m, path)

Reads `path` into `m` with the reader its name selects, as `jaos solve`
does: MPS for any name without a known extension.
"""
read_file(m::Model, path::AbstractString) =
    getfield(@__MODULE__, Symbol(:read_, _format(path)))(m, path)

"""
    write_file(m, path)

Writes `m` with the writer its name selects: `.mps`, `.lp`, `.nl`,
`.qplib`, `.cbf` or `.osil`, each also with `.gz`, and MPS for any other
name.
"""
write_file(m::Model, path::AbstractString) =
    getfield(@__MODULE__, Symbol(:write_, _format(path)))(m, path)

"""
    load_lp(m, sense, offset, cost, col_lower, col_upper, row_lower,
            row_upper, a_start, a_index, a_value)

The whole model in one call, the matrix column-wise with 0-based starts
and row indices; `sense` is `:min` or `:max`.
"""
function load_lp(m::Model, sense::Symbol, offset::Real, cost::Vector{Float64},
                 cl::Vector{Float64}, cu::Vector{Float64},
                 rl::Vector{Float64}, ru::Vector{Float64},
                 a_start::Vector{Int64}, a_index::Vector{Int64},
                 a_value::Vector{Float64})
    nc, nr = length(cost), length(rl)
    return _check(m, ccall((:jaos_load_lp, libjaos), Cint,
        (Ptr{Cvoid}, Int64, Int64, Cint, Cdouble, Ptr{Cdouble}, Ptr{Cdouble},
         Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cdouble}, Int64, Ptr{Int64},
         Ptr{Int64}, Ptr{Cdouble}),
        m, nc, nr, sense === :max ? 1 : 0, offset, cost, cl, cu, rl, ru,
        length(a_value), a_start, a_index, a_value))
end

num_col(m::Model) = Int(ccall((:jaos_num_col, libjaos), Int64, (Ptr{Cvoid},), m))
num_row(m::Model) = Int(ccall((:jaos_num_row, libjaos), Int64, (Ptr{Cvoid},), m))
num_nz(m::Model) = Int(ccall((:jaos_num_nz, libjaos), Int64, (Ptr{Cvoid},), m))
num_cones(m::Model) = Int(ccall((:jaos_num_cones, libjaos), Int64, (Ptr{Cvoid},), m))
num_sos(m::Model) = Int(ccall((:jaos_num_sos, libjaos), Int64, (Ptr{Cvoid},), m))
has_integer(m::Model) = ccall((:jaos_model_has_integer, libjaos), Bool, (Ptr{Cvoid},), m)

for (fn, cfn) in ((:col_cost, :jaos_col_cost), (:col_quadratic, :jaos_col_quadratic))
    @eval function $fn(m::Model, j::Integer)
        v = Ref{Cdouble}(0.0)
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Int64, Ref{Cdouble}), m, j, v))
        return v[]
    end
end

for (fn, cfn) in ((:col_bounds, :jaos_col_bounds), (:row_bounds, :jaos_row_bounds))
    @eval function $fn(m::Model, k::Integer)
        lo, hi = Ref{Cdouble}(0.0), Ref{Cdouble}(0.0)
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Int64, Ref{Cdouble}, Ref{Cdouble}),
                        m, k, lo, hi))
        return lo[], hi[]
    end
end

for (fn, cfn) in ((:col_integer, :jaos_col_integer),
                  (:col_semicontinuous, :jaos_col_semicontinuous))
    @eval function $fn(m::Model, j::Integer)
        v = Ref{Bool}(false)
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Int64, Ref{Bool}), m, j, v))
        return v[]
    end
end

set_col_cost(m::Model, j::Integer, cost::Real) =
    _check(m, ccall((:jaos_set_col_cost, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cdouble), m, j, cost))

set_col_bounds(m::Model, j::Integer, lower::Real, upper::Real) =
    _check(m, ccall((:jaos_set_col_bounds, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cdouble, Cdouble), m, j, lower, upper))

set_row_bounds(m::Model, i::Integer, lower::Real, upper::Real) =
    _check(m, ccall((:jaos_set_row_bounds, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cdouble, Cdouble), m, i, lower, upper))

"""`:min` or `:max`."""
function objective_sense(m::Model)
    v = Ref{Cint}(0)
    _check(m, ccall((:jaos_objective_sense, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{Cint}), m, v))
    return v[] == 1 ? :max : :min
end

function objective_offset(m::Model)
    v = Ref{Cdouble}(0.0)
    _check(m, ccall((:jaos_objective_offset, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{Cdouble}), m, v))
    return v[]
end

"""Minimise with `:min` or maximise with `:max`; keeps the basis."""
set_objective_sense(m::Model, sense::Symbol) =
    _check(m, ccall((:jaos_set_objective_sense, libjaos), Cint,
                    (Ptr{Cvoid}, Cint), m, sense === :max ? 1 : 0))

set_objective_offset(m::Model, offset::Real) =
    _check(m, ccall((:jaos_set_objective_offset, libjaos), Cint,
                    (Ptr{Cvoid}, Cdouble), m, offset))

"""
A new `Model` with this one's problem, names, options, MIP start and
start basis, and not its answer. The callbacks come along and call the
same Julia functions.
"""
function Base.copy(m::Model)
    r = Ref{Ptr{Cvoid}}(C_NULL)
    _check(m, ccall((:jaos_model_copy, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{Ptr{Cvoid}}), m, r))
    return _adopt(Model(r[]), m)
end

function _entries(m::Model, call)
    n = Ref{Int64}(0)
    _check(m, call(n, C_NULL, C_NULL))
    idx, val = zeros(Int64, n[]), zeros(n[])
    _check(m, call(n, idx, val))
    return idx, val
end

"""One column of the matrix: `(row indices, values)` by increasing row."""
col_entries(m::Model, j::Integer) = _entries(m, (n, idx, val) ->
    ccall((:jaos_col_entries, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Int64}, Ptr{Int64}, Ptr{Cdouble}),
          m, j, n, idx, val))

"""One row of the matrix: `(column indices, values)` by increasing column."""
row_entries(m::Model, i::Integer) = _entries(m, (n, idx, val) ->
    ccall((:jaos_row_entries, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Int64}, Ptr{Int64}, Ptr{Cdouble}),
          m, i, n, idx, val))

"""One entry of the matrix; 0.0 where the model holds none."""
function coefficient(m::Model, i::Integer, j::Integer)
    v = Ref{Cdouble}(0.0)
    _check(m, ccall((:jaos_coefficient, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Ref{Cdouble}), m, i, j, v))
    return v[]
end

"""Sets one entry; 0 removes it."""
set_coefficient(m::Model, i::Integer, j::Integer, value::Real) =
    _check(m, ccall((:jaos_set_coefficient, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Cdouble), m, i, j, value))

function _matrix(start, index, value, count::Integer, whose::AbstractString)
    isempty(value) && return C_NULL, C_NULL, C_NULL, 0
    length(index) == length(value) ||
        throw(ArgumentError("the index array needs one entry per value"))
    length(start) == count + 1 ||
        throw(ArgumentError("the start array needs $whose + 1 entries"))
    return (Vector{Int64}(start), Vector{Int64}(index), Vector{Float64}(value),
            length(value))
end

"""
    add_cols(m, cost, lower, upper, a_start = [], a_index = [], a_value = [])

Appends columns, their entries given column by column over the rows the
model has. The basis survives, the new columns nonbasic.
"""
function add_cols(m::Model, cost, lower, upper, a_start = Int64[],
                  a_index = Int64[], a_value = Float64[])
    n = length(cost)
    s, i, v, nz = _matrix(a_start, a_index, a_value, n, "the new column count")
    return _check(m, ccall((:jaos_add_cols, libjaos), Cint,
        (Ptr{Cvoid}, Int64, Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cdouble}, Int64,
         Ptr{Int64}, Ptr{Int64}, Ptr{Cdouble}),
        m, n, _doubles(cost, n, "cost"), _doubles(lower, n, "lower"),
        _doubles(upper, n, "upper"), nz, s, i, v))
end

"""
    add_rows(m, lower, upper, ar_start = [], ar_index = [], ar_value = [])

Appends rows, their entries given row by row over the model's columns.
The basis survives, the new rows basic.
"""
function add_rows(m::Model, lower, upper, ar_start = Int64[],
                  ar_index = Int64[], ar_value = Float64[])
    n = length(lower)
    s, i, v, nz = _matrix(ar_start, ar_index, ar_value, n, "the new row count")
    return _check(m, ccall((:jaos_add_rows, libjaos), Cint,
        (Ptr{Cvoid}, Int64, Ptr{Cdouble}, Ptr{Cdouble}, Int64, Ptr{Int64},
         Ptr{Int64}, Ptr{Cdouble}),
        m, n, _doubles(lower, n, "lower"), _doubles(upper, n, "upper"), nz,
        s, i, v))
end

for (fn, cfn) in ((:delete_cols, :jaos_delete_cols),
                  (:delete_rows, :jaos_delete_rows),
                  (:delete_cones, :jaos_delete_cones))
    @eval function $fn(m::Model, which::AbstractVector{<:Integer})
        k = isempty(which) ? Int64[0] : Vector{Int64}(which)
        return _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                               (Ptr{Cvoid}, Int64, Ptr{Int64}),
                               m, length(which), k))
    end
end

function _text(m::Model, call)
    buf = zeros(UInt8, NAME_MAX + 1)
    _check(m, call(buf))
    return _cstr(buf)
end

"""The column's name: its own, or the positional `C<j+1>`."""
col_name(m::Model, j::Integer) = _text(m, buf ->
    ccall((:jaos_col_name, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ptr{UInt8}, Int64), m, j, buf, length(buf)))

"""The row's name: its own, or the positional `R<i+1>`."""
row_name(m::Model, i::Integer) = _text(m, buf ->
    ccall((:jaos_row_name, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ptr{UInt8}, Int64), m, i, buf, length(buf)))

objective_name(m::Model) = _text(m, buf ->
    ccall((:jaos_objective_name, libjaos), Cint,
          (Ptr{Cvoid}, Ptr{UInt8}, Int64), m, buf, length(buf)))

"""The model's name, `JAOS` until one is set or read."""
model_name(m::Model) = _text(m, buf ->
    ccall((:jaos_model_name, libjaos), Cint,
          (Ptr{Cvoid}, Ptr{UInt8}, Int64), m, buf, length(buf)))

"""A name of 1 to 255 bytes with no whitespace; `""` or `nothing` removes it."""
set_col_name(m::Model, j::Integer, name) =
    _check(m, ccall((:jaos_set_col_name, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cstring), m, j, something(name, "")))

set_row_name(m::Model, i::Integer, name) =
    _check(m, ccall((:jaos_set_row_name, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cstring), m, i, something(name, "")))

set_objective_name(m::Model, name) =
    _check(m, ccall((:jaos_set_objective_name, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring), m, something(name, "")))

set_model_name(m::Model, name) =
    _check(m, ccall((:jaos_set_model_name, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring), m, something(name, "")))

for (fn, cfn) in ((:col_index, :jaos_col_index), (:row_index, :jaos_row_index))
    @eval function $fn(m::Model, name::AbstractString)
        v = Ref{Int64}(0)
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Cstring, Ref{Int64}), m, name, v))
        return Int(v[])
    end
end

set_col_integer(m::Model, j::Integer, on::Bool = true) =
    _check(m, ccall((:jaos_set_col_integer, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Bool), m, j, on))

"""The objective's `q/2 x_j^2` term."""
set_col_quadratic(m::Model, j::Integer, q::Real) =
    _check(m, ccall((:jaos_set_col_quadratic, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Cdouble), m, j, q))

"""The objective's `½ x'Qx`, one entry per diagonal and per pair (0-based)."""
set_quadratic(m::Model, rows::Vector{Int64}, cols::Vector{Int64},
              vals::Vector{Float64}) =
    _check(m, ccall((:jaos_set_quadratic, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ptr{Int64}, Ptr{Int64}, Ptr{Cdouble}),
                    m, length(vals), rows, cols, vals))

quadratic_nz(m::Model) =
    Int(ccall((:jaos_quadratic_nz, libjaos), Int64, (Ptr{Cvoid},), m))

"""The objective's Q as `(rows, cols, values)`, the lower triangle."""
function quadratic(m::Model)
    n = quadratic_nz(m)
    rows, cols, vals = zeros(Int64, n), zeros(Int64, n), zeros(n)
    n == 0 || _check(m, ccall((:jaos_quadratic, libjaos), Cint,
                              (Ptr{Cvoid}, Ptr{Int64}, Ptr{Int64}, Ptr{Cdouble}),
                              m, rows, cols, vals))
    return rows, cols, vals
end

set_row_quadratic(m::Model, row::Integer, rows::Vector{Int64},
                  cols::Vector{Int64}, vals::Vector{Float64}) =
    _check(m, ccall((:jaos_set_row_quadratic, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Ptr{Int64}, Ptr{Int64},
                     Ptr{Cdouble}),
                    m, row, length(vals), rows, cols, vals))

row_quadratic_nz(m::Model, row::Integer) =
    Int(ccall((:jaos_row_quadratic_nz, libjaos), Int64,
              (Ptr{Cvoid}, Int64), m, row))

"""Row `row`'s Q as `(rows, cols, values)`, the lower triangle."""
function row_quadratic(m::Model, row::Integer)
    n = row_quadratic_nz(m, row)
    rows, cols, vals = zeros(Int64, n), zeros(Int64, n), zeros(n)
    n == 0 || _check(m, ccall((:jaos_row_quadratic, libjaos), Cint,
                              (Ptr{Cvoid}, Int64, Ptr{Int64}, Ptr{Int64},
                               Ptr{Cdouble}), m, row, rows, cols, vals))
    return rows, cols, vals
end

"""A cone over columns (0-based): `:quadratic` or `:rotated`."""
add_cone(m::Model, kind::Symbol, cols::Vector{Int64}) =
    _check(m, ccall((:jaos_add_cone, libjaos), Cint,
                    (Ptr{Cvoid}, Cint, Int64, Ptr{Int64}),
                    m, kind === :rotated ? 2 : 1, length(cols), cols))

"""Cone `k` as `(kind, columns)`, `kind` `:quadratic` or `:rotated`."""
function cone(m::Model, k::Integer)
    t, n = Ref{Cint}(0), Ref{Int64}(0)
    _check(m, ccall((:jaos_cone, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ref{Cint}, Ref{Int64}, Ptr{Int64}),
                    m, k, t, n, C_NULL))
    cols = zeros(Int64, n[])
    _check(m, ccall((:jaos_cone, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ref{Cint}, Ref{Int64}, Ptr{Int64}),
                    m, k, t, n, cols))
    return t[] == 2 ? :rotated : :quadratic, cols
end

"""A column that is 0 or between its bounds."""
set_col_semicontinuous(m::Model, j::Integer, on::Bool = true) =
    _check(m, ccall((:jaos_set_col_semicontinuous, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Bool), m, j, on))

"""An SOS set of type 1 or 2 over columns (0-based), ordered by `weights`."""
add_sos(m::Model, type::Integer, cols::Vector{Int64}, weights::Vector{Float64}) =
    _check(m, ccall((:jaos_add_sos, libjaos), Cint,
                    (Ptr{Cvoid}, Cint, Int64, Ptr{Int64}, Ptr{Cdouble}),
                    m, type, length(cols), cols, weights))

"""SOS set `k` as `(type, columns, weights)`, members in weight order."""
function sos(m::Model, k::Integer)
    t, n = Ref{Cint}(0), Ref{Int64}(0)
    _check(m, ccall((:jaos_sos, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ref{Cint}, Ref{Int64}, Ptr{Int64},
                     Ptr{Cdouble}), m, k, t, n, C_NULL, C_NULL))
    cols, weights = zeros(Int64, n[]), zeros(n[])
    _check(m, ccall((:jaos_sos, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ref{Cint}, Ref{Int64}, Ptr{Int64},
                     Ptr{Cdouble}), m, k, t, n, cols, weights))
    return Int(t[]), cols, weights
end

"""
Row `row` holds only while integer column `col` equals `value` (0 or 1);
`col = -1` makes it an ordinary row again.
"""
set_row_indicator(m::Model, row::Integer, col::Integer, value::Integer) =
    _check(m, ccall((:jaos_set_row_indicator, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Cint), m, row, col, value))

"""`(column, value)` of row `row`'s indicator, or `(nothing, 0)`."""
function row_indicator(m::Model, row::Integer)
    c, v = Ref{Int64}(0), Ref{Cint}(0)
    _check(m, ccall((:jaos_row_indicator, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ref{Int64}, Ref{Cint}), m, row, c, v))
    return c[] < 0 ? nothing : Int(c[]), Int(v[])
end

"""The tree's gap: it stops when no node beats the incumbent by more than
`gap * (1 + |incumbent|)`; 0 restores the default."""
set_mip_gap(m::Model, gap::Real) =
    _check(m, ccall((:jaos_set_mip_gap, libjaos), Cint,
                    (Ptr{Cvoid}, Cdouble), m, gap))

"""The tree stops after `nodes` nodes; 0 means no limit."""
set_mip_node_limit(m::Model, nodes::Integer) =
    _check(m, ccall((:jaos_set_mip_node_limit, libjaos), Cint,
                    (Ptr{Cvoid}, Int64), m, nodes))

const _SETTERS = (
    (:set_work_limit, Int64), (:set_primal_tolerance, Cdouble),
    (:set_dual_tolerance, Cdouble), (:set_algorithm, Cint),
    (:set_mip_tree_batch, Int64), (:set_mip_branching, Cint),
    (:set_mip_reliability, Int64), (:set_mip_probe_cap, Cdouble),
    (:set_mip_probe_depth, Int64), (:set_mip_node_select, Int64),
    (:set_mip_restart, Cint), (:set_mip_conflicts, Cint),
    (:set_mip_symmetry, Cint), (:set_mip_orbital, Cint),
    (:set_mip_propagate, Int64), (:set_mip_propagate_depth, Int64),
    (:set_mip_rcfix, Cint), (:set_mip_tighten, Cint),
    (:set_mip_probing, Cint), (:set_mip_probing_cap, Cdouble),
    (:set_mip_clique_fix, Cint), (:set_mip_cut_rounds, Int64),
    (:set_mip_cut_depth, Int64), (:set_mip_cut_drop, Bool),
    (:set_mip_node_cut_cap, Int64), (:set_mip_cover_rounds, Int64),
    (:set_mip_clique_rounds, Int64), (:set_mip_zero_half_rounds, Int64),
    (:set_mip_flow_cover_rounds, Int64), (:set_mip_cut_stall, Cdouble),
    (:set_mip_node_cut_stall, Cdouble), (:set_mip_root_cut_drop, Cint),
    (:set_mip_cover_lift, Cint), (:set_mip_mir_rounds, Int64),
    (:set_mip_node_mir, Cint), (:set_mip_mir_aggregate, Int64),
    (:set_mip_dive, Bool), (:set_mip_dive_backtrack, Int64),
    (:set_mip_dive_gap, Cdouble), (:set_mip_dive_heuristic, Int64),
    (:set_mip_dive_heuristic_depth, Int64), (:set_mip_dive_child, Cint),
    (:set_mip_dive_degrade, Cdouble), (:set_mip_rins, Int64),
    (:set_mip_local_branching, Int64), (:set_mip_feaspump, Int64),
    (:set_mip_pump_general, Cint), (:set_mip_pump_obj, Cdouble),
    (:set_mip_pump_always, Cint), (:set_mip_heuristics, Bool),
    (:set_mip_pool_size, Int64), (:set_mip_cutoff, Cdouble),
)

for (fn, T) in _SETTERS
    @eval $fn(m::Model, value) =
        _check(m, ccall(($(QuoteNode(Symbol(:jaos_, fn))), libjaos), Cint,
                        (Ptr{Cvoid}, $T), m, value))
end

"""The thread count; 1 unless set."""
threads(m::Model) = Int(ccall((:jaos_threads_of, libjaos), Int64, (Ptr{Cvoid},), m))

"""The algorithm as the C enum's value: 0 dual, 1 primal, 2 barrier, 3 PDLP,
4 concurrent."""
algorithm(m::Model) = Int(ccall((:jaos_algorithm_of, libjaos), Cint, (Ptr{Cvoid},), m))

set_option(m::Model, name::AbstractString, value) =
    _check(m, ccall((:jaos_set_option, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Cstring), m, name, string(value)))

function get_option(m::Model, name::AbstractString)
    buf = zeros(UInt8, 512)
    _check(m, ccall((:jaos_get_option, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{UInt8}, Int64),
                    m, name, buf, length(buf)))
    return _cstr(buf)
end

"""Every option name `set_option` takes, in the library's order."""
option_names() =
    [unsafe_string(ccall((:jaos_option_name, libjaos), Cstring, (Int64,), k))
     for k in 0:ccall((:jaos_num_options, libjaos), Int64, ())-1]

set_time_limit(m::Model, seconds::Real) =
    _check(m, ccall((:jaos_set_time_limit, libjaos), Cint,
                    (Ptr{Cvoid}, Cdouble), m, seconds))

set_threads(m::Model, n::Integer) =
    _check(m, ccall((:jaos_set_threads, libjaos), Cint,
                    (Ptr{Cvoid}, Int64), m, n))

set_mip_start(m::Model, x::Vector{Float64}) =
    _check(m, ccall((:jaos_set_mip_start, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}), m, x))

@static if VERSION >= v"1.12"
    _solve(m::Model) = @ccall gc_safe=true libjaos.jaos_solve(m::Ptr{Cvoid})::Cint
    _iis(m::Model, rs, cs, r) =
        @ccall gc_safe=true libjaos.jaos_iis(m::Ptr{Cvoid}, rs::Ptr{Cint}, cs::Ptr{Cint},
                                              r::Ptr{Cvoid})::Cint
    _feasrelax(m::Model, code, rm, cm, r) =
        @ccall gc_safe=true libjaos.jaos_feasrelax(m::Ptr{Cvoid}, code::Cint,
                                                    rm::Ptr{Cdouble}, cm::Ptr{Cdouble},
                                                    r::Ptr{Cvoid})::Cint
else
    _solve(m::Model) = ccall((:jaos_solve, libjaos), Cint, (Ptr{Cvoid},), m)
    _iis(m::Model, rs, cs, r) =
        ccall((:jaos_iis, libjaos), Cint, (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}, Ptr{Cvoid}),
              m, rs, cs, r)
    _feasrelax(m::Model, code, rm, cm, r) =
        ccall((:jaos_feasrelax, libjaos), Cint,
              (Ptr{Cvoid}, Cint, Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cvoid}), m, code, rm, cm, r)
end

solve(m::Model) = _check(m, _solve(m))

"""Where the last solve stopped, as the C enum's value (1 is optimal)."""
status(m::Model) = Int(ccall((:jaos_status_of, libjaos), Cint, (Ptr{Cvoid},), m))

status_string(s::Integer) =
    unsafe_string(ccall((:jaos_solve_status_str, libjaos), Cstring, (Cint,), s))

function objective(m::Model)
    v = Ref{Cdouble}(0.0)
    _check(m, ccall((:jaos_objective, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{Cdouble}), m, v))
    return v[]
end

"""
    solution(m) -> (x, row_activity, row_dual, reduced_cost)

The answer of an optimal solve, in the model's own signs.
"""
function solution(m::Model)
    nc, nr = num_col(m), num_row(m)
    x, act = zeros(nc), zeros(nr)
    y, d = zeros(nr), zeros(nc)
    _check(m, ccall((:jaos_solution, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cdouble},
                     Ptr{Cdouble}), m, x, act, y, d))
    return x, act, y, d
end

"""
The dual of cone `k` (0-based), one entry per member: the optimum's, or
the cone part of an infeasible model's certificate; `nothing` when the
solve left neither. `n` is the cone's size, read from the model when
left out.
"""
function cone_dual(m::Model, k::Integer, n::Integer = length(cone(m, k)[2]))
    z = zeros(max(n, 1))
    st = ccall((:jaos_cone_dual, libjaos), Cint,
               (Ptr{Cvoid}, Int64, Ptr{Cdouble}), m, k, z)
    return st == 0 ? z[1:n] : nothing
end

"""The C `jaos_mip_report`, field by field."""
struct MipReport
    nodes::Int64
    lp_solves::Int64
    has_incumbent::Bool
    incumbent::Cdouble
    bound::Cdouble
    cuts::Int64
    heuristic_points::Int64
    first_incumbent_node::Int64
    fixed_cols::Int64
    tightened::Int64
    symmetry_generators::Int64
    symmetry_orbits::Int64
end

function mip_report(m::Model)
    r = Ref{MipReport}()
    _check(m, ccall((:jaos_mip_result, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{MipReport}), m, r))
    return r[]
end

"""The incumbent of a tree that stopped with one: `(x, objective)`."""
function mip_incumbent(m::Model)
    x = zeros(num_col(m))
    v = Ref{Cdouble}(0.0)
    _check(m, ccall((:jaos_mip_incumbent, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ref{Cdouble}), m, x, v))
    return x, v[]
end

function mip_pool_count(m::Model)
    n = Ref{Int64}(0)
    _check(m, ccall((:jaos_mip_pool_count, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{Int64}), m, n))
    return Int(n[])
end

"""Pool entry `k` (0 the best) as `(x, objective)`."""
function mip_pool_solution(m::Model, k::Integer)
    x = zeros(num_col(m))
    v = Ref{Cdouble}(0.0)
    _check(m, ccall((:jaos_mip_pool_solution, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ptr{Cdouble}, Ref{Cdouble}), m, k, x, v))
    return x, v[]
end

"""
The solution pool after a branch and bound, best first, each entry
`(x, objective)`; `set_mip_pool_size` says how many it keeps.
"""
mip_pool(m::Model) = [mip_pool_solution(m, k) for k in 0:mip_pool_count(m)-1]

iterations(m::Model) = ccall((:jaos_iterations, libjaos), Int64, (Ptr{Cvoid},), m)
work_units(m::Model) = ccall((:jaos_work_units, libjaos), Int64, (Ptr{Cvoid},), m)
solve_time(m::Model) = ccall((:jaos_solve_time, libjaos), Cdouble, (Ptr{Cvoid},), m)

"""
    basis(m) -> (col_status, row_status) or nothing

The last simplex basis, each entry 0 basic, 1 at lower, 2 at upper,
3 free; `nothing` when the solve left no basis.
"""
function basis(m::Model)
    cs = zeros(Cint, num_col(m))
    rs = zeros(Cint, num_row(m))
    st = ccall((:jaos_basis, libjaos), Cint, (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}),
               m, cs, rs)
    return st == 0 ? (cs, rs) : nothing
end

"""The next solve starts from this basis, one status per column and row."""
set_basis(m::Model, col_status::AbstractVector{<:Integer},
          row_status::AbstractVector{<:Integer}) =
    _check(m, ccall((:jaos_set_basis, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}), m,
                    _sides(col_status, num_col(m), "col_status"),
                    _sides(row_status, num_row(m), "row_status")))

"""The next solve starts from the slack basis."""
clear_basis(m::Model) = ccall((:jaos_clear_basis, libjaos), Cvoid, (Ptr{Cvoid},), m)

"""The Farkas multipliers of an infeasible model, one per row, or `nothing`."""
function certificate(m::Model)
    y = zeros(max(num_row(m), 1))
    st = ccall((:jaos_certificate, libjaos), Cint, (Ptr{Cvoid}, Ptr{Cdouble}),
               m, y)
    return st == 0 ? y[1:num_row(m)] : nothing
end

"""The improving direction of an unbounded model, one per column, or `nothing`."""
function unbounded_ray(m::Model)
    d = zeros(max(num_col(m), 1))
    st = ccall((:jaos_unbounded_ray, libjaos), Cint,
               (Ptr{Cvoid}, Ptr{Cdouble}), m, d)
    return st == 0 ? d[1:num_col(m)] : nothing
end

"""
    read_solution(m, path) -> (objective, (x, act, y, d), (col_status, row_status))

An optimum's solution file written for this model; nothing is installed.
"""
function read_solution(m::Model, path::AbstractString)
    nc, nr = num_col(m), num_row(m)
    obj = Ref{Cdouble}(0.0)
    x, d, cs = zeros(nc), zeros(nc), zeros(Cint, nc)
    act, y, rs = zeros(nr), zeros(nr), zeros(Cint, nr)
    _check(m, ccall((:jaos_read_solution, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ref{Cdouble}, Ptr{Cdouble},
                     Ptr{Cdouble}, Ptr{Cint}, Ptr{Cdouble}, Ptr{Cdouble},
                     Ptr{Cint}), m, path, obj, x, d, cs, act, y, rs))
    return obj[], (x, act, y, d), (cs, rs)
end

"""
    read_certificate(m, path) -> (status, ray)

A certificate file: status 2 with one multiplier per row, or 3 with one
direction entry per column.
"""
function read_certificate(m::Model, path::AbstractString)
    st = Ref{Cint}(0)
    rr, cr = zeros(num_row(m)), zeros(num_col(m))
    _check(m, ccall((:jaos_read_certificate, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ref{Cint}, Ptr{Cdouble}, Ptr{Cdouble}),
                    m, path, st, rr, cr))
    return Int(st[]), st[] == 2 ? rr : cr
end

"""The cone records of a solution file, one vector per cone."""
function read_cone_duals(m::Model, path::AbstractString)
    sizes = [length(cone(m, k)[2]) for k in 0:num_cones(m)-1]
    z = zeros(max(sum(sizes; init = 0), 1))
    _check(m, ccall((:jaos_read_cone_duals, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{Cdouble}), m, path, z))
    at = cumsum([0; sizes])
    return [z[at[k]+1:at[k+1]] for k in eachindex(sizes)]
end

for (fn, cfn) in ((:read_basis, :jaos_read_basis),
                  (:read_mps_basis, :jaos_read_mps_basis))
    @eval function $fn(m::Model, path::AbstractString)
        cs, rs = zeros(Cint, num_col(m)), zeros(Cint, num_row(m))
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Cstring, Ptr{Cint}, Ptr{Cint}),
                        m, path, cs, rs))
        return cs, rs
    end
end

"""A point file from values the caller has, one per column."""
write_point_values(m::Model, path::AbstractString, x) =
    _check(m, ccall((:jaos_write_point_values, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{Cdouble}), m, path,
                    _doubles(x, num_col(m), "the point")))

"""A duals file from values the caller has, one per row."""
write_dual_values(m::Model, path::AbstractString, y) =
    _check(m, ccall((:jaos_write_dual_values, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{Cdouble}), m, path,
                    _doubles(y, num_row(m), "the duals")))

"""The column values in a point file, or another solver's solution file."""
function read_point(m::Model, path::AbstractString)
    x = zeros(num_col(m))
    _check(m, ccall((:jaos_read_point, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{Cdouble}), m, path, x))
    return x
end

"""The row multipliers in a file of the point file's shapes."""
function read_duals(m::Model, path::AbstractString)
    y = zeros(num_row(m))
    _check(m, ccall((:jaos_read_duals, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{Cdouble}), m, path, y))
    return y
end

"""What a solution file holds: 1 optimal, 2 infeasible or 3 unbounded."""
function solution_file_status(m::Model, path::AbstractString)
    st = Ref{Cint}(0)
    _check(m, ccall((:jaos_solution_file_status, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ref{Cint}), m, path, st))
    return Int(st[])
end

"""The `.sol` file AMPL reads after a solve; `message` opens it when given."""
write_sol_ampl(m::Model, path::AbstractString, message = nothing) =
    _check(m, ccall((:jaos_write_sol_ampl, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Cstring), m, path,
                    something(message, "")))

"""The C `jaos_check_report`, field by field; jaos.h says what each means."""
struct CheckReport
    max_col_violation::Cdouble
    max_row_violation::Cdouble
    max_row_violation_relative::Cdouble
    max_dual_violation::Cdouble
    primal_objective::Cdouble
    dual_objective::Cdouble
    objective_gap::Cdouble
    gap_positive::Cdouble
    gap_negative::Cdouble
    max_dropped_multiplier::Cdouble
    dropped_terms::Int64
    certified_suboptimality::Cdouble
    unquantified_rays::Int64
    relative_suboptimality::Cdouble
    primal_feasible::Bool
    dual_feasible::Bool
    checked_duals::Bool
    gap_certified::Bool
    max_integrality_violation::Cdouble
    max_cone_violation::Cdouble
end

"""The C `jaos_certificate_report`, field by field."""
struct CertificateReport
    sup_columns::Cdouble
    inf_rows::Cdouble
    gap::Cdouble
    certified::Bool
end

"""The C `jaos_ray_report`, field by field."""
struct RayReport
    rate::Cdouble
    max_col_escape::Cdouble
    max_row_escape::Cdouble
    curvature::Cdouble
    certified::Bool
end

function _cone_vector(m::Model, parts, what::AbstractString)
    nk = num_cones(m)
    length(parts) == nk ||
        throw(ArgumentError("$what has $(length(parts)) cones, expected $nk"))
    flat = Float64[]
    for (k, part) in enumerate(parts)
        append!(flat, _doubles(part, length(cone(m, k - 1)[2]), "$what[$k]"))
    end
    return isempty(flat) ? [0.0] : flat
end

"""
    check_solution(m, x, row_dual = nothing; tol = 1e-7) -> CheckReport

The library's independent checker on a point, and on row duals when
given, against the model as loaded.
"""
function check_solution(m::Model, x, row_dual = nothing; tol::Real = 1e-7)
    xs = _doubles(x, num_col(m), "the point")
    y = row_dual === nothing ? C_NULL : _doubles(row_dual, num_row(m), "row_dual")
    r = Ref{CheckReport}()
    _check(m, ccall((:jaos_check_solution, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}, Cdouble,
                     Ref{CheckReport}), m, xs, y, tol, r))
    return r[]
end

"""`check_solution` with the cone duals, one vector per cone."""
function check_conic_solution(m::Model, x, row_dual, cone_dual; tol::Real = 1e-7)
    xs = _doubles(x, num_col(m), "the point")
    y = row_dual === nothing ? C_NULL : _doubles(row_dual, num_row(m), "row_dual")
    z = _cone_vector(m, cone_dual, "cone_dual")
    r = Ref{CheckReport}()
    _check(m, ccall((:jaos_check_conic_solution, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cdouble},
                     Cdouble, Ref{CheckReport}), m, xs, y, z, tol, r))
    return r[]
end

"""Judges a Farkas ray, one multiplier per row, from the model alone."""
function check_certificate(m::Model, row_ray; tol::Real = 1e-7)
    r = Ref{CertificateReport}()
    _check(m, ccall((:jaos_check_certificate, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Cdouble, Ref{CertificateReport}),
                    m, _doubles(row_ray, num_row(m), "row_ray"), tol, r))
    return r[]
end

"""`check_certificate` with the cone part, one vector per cone."""
function check_conic_certificate(m::Model, row_ray, cone_ray; tol::Real = 1e-7)
    r = Ref{CertificateReport}()
    _check(m, ccall((:jaos_check_conic_certificate, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}, Cdouble,
                     Ref{CertificateReport}),
                    m, _doubles(row_ray, num_row(m), "row_ray"),
                    _cone_vector(m, cone_ray, "cone_ray"), tol, r))
    return r[]
end

"""Judges an unbounded direction, one entry per column."""
function check_ray(m::Model, col_ray; tol::Real = 1e-7)
    r = Ref{RayReport}()
    _check(m, ccall((:jaos_check_ray, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Cdouble, Ref{RayReport}),
                    m, _doubles(col_ray, num_col(m), "col_ray"), tol, r))
    return r[]
end

"""The C `jaos_iis_report`, field by field."""
struct IisReport
    members::Int64
    candidates::Int64
    solves::Int64
    work_units::Int64
    from_certificate::Bool
end

"""
    iis(m) -> (row_side, col_side, report) or nothing

One irreducible infeasible subsystem of an infeasible model's linear
relaxation, each side 0 none, 1 lower, 2 upper, 3 both; `nothing` when
there is none to find, with the reason in `error_message(m)`.
"""
function iis(m::Model)
    nr, nc = num_row(m), num_col(m)
    rs, cs = zeros(Cint, max(nr, 1)), zeros(Cint, max(nc, 1))
    r = Ref{IisReport}()
    st = _iis(m, rs, cs, r)
    _rethrow(m)
    return st == 0 ? (rs[1:nr], cs[1:nc], r[]) : nothing
end

"""
    iis_model(m, row_side, col_side) -> Model

The subsystem the sides describe as a model of its own: costs zeroed,
the other sides relaxed to infinity, rows and columns with nothing left
dropped. `iis_model(m, iis(m))` takes the triple `iis` returns.
"""
function iis_model(m::Model, row_side::AbstractVector{<:Integer},
                   col_side::AbstractVector{<:Integer})
    r = Ref{Ptr{Cvoid}}(C_NULL)
    _check(m, ccall((:jaos_iis_model, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}, Ref{Ptr{Cvoid}}), m,
                    _sides(row_side, num_row(m), "row_side"),
                    _sides(col_side, num_col(m), "col_side"), r))
    return _adopt(Model(r[]), m)
end

iis_model(m::Model, found::Tuple) = iis_model(m, found[1], found[2])

"""The C `jaos_relax_report`, field by field; `status` is a solve status."""
struct RelaxReport
    total::Cdouble
    rows_moved::Int64
    cols_moved::Int64
    at_row::Int64
    at_col::Int64
    largest::Cdouble
    work_units::Int64
    status::Cint
end

"""
    feasrelax(m, scope = :both) -> (row_move, col_move, report)

The smallest total move of bounds that makes the model feasible; `scope`
is `:rows`, `:cols` or `:both`. A negative move lowers a lower bound, a
positive one raises an upper bound.
"""
function feasrelax(m::Model, scope::Symbol = :both)
    code = scope === :rows ? 1 : scope === :cols ? 2 : scope === :both ? 3 :
           throw(ArgumentError("scope is :rows, :cols or :both"))
    nr, nc = num_row(m), num_col(m)
    rm, cm = zeros(nr), zeros(nc)
    r = Ref{RelaxReport}()
    _check(m, _feasrelax(m, code, rm, cm, r))
    return rm, cm, r[]
end

"""How far each column's cost may move with the optimal basis kept: `(lower, upper)`."""
function cost_ranging(m::Model)
    nc = num_col(m)
    lo, hi = zeros(nc), zeros(nc)
    _check(m, ccall((:jaos_cost_ranging, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}), m, lo, hi))
    return lo, hi
end

for (fn, cfn, count) in ((:rhs_ranging, :jaos_rhs_ranging, :num_row),
                         (:bound_ranging, :jaos_bound_ranging, :num_col))
    @eval function $fn(m::Model)
        n = $count(m)
        a, b, c, d = zeros(n), zeros(n), zeros(n), zeros(n)
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Ptr{Cdouble}, Ptr{Cdouble}, Ptr{Cdouble},
                         Ptr{Cdouble}), m, a, b, c, d))
        return a, b, c, d
    end
end

@doc """`(lower_lo, lower_hi, upper_lo, upper_hi)` per row: how far each of
its bounds may move with the optimal basis kept.""" rhs_ranging

@doc """`(lower_lo, lower_hi, upper_lo, upper_hi)` per column, for its own
bounds.""" bound_ranging

"""
The C `jaos_verify_report`, field by field: `status` 0 proved optimal,
1 broken, 2 refused; `stage` 0 none, 1 rank, 2 primal, 3 dual.
"""
struct VerifyReport
    status::Cint
    stage::Cint
    bound_bits::Cdouble
    capacity_bits::Cdouble
    blocks::Int64
    largest_block::Int64
    at_row::Int64
    at_col::Int64
    violation::Cdouble
    bytes_held::Int64
    terms::Int64
end

"""Proves, in exact arithmetic, that the basis behind the last optimum is optimal."""
function verify(m::Model)
    r = Ref{VerifyReport}()
    _check(m, ccall((:jaos_verify, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{VerifyReport}), m, r))
    return r[]
end

"""The same proof over a basis handed in, with no solve."""
function verify_basis(m::Model, col_status::AbstractVector{<:Integer},
                      row_status::AbstractVector{<:Integer})
    r = Ref{VerifyReport}()
    _check(m, ccall((:jaos_verify_basis, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}, Ref{VerifyReport}), m,
                    _sides(col_status, num_col(m), "col_status"),
                    _sides(row_status, num_row(m), "row_status"), r))
    return r[]
end

function _rational(s::AbstractString)
    parts = split(s, '/')
    num = parse(BigInt, parts[1])
    return length(parts) == 1 ? num // big(1) : num // parse(BigInt, parts[2])
end

function _exact(m::Model, call)
    out = Ref{Ptr{UInt8}}(C_NULL)
    _check(m, call(out))
    return _rational(unsafe_string(out[]))
end

"""A column's exact value at the proved optimum, as a `Rational{BigInt}`."""
exact_col_value(m::Model, j::Integer) = _exact(m, out ->
    ccall((:jaos_exact_col_value, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Ptr{UInt8}}), m, j, out))

exact_row_dual(m::Model, i::Integer) = _exact(m, out ->
    ccall((:jaos_exact_row_dual, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Ptr{UInt8}}), m, i, out))

exact_objective(m::Model) = _exact(m, out ->
    ccall((:jaos_exact_objective, libjaos), Cint,
          (Ptr{Cvoid}, Ref{Ptr{UInt8}}), m, out))

exact_row_multiplier(m::Model, i::Integer) = _exact(m, out ->
    ccall((:jaos_exact_row_multiplier, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Ptr{UInt8}}), m, i, out))

exact_col_direction(m::Model, j::Integer) = _exact(m, out ->
    ccall((:jaos_exact_col_direction, libjaos), Cint,
          (Ptr{Cvoid}, Int64, Ref{Ptr{UInt8}}), m, j, out))

"""The C `jaos_exact_ray_report`, field by field."""
struct ExactRayReport
    derived::Bool
    bound_bits::Cdouble
    capacity_bits::Cdouble
    blocks::Int64
    largest_block::Int64
    at_row::Int64
    bytes_held::Int64
    terms::Int64
end

for (fn, cfn) in ((:exact_certificate, :jaos_exact_certificate),
                  (:exact_unbounded_ray, :jaos_exact_unbounded_ray))
    @eval function $fn(m::Model)
        r = Ref{ExactRayReport}()
        _check(m, ccall(($(QuoteNode(cfn)), libjaos), Cint,
                        (Ptr{Cvoid}, Ref{ExactRayReport}), m, r))
        return r[]
    end
end

@doc """Derives the Farkas multipliers of an infeasible answer exactly;
`exact_row_multiplier` reads them.""" exact_certificate

@doc """Derives the direction of an unbounded answer exactly;
`exact_col_direction` reads it.""" exact_unbounded_ray

"""
The C `jaos_proof_report`, field by field: `kind` 0 optimal, 1 infeasible,
2 unbounded.
"""
struct ProofReport
    primal::Bool
    dual::Bool
    objective::Bool
    bad_row::Int64
    bad_col::Int64
    terms::Int64
    kind::Cint
    certified::Bool
end

"""Judges a proof file from the model alone, over the rationals."""
function check_proof(m::Model, path::AbstractString)
    r = Ref{ProofReport}()
    _check(m, ccall((:jaos_check_proof, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ref{ProofReport}), m, path, r))
    return r[]
end

"""The C `jaos_model_stats`, field by field."""
struct ModelStats
    num_row::Int64
    num_col::Int64
    num_nz::Int64
    integer_col::Int64
    binary_col::Int64
    equality_row::Int64
    ranged_row::Int64
    one_sided_row::Int64
    free_row::Int64
    fixed_col::Int64
    ranged_col::Int64
    one_sided_col::Int64
    free_col::Int64
    empty_row::Int64
    empty_col::Int64
    obj_nz::Int64
    min_abs::Cdouble
    max_abs::Cdouble
    obj_min_abs::Cdouble
    obj_max_abs::Cdouble
    semicontinuous_col::Int64
    sos_set::Int64
    indicator_row::Int64
    quadratic_col::Int64
    cone_set::Int64
    quadratic_row::Int64
end

"""What the model is, counted in one pass; solves nothing."""
function statistics(m::Model)
    r = Ref{ModelStats}()
    _check(m, ccall((:jaos_model_statistics, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{ModelStats}), m, r))
    return r[]
end

"""The C `jaos_presolve_report`, field by field."""
struct PresolveReport
    num_row::Int64
    num_col::Int64
    num_nz::Int64
    rounds::Int64
    fixed_col::Int64
    empty_row::Int64
    empty_col::Int64
    singleton_row::Int64
    singleton_col::Int64
    free_col_singleton::Int64
    forcing_row::Int64
    redundant_row::Int64
    implied_free_col::Int64
    tightened_bound::Int64
    duplicate_row::Int64
    duplicate_col::Int64
    dominated_col::Int64
    aggregated_col::Int64
end

"""What presolve did on the last solve; all zero before one."""
function presolve_report(m::Model)
    r = Ref{PresolveReport}()
    _check(m, ccall((:jaos_presolve_result, libjaos), Cint,
                    (Ptr{Cvoid}, Ref{PresolveReport}), m, r))
    return r[]
end

"""What the progress callback sees: the C `jaos_progress`."""
struct Progress
    iterations::Int64
    work_units::Int64
    primal_infeasibility::Cdouble
end

struct _Incumbent
    node::Int64
    objective::Cdouble
    bound::Cdouble
    col_value::Ptr{Cdouble}
    num_col::Int64
    by_rounding::Bool
end

"""
What the incumbent callback sees: the node, the incumbent's objective,
the tree's bound, the point by column, and whether a heuristic or the
MIP start found it.
"""
struct Incumbent
    node::Int64
    objective::Float64
    bound::Float64
    values::Vector{Float64}
    by_rounding::Bool
end

struct _Node
    node::Int64
    depth::Int64
    objective::Cdouble
    bound::Cdouble
    col_value::Ptr{Cdouble}
    num_col::Int64
    integral::Bool
    branch_col::Int64
    internal::Ptr{Cvoid}
end

"""
What the node callback sees: `node`, `depth`, `objective` (the node's
relaxation), `bound` (the tree's), `values` (the point by column),
`integral` (the point would become an incumbent) and `branch_col`, the
0-based column the tree branches on next, or -1. Setting `branch_col` to
another integer column fractional at the point branches there instead.
`node_add_row` adds a row to the search. The event is live only while
the callback runs.
"""
mutable struct NodeEvent
    ptr::Ptr{_Node}
    node::Int64
    depth::Int64
    objective::Float64
    bound::Float64
    values::Vector{Float64}
    integral::Bool
    branch_col::Int64
end

"""
    node_add_row(ev, index, value, lower = -Inf, upper = Inf)

Adds `lower <= sum(value[k] * x[index[k]]) <= upper` (0-based columns)
from inside a node callback. It holds for every solution the tree takes
from then on: a lazy row at an integral point, a cut at a fractional one.
"""
function node_add_row(ev::NodeEvent, index::AbstractVector{<:Integer},
                      value::AbstractVector{<:Real}, lower::Real = -Inf,
                      upper::Real = Inf)
    ev.ptr == C_NULL && throw(JaosError(1, "this node event is over"))
    length(index) == length(value) ||
        throw(ArgumentError("index and value differ in length"))
    st = ccall((:jaos_node_add_row, libjaos), Cint,
               (Ptr{_Node}, Int64, Ptr{Int64}, Ptr{Cdouble}, Cdouble, Cdouble),
               ev.ptr, length(index), isempty(index) ? Int64[0] : Vector{Int64}(index),
               isempty(value) ? [0.0] : Vector{Float64}(value), lower, upper)
    st == 0 || throw(JaosError(st, "the node callback may not add this row: " *
                               "a column out of range, a value that is not " *
                               "finite, or bounds that cross"))
    return
end

"""
    node_add_solution(ev, values)

Hands the tree a point, one value per column, from inside a node callback.
Before its next node the tree rounds the integer columns and takes the
point as its incumbent when it meets every bound and row and beats the
incumbent it has; a later call replaces an earlier one.
"""
function node_add_solution(ev::NodeEvent, values::AbstractVector{<:Real})
    ev.ptr == C_NULL && throw(JaosError(1, "this node event is over"))
    st = ccall((:jaos_node_add_solution, libjaos), Cint,
               (Ptr{_Node}, Int64, Ptr{Cdouble}),
               ev.ptr, length(values),
               isempty(values) ? [0.0] : Vector{Float64}(values))
    st == 0 || throw(JaosError(st, "the node callback may not hand this " *
                               "point: one value per column, every value " *
                               "finite"))
    return
end

_values(p::Ptr{Cdouble}, n::Integer) = n == 0 ? Float64[] : copy(unsafe_wrap(Array, p, n))

function _act(f::F, m::Model) where {F}
    try
        return f() === :stop ? Cint(1) : Cint(0)
    catch e
        m.callback_error === nothing && (m.callback_error = e)
        return Cint(1)
    end
end

function _log_line(user::Ptr{Cvoid}, level::Cint, line::Cstring)
    m = unsafe_pointer_to_objref(user)::Model
    try
        m.log_sink(unsafe_string(line))
    catch e
        m.callback_error === nothing && (m.callback_error = e)
    end
    return
end

function _progress_event(p::Ptr{Progress}, user::Ptr{Cvoid})
    m = unsafe_pointer_to_objref(user)::Model
    return _act(() -> m.progress_fn(unsafe_load(p)), m)
end

function _incumbent_event(p::Ptr{_Incumbent}, user::Ptr{Cvoid})
    m = unsafe_pointer_to_objref(user)::Model
    c = unsafe_load(p)
    inc = Incumbent(c.node, c.objective, c.bound, _values(c.col_value, c.num_col),
                    c.by_rounding)
    return _act(() -> m.incumbent_fn(inc), m)
end

function _node_event(p::Ptr{_Node}, user::Ptr{Cvoid})
    m = unsafe_pointer_to_objref(user)::Model
    c = unsafe_load(p)
    ev = NodeEvent(p, c.node, c.depth, c.objective, c.bound,
                   _values(c.col_value, c.num_col), c.integral, c.branch_col)
    r = _act(() -> m.node_fn(ev), m)
    unsafe_store!(Ptr{Int64}(p + fieldoffset(_Node, 8)), ev.branch_col)
    ev.ptr = C_NULL
    return r
end

_user(m::Model, on::Bool) = on ? pointer_from_objref(m) : C_NULL

function _install_log(m::Model)
    on = m.log_sink !== nothing
    return _check(m, ccall((:jaos_set_log_callback, libjaos), Cint,
        (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}), m,
        on ? @cfunction(_log_line, Cvoid, (Ptr{Cvoid}, Cint, Cstring)) : C_NULL,
        _user(m, on)))
end

"""
Sends the solver's log at `level` (0 off, 1 summary, 2 progress, 3 detail)
to `sink`, a function of one line; `println` by default. An exception in
`sink` comes out of the call that was logging.
"""
function set_log(m::Model, level::Integer; sink = println)
    m.log_sink = level > 0 ? sink : nothing
    _install_log(m)
    return _check(m, ccall((:jaos_set_log_level, libjaos), Cint,
                           (Ptr{Cvoid}, Cint), m, level))
end

"""
    set_progress_callback(m, fn)

Calls `fn(p::Progress)` while a solve iterates. Returning `:stop` ends
the solve as interrupted (status 7); anything else lets it run on.
`nothing` removes it. It runs on the thread that called `solve`; on more
than one thread the concurrent solve and a tree call it less often. An
exception in `fn` stops the solve and comes out of `solve`.
"""
function set_progress_callback(m::Model, fn)
    m.progress_fn = fn
    on = fn !== nothing
    return _check(m, ccall((:jaos_set_progress_callback, libjaos), Cint,
        (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}), m,
        on ? @cfunction(_progress_event, Cint, (Ptr{Progress}, Ptr{Cvoid})) : C_NULL,
        _user(m, on)))
end

"""
    set_incumbent_callback(m, fn)

Calls `fn(inc::Incumbent)` at each new incumbent of a branch and bound.
Returning `:stop` ends the search as interrupted with the incumbent kept.
The rest is `set_progress_callback`'s rule.
"""
function set_incumbent_callback(m::Model, fn)
    m.incumbent_fn = fn
    on = fn !== nothing
    return _check(m, ccall((:jaos_set_incumbent_callback, libjaos), Cint,
        (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}), m,
        on ? @cfunction(_incumbent_event, Cint, (Ptr{_Incumbent}, Ptr{Cvoid})) : C_NULL,
        _user(m, on)))
end

"""
    set_node_callback(m, fn)

Calls `fn(ev::NodeEvent)` at every node of a branch and bound once its
relaxation is solved, and at every point a heuristic would make an
incumbent. `fn` may add rows with `node_add_row` and set
`ev.branch_col`. A node whose point a new row cuts is solved again and
seen again. Returning `:stop` ends the search as interrupted.
"""
function set_node_callback(m::Model, fn)
    m.node_fn = fn
    on = fn !== nothing
    return _check(m, ccall((:jaos_set_node_callback, libjaos), Cint,
        (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}), m,
        on ? @cfunction(_node_event, Cint, (Ptr{_Node}, Ptr{Cvoid})) : C_NULL,
        _user(m, on)))
end

function _adopt(new::Model, old::Model)
    new.log_sink = old.log_sink
    _install_log(new)
    set_progress_callback(new, old.progress_fn)
    set_incumbent_callback(new, old.incumbent_fn)
    set_node_callback(new, old.node_fn)
    return new
end

include("MOI_wrapper.jl")

end
