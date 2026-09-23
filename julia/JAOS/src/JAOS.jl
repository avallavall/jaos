# SPDX-License-Identifier: Apache-2.0
"""
JAOS from Julia: the C library through `ccall`, and a MathOptInterface
optimizer, `JAOS.Optimizer`, so JuMP can use it directly.

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
    print(io, "JAOS error ", e.status, ": ", e.msg)

version() = unsafe_string(ccall((:jaos_version, libjaos), Cstring, ()))

"""A `jaos_model`, freed when Julia collects it."""
mutable struct Model
    ptr::Ptr{Cvoid}
    log_sink::Any
    function Model()
        r = Ref{Ptr{Cvoid}}(C_NULL)
        st = ccall((:jaos_model_new, libjaos), Cint, (Ref{Ptr{Cvoid}},), r)
        st == 0 || throw(JaosError(st, "jaos_model_new failed"))
        m = new(r[], println)
        finalizer(_free, m)
        return m
    end
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

_check(m::Model, st::Cint) = st == 0 ? nothing : throw(JaosError(st, error_message(m)))

const _READERS = [
    (".mps", :jaos_read_mps), (".mps.gz", :jaos_read_mps),
    (".lp", :jaos_read_lp), (".lp.gz", :jaos_read_lp),
    (".nl", :jaos_read_nl), (".nl.gz", :jaos_read_nl),
    (".qplib", :jaos_read_qplib), (".qplib.gz", :jaos_read_qplib),
    (".osil", :jaos_read_osil), (".osil.gz", :jaos_read_osil),
    (".cbf", :jaos_read_cbf), (".cbf.gz", :jaos_read_cbf),
]

"""
    read_file(m, path)

Reads `path` into `m` with the reader its name selects, as `jaos solve`
does: MPS for any name without a known extension.
"""
function read_file(m::Model, path::AbstractString)
    for (ext, fn) in _READERS
        if endswith(path, ext)
            return _read(m, fn, path)
        end
    end
    return _read(m, :jaos_read_mps, path)
end

for fn in unique(last.(_READERS))
    @eval function _read(m::Model, ::Val{$(QuoteNode(fn))}, path::AbstractString)
        return _check(m, ccall(($(QuoteNode(fn)), libjaos), Cint,
                               (Ptr{Cvoid}, Cstring), m, path))
    end
end
_read(m::Model, fn::Symbol, path::AbstractString) = _read(m, Val(fn), path)

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

set_col_integer(m::Model, j::Integer, on::Bool = true) =
    _check(m, ccall((:jaos_set_col_integer, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Bool), m, j, on))

"""The objective's `½ x'Qx`, one entry per diagonal and per pair (0-based)."""
set_quadratic(m::Model, rows::Vector{Int64}, cols::Vector{Int64},
              vals::Vector{Float64}) =
    _check(m, ccall((:jaos_set_quadratic, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Ptr{Int64}, Ptr{Int64}, Ptr{Cdouble}),
                    m, length(vals), rows, cols, vals))

set_row_quadratic(m::Model, row::Integer, rows::Vector{Int64},
                  cols::Vector{Int64}, vals::Vector{Float64}) =
    _check(m, ccall((:jaos_set_row_quadratic, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Ptr{Int64}, Ptr{Int64},
                     Ptr{Cdouble}),
                    m, row, length(vals), rows, cols, vals))

"""A cone over columns (0-based): `:quadratic` or `:rotated`."""
add_cone(m::Model, kind::Symbol, cols::Vector{Int64}) =
    _check(m, ccall((:jaos_add_cone, libjaos), Cint,
                    (Ptr{Cvoid}, Cint, Int64, Ptr{Int64}),
                    m, kind === :rotated ? 2 : 1, length(cols), cols))

"""A column that is 0 or between its bounds."""
set_col_semicontinuous(m::Model, j::Integer, on::Bool = true) =
    _check(m, ccall((:jaos_set_col_semicontinuous, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Bool), m, j, on))

"""An SOS set of type 1 or 2 over columns (0-based), ordered by `weights`."""
add_sos(m::Model, type::Integer, cols::Vector{Int64}, weights::Vector{Float64}) =
    _check(m, ccall((:jaos_add_sos, libjaos), Cint,
                    (Ptr{Cvoid}, Cint, Int64, Ptr{Int64}, Ptr{Cdouble}),
                    m, type, length(cols), cols, weights))

"""Row `row` holds only while integer column `col` equals `value` (0 or 1)."""
set_row_indicator(m::Model, row::Integer, col::Integer, value::Integer) =
    _check(m, ccall((:jaos_set_row_indicator, libjaos), Cint,
                    (Ptr{Cvoid}, Int64, Int64, Cint), m, row, col, value))

"""The tree's gap: it stops when no node beats the incumbent by more than
`gap * (1 + |incumbent|)`; 0 restores the default."""
set_mip_gap(m::Model, gap::Real) =
    _check(m, ccall((:jaos_set_mip_gap, libjaos), Cint,
                    (Ptr{Cvoid}, Cdouble), m, gap))

"""The tree stops after `nodes` nodes; 0 means no limit."""
set_mip_node_limit(m::Model, nodes::Integer) =
    _check(m, ccall((:jaos_set_mip_node_limit, libjaos), Cint,
                    (Ptr{Cvoid}, Int64), m, nodes))

set_option(m::Model, name::AbstractString, value) =
    _check(m, ccall((:jaos_set_option, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Cstring), m, name, string(value)))

function get_option(m::Model, name::AbstractString)
    buf = zeros(UInt8, 512)
    _check(m, ccall((:jaos_get_option, libjaos), Cint,
                    (Ptr{Cvoid}, Cstring, Ptr{UInt8}, Int64),
                    m, name, buf, length(buf)))
    return unsafe_string(pointer(buf))
end

set_time_limit(m::Model, seconds::Real) =
    _check(m, ccall((:jaos_set_time_limit, libjaos), Cint,
                    (Ptr{Cvoid}, Cdouble), m, seconds))

set_threads(m::Model, n::Integer) =
    _check(m, ccall((:jaos_set_threads, libjaos), Cint,
                    (Ptr{Cvoid}, Int64), m, n))

set_mip_start(m::Model, x::Vector{Float64}) =
    _check(m, ccall((:jaos_set_mip_start, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cdouble}), m, x))

solve(m::Model) = _check(m, ccall((:jaos_solve, libjaos), Cint, (Ptr{Cvoid},), m))

"""Where the last solve stopped, as the C enum's value (1 is optimal)."""
status(m::Model) = Int(ccall((:jaos_status_of, libjaos), Cint, (Ptr{Cvoid},), m))

status_string(s::Integer) =
    unsafe_string(ccall((:jaos_solve_status_str, libjaos), Cstring, (Cint,), s))

num_col(m::Model) = Int(ccall((:jaos_num_col, libjaos), Int64, (Ptr{Cvoid},), m))
num_row(m::Model) = Int(ccall((:jaos_num_row, libjaos), Int64, (Ptr{Cvoid},), m))
num_cones(m::Model) = Int(ccall((:jaos_num_cones, libjaos), Int64, (Ptr{Cvoid},), m))
has_integer(m::Model) = ccall((:jaos_model_has_integer, libjaos), Bool, (Ptr{Cvoid},), m)

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
solve left neither.
"""
function cone_dual(m::Model, k::Integer, n::Integer)
    z = zeros(n)
    st = ccall((:jaos_cone_dual, libjaos), Cint,
               (Ptr{Cvoid}, Int64, Ptr{Cdouble}), m, k, z)
    return st == 0 ? z : nothing
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

"""The Farkas multipliers of an infeasible model, one per row, or `nothing`."""
function certificate(m::Model)
    y = zeros(max(num_row(m), 1))
    st = ccall((:jaos_certificate, libjaos), Cint, (Ptr{Cvoid}, Ptr{Cdouble}),
               m, y)
    return st == 0 ? y[1:num_row(m)] : nothing
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
    st = ccall((:jaos_iis, libjaos), Cint,
               (Ptr{Cvoid}, Ptr{Cint}, Ptr{Cint}, Ref{IisReport}), m, rs, cs, r)
    return st == 0 ? (rs[1:nr], cs[1:nc], r[]) : nothing
end

"""The improving direction of an unbounded model, one per column, or `nothing`."""
function unbounded_ray(m::Model)
    d = zeros(max(num_col(m), 1))
    st = ccall((:jaos_unbounded_ray, libjaos), Cint,
               (Ptr{Cvoid}, Ptr{Cdouble}), m, d)
    return st == 0 ? d[1:num_col(m)] : nothing
end

function _log_line(user::Ptr{Cvoid}, level::Cint, line::Cstring)
    m = unsafe_pointer_to_objref(user)::Model
    m.log_sink(unsafe_string(line))
    return
end

"""
Sends the solver's log at `level` (0 off, 1 summary, 2 progress, 3 detail)
to `sink`, a function of one line; `println` by default.
"""
function set_log(m::Model, level::Integer; sink = println)
    m.log_sink = sink
    cb = @cfunction(_log_line, Cvoid, (Ptr{Cvoid}, Cint, Cstring))
    _check(m, ccall((:jaos_set_log_callback, libjaos), Cint,
                    (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}), m,
                    level > 0 ? cb : C_NULL,
                    level > 0 ? pointer_from_objref(m) : C_NULL))
    return _check(m, ccall((:jaos_set_log_level, libjaos), Cint,
                           (Ptr{Cvoid}, Cint), m, level))
end

include("MOI_wrapper.jl")

end
