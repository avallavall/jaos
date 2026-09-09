"""JAOS from Python.

A ctypes wrapper over `libjaos.so`, or `jaos.dll` on Windows. The standard
library and nothing else,
which is the same rule the C library holds itself to: a binding that needs a
compiler, a header or a package index at install time is a dependency, and
this project does not take those.

ctypes rather than a C extension for three reasons. It needs no `Python.h`
and no compiler on the user's machine. It works on any CPython 3 without
rebuilding. And the call overhead it costs is measured in microseconds
against solves measured in seconds, so it buys all of that for nothing that
matters here.

Finding the library, in order:

1. the ``JAOS_LIBRARY`` environment variable, a full path;
2. the library beside this file;
3. ``build/release/libjaos.so`` under the current directory, or the CMake
   build's ``jaos.dll`` under ``build/cmake`` on Windows;
4. the system loader's search path.

The file name is ``libjaos.so`` on Linux, ``libjaos.dylib`` on macOS and
``jaos.dll`` or ``libjaos.dll`` on Windows.

Build it with ``make shared``.

Two layers, and both are public. `Problem` is where a model is written by
hand: variables, expressions with ordinary arithmetic, constraints from
ordinary comparisons.

    import jaos

    p = jaos.Problem()
    x = p.add_var(ub=4)
    y = p.add_var()
    p.add(x + y <= 4)
    p.maximize(x + 2 * y)
    if p.solve() is jaos.SolveStatus.OPTIMAL:
        print(p.objective_value, x.value, y.value)

`Model` is the C API one call to one call, and is where a model from a file
lives:

    m = jaos.Model()
    m.read_mps("model.mps")
    m.solve()
    if m.status is jaos.SolveStatus.OPTIMAL:
        print(m.objective(), m.solution().col_value)

`Problem` is sugar over `Model` and owns no arithmetic: everything it builds
is validated again by the C side it hands the arrays to. `Problem.model`
reaches the layer below when both are wanted at once.

Errors that the C API reports as a status become `JaosError`, carrying both
the status and whatever `jaos_model_error` had to say. Nothing returns a
status code to be checked by hand.
"""

import ctypes
import ctypes.util
import enum
import os
import sys
from collections import namedtuple
from fractions import Fraction

__all__ = [
    "Model", "JaosError", "Status", "SolveStatus", "ObjSense", "LogLevel",
    "BasisStatus", "CallbackAction", "Solution", "Basis", "INFINITY",
    "NodeEvent",
    "NAME_MAX", "version", "library_path",
    "Problem", "Var", "LinExpr", "Constraint", "quicksum",
    "CheckReport", "CertificateReport", "RayReport", "Progress",
    "IISSide", "IISReport", "IIS",
    "RelaxScope", "RelaxReport", "Relaxation",
    "Proof", "ProofStage", "VerifyReport", "ExactRayReport",
    "MipReport",
]

class Status(enum.IntEnum):
    """What a C call returned. Only OK reaches the caller of this module."""
    OK = 0
    ERR_INVALID_INPUT = 1
    ERR_OUT_OF_MEMORY = 2
    ERR_IO = 3
    ERR_NUMERICAL = 4

class SolveStatus(enum.IntEnum):
    """Where a solve stopped. A budget stop is an honest report, not a
    failure, which is why it lives here and not in `Status`."""
    NOT_RUN = 0
    OPTIMAL = 1
    INFEASIBLE = 2
    UNBOUNDED = 3
    WORK_LIMIT = 4
    TIME_LIMIT = 5
    NUMERICAL_ERROR = 6
    INTERRUPTED = 7
    NODE_LIMIT = 8

class ObjSense(enum.IntEnum):
    MINIMIZE = 0
    MAXIMIZE = 1

class LogLevel(enum.IntEnum):
    OFF = 0
    SUMMARY = 1
    PROGRESS = 2
    DETAIL = 3

class BasisStatus(enum.IntEnum):
    BASIC = 0
    AT_LOWER = 1
    AT_UPPER = 2
    FREE = 3

class CallbackAction(enum.IntEnum):
    CONTINUE = 0
    STOP = 1

class Branching(enum.IntEnum):
    """Which column a fractional node branches on (D292); jaos_branching."""
    PSEUDOCOST = 0
    MOST_FRACTIONAL = 1

class Algorithm(enum.IntEnum):
    """What solves an LP; jaos_algorithm. The barrier is an interior point
    method and PDLP a first-order one, both for plain LPs and both
    finished by a crossover to the dual simplex; a MIP's relaxations stay
    on the dual simplex. CONCURRENT runs the dual, the primal and the
    barrier in turn on growing work budgets and keeps the first answer;
    it costs the sum of the three."""
    DUAL = 0
    PRIMAL = 1
    BARRIER = 2
    PDLP = 3
    CONCURRENT = 4

class DiveChild(enum.IntEnum):
    """Which child a dive solves first (D295); jaos_dive_child."""
    NEARER = 0
    UP = 1
    DOWN = 2
    PSEUDOCOST = 3

class JaosError(Exception):
    """A C call that did not return OK.

    `status` is the `Status` it returned and `detail` is the model's own
    message, which is usually the specific one: a line number, a column
    index, or what the solver refused to conclude.
    """

    def __init__(self, status, detail=""):
        self.status = Status(status)
        self.detail = detail
        name = _lib.jaos_status_str(int(status)).decode("utf-8", "replace")
        super().__init__(f"{name}: {detail}" if detail else name)

Solution = namedtuple("Solution",
                      "col_value row_activity row_dual col_dual")
Basis = namedtuple("Basis", "col_status row_status")

CostRanging = namedtuple("CostRanging", "lower upper")
BoundRanging = namedtuple("BoundRanging",
                          "lower_lo lower_hi upper_lo upper_hi")

Progress = namedtuple("Progress",
                      "iterations work_units primal_infeasibility")

Incumbent = namedtuple("Incumbent",
                       "node objective bound values by_rounding")

def _library_names(platform=sys.platform):
    """The file names the library may carry on this platform, in the order
    they are tried, and the build directories that may hold them."""
    if platform.startswith("win"):
        return ["jaos.dll", "libjaos.dll"], [
            os.path.join("build", "cmake"),
            os.path.join("build", "cmake", "Release"),
            os.path.join("build", "release")]
    if platform == "darwin":
        return ["libjaos.dylib", "libjaos.so"], [
            os.path.join("build", "release"), os.path.join("build", "cmake")]
    return ["libjaos.so"], [
        os.path.join("build", "release"), os.path.join("build", "cmake")]


def _find_library():
    env = os.environ.get("JAOS_LIBRARY")
    if env:
        if not os.path.exists(env):
            raise OSError(f"JAOS_LIBRARY is set to {env!r}, which does not "
                          f"exist")
        return env
    names, dirs = _library_names()
    here = os.path.dirname(os.path.abspath(__file__))
    for name in names:
        p = os.path.join(here, name)
        if os.path.exists(p):
            return p
    for d in dirs:
        for name in names:
            p = os.path.join(os.getcwd(), d, name)
            if os.path.exists(p):
                return p
    found = ctypes.util.find_library("jaos")
    if found:
        return found
    raise OSError(
        f"{names[0]} not found. Build it with `make shared` or the CMake "
        "package, then either run from the repository root or set "
        "JAOS_LIBRARY to its full path.")

_LIB_PATH = _find_library()
_lib = ctypes.CDLL(_LIB_PATH)

def library_path():
    """The shared library this module actually loaded."""
    return _LIB_PATH

_P = ctypes.POINTER
_I64 = ctypes.c_int64
_D = ctypes.c_double
_VP = ctypes.c_void_p
_CS = ctypes.c_char_p

class _Progress(ctypes.Structure):
    """jaos_progress, field for field."""
    _fields_ = [
        ("iterations", _I64),
        ("work_units", _I64),
        ("primal_infeasibility", _D),
    ]

class _CheckReport(ctypes.Structure):
    """jaos_check_report, field for field and in the header's order. What
    each field means, and what none of them decides, is documented on the
    struct in jaos.h; this mirror carries the layout only."""
    _fields_ = [
        ("max_col_violation", _D),
        ("max_row_violation", _D),
        ("max_row_violation_relative", _D),
        ("max_dual_violation", _D),
        ("primal_objective", _D),
        ("dual_objective", _D),
        ("objective_gap", _D),
        ("gap_positive", _D),
        ("gap_negative", _D),
        ("max_dropped_multiplier", _D),
        ("dropped_terms", _I64),
        ("certified_suboptimality", _D),
        ("unquantified_rays", _I64),
        ("relative_suboptimality", _D),
        ("primal_feasible", ctypes.c_bool),
        ("dual_feasible", ctypes.c_bool),
        ("checked_duals", ctypes.c_bool),
        ("gap_certified", ctypes.c_bool),
        ("max_integrality_violation", _D),
    ]

class _PresolveReport(ctypes.Structure):
    """jaos_presolve_report, in the header's order."""
    _fields_ = [
        ("num_row", _I64),
        ("num_col", _I64),
        ("num_nz", _I64),
        ("rounds", _I64),
        ("fixed_col", _I64),
        ("empty_row", _I64),
        ("empty_col", _I64),
        ("singleton_row", _I64),
        ("singleton_col", _I64),
        ("free_col_singleton", _I64),
        ("forcing_row", _I64),
        ("redundant_row", _I64),
        ("implied_free_col", _I64),
        ("tightened_bound", _I64),
        ("duplicate_row", _I64),
        ("duplicate_col", _I64),
        ("dominated_col", _I64),
    ]

class _ModelStats(ctypes.Structure):
    """jaos_model_stats, in the header's order."""
    _fields_ = [
        ("num_row", _I64),
        ("num_col", _I64),
        ("num_nz", _I64),
        ("integer_col", _I64),
        ("binary_col", _I64),
        ("equality_row", _I64),
        ("ranged_row", _I64),
        ("one_sided_row", _I64),
        ("free_row", _I64),
        ("fixed_col", _I64),
        ("ranged_col", _I64),
        ("one_sided_col", _I64),
        ("free_col", _I64),
        ("empty_row", _I64),
        ("empty_col", _I64),
        ("obj_nz", _I64),
        ("min_abs", _D),
        ("max_abs", _D),
        ("obj_min_abs", _D),
        ("obj_max_abs", _D),
        ("semicontinuous_col", _I64),
        ("sos_set", _I64),
        ("indicator_row", _I64),
        ("quadratic_col", _I64),
    ]

class ProofKind(enum.IntEnum):
    """Which of the three a proof file claims (jaos_proof_kind, D328)."""
    OPTIMAL = 0
    INFEASIBLE = 1
    UNBOUNDED = 2

class _ProofReport(ctypes.Structure):
    """jaos_proof_report, in the header's order."""
    _fields_ = [
        ("primal", ctypes.c_bool),
        ("dual", ctypes.c_bool),
        ("objective", ctypes.c_bool),
        ("bad_row", _I64),
        ("bad_col", _I64),
        ("terms", _I64),
        ("kind", ctypes.c_int),
        ("certified", ctypes.c_bool),
    ]

class _MipReport(ctypes.Structure):
    """jaos_mip_report, in the header's order."""
    _fields_ = [
        ("nodes", _I64),
        ("lp_solves", _I64),
        ("has_incumbent", ctypes.c_bool),
        ("incumbent", _D),
        ("bound", _D),
        ("cuts", _I64),
        ("heuristic_points", _I64),
        ("first_incumbent_node", _I64),
        ("fixed_cols", _I64),
        ("tightened", _I64),
        ("symmetry_generators", _I64),
        ("symmetry_orbits", _I64),
    ]

MipReport = namedtuple("MipReport", [f for f, _ in _MipReport._fields_])

ProofReport = namedtuple("ProofReport",
                         [f for f, _ in _ProofReport._fields_])

ModelStats = namedtuple("ModelStats", [f for f, _ in _ModelStats._fields_])

PresolveReport = namedtuple("PresolveReport",
                            [f for f, _ in _PresolveReport._fields_])

CheckReport = namedtuple("CheckReport",
                         [f for f, _ in _CheckReport._fields_])

class _CertificateReport(ctypes.Structure):
    """jaos_certificate_report, field for field. The proof is a difference
    of two sums, and both halves come back beside it (jaos.h)."""
    _fields_ = [
        ("sup_columns", _D),
        ("inf_rows", _D),
        ("gap", _D),
        ("certified", ctypes.c_bool),
    ]

class _RayReport(ctypes.Structure):
    """jaos_ray_report, field for field."""
    _fields_ = [
        ("rate", _D),
        ("max_col_escape", _D),
        ("max_row_escape", _D),
        ("certified", ctypes.c_bool),
    ]

CertificateReport = namedtuple("CertificateReport",
                               [f for f, _ in _CertificateReport._fields_])
RayReport = namedtuple("RayReport", [f for f, _ in _RayReport._fields_])

class IISSide(enum.IntFlag):
    """Which sides of a bound belong to an irreducible infeasible
    subsystem (jaos_iis_side): a row's or a column's two bounds are two
    constraints, and an IIS may hold either without the other."""
    NONE = 0
    LOWER = 1
    UPPER = 2
    BOTH = 3

class _IISReport(ctypes.Structure):
    """jaos_iis_report, field for field."""
    _fields_ = [
        ("members", ctypes.c_int64),
        ("candidates", ctypes.c_int64),
        ("solves", ctypes.c_int64),
        ("work_units", ctypes.c_int64),
        ("from_certificate", ctypes.c_bool),
    ]

IISReport = namedtuple("IISReport", [f for f, _ in _IISReport._fields_])
IIS = namedtuple("IIS", "row_side col_side report")

class RelaxScope(enum.IntEnum):
    """Which bounds a feasibility relaxation may move (jaos_relax_scope)."""
    ROWS = 1
    COLS = 2
    BOTH = 3

class _RelaxReport(ctypes.Structure):
    """jaos_relax_report, field for field."""
    _fields_ = [
        ("total", _D),
        ("rows_moved", ctypes.c_int64),
        ("cols_moved", ctypes.c_int64),
        ("at_row", ctypes.c_int64),
        ("at_col", ctypes.c_int64),
        ("largest", _D),
        ("work_units", ctypes.c_int64),
        ("status", ctypes.c_int),
    ]

RelaxReport = namedtuple("RelaxReport", [f for f, _ in _RelaxReport._fields_])
Relaxation = namedtuple("Relaxation", "row_move col_move report")

class Proof(enum.IntEnum):
    """What jaos_verify concluded (jaos_proof). REFUSED is not a failure:
    it is the honest answer when the numbers a proof needs do not fit, and
    the report says how far outside they were."""
    OPTIMAL = 0
    BROKEN = 1
    REFUSED = 2

class ProofStage(enum.IntEnum):
    """Which check a BROKEN verdict came from (jaos_proof_stage). They run
    in this order and the first to fail is the one reported."""
    NONE = 0
    RANK = 1
    PRIMAL = 2
    DUAL = 3

class _VerifyReport(ctypes.Structure):
    """jaos_verify_report, field for field."""
    _fields_ = [
        ("status", ctypes.c_int),
        ("stage", ctypes.c_int),
        ("bound_bits", _D),
        ("capacity_bits", _D),
        ("blocks", ctypes.c_int64),
        ("largest_block", ctypes.c_int64),
        ("at_row", ctypes.c_int64),
        ("at_col", ctypes.c_int64),
        ("violation", _D),
        ("bytes_held", ctypes.c_int64),
        ("terms", ctypes.c_int64),
    ]

VerifyReport = namedtuple("VerifyReport",
                          [f for f, _ in _VerifyReport._fields_])

class _ExactRayReport(ctypes.Structure):
    """jaos_exact_ray_report, field for field."""
    _fields_ = [
        ("derived", ctypes.c_bool),
        ("bound_bits", _D),
        ("capacity_bits", _D),
        ("blocks", ctypes.c_int64),
        ("largest_block", ctypes.c_int64),
        ("at_row", ctypes.c_int64),
        ("bytes_held", ctypes.c_int64),
        ("terms", ctypes.c_int64),
    ]

ExactRayReport = namedtuple("ExactRayReport",
                            [f for f, _ in _ExactRayReport._fields_])

_LOG_FN = ctypes.CFUNCTYPE(None, _VP, ctypes.c_int, _CS)
_PROGRESS_FN = ctypes.CFUNCTYPE(ctypes.c_int, _P(_Progress), _VP)

class _Incumbent(ctypes.Structure):
    """jaos_incumbent, field for field."""
    _fields_ = [
        ("node", _I64),
        ("objective", _D),
        ("bound", _D),
        ("col_value", _P(_D)),
        ("num_col", _I64),
        ("by_rounding", ctypes.c_bool),
    ]

_INCUMBENT_FN = ctypes.CFUNCTYPE(ctypes.c_int, _P(_Incumbent), _VP)

class _Node(ctypes.Structure):
    """jaos_node, field for field."""
    _fields_ = [
        ("node", _I64),
        ("depth", _I64),
        ("objective", _D),
        ("bound", _D),
        ("col_value", _P(_D)),
        ("num_col", _I64),
        ("integral", ctypes.c_bool),
        ("branch_col", _I64),
        ("internal", _VP),
    ]

_NODE_FN = ctypes.CFUNCTYPE(ctypes.c_int, _P(_Node), _VP)

class NodeEvent:
    """What the node callback sees: `node`, `depth`, `objective` (the
    node's relaxation), `bound` (the tree's), `values` (the point, a
    list by column), `integral` (the point is integer feasible and
    becomes an incumbent unless a row added here cuts it) and
    `branch_col`, the solver's choice, which the callback may set to
    any integer column fractional at the point. `add_row` adds a row
    that must hold for every solution of the model; it stays for the
    rest of the search. Live only while the callback runs."""

    __slots__ = ("_c", "node", "depth", "objective", "bound", "values",
                 "integral", "branch_col")

    def __init__(self, c):
        self._c = c
        self.node = c.node
        self.depth = c.depth
        self.objective = c.objective
        self.bound = c.bound
        self.values = [c.col_value[i] for i in range(c.num_col)]
        self.integral = bool(c.integral)
        self.branch_col = c.branch_col

    def add_row(self, index, value, lower=-float("inf"), upper=float("inf")):
        """Adds `lower <= sum(value[k] * x[index[k]]) <= upper`."""
        if self._c is None:
            raise JaosError(Status.ERR_INVALID_INPUT,
                            "this node event is over")
        n = len(index)
        if len(value) != n:
            raise ValueError("index and value differ in length")
        idx = (_I64 * max(n, 1))(*[int(i) for i in index])
        val = (_D * max(n, 1))(*[float(v) for v in value])
        rc = _lib.jaos_node_add_row(ctypes.byref(self._c), n, idx, val,
                                    float(lower), float(upper))
        if rc != Status.OK:
            raise JaosError(Status(rc), "the row is not one the node "
                            "callback may add: a column index out of "
                            "range, a non-finite coefficient, or bounds "
                            "that cross")

def _sig(name, restype, *argtypes):
    fn = getattr(_lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn

_sig("jaos_version", _CS)
_sig("jaos_status_str", _CS, ctypes.c_int)
_sig("jaos_solve_status_str", _CS, ctypes.c_int)
_sig("jaos_infinity", _D)
_sig("jaos_model_new", ctypes.c_int, _P(_VP))
_sig("jaos_model_free", None, _VP)
_sig("jaos_model_error", _CS, _VP)
_sig("jaos_load_lp", ctypes.c_int, _VP, _I64, _I64, ctypes.c_int, _D,
     _P(_D), _P(_D), _P(_D), _P(_D), _P(_D), _I64, _P(_I64), _P(_I64), _P(_D))
_sig("jaos_num_col", _I64, _VP)
_sig("jaos_num_row", _I64, _VP)
_sig("jaos_num_nz", _I64, _VP)
_sig("jaos_col_cost", ctypes.c_int, _VP, _I64, _P(_D))
_sig("jaos_col_bounds", ctypes.c_int, _VP, _I64, _P(_D), _P(_D))
_sig("jaos_row_bounds", ctypes.c_int, _VP, _I64, _P(_D), _P(_D))
_sig("jaos_objective_sense", ctypes.c_int, _VP, _P(ctypes.c_int))
_sig("jaos_objective_offset", ctypes.c_int, _VP, _P(_D))
_sig("jaos_set_objective_sense", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_objective_offset", ctypes.c_int, _VP, _D)
_sig("jaos_col_name", ctypes.c_int, _VP, _I64, ctypes.c_char_p, _I64)
_sig("jaos_row_name", ctypes.c_int, _VP, _I64, ctypes.c_char_p, _I64)
_sig("jaos_objective_name", ctypes.c_int, _VP, ctypes.c_char_p, _I64)
_sig("jaos_set_col_name", ctypes.c_int, _VP, _I64, _CS)
_sig("jaos_set_row_name", ctypes.c_int, _VP, _I64, _CS)
_sig("jaos_set_objective_name", ctypes.c_int, _VP, _CS)
_sig("jaos_col_index", ctypes.c_int, _VP, _CS, _P(_I64))
_sig("jaos_row_index", ctypes.c_int, _VP, _CS, _P(_I64))
_sig("jaos_set_col_integer", ctypes.c_int, _VP, _I64, ctypes.c_bool)
_sig("jaos_col_integer", ctypes.c_int, _VP, _I64, _P(ctypes.c_bool))
_sig("jaos_set_col_semicontinuous", ctypes.c_int, _VP, _I64, ctypes.c_bool)
_sig("jaos_col_semicontinuous", ctypes.c_int, _VP, _I64, _P(ctypes.c_bool))
_sig("jaos_set_col_quadratic", ctypes.c_int, _VP, _I64, _D)
_sig("jaos_col_quadratic", ctypes.c_int, _VP, _I64, _P(_D))
_sig("jaos_add_sos", ctypes.c_int, _VP, ctypes.c_int, _I64, _P(_I64), _P(_D))
_sig("jaos_num_sos", _I64, _VP)
_sig("jaos_set_row_indicator", ctypes.c_int, _VP, _I64, _I64, ctypes.c_int)
_sig("jaos_row_indicator", ctypes.c_int, _VP, _I64, _P(_I64), _P(ctypes.c_int))
_sig("jaos_sos", ctypes.c_int, _VP, _I64, _P(ctypes.c_int), _P(_I64),
     _P(_I64), _P(_D))
_sig("jaos_set_mip_gap", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_dive", ctypes.c_int, _VP, ctypes.c_bool)
_sig("jaos_set_mip_cut_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_cut_depth", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_cover_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_clique_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_zero_half_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_flow_cover_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_node_cut_cap", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_cut_stall", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_node_cut_stall", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_root_cut_drop", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_cover_lift", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_mir_rounds", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_dive_backtrack", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_dive_gap", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_node_mir", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_mir_aggregate", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_dive_heuristic", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_dive_heuristic_depth", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_rins", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_feaspump", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_pump_general", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_pump_obj", ctypes.c_int, _VP, ctypes.c_double)
_sig("jaos_set_mip_pump_always", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_rcfix", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_tighten", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_probing", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_probing_cap", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_clique_fix", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_conflicts", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_symmetry", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_orbital", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_propagate", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_propagate_depth", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_dive_degrade", ctypes.c_int, _VP, ctypes.c_double)
_sig("jaos_set_mip_heuristics", ctypes.c_int, _VP, ctypes.c_bool)
_sig("jaos_mip_result", ctypes.c_int, _VP, _P(_MipReport))
_sig("jaos_model_statistics", ctypes.c_int, _VP, _P(_ModelStats))
_sig("jaos_presolve_result", ctypes.c_int, _VP, _P(_PresolveReport))
_sig("jaos_set_mip_start", ctypes.c_int, _VP, _P(_D))
_sig("jaos_set_mip_cutoff", ctypes.c_int, _VP, ctypes.c_double)
_sig("jaos_write_proof", ctypes.c_int, _VP, ctypes.c_char_p)
_sig("jaos_check_proof", ctypes.c_int, _VP, ctypes.c_char_p,
     _P(_ProofReport))
_sig("jaos_mip_incumbent", ctypes.c_int, _VP, _P(_D), _P(_D))
_sig("jaos_model_name", ctypes.c_int, _VP, ctypes.c_char_p, _I64)
_sig("jaos_set_model_name", ctypes.c_int, _VP, _CS)
_sig("jaos_model_copy", ctypes.c_int, _VP, _P(_VP))
_sig("jaos_exact_col_value", ctypes.c_int, _VP, _I64, _P(_CS))
_sig("jaos_exact_row_dual", ctypes.c_int, _VP, _I64, _P(_CS))
_sig("jaos_exact_objective", ctypes.c_int, _VP, _P(_CS))
_sig("jaos_col_entries", ctypes.c_int, _VP, _I64, _P(_I64), _P(_I64), _P(_D))
_sig("jaos_row_entries", ctypes.c_int, _VP, _I64, _P(_I64), _P(_I64), _P(_D))
_sig("jaos_coefficient", ctypes.c_int, _VP, _I64, _I64, _P(_D))
_sig("jaos_set_col_cost", ctypes.c_int, _VP, _I64, _D)
_sig("jaos_set_col_bounds", ctypes.c_int, _VP, _I64, _D, _D)
_sig("jaos_set_row_bounds", ctypes.c_int, _VP, _I64, _D, _D)
_sig("jaos_set_coefficient", ctypes.c_int, _VP, _I64, _I64, _D)
_sig("jaos_add_cols", ctypes.c_int, _VP, _I64, _P(_D), _P(_D), _P(_D),
     _I64, _P(_I64), _P(_I64), _P(_D))
_sig("jaos_add_rows", ctypes.c_int, _VP, _I64, _P(_D), _P(_D),
     _I64, _P(_I64), _P(_I64), _P(_D))
_sig("jaos_delete_cols", ctypes.c_int, _VP, _I64, _P(_I64))
_sig("jaos_delete_rows", ctypes.c_int, _VP, _I64, _P(_I64))
_sig("jaos_read_mps", ctypes.c_int, _VP, _CS)
_sig("jaos_read_lp", ctypes.c_int, _VP, _CS)
_sig("jaos_read_nl", ctypes.c_int, _VP, _CS)
_sig("jaos_write_mps", ctypes.c_int, _VP, _CS)
_sig("jaos_write_lp", ctypes.c_int, _VP, _CS)
_sig("jaos_write_nl", ctypes.c_int, _VP, _CS)
_sig("jaos_read_qplib", ctypes.c_int, _VP, _CS)
_sig("jaos_write_qplib", ctypes.c_int, _VP, _CS)
_sig("jaos_read_osil", ctypes.c_int, _VP, _CS)
_sig("jaos_write_osil", ctypes.c_int, _VP, _CS)
_sig("jaos_write_solution", ctypes.c_int, _VP, _CS)
_sig("jaos_read_solution", ctypes.c_int, _VP, _CS, _P(_D),
     _P(_D), _P(_D), _P(ctypes.c_int),
     _P(_D), _P(_D), _P(ctypes.c_int))
_sig("jaos_read_certificate", ctypes.c_int, _VP, _CS, _P(ctypes.c_int),
     _P(_D), _P(_D))
_sig("jaos_solution_file_status", ctypes.c_int, _VP, _CS, _P(ctypes.c_int))
_sig("jaos_read_basis", ctypes.c_int, _VP, _CS, _P(ctypes.c_int),
     _P(ctypes.c_int))
_sig("jaos_write_mps_basis", ctypes.c_int, _VP, _CS)
_sig("jaos_read_mps_basis", ctypes.c_int, _VP, _CS, _P(ctypes.c_int),
     _P(ctypes.c_int))
_sig("jaos_write_point", ctypes.c_int, _VP, _CS)
_sig("jaos_write_point_values", ctypes.c_int, _VP, _CS, _P(_D))
_sig("jaos_write_duals", ctypes.c_int, _VP, _CS)
_sig("jaos_write_dual_values", ctypes.c_int, _VP, _CS, _P(_D))
_sig("jaos_read_point", ctypes.c_int, _VP, _CS, _P(_D))
_sig("jaos_read_duals", ctypes.c_int, _VP, _CS, _P(_D))
_sig("jaos_set_work_limit", ctypes.c_int, _VP, _I64)
_sig("jaos_set_threads", ctypes.c_int, _VP, _I64)
_sig("jaos_threads_of", _I64, _VP)
_sig("jaos_set_time_limit", ctypes.c_int, _VP, _D)
_sig("jaos_set_primal_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_dual_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_log_callback", ctypes.c_int, _VP, _LOG_FN, _VP)
_sig("jaos_set_log_level", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_progress_callback", ctypes.c_int, _VP, _PROGRESS_FN, _VP)
_sig("jaos_set_mip_node_limit", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_branching", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_algorithm", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_option", ctypes.c_int, _VP, ctypes.c_char_p, ctypes.c_char_p)
_sig("jaos_get_option", ctypes.c_int, _VP, ctypes.c_char_p, ctypes.c_char_p,
     _I64)
_sig("jaos_read_options", ctypes.c_int, _VP, ctypes.c_char_p)
_sig("jaos_num_options", _I64)
_sig("jaos_option_name", ctypes.c_char_p, _I64)
_sig("jaos_algorithm_of", ctypes.c_int, _VP)
_sig("jaos_set_mip_reliability", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_probe_cap", ctypes.c_int, _VP, _D)
_sig("jaos_set_mip_dive_child", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_mip_cut_drop", ctypes.c_int, _VP, ctypes.c_bool)
_sig("jaos_set_mip_probe_depth", ctypes.c_int, _VP, _I64)
_sig("jaos_set_mip_pool_size", ctypes.c_int, _VP, _I64)
_sig("jaos_mip_pool_count", ctypes.c_int, _VP, _P(_I64))
_sig("jaos_mip_pool_solution", ctypes.c_int, _VP, _I64, _P(_D), _P(_D))
_sig("jaos_set_incumbent_callback", ctypes.c_int, _VP, _INCUMBENT_FN, _VP)
_sig("jaos_set_node_callback", ctypes.c_int, _VP, _NODE_FN, _VP)
_sig("jaos_node_add_row", ctypes.c_int, _P(_Node), _I64, _P(_I64), _P(_D), _D, _D)
_sig("jaos_solve", ctypes.c_int, _VP)
_sig("jaos_status_of", ctypes.c_int, _VP)
_sig("jaos_objective", ctypes.c_int, _VP, _P(_D))
_sig("jaos_solution", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_basis", ctypes.c_int, _VP, _P(ctypes.c_int), _P(ctypes.c_int))
_sig("jaos_set_basis", ctypes.c_int, _VP, _P(ctypes.c_int),
     _P(ctypes.c_int))
_sig("jaos_clear_basis", None, _VP)
_sig("jaos_check_solution", ctypes.c_int, _VP, _P(_D), _P(_D), _D,
     _P(_CheckReport))
_sig("jaos_certificate", ctypes.c_int, _VP, _P(_D))
_sig("jaos_check_certificate", ctypes.c_int, _VP, _P(_D), _D,
     _P(_CertificateReport))
_sig("jaos_unbounded_ray", ctypes.c_int, _VP, _P(_D))
_sig("jaos_check_ray", ctypes.c_int, _VP, _P(_D), _D, _P(_RayReport))
_sig("jaos_iis_model", ctypes.c_int, _VP, _P(ctypes.c_int),
     _P(ctypes.c_int), _P(_VP))
_sig("jaos_iis", ctypes.c_int, _VP, _P(ctypes.c_int), _P(ctypes.c_int),
     _P(_IISReport))
_sig("jaos_feasrelax", ctypes.c_int, _VP, ctypes.c_int, _P(_D), _P(_D),
     _P(_RelaxReport))
_sig("jaos_cost_ranging", ctypes.c_int, _VP, _P(_D), _P(_D))
_sig("jaos_rhs_ranging", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_bound_ranging", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_verify", ctypes.c_int, _VP, _P(_VerifyReport))
_sig("jaos_verify_basis", ctypes.c_int, _VP, _P(ctypes.c_int),
     _P(ctypes.c_int), _P(_VerifyReport))
_sig("jaos_exact_certificate", ctypes.c_int, _VP, _P(_ExactRayReport))
_sig("jaos_exact_row_multiplier", ctypes.c_int, _VP, ctypes.c_int64,
     _P(_CS))
_sig("jaos_exact_unbounded_ray", ctypes.c_int, _VP, _P(_ExactRayReport))
_sig("jaos_exact_col_direction", ctypes.c_int, _VP, ctypes.c_int64,
     _P(_CS))
_sig("jaos_work_units", _I64, _VP)
_sig("jaos_iterations", _I64, _VP)
_sig("jaos_solve_time", _D, _VP)

INFINITY = _lib.jaos_infinity()

NAME_MAX = 255

def version():
    """The library's version string, from the library rather than from here.

    Two version numbers in one project drift, so this module keeps none of
    its own.
    """
    return _lib.jaos_version().decode("utf-8")

def _doubles(seq, name, want=None):
    if seq is None:
        return None, 0
    buf = (_D * len(seq))(*(float(v) for v in seq))
    if want is not None and len(seq) != want:
        raise ValueError(f"{name} has {len(seq)} entries, expected {want}")
    return buf, len(seq)

def _int64s(seq, name, want=None):
    if seq is None:
        return None, 0
    buf = (_I64 * len(seq))(*(int(v) for v in seq))
    if want is not None and len(seq) != want:
        raise ValueError(f"{name} has {len(seq)} entries, expected {want}")
    return buf, len(seq)

class Model:
    """One problem, and the answer to it.

    Not thread-safe, which is the C library's rule and not this module's:
    one model is used by one thread at a time, and distinct models are
    independent.
    """

    def __init__(self, _handle=None):

        if _handle is not None:
            handle = _handle
        else:
            handle = _VP()
            rc = _lib.jaos_model_new(ctypes.byref(handle))
            if rc != Status.OK:
                raise JaosError(rc, "could not allocate a model")
        self._m = handle

        self._log_cb = None
        self._progress_cb = None
        self._incumbent_cb = None

    def close(self):
        """Frees the model. Safe to call twice; the object is unusable
        afterwards."""
        if getattr(self, "_m", None) is not None:
            _lib.jaos_model_free(self._m)
            self._m = None
            self._log_cb = None
            self._progress_cb = None
            self._incumbent_cb = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def _handle(self):
        if getattr(self, "_m", None) is None:
            raise ValueError("this model has been closed")
        return self._m

    def _check(self, rc):
        if rc != Status.OK:
            raise JaosError(rc, self.error)
        return None

    @property
    def error(self):
        """The model's own message about the last failure, or ''."""
        if getattr(self, "_m", None) is None:
            return ""
        return _lib.jaos_model_error(self._m).decode("utf-8", "replace")

    def load(self, num_col, num_row, col_cost, col_lower, col_upper,
             row_lower, row_upper, a_start=None, a_index=None, a_value=None,
             sense=ObjSense.MINIMIZE, obj_offset=0.0):
        """Loads a complete problem, replacing whatever the model held.

        The matrix is compressed sparse column: `a_start` has num_col + 1
        entries, `a_index` the row indices and `a_value` the coefficients.
        Pass all three as None for an all-zero matrix. Every array is
        copied; nothing here is retained.
        """
        cost, _ = _doubles(col_cost, "col_cost", num_col)
        cl, _ = _doubles(col_lower, "col_lower", num_col)
        cu, _ = _doubles(col_upper, "col_upper", num_col)
        rl, _ = _doubles(row_lower, "row_lower", num_row)
        ru, _ = _doubles(row_upper, "row_upper", num_row)

        if a_value is None:
            num_nz = 0
            starts = idx = vals = None
        else:
            num_nz = len(a_value)
            if a_index is None or len(a_index) != num_nz:
                raise ValueError("a_index must have one entry per value")
            if a_start is None or len(a_start) != num_col + 1:
                raise ValueError("a_start must have num_col + 1 entries")
            starts, _ = _int64s(a_start, "a_start")
            idx, _ = _int64s(a_index, "a_index")
            vals, _ = _doubles(a_value, "a_value")

        self._check(_lib.jaos_load_lp(
            self._handle(), num_col, num_row, int(sense), float(obj_offset),
            cost, cl, cu, rl, ru, num_nz, starts, idx, vals))
        return self

    def read_mps(self, path):
        """Reads an MPS file, fixed or free layout. A gzip-compressed file
        is accepted wherever a plain one is."""
        self._check(_lib.jaos_read_mps(self._handle(), _path(path)))
        return self

    def read_lp(self, path):
        """Reads a CPLEX-style LP file. gzip is accepted here too."""
        self._check(_lib.jaos_read_lp(self._handle(), _path(path)))
        return self

    def read_nl(self, path):
        """Reads an AMPL .nl file in its text form: a linear model with
        its bounds and integer columns, and the names from the .col and
        .row files beside it when both are there. A nonlinear expression,
        a defined variable or a complementarity condition is refused by
        line. gzip is accepted here too."""
        self._check(_lib.jaos_read_nl(self._handle(), _path(path)))
        return self

    def write_mps(self, path):
        self._check(_lib.jaos_write_mps(self._handle(), _path(path)))
        return self

    def write_lp(self, path):
        self._check(_lib.jaos_write_lp(self._handle(), _path(path)))
        return self

    def write_nl(self, path):
        """Writes an AMPL .nl file in its text form, with the names in
        .col and .row files beside it. The format lists the integer
        columns last, so a model with an integer column before a
        continuous one reads back with its columns in that order, names
        carried. SOS sets, semi-continuous columns and indicator rows
        are refused; write MPS for those."""
        self._check(_lib.jaos_write_nl(self._handle(), _path(path)))
        return self

    def read_qplib(self, path):
        """Reads a QPLIB file: a linear or separable quadratic objective,
        linear rows, bounds, integer columns and the names. An
        off-diagonal Q entry or a quadratic constraint is refused by
        line. gzip is accepted here too."""
        self._check(_lib.jaos_read_qplib(self._handle(), _path(path)))
        return self

    def write_qplib(self, path):
        """Writes the model in the QPLIB format; SOS sets, semi-continuous
        columns and indicator rows are refused, write MPS for those."""
        self._check(_lib.jaos_write_qplib(self._handle(), _path(path)))
        return self

    def read_osil(self, path):
        """Reads an OSiL XML file: variables with bounds and types, one
        objective with its constant and its diagonal quadratic terms,
        constraints and the matrix in either the column-wise or the
        row-wise layout. A nonlinear or quadratic-constraint block, an
        off-diagonal quadratic term and a second objective are refused
        by line. gzip is accepted here too."""
        self._check(_lib.jaos_read_osil(self._handle(), _path(path)))
        return self

    def write_osil(self, path):
        """Writes the model as OSiL XML: variables with bounds and types,
        one objective with its quadratic terms, constraints and the
        column-wise matrix."""
        self._check(_lib.jaos_write_osil(self._handle(), _path(path)))
        return self

    def write_solution(self, path):
        self._check(_lib.jaos_write_solution(self._handle(), _path(path)))

    def read_solution(self, path):
        """Reads back a file write_solution wrote, as a
        (objective, Solution, Basis) triple.

        The model decides the shape: a file whose counts or names do not
        match this model is refused rather than read. Nothing is installed
        -- pass the basis to set_basis() to warm-start from it, which keeps
        reading a file and changing a model two separate decisions."""
        nc, nr = self.num_col, self.num_row
        obj = _D()
        cv = (_D * max(nc, 1))()
        cd = (_D * max(nc, 1))()
        cs = (ctypes.c_int * max(nc, 1))()
        ra = (_D * max(nr, 1))()
        rd = (_D * max(nr, 1))()
        rs = (ctypes.c_int * max(nr, 1))()
        self._check(_lib.jaos_read_solution(
            self._handle(), _path(path), ctypes.byref(obj),
            cv, cd, cs, ra, rd, rs))
        return (obj.value,
                Solution(list(cv[:nc]), list(ra[:nr]),
                         list(rd[:nr]), list(cd[:nc])),
                Basis([BasisStatus(v) for v in cs[:nc]],
                      [BasisStatus(v) for v in rs[:nr]]))

    def read_basis(self, path):
        """The `Basis` out of a solution file of either kind: an optimum's,
        which has carried one all along, or a certificate's, which carries
        one since D332. Nothing is installed -- pass it to set_basis() to
        warm-start from it. Raises when the file carries no basis."""
        nc, nr = self.num_col, self.num_row
        cs = (ctypes.c_int * max(nc, 1))()
        rs = (ctypes.c_int * max(nr, 1))()
        self._check(_lib.jaos_read_basis(self._handle(), _path(path), cs, rs))
        return Basis([BasisStatus(v) for v in cs[:nc]],
                     [BasisStatus(v) for v in rs[:nr]])

    def write_mps_basis(self, path):
        """Writes the basis the last solve left in the MPS basis file
        format, which every solver in the field reads (D338). The rule is
        basis()'s: an optimum, a refusal, an unboundedness and a budget
        stop all have one, and a solve that never ran does not."""
        self._check(_lib.jaos_write_mps_basis(self._handle(), _path(path)))

    def read_mps_basis(self, path):
        """The `Basis` in an MPS basis file, written here or by another
        solver. Nothing is installed -- pass it to set_basis() to
        warm-start from it."""
        nc, nr = self.num_col, self.num_row
        cs = (ctypes.c_int * max(nc, 1))()
        rs = (ctypes.c_int * max(nr, 1))()
        self._check(_lib.jaos_read_mps_basis(
            self._handle(), _path(path), cs, rs))
        return Basis([BasisStatus(v) for v in cs[:nc]],
                     [BasisStatus(v) for v in rs[:nr]])

    def write_point(self, path):
        """Writes the last optimum's point as a point file: one
        `NAME VALUE` line per column and nothing else (D342). The
        availability rule is solution()'s."""
        self._check(_lib.jaos_write_point(self._handle(), _path(path)))

    def write_point_values(self, path, col_value):
        """The same file from values the caller has (D344): one of
        mip_pool_solution()'s, an incumbent a budget stop left, or a point
        from somewhere else. No solve is needed, because nothing here
        reads one."""
        nc = self.num_col
        if len(col_value) != nc:
            raise ValueError("a point needs %d values, and got %d"
                             % (nc, len(col_value)))
        x = (_D * max(nc, 1))(*[float(v) for v in col_value])
        self._check(_lib.jaos_write_point_values(
            self._handle(), _path(path), x))

    def write_duals(self, path):
        """The last optimum's row multipliers, in the point file's shape
        (D348), so read_duals() reads back what this wrote."""
        self._check(_lib.jaos_write_duals(self._handle(), _path(path)))

    def write_dual_values(self, path, row_dual):
        """The same file from multipliers the caller has; no solve is
        needed."""
        nr = self.num_row
        if len(row_dual) != nr:
            raise ValueError("a duals file needs %d values, and got %d"
                             % (nr, len(row_dual)))
        y = (_D * max(nr, 1))(*[float(v) for v in row_dual])
        self._check(_lib.jaos_write_dual_values(
            self._handle(), _path(path), y))

    def read_point(self, path):
        """The list of column values in a point file, in index order.

        The file is what another solver's answer arrives in: one
        `NAME VALUE` line per column, in any order, `#` for a comment.
        Every column must appear exactly once -- a missing one is an
        error with its name, because defaulting it to zero is how a wrong
        answer gets judged feasible. Pass the result to check_solution()."""
        nc = self.num_col
        x = (_D * max(nc, 1))()
        self._check(_lib.jaos_read_point(self._handle(), _path(path), x))
        return list(x[:nc])

    def read_duals(self, path):
        """The row multipliers in a file of the same shape, in index
        order: the other half of read_point(), for the dual half of the
        checker's report."""
        nr = self.num_row
        y = (_D * max(nr, 1))()
        self._check(_lib.jaos_read_duals(self._handle(), _path(path), y))
        return list(y[:nr])

    def solution_file_status(self, path):
        """Which of the three a solution file holds: OPTIMAL, INFEASIBLE or
        UNBOUNDED. The whole file is read, so one the readers would refuse
        is refused here too (D285)."""
        out = ctypes.c_int()
        self._check(_lib.jaos_solution_file_status(
            self._handle(), _path(path), ctypes.byref(out)))
        return SolveStatus(out.value)

    def read_certificate(self, path):
        """Reads back the certificate write_solution wrote for an
        INFEASIBLE or UNBOUNDED answer, as a (status, ray) pair: one
        multiplier per row when infeasible, one direction per column when
        unbounded. check_certificate() or check_ray() judges it from the
        model alone. A file holding an optimum is refused; read_solution
        reads those."""
        nc, nr = self.num_col, self.num_row
        st = ctypes.c_int()
        rr = (_D * max(nr, 1))()
        cr = (_D * max(nc, 1))()
        self._check(_lib.jaos_read_certificate(
            self._handle(), _path(path), ctypes.byref(st), rr, cr))
        status = SolveStatus(st.value)
        ray = list(rr[:nr]) if status is SolveStatus.INFEASIBLE \
            else list(cr[:nc])
        return status, ray

    @property
    def num_col(self):
        return _lib.jaos_num_col(self._handle())

    @property
    def num_row(self):
        return _lib.jaos_num_row(self._handle())

    @property
    def num_nz(self):
        return _lib.jaos_num_nz(self._handle())

    def col_cost(self, col):
        out = _D()
        self._check(_lib.jaos_col_cost(self._handle(), col, ctypes.byref(out)))
        return out.value

    def col_bounds(self, col):
        lo, hi = _D(), _D()
        self._check(_lib.jaos_col_bounds(self._handle(), col,
                                         ctypes.byref(lo), ctypes.byref(hi)))
        return lo.value, hi.value

    def row_bounds(self, row):
        lo, hi = _D(), _D()
        self._check(_lib.jaos_row_bounds(self._handle(), row,
                                         ctypes.byref(lo), ctypes.byref(hi)))
        return lo.value, hi.value

    @property
    def sense(self):
        out = ctypes.c_int()
        self._check(_lib.jaos_objective_sense(self._handle(),
                                              ctypes.byref(out)))
        return ObjSense(out.value)

    @property
    def obj_offset(self):
        out = _D()
        self._check(_lib.jaos_objective_offset(self._handle(),
                                               ctypes.byref(out)))
        return out.value

    def _name(self, fn, *args):
        buf = ctypes.create_string_buffer(NAME_MAX + 1)
        self._check(fn(self._handle(), *args, buf, NAME_MAX + 1))
        return buf.value.decode("utf-8", "replace")

    def col_name(self, col):
        return self._name(_lib.jaos_col_name, int(col))

    def row_name(self, row):
        return self._name(_lib.jaos_row_name, int(row))

    @property
    def objective_name(self):
        return self._name(_lib.jaos_objective_name)

    @staticmethod
    def _name_arg(name):
        return None if name is None else str(name).encode("utf-8")

    def set_col_name(self, col, name):
        self._check(_lib.jaos_set_col_name(self._handle(), int(col),
                                           self._name_arg(name)))

    def set_row_name(self, row, name):
        self._check(_lib.jaos_set_row_name(self._handle(), int(row),
                                           self._name_arg(name)))

    def set_objective_name(self, name):
        self._check(_lib.jaos_set_objective_name(self._handle(),
                                                 self._name_arg(name)))

    def set_col_integer(self, col, is_integer=True):
        """Marks a column integer; a model with one solves by branch and
        bound. Discards the answer like every modification."""
        self._check(_lib.jaos_set_col_integer(self._handle(), int(col),
                                              bool(is_integer)))

    def col_integer(self, col):
        out = ctypes.c_bool()
        self._check(_lib.jaos_col_integer(self._handle(), int(col),
                                          ctypes.byref(out)))
        return out.value

    def set_col_semicontinuous(self, col, is_semi=True):
        """Marks a column semi-continuous: it rests at zero or inside its
        own bounds, and the model solves by branch and bound."""
        self._check(_lib.jaos_set_col_semicontinuous(self._handle(),
                                                     int(col), bool(is_semi)))

    def col_semicontinuous(self, col):
        out = ctypes.c_bool()
        self._check(_lib.jaos_col_semicontinuous(self._handle(), int(col),
                                                 ctypes.byref(out)))
        return out.value

    def set_col_quadratic(self, col, q):
        """Gives a column a quadratic objective term q/2 * x**2. A model
        with one is a convex QP (q >= 0 when minimising, <= 0 when
        maximising) and solves by the barrier; the two simplexes, PDLP
        and branch and bound refuse it."""
        self._check(_lib.jaos_set_col_quadratic(self._handle(), int(col),
                                                float(q)))

    def col_quadratic(self, col):
        out = ctypes.c_double()
        self._check(_lib.jaos_col_quadratic(self._handle(), int(col),
                                            ctypes.byref(out)))
        return out.value

    def add_sos(self, sos_type, cols, weights):
        """A special ordered set of type 1 (at most one member nonzero) or
        2 (at most two, adjacent in weight order) over `cols`."""
        cols = [int(c) for c in cols]
        weights = [float(w) for w in weights]
        if len(cols) != len(weights):
            raise ValueError("one weight per member")
        n = len(cols)
        ca = (_I64 * max(n, 1))(*cols)
        wa = (_D * max(n, 1))(*weights)
        self._check(_lib.jaos_add_sos(self._handle(), int(sos_type), n,
                                      ca, wa))

    def num_sos(self):
        return int(_lib.jaos_num_sos(self._handle()))

    def set_row_indicator(self, row, col, value=1):
        """Row `row` holds only while integer column `col` equals `value`
        (0 or 1); `col=None` makes it an ordinary row again."""
        self._check(_lib.jaos_set_row_indicator(
            self._handle(), int(row), -1 if col is None else int(col),
            int(value)))

    def row_indicator(self, row):
        """(column, value) of the row's indicator, or (None, 0)."""
        c = _I64()
        v = ctypes.c_int()
        self._check(_lib.jaos_row_indicator(self._handle(), int(row),
                                            ctypes.byref(c), ctypes.byref(v)))
        return (None if c.value < 0 else int(c.value)), int(v.value)

    def sos(self, k):
        """(type, columns, weights) of set `k`, members in weight order."""
        t = ctypes.c_int()
        n = _I64()
        self._check(_lib.jaos_sos(self._handle(), int(k), ctypes.byref(t),
                                  ctypes.byref(n), None, None))
        ca = (_I64 * max(n.value, 1))()
        wa = (_D * max(n.value, 1))()
        self._check(_lib.jaos_sos(self._handle(), int(k), None, None, ca, wa))
        return int(t.value), list(ca)[:n.value], list(wa)[:n.value]

    def set_mip_gap(self, gap):
        """The relative gap that closes a branch and bound; 0 restores the
        default of 1e-6."""
        self._check(_lib.jaos_set_mip_gap(self._handle(), float(gap)))

    def set_mip_dive(self, on=True):
        """Whether a selected node is dived from. Off by default: the dive
        measured 1.125x the work of the plain best-bound order (D289)."""
        self._check(_lib.jaos_set_mip_dive(self._handle(), bool(on)))

    def set_mip_cut_rounds(self, rounds):
        """Rounds of Gomory cuts at the root (D289): 0 for none, a
        negative value for the default of 1."""
        self._check(_lib.jaos_set_mip_cut_rounds(self._handle(), int(rounds)))

    def set_mip_cut_depth(self, depth):
        """One round of Gomory cuts at every node down to `depth` (D296),
        each valid in its node's subtree; 0 is the root only, 3 the
        default (D301), and a negative value restores it."""
        self._check(_lib.jaos_set_mip_cut_depth(self._handle(), int(depth)))

    def set_mip_cover_rounds(self, rounds):
        """Rounds of knapsack cover cuts at the root, beside the Gomory
        rounds (D300): 0 for none, a negative value for the default."""
        self._check(_lib.jaos_set_mip_cover_rounds(self._handle(), int(rounds)))

    def set_mip_clique_rounds(self, rounds):
        """Rounds of clique cuts at the root, from the conflicts the rows
        put between binary columns: 0 for none, negative for the default."""
        self._check(_lib.jaos_set_mip_clique_rounds(self._handle(), int(rounds)))

    def set_mip_zero_half_rounds(self, rounds):
        """Rounds of zero-half cuts at the root: one, two or three rows
        with integer data on integer columns, halved with the bound rows
        that make every coefficient even, and rounded down where the
        right-hand side is odd. 0 for none, negative for the default."""
        self._check(_lib.jaos_set_mip_zero_half_rounds(self._handle(), int(rounds)))

    def set_mip_flow_cover_rounds(self, rounds):
        """Rounds of flow cover cuts at the root: a row read as a
        single-node flow set, its columns' variable upper bounds found in
        the two-entry rows, a cover picked greedily, and the Padberg, Van
        Roy and Wolsey inequality. 0 for none, negative for the default."""
        self._check(_lib.jaos_set_mip_flow_cover_rounds(self._handle(), int(rounds)))

    def set_mip_node_cut_cap(self, cap):
        """At most `cap` cuts per node below the root, the most efficacious
        kept (D301); 0 for no cap, a negative value for the default."""
        self._check(_lib.jaos_set_mip_node_cut_cap(self._handle(), int(cap)))

    def set_mip_cut_stall(self, fraction):
        """The root's cut rounds end once one moves the bound by less than
        `fraction` of (1 + |bound|) (D304); 0 ends them only when a round
        adds nothing, a negative value restores the default, NaN and
        infinity are refused."""
        self._check(_lib.jaos_set_mip_cut_stall(self._handle(), float(fraction)))

    def set_mip_node_cut_stall(self, fraction):
        """No cut round under a node whose own round moved its bound by
        less than `fraction` of (1 + |bound|), the root's whole cut phase
        judged the same way (D305); 0 never switches a subtree off, a
        negative value restores the default, NaN and infinity are
        refused."""
        self._check(_lib.jaos_set_mip_node_cut_stall(self._handle(),
                                                     float(fraction)))

    def set_mip_root_cut_drop(self, on=True):
        """Whether the root's cuts leave the relaxation below a node where
        their slack is basic, like a node's own (D306); on by default, and
        None restores that."""
        v = -1 if on is None else int(bool(on))
        self._check(_lib.jaos_set_mip_root_cut_drop(self._handle(), v))

    def set_mip_cover_lift(self, on=True):
        """Whether a cover cut carries Balas's lifting coefficients (D307)
        instead of the extended cover's; None restores the default."""
        v = -1 if on is None else int(bool(on))
        self._check(_lib.jaos_set_mip_cover_lift(self._handle(), v))

    def set_mip_mir_rounds(self, rounds):
        """Rounds of mixed-integer rounding cuts on the model's rows at the
        root (D309): 0 for none, a negative value for the default of 6."""
        self._check(_lib.jaos_set_mip_mir_rounds(self._handle(), int(rounds)))

    def set_mip_dive_backtrack(self, times):
        """How many times a dive may resume from the deepest sibling it
        left on its stack (D308); 0 is D289's dive, a negative value the
        default. Only matters with the dive on."""
        self._check(_lib.jaos_set_mip_dive_backtrack(self._handle(), int(times)))

    def set_mip_dive_gap(self, fraction):
        """The dive resumes from a waiting sibling only while its bound is
        within `fraction` of (1 + |best open bound|) (D311); 0 puts no
        bound on it, a negative value restores the default, NaN and
        infinity are refused. Only matters with the dive on."""
        self._check(_lib.jaos_set_mip_dive_gap(self._handle(), float(fraction)))

    def set_mip_node_mir(self, on=True):
        """Whether a node inside the cut depth gets MIR cuts over its own
        bounds beside its Gomory round (D310); None restores the default."""
        v = -1 if on is None else int(bool(on))
        self._check(_lib.jaos_set_mip_node_mir(self._handle(), v))

    def set_mip_mir_aggregate(self, rows):
        """How many other rows a MIR cut's aggregate may absorb before it
        is rounded (D312); 0 is the single-row form, a negative value
        restores the default."""
        self._check(_lib.jaos_set_mip_mir_aggregate(self._handle(), int(rows)))

    def set_mip_dive_heuristic(self, solves):
        """Relaxations a dive for a first incumbent may solve at the root
        (D313); 0 is off, 50 is the default and a negative value restores
        it."""
        self._check(_lib.jaos_set_mip_dive_heuristic(self._handle(),
                                                     int(solves)))

    def set_mip_dive_heuristic_depth(self, depth):
        """Deepest node the dive heuristic runs at, the root being 0 (D314).

        0 is the root alone and the default; a negative value restores it.
        """
        self._check(_lib.jaos_set_mip_dive_heuristic_depth(self._handle(),
                                                           int(depth)))

    def set_mip_rins(self, solves):
        """Relaxations a RINS dive may solve at a node (D315).

        The columns the incumbent and the node's relaxation agree on are
        fixed there first. 0, the default, is off; a negative value
        restores it.
        """
        self._check(_lib.jaos_set_mip_rins(self._handle(), int(solves)))

    def set_mip_feaspump(self, rounds):
        """Rounds the feasibility pump may run at the root (D318).

        Each round rounds the point it holds and re-solves for the nearest
        point of the relaxation in L1. 0, the default, is off; a negative
        value restores it.
        """
        self._check(_lib.jaos_set_mip_feaspump(self._handle(), int(rounds)))

    def set_mip_pump_general(self, on):
        """Whether the pump carries an auxiliary distance column per
        general integer column, so such a column's distance to its
        rounding counts wherever the rounding sits. Off by default; a
        negative value restores the default.
        """
        self._check(_lib.jaos_set_mip_pump_general(self._handle(), int(on)))

    def set_mip_pump_obj(self, decay):
        """The objective pump's decay.

        Each round blends the model's own objective into the pump's
        distance at a weight that multiplies by ``decay`` per round from
        1. 0 is the plain pump and 0.5 the default; a negative value
        restores it; 1 or more is refused.
        """
        self._check(_lib.jaos_set_mip_pump_obj(self._handle(),
                                               float(decay)))

    def set_mip_pump_always(self, on):
        """Whether the pump runs at the root where an incumbent exists.

        Off by default: D318's pump looks for any feasible point, so it
        can only cost where the root already has one. A negative value
        restores the default.
        """
        self._check(_lib.jaos_set_mip_pump_always(self._handle(), int(on)))

    def set_mip_rcfix(self, on):
        """Reduced-cost fixing at the root.

        Once the root holds an incumbent, an integer column resting at a
        bound has its other bound pulled in to the furthest integer its
        reduced cost still allows. On by default; a negative value
        restores it.
        """
        self._check(_lib.jaos_set_mip_rcfix(self._handle(), int(on)))

    def set_mip_tighten(self, on):
        """Coefficient tightening at the root.

        A binary column whose coefficient in a one-sided row can never
        make the row tight has that coefficient shrunk, and the bound with
        it, so the relaxation loses nothing integral and gains a tighter
        face. On by default; a negative value restores it.
        """
        self._check(_lib.jaos_set_mip_tighten(self._handle(), int(on)))

    def set_mip_probing(self, on):
        """Probing after the root solve.

        Each binary column fractional at the root is tried at 0 and at 1
        with the rows propagated over the bounds, most fractional first,
        under the cap of `set_mip_probing_cap`. A setting that makes some
        row impossible fixes the column the other way, the bounds both
        settings imply are kept, and a column that fits neither way makes
        the model infeasible. Off by default; a negative value restores
        it.
        """
        self._check(_lib.jaos_set_mip_probing(self._handle(), int(on)))

    def set_mip_probing_cap(self, multiple):
        """A work cap on the root's probing, as a multiple of the work
        the root solve itself took. 0 removes the cap; a negative value
        restores the default; NaN and infinity are refused."""
        self._check(_lib.jaos_set_mip_probing_cap(self._handle(), float(multiple)))

    def set_mip_clique_fix(self, on):
        """Fixing by the clique table at each node.

        The root builds a table of the conflicts its all-binary rows put
        between literals, plus the ones probing found. At each node a
        binary fixed to one setting fixes every literal in conflict with
        it, and a node holding both sides of a conflict is cut with no
        solve. Off by default (1.026x over the MIP set, enigma 1.853x);
        a negative value restores it.
        """
        self._check(_lib.jaos_set_mip_clique_fix(self._handle(), int(on)))

    def set_mip_conflicts(self, on):
        """Conflict analysis at an infeasible node.

        The Farkas proof of the node's relaxation is read over the
        branching fixings on the path; the fixings the proof can do
        without are dropped, and when the rest are binaries fixed to a
        value, a row forbidding that combination is kept for the rest of
        the search. On by default; a negative value restores it.
        """
        self._check(_lib.jaos_set_mip_conflicts(self._handle(), int(on)))

    def set_mip_symmetry(self, on):
        """Symmetry detection at the root: the model as a coloured graph,
        colour refinement, and a partition search under a work cap for
        the column permutations that map the model to itself. The
        report says how many generators and orbits were found. Off by
        default until the tree uses them; a negative value restores it."""
        self._check(_lib.jaos_set_mip_symmetry(self._handle(), int(on)))

    def set_mip_orbital(self, on):
        """Orbital branching and fixing (Ostrowski, Linderoth, Rossi and
        Smriglio): a branching on a binary zeroes its whole orbit on the
        zero side, and a node zeroes every orbit that holds a binary the
        path zeroed, the orbits taken under the generators that fix the
        path's ones. Turns the symmetry search on. On by default; a
        negative value restores it."""
        self._check(_lib.jaos_set_mip_orbital(self._handle(), int(on)))

    def set_mip_propagate(self, rounds):
        """Passes of bound propagation at each node.

        Each pass reads the model's rows over the node's own bounds,
        proves the node infeasible where a row admits no point, and pulls
        in the integer bounds the rows imply. 0 is off; a negative value
        restores the default.
        """
        self._check(_lib.jaos_set_mip_propagate(self._handle(), int(rounds)))

    def set_mip_propagate_depth(self, depth):
        """The deepest node bound propagation runs at, the root being 0.

        Negative, the default, is every node. The root's deductions are
        made over the model's own bounds and the whole tree keeps them; a
        deeper node's hold in its subtree alone.
        """
        self._check(_lib.jaos_set_mip_propagate_depth(self._handle(),
                                                      int(depth)))

    def set_mip_dive_degrade(self, frac):
        """How far a node's bound may fall from its parent's and still dive.

        A fraction of (1 + |parent's bound|); 0, the default, puts no bound
        on it. A negative value restores it (D316).
        """
        self._check(_lib.jaos_set_mip_dive_degrade(self._handle(),
                                                   float(frac)))

    def set_mip_heuristics(self, on=True):
        """Whether every fractional node is rounded for an incumbent (D290);
        on by default."""
        self._check(_lib.jaos_set_mip_heuristics(self._handle(), bool(on)))

    def set_mip_node_limit(self, nodes):
        """Stops a branch and bound before its `nodes`-th node past the
        limit, as NODE_LIMIT, keeping the incumbent; 0 removes it (D291)."""
        self._check(_lib.jaos_set_mip_node_limit(self._handle(), int(nodes)))

    def set_mip_branching(self, rule):
        """Which column a fractional node branches on: a `Branching`;
        PSEUDOCOST by default (D292)."""
        self._check(_lib.jaos_set_mip_branching(self._handle(), int(rule)))

    def set_algorithm(self, alg):
        """What solves an LP: an `Algorithm`, DUAL by default. DUAL and
        PRIMAL also solve the root relaxation and every node of a MIP;
        BARRIER, PDLP and CONCURRENT solve plain LPs only and leave a MIP
        to the dual. CONCURRENT refuses a quadratic objective, as PRIMAL
        and PDLP do."""
        self._check(_lib.jaos_set_algorithm(self._handle(), int(alg)))

    @property
    def algorithm(self):
        return Algorithm(_lib.jaos_algorithm_of(self._handle()))

    def set_option(self, name, value):
        """Any option by name, its value as text or a Python value:
        set_option("mip_cut_rounds", 2), set_option("algorithm", "primal")."""
        if isinstance(value, bool):
            value = "true" if value else "false"
        self._check(_lib.jaos_set_option(self._handle(), str(name).encode(),
                                         str(value).encode()))

    def get_option(self, name):
        """The option's current value, as text."""
        buf = ctypes.create_string_buffer(64)
        self._check(_lib.jaos_get_option(self._handle(), str(name).encode(),
                                         buf, 64))
        return buf.value.decode()

    def read_options(self, path):
        """Options from a file, one `name value` per line, `#` comments."""
        self._check(_lib.jaos_read_options(self._handle(),
                                           _path(path)))

    @staticmethod
    def option_names():
        n = int(_lib.jaos_num_options())
        return [_lib.jaos_option_name(k).decode() for k in range(n)]

    def set_mip_reliability(self, branches):
        """Branches per direction before a column's pseudocost is trusted;
        below it the column's children are solved on the spot (D293). 0
        never probes and is the default, since the probes measured worse
        as a default; a negative value restores 0."""
        self._check(_lib.jaos_set_mip_reliability(self._handle(), int(branches)))

    def set_mip_probe_cap(self, multiple):
        """A work cap on each strong-branching probe, as a multiple of the
        work the node's own relaxation took (D294): a child solve that
        reaches it stops and teaches nothing. 0 removes the cap; a negative
        value restores the default; NaN and infinity are refused."""
        self._check(_lib.jaos_set_mip_probe_cap(self._handle(), float(multiple)))

    def set_mip_dive_child(self, rule):
        """Which child a dive solves first, a `DiveChild` (D295); NEARER by
        default. Only matters with the dive on."""
        self._check(_lib.jaos_set_mip_dive_child(self._handle(), int(rule)))

    def set_mip_cut_drop(self, on=True):
        """Whether a local cut leaves the relaxation once its slack is basic
        at a node (D297); on by default. Only matters with a cut depth."""
        self._check(_lib.jaos_set_mip_cut_drop(self._handle(), bool(on)))

    def set_mip_probe_depth(self, depth):
        """Strong branching probes at nodes down to `depth` only, the root
        being 0 (D298); a negative value, the default, probes at every
        depth. Nothing happens under reliability 0."""
        self._check(_lib.jaos_set_mip_probe_depth(self._handle(), int(depth)))

    def set_mip_pool_size(self, size):
        """How many of the best integer points a branch and bound keeps
        (D299); 1, the default, is the incumbent alone. 0 is refused and a
        negative value restores 1."""
        self._check(_lib.jaos_set_mip_pool_size(self._handle(), int(size)))

    def mip_pool(self):
        """The solution pool after a branch and bound (D299): a list of
        (objective, values), best first; empty when no integer point was
        found."""
        n = _I64()
        self._check(_lib.jaos_mip_pool_count(self._handle(), ctypes.byref(n)))
        nc = self.num_col
        out = []
        for k in range(n.value):
            x = (_D * max(nc, 1))()
            obj = _D()
            self._check(_lib.jaos_mip_pool_solution(self._handle(), k, x,
                                                    ctypes.byref(obj)))
            out.append((obj.value, list(x[:nc])))
        return out

    def set_incumbent_callback(self, fn):
        """Asks a branch and bound to call `fn(incumbent)` for each new
        incumbent, where `incumbent` is an `Incumbent` tuple (D291). Return
        `CallbackAction.STOP` to stop the search, which keeps the incumbent
        and ends as INTERRUPTED; None or CONTINUE lets it run on. Pass None
        to remove the callback. The rule and the exception handling are the
        progress callback's."""
        if fn is None:
            self._incumbent_cb = None
            self._check(_lib.jaos_set_incumbent_callback(
                self._handle(), ctypes.cast(None, _INCUMBENT_FN), None))
            return self

        def trampoline(p, _user):
            try:
                c = p.contents
                vals = [c.col_value[i] for i in range(c.num_col)]
                r = fn(Incumbent(c.node, c.objective, c.bound, vals,
                                 bool(c.by_rounding)))
                return int(CallbackAction.CONTINUE if r is None else r)
            except Exception:
                sys.excepthook(*sys.exc_info())
                return int(CallbackAction.STOP)

        self._incumbent_cb = _INCUMBENT_FN(trampoline)
        self._check(_lib.jaos_set_incumbent_callback(self._handle(),
                                                     self._incumbent_cb, None))
        return self

    def set_node_callback(self, fn):
        """Asks a branch and bound to call `fn(event)` at every node
        once its relaxation is solved and cut, and for every point a
        heuristic would make an incumbent, with a `NodeEvent`. The
        callback may add rows that hold for every solution (user cuts
        at a fractional point, lazy constraints against an integral
        one, which is then not taken), and may set `event.branch_col`.
        A node whose point a new row cuts is solved again and seen
        again. Return `CallbackAction.STOP` to end the search as
        INTERRUPTED. Pass None to remove the callback."""
        if fn is None:
            self._node_cb = None
            self._check(_lib.jaos_set_node_callback(
                self._handle(), ctypes.cast(None, _NODE_FN), None))
            return self

        def trampoline(p, _user):
            ev = None
            try:
                ev = NodeEvent(p.contents)
                r = fn(ev)
                b = ev.branch_col
                p.contents.branch_col = -1 if b is None else int(b)
                return int(CallbackAction.CONTINUE if r is None else r)
            except Exception:
                sys.excepthook(*sys.exc_info())
                return int(CallbackAction.STOP)
            finally:
                if ev is not None:
                    ev._c = None

        self._node_cb = _NODE_FN(trampoline)
        self._check(_lib.jaos_set_node_callback(self._handle(),
                                                self._node_cb, None))
        return self

    def presolve_report(self):
        """What presolve did on the last solve: the sizes the simplex ran
        on, the round count, and how many of each family fired. All zero
        before a solve, and all zero under a build with presolve compiled
        out, which is what it did."""
        rep = _PresolveReport()
        self._check(_lib.jaos_presolve_result(self._handle(),
                                              ctypes.byref(rep)))
        return PresolveReport(*[getattr(rep, f)
                                for f, _ in _PresolveReport._fields_])

    def statistics(self):
        """What the model is, counted in one pass: sizes, row and column
        kinds, integer and binary counts, empty rows and columns, and the
        magnitude range of the matrix and of the objective. Solves
        nothing."""
        st = _ModelStats()
        self._check(_lib.jaos_model_statistics(self._handle(),
                                               ctypes.byref(st)))
        return ModelStats(*[getattr(st, f) for f, _ in _ModelStats._fields_])

    def set_mip_start(self, col_value):
        """Hand the tree an integer point before it runs, or None to clear.

        The library checks it at the root and runs without it when it is
        not a feasible integer point, so a wrong point is never published
        as an answer.
        """
        if col_value is None:
            self._check(_lib.jaos_set_mip_start(self._handle(), None))
            return self
        nc = self.num_col
        buf = (_D * max(nc, 1))(*[float(v) for v in col_value[:nc]])
        self._check(_lib.jaos_set_mip_start(self._handle(), buf))
        return self

    def set_mip_cutoff(self, cutoff):
        """An objective the caller does not care to beat, in the model's
        own sense. An infinity removes it. A cutoff tighter than the true
        optimum ends the search INFEASIBLE, which is the honest answer to
        the question it asks."""
        self._check(_lib.jaos_set_mip_cutoff(self._handle(), float(cutoff)))
        return self

    def exact_certificate(self):
        """Derive the Farkas multipliers behind an INFEASIBLE answer
        exactly, from the basis the refusal stopped on (D333).

        Returns an `ExactRayReport`. `derived` says the arithmetic fitted;
        false with no exception is the honest refusal, and `bound_bits`
        against `capacity_bits` says how far outside it was. The
        multipliers are then on the model, readable with
        `exact_row_multiplier()` and written into the proof file by
        `write_proof()` in place of the published doubles.

        It derives and does not judge: hand the result to
        `check_certificate()` at a tolerance of zero, or let
        `check_proof()` judge the file. Raises unless the last solve
        answered INFEASIBLE with both a ray and a basis."""
        rep = _ExactRayReport()
        self._check(_lib.jaos_exact_certificate(self._handle(),
                                                ctypes.byref(rep)))
        return ExactRayReport(*(getattr(rep, f)
                                for f, _ in _ExactRayReport._fields_))

    def exact_row_multiplier(self, row):
        """One row's exact Farkas multiplier as a `fractions.Fraction`,
        after exact_certificate(). Raises when there is none."""
        out = _CS()
        self._check(_lib.jaos_exact_row_multiplier(self._handle(), int(row),
                                                   ctypes.byref(out)))
        return Fraction(out.value.decode())

    def exact_unbounded_ray(self):
        """Derive the unbounded direction exactly, from the basis the
        solve stopped on (D336). The symmetric half of
        exact_certificate(): same report, same refusal rule, and the same
        split between deriving and judging. Raises unless the last solve
        answered UNBOUNDED with both a ray and a basis."""
        rep = _ExactRayReport()
        self._check(_lib.jaos_exact_unbounded_ray(self._handle(),
                                                  ctypes.byref(rep)))
        return ExactRayReport(*(getattr(rep, f)
                                for f, _ in _ExactRayReport._fields_))

    def exact_col_direction(self, col):
        """One column's exact ray component as a `fractions.Fraction`,
        after exact_unbounded_ray(). Raises when there is none."""
        out = _CS()
        self._check(_lib.jaos_exact_col_direction(self._handle(), int(col),
                                                  ctypes.byref(out)))
        return Fraction(out.value.decode())

    def write_proof(self, path):
        """Write the answer's exact proof to a file.

        An optimum needs a verify() that returned Proof.OPTIMAL; an
        INFEASIBLE or an UNBOUNDED answer needs none, because its
        certificate is a vector the solve already published and every
        double in it is an exact rational (D328). For an optimum the file
        holds
        every column's value and every row's dual as exact rationals, with
        the exact objective, and no basis. Raises when there is no proof
        to write.
        """
        self._check(_lib.jaos_write_proof(self._handle(), _path(path)))
        return self

    def check_proof(self, path):
        """Judge a proof file from this model alone, over the rationals.

        Reads no basis and needs no solve. Returns a ProofReport whose
        `kind` says what the file claims and whose `certified` is the
        verdict. For an optimum, `certified` is `primal and dual and
        objective`; for a certificate those three are False and mean
        nothing, and `bad_row` or `bad_col` names where it failed. Raises
        when the file is not a proof for this model, or when the exact
        arithmetic ran out.
        """
        rep = _ProofReport()
        self._check(_lib.jaos_check_proof(self._handle(), _path(path),
                                          ctypes.byref(rep)))
        vals = {f: getattr(rep, f) for f, _ in _ProofReport._fields_}
        vals["kind"] = ProofKind(vals["kind"])
        return ProofReport(**vals)

    def mip_report(self):
        rep = _MipReport()
        self._check(_lib.jaos_mip_result(self._handle(), ctypes.byref(rep)))
        return MipReport(*[getattr(rep, f) for f, _ in _MipReport._fields_])

    def mip_incumbent(self):
        """The best integer point a branch and bound found, proved or not,
        as (objective, values). Raises when there is none."""
        nc = self.num_col
        x = (_D * max(nc, 1))()
        obj = _D()
        self._check(_lib.jaos_mip_incumbent(self._handle(), x,
                                            ctypes.byref(obj)))
        return obj.value, list(x[:nc])

    @property
    def name(self):
        """The model's own name: an MPS file's NAME word, "JAOS" until one
        is given."""
        return self._name(_lib.jaos_model_name)

    @name.setter
    def name(self, value):
        self._check(_lib.jaos_set_model_name(self._handle(),
                                             self._name_arg(value)))

    def copy(self):
        """A new Model holding this one's problem, names, settings and
        starting basis, and not its answer (D287)."""
        handle = ctypes.c_void_p()
        self._check(_lib.jaos_model_copy(self._handle(),
                                         ctypes.byref(handle)))
        m = Model.__new__(Model)
        m._m = handle
        m._log_cb = None
        m._progress_cb = None
        m._incumbent_cb = None
        return m

    def col_index(self, name):
        """The column called `name`, positional names included. Raises
        JaosError when nothing is."""
        out = _I64()
        self._check(_lib.jaos_col_index(self._handle(),
                                        str(name).encode("utf-8"),
                                        ctypes.byref(out)))
        return out.value

    def row_index(self, name):
        out = _I64()
        self._check(_lib.jaos_row_index(self._handle(),
                                        str(name).encode("utf-8"),
                                        ctypes.byref(out)))
        return out.value

    def _exact(self, fn, *args):
        out = _CS()
        self._check(fn(self._handle(), *args, ctypes.byref(out)))
        return Fraction(out.value.decode("ascii"))

    def exact_col_value(self, col):
        """The exact value of a column at the proved optimum. Raises
        JaosError until verify() has proved the last answer."""
        return self._exact(_lib.jaos_exact_col_value, int(col))

    def exact_row_dual(self, row):
        return self._exact(_lib.jaos_exact_row_dual, int(row))

    def exact_objective(self):
        """c'x + c0 summed with no rounding. Raises when the sum did not
        fit the limb budget although the values did."""
        return self._exact(_lib.jaos_exact_objective)

    def _entries(self, fn, k):

        n = _I64()
        self._check(fn(self._handle(), k, ctypes.byref(n), None, None))
        idx = (_I64 * max(n.value, 1))()
        val = (_D * max(n.value, 1))()
        self._check(fn(self._handle(), k, ctypes.byref(n), idx, val))
        return list(idx[:n.value]), list(val[:n.value])

    def col_entries(self, col):
        """One column of the matrix: (row indices, values), ascending by
        row, with no explicit zeros."""
        return self._entries(_lib.jaos_col_entries, col)

    def row_entries(self, row):
        """One row of the matrix: (column indices, values), ascending by
        column. The first call after a matrix change builds the row-wise
        copy the solve would have built anyway."""
        return self._entries(_lib.jaos_row_entries, row)

    def coefficient(self, row, col):
        """One entry; 0.0 where the model holds none."""
        out = _D()
        self._check(_lib.jaos_coefficient(self._handle(), row, col,
                                          ctypes.byref(out)))
        return out.value

    def set_sense(self, sense):
        """Minimize or maximize. Discards the answer, keeps the basis."""
        self._check(_lib.jaos_set_objective_sense(self._handle(),
                                                  int(ObjSense(sense))))
        return self

    def set_obj_offset(self, offset):
        """The objective's constant term. Discards the answer, keeps the
        basis."""
        self._check(_lib.jaos_set_objective_offset(self._handle(),
                                                   float(offset)))
        return self

    def set_col_cost(self, col, cost):
        self._check(_lib.jaos_set_col_cost(self._handle(), col, float(cost)))
        return self

    def set_col_bounds(self, col, lower, upper):
        self._check(_lib.jaos_set_col_bounds(self._handle(), col,
                                             float(lower), float(upper)))
        return self

    def set_row_bounds(self, row, lower, upper):
        self._check(_lib.jaos_set_row_bounds(self._handle(), row,
                                             float(lower), float(upper)))
        return self

    def set_coefficient(self, row, col, value):
        """Sets one entry. Zero deletes it; a new index inserts one."""
        self._check(_lib.jaos_set_coefficient(self._handle(), row, col,
                                              float(value)))
        return self

    @staticmethod
    def _matrix(a_start, a_index, a_value, num_major, whose):
        """The sparse triplet the two append calls share, validated the way
        load() validates its own."""
        if not a_value:
            return None, None, None, 0
        num_nz = len(a_value)
        if a_index is None or len(a_index) != num_nz:
            raise ValueError("a_index must have one entry per value")
        if a_start is None or len(a_start) != num_major + 1:
            raise ValueError(f"a_start must have {whose} + 1 entries")
        starts, _ = _int64s(a_start, "a_start")
        idx, _ = _int64s(a_index, "a_index")
        vals, _ = _doubles(a_value, "a_value")
        return starts, idx, vals, num_nz

    def add_cols(self, col_cost, col_lower, col_upper,
                 a_start=None, a_index=None, a_value=None):
        """Appends columns after the existing ones.

        The triplet follows load()'s layout and describes the new columns
        down: `a_index` holds row indices into the rows the model already
        has. All None appends columns with no coefficients. The basis
        survives; the new columns arrive nonbasic, as jaos.h says.
        """
        n = len(col_cost)
        cost, _ = _doubles(col_cost, "col_cost", n)
        cl, _ = _doubles(col_lower, "col_lower", n)
        cu, _ = _doubles(col_upper, "col_upper", n)
        starts, idx, vals, num_nz = self._matrix(a_start, a_index, a_value,
                                                 n, "the new column count")
        self._check(_lib.jaos_add_cols(self._handle(), n, cost, cl, cu,
                                       num_nz, starts, idx, vals))
        return self

    def add_rows(self, row_lower, row_upper,
                 a_start=None, a_index=None, a_value=None):
        """Appends rows after the existing ones.

        Here the triplet describes the new rows across: `a_index` holds
        column indices. The basis survives; the new rows arrive basic,
        which is the warm re-solve case the C header points at.
        """
        n = len(row_lower)
        rl, _ = _doubles(row_lower, "row_lower", n)
        ru, _ = _doubles(row_upper, "row_upper", n)
        starts, idx, vals, num_nz = self._matrix(a_start, a_index, a_value,
                                                 n, "the new row count")
        self._check(_lib.jaos_add_rows(self._handle(), n, rl, ru,
                                       num_nz, starts, idx, vals))
        return self

    def delete_cols(self, cols):
        """Removes a set of columns in one call; the survivors keep their
        order and are renumbered densely from zero. A repeated index is
        refused, and jaos.h says why the API takes the whole set."""
        idx, n = _int64s(list(cols), "cols")
        self._check(_lib.jaos_delete_cols(self._handle(), n, idx))
        return self

    def delete_rows(self, rows):
        """Same contract as delete_cols, for rows."""
        idx, n = _int64s(list(rows), "rows")
        self._check(_lib.jaos_delete_rows(self._handle(), n, idx))
        return self

    def set_work_limit(self, units):
        self._check(_lib.jaos_set_work_limit(self._handle(), int(units)))
        return self

    def set_threads(self, threads):
        """The thread count, 1 by default. Only Algorithm.CONCURRENT
        runs more than one: above 1 it runs the dual, the primal and
        the barrier at once and stops the ones a winner has already
        beaten. The answer and the work units are the same at any
        count; the wall clock is not. Everything else runs one
        thread whatever this says. Zero or a negative count
        raises."""
        self._check(_lib.jaos_set_threads(self._handle(), int(threads)))
        return self

    @property
    def threads(self):
        """The thread count set by set_threads; 1 by default."""
        return _lib.jaos_threads_of(self._handle())

    def set_time_limit(self, seconds):
        self._check(_lib.jaos_set_time_limit(self._handle(), float(seconds)))
        return self

    def set_primal_tolerance(self, tol):
        self._check(_lib.jaos_set_primal_tolerance(self._handle(),
                                                   float(tol)))
        return self

    def set_dual_tolerance(self, tol):
        self._check(_lib.jaos_set_dual_tolerance(self._handle(), float(tol)))
        return self

    def set_log_callback(self, fn, level=LogLevel.SUMMARY):
        """Sends the solver's output to `fn(level, line)`.

        Pass None to turn it off. There is no default destination: without a
        callback JAOS says nothing, whatever the level.

        The C API forbids calling back into JAOS on the same model from
        inside this, and that rule reaches here unchanged.
        """
        if fn is None:
            self._log_cb = None
            self._check(_lib.jaos_set_log_callback(self._handle(),
                                                   ctypes.cast(None, _LOG_FN),
                                                   None))
            self._check(_lib.jaos_set_log_level(self._handle(),
                                                int(LogLevel.OFF)))
            return self

        def trampoline(_user, lvl, line):

            try:
                fn(LogLevel(lvl), line.decode("utf-8", "replace"))
            except Exception:
                sys.excepthook(*sys.exc_info())

        self._log_cb = _LOG_FN(trampoline)
        self._check(_lib.jaos_set_log_callback(self._handle(), self._log_cb,
                                               None))
        self._check(_lib.jaos_set_log_level(self._handle(), int(level)))
        return self

    def set_progress_callback(self, fn):
        """Asks the solve to call `fn(progress)` as it runs, where
        `progress` is a `Progress` tuple. Return `CallbackAction.STOP` to
        stop the solve; returning None, or `CONTINUE`, lets it run on. Pass
        None to remove the callback.

        A stopped solve ends as INTERRUPTED with no solution to read, and
        keeps the basis it stopped on, so calling solve() again continues.
        The callback may look and it may stop; it may not call back into
        JAOS on this model — jaos.h owns that rule and the reasons.

        An exception in `fn` cannot cross the C frame, so it is reported
        through sys.excepthook and the solve is stopped: a callback that is
        broken should not silently wave the solve on.
        """
        if fn is None:
            self._progress_cb = None
            self._check(_lib.jaos_set_progress_callback(
                self._handle(), ctypes.cast(None, _PROGRESS_FN), None))
            return self

        def trampoline(p, _user):
            try:
                c = p.contents
                r = fn(Progress(c.iterations, c.work_units,
                                c.primal_infeasibility))
                return int(CallbackAction.CONTINUE if r is None else r)
            except Exception:
                sys.excepthook(*sys.exc_info())
                return int(CallbackAction.STOP)

        self._progress_cb = _PROGRESS_FN(trampoline)
        self._check(_lib.jaos_set_progress_callback(self._handle(),
                                                    self._progress_cb, None))
        return self

    def solve(self):
        """Runs the solve. Returns the outcome.

        A return of OK from the C call means the solve ran, not that it
        found an optimum, so the outcome is what comes back here.
        """
        self._check(_lib.jaos_solve(self._handle()))
        return self.status

    @property
    def status(self):
        return SolveStatus(_lib.jaos_status_of(self._handle()))

    def objective(self):
        """The optimal objective. Raises unless the last solve found one:
        a zero handed out here could not be told from an answer that is
        genuinely zero."""
        out = _D()
        self._check(_lib.jaos_objective(self._handle(), ctypes.byref(out)))
        return out.value

    def solution(self):
        """Four lists: column values, row activities, row duals and reduced
        costs. Same rule as `objective` about when it is available."""
        nc, nr = self.num_col, self.num_row
        cv = (_D * max(nc, 1))()
        ra = (_D * max(nr, 1))()
        rd = (_D * max(nr, 1))()
        cd = (_D * max(nc, 1))()
        self._check(_lib.jaos_solution(self._handle(), cv, ra, rd, cd))
        return Solution(list(cv[:nc]), list(ra[:nr]),
                        list(rd[:nr]), list(cd[:nc]))

    def basis(self):
        """Where each variable rests in the basis behind the answer."""
        nc, nr = self.num_col, self.num_row
        cs = (ctypes.c_int * max(nc, 1))()
        rs = (ctypes.c_int * max(nr, 1))()
        self._check(_lib.jaos_basis(self._handle(), cs, rs))
        return Basis([BasisStatus(v) for v in cs[:nc]],
                     [BasisStatus(v) for v in rs[:nr]])

    def set_basis(self, col_status, row_status):
        """Hands the next solve its starting basis: one `BasisStatus` per
        column and one per row, the shape basis() returns. The C side
        refuses a set that is not a basis rather than repairing it."""
        nc, nr = self.num_col, self.num_row
        if len(col_status) != nc or len(row_status) != nr:
            raise ValueError(
                f"want {nc} column and {nr} row statuses, "
                f"got {len(col_status)} and {len(row_status)}")
        cs = (ctypes.c_int * max(nc, 1))(*(int(s) for s in col_status))
        rs = (ctypes.c_int * max(nr, 1))(*(int(s) for s in row_status))
        self._check(_lib.jaos_set_basis(self._handle(), cs, rs))
        return self

    def clear_basis(self):
        """Asks the next solve to start cold."""
        _lib.jaos_clear_basis(self._handle())
        return self

    def check_solution(self, col_value, row_dual=None, tol=1e-7):
        """Runs the library's independent checker on a candidate answer,
        against the model as loaded. Returns a `CheckReport`; what each
        field means, and why most of them decide nothing on their own, is
        on jaos_check_report in jaos.h. Pass row_dual=None to skip the dual
        conditions; the report then says checked_duals=False."""
        nc, nr = self.num_col, self.num_row
        cv, _ = _doubles(col_value, "col_value", nc)
        rd = None
        if row_dual is not None:
            rd, _ = _doubles(row_dual, "row_dual", nr)
        rep = _CheckReport()
        self._check(_lib.jaos_check_solution(self._handle(), cv, rd,
                                             float(tol), ctypes.byref(rep)))
        return CheckReport(*(getattr(rep, f)
                             for f, _ in _CheckReport._fields_))

    def certificate(self):
        """The Farkas ray behind the last solve's INFEASIBLE, one value per
        row. Raises unless the last solve answered INFEASIBLE with a ray to
        publish; a model whose own bounds are inverted has none, and the
        bounds are its proof (jaos_certificate in jaos.h)."""
        nr = self.num_row
        y = (_D * max(nr, 1))()
        self._check(_lib.jaos_certificate(self._handle(), y))
        return list(y[:nr])

    def check_certificate(self, row_ray, tol=1e-7):
        """Judges a claimed infeasibility certificate against the model as
        loaded, from the model alone. Returns a `CertificateReport`: the
        two halves and the gap between them, and `certified` when the gap
        clears tol against the size of the halves."""
        y, _ = _doubles(row_ray, "row_ray", self.num_row)
        rep = _CertificateReport()
        self._check(_lib.jaos_check_certificate(self._handle(), y,
                                                float(tol),
                                                ctypes.byref(rep)))
        return CertificateReport(*(getattr(rep, f)
                                   for f, _ in _CertificateReport._fields_))

    def unbounded_ray(self):
        """The direction behind the last solve's UNBOUNDED, one value per
        column. Raises unless the last solve answered UNBOUNDED."""
        nc = self.num_col
        d = (_D * max(nc, 1))()
        self._check(_lib.jaos_unbounded_ray(self._handle(), d))
        return list(d[:nc])

    def check_ray(self, col_ray, tol=1e-7):
        """Judges a claimed unbounded ray against the model as loaded, from
        the model alone. Returns a `RayReport`: the objective's rate along
        the ray, the largest push past a finite column or row side, and
        `certified` when both pushes are zero and the rate improves."""
        d, _ = _doubles(col_ray, "col_ray", self.num_col)
        rep = _RayReport()
        self._check(_lib.jaos_check_ray(self._handle(), d, float(tol),
                                        ctypes.byref(rep)))
        return RayReport(*(getattr(rep, f) for f, _ in _RayReport._fields_))

    def iis(self):
        """An irreducible infeasible subsystem of the last INFEASIBLE
        answer: an `IIS` of one `IISSide` per row and per column naming
        the bound sides that are infeasible on their own and all needed,
        and an `IISReport` of what finding it cost. The re-solves run on a
        private copy, so this model's answer and certificate stay as they
        are. Raises unless the last solve answered INFEASIBLE, and when a
        re-solve stopped on a budget or a numerical failure (jaos_iis in
        jaos.h)."""
        nr, nc = self.num_row, self.num_col
        rs = (ctypes.c_int * max(nr, 1))()
        cs = (ctypes.c_int * max(nc, 1))()
        rep = _IISReport()
        self._check(_lib.jaos_iis(self._handle(), rs, cs, ctypes.byref(rep)))
        return IIS([IISSide(v) for v in rs[:nr]],
                   [IISSide(v) for v in cs[:nc]],
                   IISReport(*(getattr(rep, f)
                               for f, _ in _IISReport._fields_)))

    def iis_model(self, iis):
        """The subsystem `iis` describes, as a `Model` of its own (D343):
        something to write to a file, open in an editor or solve again.

        Every cost is zeroed, because a subsystem is a feasibility
        question; a side that is not a member goes to the infinity that
        relaxes it; and a row or column nothing is left to say about is
        dropped. So the result is infeasible and its own solve() says so.
        Names survive and indices do not.

        `iis` is what iis() returned, or any pair of side lists of the
        right lengths."""
        nr, nc = self.num_row, self.num_col
        rows = getattr(iis, "row_side", None)
        cols = getattr(iis, "col_side", None)
        if rows is None or cols is None:
            rows, cols = iis
        if len(rows) != nr or len(cols) != nc:
            raise ValueError(
                "an IIS needs %d row sides and %d column sides, and got "
                "%d and %d" % (nr, nc, len(rows), len(cols)))
        rs = (ctypes.c_int * max(nr, 1))(*[int(s) for s in rows])
        cs = (ctypes.c_int * max(nc, 1))(*[int(s) for s in cols])
        out = _VP()
        self._check(_lib.jaos_iis_model(self._handle(), rs, cs,
                                        ctypes.byref(out)))
        return Model(_handle=out)

    def feasrelax(self, scope=RelaxScope.BOTH):
        """The smallest total change to the bounds that makes this model
        feasible: a `Relaxation` of one signed move per row, one per
        column, and a `RelaxReport`. A move below zero says that bound's
        LOWER side has to come down by that much, above zero that its
        UPPER side has to go up by it, zero that it does not move. Adding
        every move to the bound it names gives a model with a feasible
        point, and no other set of moves has a smaller total.

        `scope` is a `RelaxScope` and says which bounds may move. The work
        runs on a private copy, so this model's answer, certificate and
        basis stay as they are, and nothing needs to have been solved
        first: a feasible model answers 0. Raises when the model has no
        relaxation at all -- a lower bound above its upper -- or the copy
        did not finish (jaos_feasrelax in jaos.h)."""
        nr, nc = self.num_row, self.num_col
        rm = (_D * max(nr, 1))()
        cm = (_D * max(nc, 1))()
        rep = _RelaxReport()
        self._check(_lib.jaos_feasrelax(self._handle(), int(scope), rm, cm,
                                        ctypes.byref(rep)))
        return Relaxation(list(rm[:nr]), list(cm[:nc]),
                          RelaxReport(*(getattr(rep, f)
                                        for f, _ in _RelaxReport._fields_)))

    def verify(self):
        """Prove, or refuse to prove, that the basis behind the last
        optimum certifies its answer. Returns a `VerifyReport` whose
        `status` is a `Proof`: OPTIMAL when every basic value lies inside
        its bounds and every nonbasic reduced cost points into the model,
        BROKEN with `stage`, `at_row` or `at_col` and `violation` naming
        what fails, REFUSED when the numbers the proof needs do not fit in
        the arithmetic there is.

        Nothing here compares against a tolerance. The basis is rebuilt
        over the integers, split into blocks and eliminated by Bareiss's
        fraction-free method, so `violation` is the only rounded number
        the report carries and it decided nothing.

        REFUSED is normal rather than exceptional: over the gate it is 74
        of 110, and `bound_bits` against `capacity_bits` says by how much
        (D274). The cost is stated rather than billed to work units, and
        it is not small; `terms` says what it was. Needs an optimum, like
        basis() (jaos_verify in jaos.h)."""
        rep = _VerifyReport()
        self._check(_lib.jaos_verify(self._handle(), ctypes.byref(rep)))
        return self._verify_report(rep)

    @staticmethod
    def _verify_report(rep):
        vals = {f: getattr(rep, f) for f, _ in _VerifyReport._fields_}
        vals["status"] = Proof(vals["status"])
        vals["stage"] = ProofStage(vals["stage"])
        return VerifyReport(**vals)

    def verify_basis(self, col_status, row_status):
        """The same proof over a basis handed in, with no solve at all
        (D339). This is what makes JAOS a checker of somebody else's
        answer: read a model, read the basis another solver stopped on --
        read_mps_basis() reads the format the field writes it in -- and
        this says, over the rationals and with no tolerance, whether that
        basis is an optimal basis of that model.

        `col_status` holds one status per column and `row_status` one per
        row. Refused for a value that is not a `BasisStatus` and for any
        count of basic variables other than the row count.

        The model is not solved and its own state is not touched; what a
        proof leaves behind is the exact values it derived, readable
        through exact_col_value() and the two beside it."""
        nc, nr = self.num_col, self.num_row
        if len(col_status) != nc or len(row_status) != nr:
            raise ValueError(
                "a basis needs %d column statuses and %d row statuses, "
                "and got %d and %d"
                % (nc, nr, len(col_status), len(row_status)))
        cs = (ctypes.c_int * max(nc, 1))(*[int(s) for s in col_status])
        rs = (ctypes.c_int * max(nr, 1))(*[int(s) for s in row_status])
        rep = _VerifyReport()
        self._check(_lib.jaos_verify_basis(self._handle(), cs, rs,
                                           ctypes.byref(rep)))
        return self._verify_report(rep)

    def cost_ranging(self):
        """How far each column's cost may move, everything else held, with
        the basis behind the last optimum staying optimal: a `CostRanging`
        of two lists, one interval end per column. An open end is +-inf.
        Needs an optimum, like basis(); jaos.h states the cost."""
        nc = self.num_col
        lo = (_D * max(nc, 1))()
        hi = (_D * max(nc, 1))()
        self._check(_lib.jaos_cost_ranging(self._handle(), lo, hi))
        return CostRanging(list(lo[:nc]), list(hi[:nc]))

    def _bound_ranging(self, fn, n):
        arrs = [(_D * max(n, 1))() for _ in range(4)]
        self._check(fn(self._handle(), *arrs))
        return BoundRanging(*(list(a[:n]) for a in arrs))

    def rhs_ranging(self):
        """How far each row's two bounds may move: a `BoundRanging` of four
        lists, [lower_lo, lower_hi] for row_lower and [upper_lo, upper_hi]
        for row_upper, one entry per row."""
        return self._bound_ranging(_lib.jaos_rhs_ranging, self.num_row)

    def bound_ranging(self):
        """The same for each column's own bounds, one entry per column."""
        return self._bound_ranging(_lib.jaos_bound_ranging, self.num_col)

    @property
    def work_units(self):
        return _lib.jaos_work_units(self._handle())

    @property
    def iterations(self):
        return _lib.jaos_iterations(self._handle())

    @property
    def solve_time(self):
        """Seconds the last solve took. A development number: it does not
        repeat, and nothing should branch on it."""
        return _lib.jaos_solve_time(self._handle())

    def __repr__(self):
        if getattr(self, "_m", None) is None:
            return "<jaos.Model closed>"
        return (f"<jaos.Model {self.num_row}x{self.num_col}, "
                f"{self.num_nz} nonzeros, {self.status.name.lower()}>")

def _path(p):
    return os.fspath(p).encode(sys.getfilesystemencoding())

def quicksum(terms):
    """One expression from an iterable of variables, expressions and
    numbers, built in a single pass. `sum()` also works, but it builds one
    intermediate expression per term, which is quadratic in their count."""
    t, c, p = {}, 0.0, None
    for o in terms:
        e = _as_expr(o)
        if e is None:
            raise TypeError(f"cannot sum {o!r} into a linear expression")
        p = _merge_problem(p, e._p)
        for v, k in e._t.items():
            t[v] = t.get(v, 0.0) + k
        c += e._c
    return LinExpr(t, c, p)

def _as_expr(o):
    """The LinExpr view of an operand, or None when there is none."""
    if isinstance(o, LinExpr):
        return o
    if isinstance(o, Var):
        return LinExpr({o: 1.0}, 0.0, o._p)
    if isinstance(o, (int, float)):
        return LinExpr({}, float(o), None)
    return None

def _merge_problem(a, b):
    if a is None:
        return b
    if b is None or a is b:
        return a
    raise ValueError("these variables belong to two different Problems")

_NOT_LINEAR = ("JAOS solves linear programs; a product or quotient "
               "involving two variables is not linear")
_NOT_SEPARABLE = ("JAOS reads a separable quadratic objective only: x * x "
                  "or x ** 2, never a product of two different variables")

class Var:
    """One variable of a Problem. Made by add_var, never directly.

    Arithmetic on it builds a LinExpr; comparing it builds a Constraint.
    `==` between variables therefore means an equality constraint, not
    identity — use `is` to ask whether two names are the same variable.
    """

    __hash__ = object.__hash__

    __slots__ = ("_p", "_i", "_lb", "_ub", "name", "integer",
                 "semicontinuous")

    def __init__(self, problem, index, lb, ub, name):
        self._p = problem
        self._i = index
        self._lb = float(lb)
        self._ub = float(ub)
        self.name = name

    @property
    def lb(self):
        return self._lb

    @lb.setter
    def lb(self, v):
        self._lb = float(v)
        self._p._var_bounds_changed(self._i)

    @property
    def ub(self):
        return self._ub

    @ub.setter
    def ub(self, v):
        self._ub = float(v)
        self._p._var_bounds_changed(self._i)

    @property
    def value(self):
        """This variable's value in the held solution."""
        return self._p._solution().col_value[self._i]

    @property
    def reduced_cost(self):
        return self._p._solution().col_dual[self._i]

    def __add__(self, o):
        return _as_expr(self) + o
    __radd__ = __add__

    def __sub__(self, o):
        return _as_expr(self) - o

    def __rsub__(self, o):
        return (-_as_expr(self)) + o

    def __mul__(self, o):
        if isinstance(o, Var):
            if o is not self:
                raise TypeError(_NOT_SEPARABLE)
            return LinExpr({}, 0.0, self._p, {self: 1.0})
        return _as_expr(self) * o
    __rmul__ = __mul__

    def __pow__(self, n):
        if n != 2:
            raise TypeError(_NOT_SEPARABLE)
        return LinExpr({}, 0.0, self._p, {self: 1.0})

    def __truediv__(self, o):
        return _as_expr(self) / o

    def __neg__(self):
        return _as_expr(self) * -1.0

    def __pos__(self):
        return _as_expr(self)

    def __le__(self, o):
        return _as_expr(self) <= o

    def __ge__(self, o):
        return _as_expr(self) >= o

    def __eq__(self, o):
        return _as_expr(self) == o

    def __ne__(self, o):
        raise TypeError("a linear program has no 'not equal' constraint")

    def __repr__(self):
        return self.name

class LinExpr:
    """A linear expression: coefficients on variables plus a constant.

    Built by arithmetic on Var; rarely spelled out. Immutable in use —
    every operation returns a new expression.
    """

    __slots__ = ("_t", "_c", "_p", "_q")

    def __init__(self, terms=None, constant=0.0, problem=None, quad=None):
        self._t = dict(terms) if terms else {}
        self._c = float(constant)
        self._p = problem
        self._q = dict(quad) if quad else {}

    def __add__(self, o):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        p = _merge_problem(self._p, e._p)
        t = dict(self._t)
        for v, k in e._t.items():
            t[v] = t.get(v, 0.0) + k
        q = dict(self._q)
        for v, k in e._q.items():
            q[v] = q.get(v, 0.0) + k
        return LinExpr(t, self._c + e._c, p, q)
    __radd__ = __add__

    def __sub__(self, o):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        return self + (e * -1.0)

    def __rsub__(self, o):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        return e + (self * -1.0)

    def __mul__(self, o):
        if isinstance(o, Var) and not self._q and len(self._t) == 1 \
                and self._c == 0.0 and o in self._t:
            return LinExpr({}, 0.0, self._p, {o: self._t[o]})
        if isinstance(o, (Var, LinExpr)):
            raise TypeError(_NOT_SEPARABLE if isinstance(o, Var)
                            else _NOT_LINEAR)
        if not isinstance(o, (int, float)):
            return NotImplemented
        k = float(o)
        return LinExpr({v: c * k for v, c in self._t.items()},
                       self._c * k, self._p,
                       {v: c * k for v, c in self._q.items()})
    __rmul__ = __mul__

    def __truediv__(self, o):
        if isinstance(o, (Var, LinExpr)):
            raise TypeError(_NOT_LINEAR)
        if not isinstance(o, (int, float)):
            return NotImplemented
        return self * (1.0 / float(o))

    def __neg__(self):
        return self * -1.0

    def __pos__(self):
        return self

    def _rel(self, o, lower, upper):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        d = self - e
        if d._q:
            raise TypeError("a quadratic term belongs in the objective; "
                            "JAOS has no quadratic constraints")
        lo = -d._c if lower else -INFINITY
        hi = -d._c if upper else INFINITY
        return Constraint(d._p, d._t, lo, hi)

    def __le__(self, o):
        return self._rel(o, lower=False, upper=True)

    def __ge__(self, o):
        return self._rel(o, lower=True, upper=False)

    def __eq__(self, o):
        return self._rel(o, lower=True, upper=True)

    def __ne__(self, o):
        raise TypeError("a linear program has no 'not equal' constraint")

    __hash__ = None

    @property
    def value(self):
        """The expression evaluated at the held solution."""
        if self._p is None:
            return self._c
        col = self._p._solution().col_value
        return (self._c + sum(c * col[v._i] for v, c in self._t.items())
                + sum(c * col[v._i] ** 2 for v, c in self._q.items()))

    def __repr__(self):
        parts = [f"{c:g}*{v.name}" for v, c in self._t.items()]
        parts += [f"{c:g}*{v.name}**2" for v, c in self._q.items()]
        if self._c or not parts:
            parts.append(f"{self._c:g}")
        return " + ".join(parts)

class Constraint:
    """One linear constraint. Made by comparing expressions; a row of the
    problem once Problem.add has taken it.

    Its bounds stay writable afterwards: setting `lb` or `ub` on an added
    constraint is how a right-hand side is moved between solves, and only
    the bound crosses to the C side, so the next solve resumes warm.
    """

    __slots__ = ("_p", "_t", "_lo", "_hi", "_i", "name")

    def __init__(self, problem, terms, lo, hi):
        self._p = problem
        self._t = dict(terms)
        self._lo = float(lo)
        self._hi = float(hi)
        self._i = None
        self.name = None

    def __bool__(self):
        raise TypeError(
            "a constraint has no truth value. The usual way here is a "
            "chained comparison like  lo <= expr <= hi , which Python "
            "evaluates as two comparisons joined by 'and' and would "
            "silently drop the first bound — use "
            "Problem.add_range(lo, expr, hi). Testing variables for "
            "membership trips this too; compare them with 'is'.")

    @property
    def lb(self):
        return self._lo

    @lb.setter
    def lb(self, v):
        self._lo = float(v)
        if self._i is not None:
            self._p._row_bounds_changed(self._i)

    @property
    def ub(self):
        return self._hi

    @ub.setter
    def ub(self, v):
        self._hi = float(v)
        if self._i is not None:
            self._p._row_bounds_changed(self._i)

    def _require_added(self):
        if self._i is None:
            raise ValueError("this constraint is not in a Problem yet")

    @property
    def activity(self):
        """The row's left-hand side at the held solution."""
        self._require_added()
        return self._p._solution().row_activity[self._i]

    @property
    def dual(self):
        self._require_added()
        return self._p._solution().row_dual[self._i]

    def __repr__(self):
        e = repr(LinExpr(self._t, 0.0, self._p))
        if self._lo == self._hi:
            return f"{e} == {self._hi:g}"
        if self._lo == -INFINITY and self._hi == INFINITY:
            return f"{e} free"
        if self._lo == -INFINITY:
            return f"{e} <= {self._hi:g}"
        if self._hi == INFINITY:
            return f"{e} >= {self._lo:g}"
        return f"{self._lo:g} <= {e} <= {self._hi:g}"

class Problem:
    """A linear program written in variables and expressions.

        p = Problem()
        x = p.add_var(ub=4)
        y = p.add_var()
        p.add(x + y <= 4)
        p.maximize(x + 2 * y)
        p.solve()

    The problem is built in Python and loaded into a Model whole at the
    first solve. From then on, moving bounds or objective coefficients, or
    changing the objective's sense or constant, goes through the C setters
    and the next solve resumes warm; adding variables or constraints
    rebuilds and the next solve runs cold.

    Reading a value after the problem changed raises rather than answering
    from the stale solution. The header rule reaches here: no numbers the
    library will not stand behind.
    """

    def __init__(self):
        self._m = Model()
        self._vars = []
        self._cons = []
        self._sos = []
        self._ind = []
        self._obj = {}
        self._objq = {}
        self._obj_c = 0.0
        self._sense = ObjSense.MINIMIZE
        self._loaded = False
        self._structural = False
        self._sol = None

        self._dirty_var_bounds = set()
        self._dirty_costs = set()
        self._dirty_quad = set()
        self._dirty_objective = False
        self._dirty_row_bounds = set()

    @property
    def model(self):
        """The Model underneath, for what this layer does not wrap."""
        return self._m

    def close(self):
        self._m.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def add_var(self, lb=0.0, ub=INFINITY, name=None, integer=False,
                binary=False, semicontinuous=False):
        """A new variable, bounded below at zero unless said otherwise.
        `integer=True` marks it integer, and the problem then solves by
        branch and bound (D288); `binary=True` is integer in [0, 1]."""
        if binary:
            lb, ub, integer = 0.0, 1.0, True
        v = Var(self, len(self._vars), lb, ub,
                name if name is not None else f"x{len(self._vars)}")
        v.integer = bool(integer)
        v.semicontinuous = bool(semicontinuous)
        self._vars.append(v)
        self._touch_structure()
        return v

    def add_vars(self, count, lb=0.0, ub=INFINITY, name=None):
        """`count` variables at once, as a list. A `name` becomes a prefix:
        add_vars(3, name="y") names them y0, y1, y2."""
        return [self.add_var(lb, ub,
                             None if name is None else f"{name}{k}")
                for k in range(count)]

    def add(self, cons, name=None):
        """Takes a constraint built by comparison:  p.add(x + 2*y <= 5).
        `p += x + 2*y <= 5` does the same without keeping the handle."""
        if not isinstance(cons, Constraint):
            raise TypeError(
                "add() wants a constraint, built like  x + 2*y <= 5")
        if cons._i is not None:
            raise ValueError("this constraint is already in a Problem")
        if cons._p is not None and cons._p is not self:
            raise ValueError(
                "this constraint's variables belong to a different Problem")
        cons._p = self
        cons._i = len(self._cons)
        cons.name = name if name is not None else f"c{len(self._cons)}"
        self._cons.append(cons)
        self._touch_structure()
        return cons

    def __iadd__(self, cons):
        self.add(cons)
        return self

    def add_range(self, lo, expr, hi, name=None):
        """The two-sided row  lo <= expr <= hi , which the chained
        comparison cannot spell (Constraint.__bool__ says why)."""
        if isinstance(lo, (Var, LinExpr)) or isinstance(hi, (Var, LinExpr)):
            raise TypeError("add_range wants numbers on both outsides: "
                            "add_range(number, expression, number)")
        e = _as_expr(expr)
        if e is None:
            raise TypeError(f"cannot make a constraint from {expr!r}")
        return self.add(Constraint(e._p, e._t,
                                   float(lo) - e._c, float(hi) - e._c), name)

    def minimize(self, expr):
        """Sets the objective. A constant term is kept and reported —
        minimize(x + 7) answers 7 more than minimize(x)."""
        return self._set_objective(expr, ObjSense.MINIMIZE)

    def maximize(self, expr):
        return self._set_objective(expr, ObjSense.MAXIMIZE)

    def _set_objective(self, expr, sense):
        e = _as_expr(expr)
        if e is None:
            raise TypeError(f"cannot make an objective from {expr!r}")
        if e._p is not None and e._p is not self:
            raise ValueError(
                "this objective's variables belong to a different Problem")
        new = {v: float(c) for v, c in e._t.items()}
        newq = {v: 2.0 * float(c) for v, c in e._q.items() if c != 0.0}
        self._sol = None
        if self._loaded and not self._structural:

            if sense is not self._sense or float(e._c) != self._obj_c:
                self._dirty_objective = True
            for v in set(self._obj) | set(new):
                if self._obj.get(v, 0.0) != new.get(v, 0.0):
                    self._dirty_costs.add(v._i)
            for v in set(self._objq) | set(newq):
                if self._objq.get(v, 0.0) != newq.get(v, 0.0):
                    self._dirty_quad.add(v._i)
        self._obj = new
        self._objq = newq
        self._obj_c = float(e._c)
        self._sense = sense
        return self

    def add_indicator(self, indicator, value, cons, name=None):
        """`cons` holds only while the integer variable `indicator` equals
        `value` (0 or 1):  p.add_indicator(z, 1, x + y <= 5)."""
        if indicator._p is not self:
            raise ValueError(f"{indicator.name} belongs to a different "
                             f"Problem")
        c = self.add(cons, name)
        self._ind.append((c, indicator, int(value)))
        return c

    def add_sos(self, sos_type, variables, weights=None):
        """A special ordered set over `variables`: type 1 lets one be
        nonzero, type 2 two adjacent ones. `weights` orders them; by default
        1, 2, 3, ..."""
        variables = list(variables)
        if weights is None:
            weights = [float(k + 1) for k in range(len(variables))]
        weights = [float(w) for w in weights]
        if len(weights) != len(variables):
            raise ValueError("one weight per variable")
        for v in variables:
            if v._p is not self:
                raise ValueError(f"{v.name} belongs to a different Problem")
        self._sos.append((int(sos_type), variables, weights))
        self._touch_structure()
        return self

    def _touch_structure(self):
        self._sol = None
        if self._loaded:
            self._structural = True

    def _var_bounds_changed(self, i):
        self._sol = None
        if self._loaded and not self._structural:
            self._dirty_var_bounds.add(i)

    def _row_bounds_changed(self, i):
        self._sol = None
        if self._loaded and not self._structural:
            self._dirty_row_bounds.add(i)

    def _pending(self):
        return (not self._loaded or self._structural
                or self._dirty_objective
                or bool(self._dirty_costs) or bool(self._dirty_var_bounds)
                or bool(self._dirty_quad)
                or bool(self._dirty_row_bounds))

    def _build_and_load(self):
        nc, nr = len(self._vars), len(self._cons)
        cost = [0.0] * nc
        for v, c in self._obj.items():
            cost[v._i] = c

        cols = [[] for _ in range(nc)]
        for r, con in enumerate(self._cons):
            for v, c in con._t.items():
                if v._p is not self:
                    raise ValueError(f"{v.name} belongs to a different "
                                     f"Problem")
                if c != 0.0:
                    cols[v._i].append((r, c))
        a_start, a_index, a_value = [0], [], []
        for entries in cols:
            for r, c in entries:
                a_index.append(r)
                a_value.append(c)
            a_start.append(len(a_index))
        if not a_value:
            a_start = a_index = a_value = None
        self._m.load(nc, nr, cost,
                     [v._lb for v in self._vars],
                     [v._ub for v in self._vars],
                     [c._lo for c in self._cons],
                     [c._hi for c in self._cons],
                     a_start, a_index, a_value,
                     sense=self._sense, obj_offset=self._obj_c)

        for v in self._vars:
            self._m.set_col_name(v._i, v.name)
            if getattr(v, "integer", False):
                self._m.set_col_integer(v._i, True)
            if getattr(v, "semicontinuous", False):
                self._m.set_col_semicontinuous(v._i, True)
        for v, q in self._objq.items():
            self._m.set_col_quadratic(v._i, q)
        self._dirty_quad.clear()
        for t, vs, ws in self._sos:
            self._m.add_sos(t, [v._i for v in vs], ws)
        for c, z, v in self._ind:
            self._m.set_row_indicator(c._i, z._i, v)
        for c in self._cons:
            self._m.set_row_name(c._i, c.name)
        self._dirty_var_bounds.clear()
        self._dirty_costs.clear()
        self._dirty_row_bounds.clear()
        self._dirty_objective = False
        self._structural = False
        self._loaded = True

    def _load_changes(self):
        """Puts everything the problem has changed onto the model. Every
        call that reaches the library through this layer goes through it
        first, so the model the library sees is the problem as written."""
        if not self._loaded or self._structural:
            self._build_and_load()
        else:
            if self._dirty_objective:
                self._m.set_sense(self._sense)
                self._m.set_obj_offset(self._obj_c)
                self._dirty_objective = False
            for i in self._dirty_costs:
                self._m.set_col_cost(i, self._obj.get(self._vars[i], 0.0))
            for i in self._dirty_quad:
                self._m.set_col_quadratic(i, self._objq.get(self._vars[i], 0.0))
            self._dirty_quad.clear()
            for i in self._dirty_var_bounds:
                v = self._vars[i]
                self._m.set_col_bounds(i, v._lb, v._ub)
            for i in self._dirty_row_bounds:
                c = self._cons[i]
                self._m.set_row_bounds(i, c._lo, c._hi)
            self._dirty_costs.clear()
            self._dirty_var_bounds.clear()
            self._dirty_row_bounds.clear()

    def solve(self):
        """Loads what changed, runs the solve, returns the outcome."""
        self._sol = None
        self._load_changes()
        return self._m.solve()

    def _solution(self):
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        if self._sol is None:
            self._sol = self._m.solution()
        return self._sol

    @property
    def status(self):
        return self._m.status

    @property
    def objective_value(self):
        """The optimal objective. Raises while the problem is ahead of its
        last solve, for the same reason Model.objective raises before one:
        a stale number cannot be told from a right one."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        return self._m.objective()

    def check(self, tol=1e-7):
        """The library's independent checker, on the held solution against
        the model as loaded. Returns a CheckReport."""
        s = self._solution()
        return self._m.check_solution(s.col_value, s.row_dual, tol)

    def certificate(self):
        """The Farkas ray behind an INFEASIBLE answer: one multiplier per
        constraint, in the order they were added. Raises while the problem
        is ahead of its last solve, and when there is no ray to publish;
        `model.check_certificate` judges it from the model alone."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        return self._m.certificate()

    def unbounded_ray(self):
        """The direction behind an UNBOUNDED answer: one step per variable,
        in the order they were added. Same availability as certificate()."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        return self._m.unbounded_ray()

    def iis(self):
        """An irreducible infeasible subsystem of an INFEASIBLE answer, in
        this layer's own terms: a list of (Constraint, IISSide) and a list
        of (Var, IISSide), members only, and the `IISReport` behind them.
        Same availability as certificate()."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        found = self._m.iis()
        cons = [(c, s) for c, s in zip(self._cons, found.row_side)
                if s != IISSide.NONE]
        bounds = [(v, s) for v, s in zip(self._vars, found.col_side)
                  if s != IISSide.NONE]
        return IIS(cons, bounds, found.report)

    def iis_model(self):
        """The subsystem as a `Model` of its own; see Model.iis_model.
        This layer's iis() reports members only, so the IIS is taken
        again from the model underneath rather than from what iis()
        returned."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        found = self._m.iis()
        return self._m.iis_model(found)

    def feasrelax(self, scope=RelaxScope.BOTH):
        """The smallest total change to the bounds that makes this problem
        feasible, in this layer's own terms: a list of (Constraint, move)
        and a list of (Var, move), the bounds that actually move only, and
        the `RelaxReport` behind them. A move below zero is that bound's
        lower side coming down, above zero its upper side going up.

        Nothing needs solving first and nothing is solved: the work runs on
        an elastic copy, and a feasible problem answers 0. What the problem
        has changed since its last solve is loaded first, so the relaxation
        is about the problem as written."""
        self._load_changes()
        found = self._m.feasrelax(scope)
        cons = [(c, v) for c, v in zip(self._cons, found.row_move) if v != 0.0]
        bounds = [(x, v) for x, v in zip(self._vars, found.col_move)
                  if v != 0.0]
        return Relaxation(cons, bounds, found.report)

    def _settled(self):
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")

    def verify(self):
        """Prove, or refuse to prove, that the basis behind this answer
        certifies it, with no tolerance anywhere. Returns a `VerifyReport`;
        see `Model.verify`. `at_row` and `at_col` are indices into the
        constraints and the variables in the order they were added, or -1.
        Same availability as cost_ranging()."""
        self._settled()
        return self._m.verify()

    def verify_basis(self, col_status, row_status):
        """The same proof over a basis handed in, with no solve at all;
        see `Model.verify_basis`. The statuses are in the order the
        variables and the constraints were added."""
        if self._pending():
            self._build_and_load()
        return self._m.verify_basis(col_status, row_status)

    def cost_ranging(self):
        """How far each variable's objective coefficient may move with the
        basis behind the answer staying optimal: a `CostRanging` of two
        lists, one entry per variable in the order they were added."""
        self._settled()
        return self._m.cost_ranging()

    def rhs_ranging(self):
        """How far each constraint's two bounds may move: a `BoundRanging`
        of four lists, one entry per constraint in the order they were
        added."""
        self._settled()
        return self._m.rhs_ranging()

    def bound_ranging(self):
        """The same for each variable's own bounds."""
        self._settled()
        return self._m.bound_ranging()

    @property
    def variables(self):
        return tuple(self._vars)

    @property
    def constraints(self):
        return tuple(self._cons)

    def set_work_limit(self, units):
        self._m.set_work_limit(units)
        return self

    def set_threads(self, threads):
        self._m.set_threads(threads)
        return self

    def set_time_limit(self, seconds):
        self._m.set_time_limit(seconds)
        return self

    def set_primal_tolerance(self, tol):
        self._m.set_primal_tolerance(tol)
        return self

    def set_dual_tolerance(self, tol):
        self._m.set_dual_tolerance(tol)
        return self

    def set_mip_gap(self, gap):
        """The relative gap that closes a branch and bound; 0 restores the
        default of 1e-6."""
        self._m.set_mip_gap(gap)
        return self

    def set_mip_dive(self, on=True):
        """Whether a selected node is dived from; off by default (D289)."""
        self._m.set_mip_dive(on)
        return self

    def set_mip_cut_rounds(self, rounds):
        """Rounds of Gomory cuts at the root: 0 for none, negative for the
        default of 1 (D289)."""
        self._m.set_mip_cut_rounds(rounds)
        return self

    def set_mip_cut_depth(self, depth):
        """One round of Gomory cuts at every node down to `depth` (D296);
        0 is the root only, 3 the default (D301)."""
        self._m.set_mip_cut_depth(depth)
        return self

    def set_mip_cover_rounds(self, rounds):
        """Rounds of knapsack cover cuts at the root (D300); 0 for none,
        negative for the default."""
        self._m.set_mip_cover_rounds(rounds)
        return self

    def set_mip_clique_rounds(self, rounds):
        self._m.set_mip_clique_rounds(rounds)
        return self

    def set_mip_zero_half_rounds(self, rounds):
        self._m.set_mip_zero_half_rounds(rounds)
        return self

    def set_mip_flow_cover_rounds(self, rounds):
        self._m.set_mip_flow_cover_rounds(rounds)
        return self

    def set_mip_node_cut_cap(self, cap):
        """At most `cap` cuts per node below the root (D301); 0 for no cap,
        negative for the default."""
        self._m.set_mip_node_cut_cap(cap)
        return self

    def set_mip_cut_stall(self, fraction):
        """The root's cut rounds end once one moves the bound by less than
        `fraction` of (1 + |bound|) (D304); 0 never, negative the default."""
        self._m.set_mip_cut_stall(fraction)
        return self

    def set_mip_node_cut_stall(self, fraction):
        """No cut round under a node whose round moved its bound by less
        than `fraction` of (1 + |bound|) (D305); 0 never, negative the
        default."""
        self._m.set_mip_node_cut_stall(fraction)
        return self

    def set_mip_root_cut_drop(self, on=True):
        """Whether the root's cuts leave below a node where slack (D306);
        None restores the default."""
        self._m.set_mip_root_cut_drop(on)
        return self

    def set_mip_cover_lift(self, on=True):
        """Whether a cover cut is lifted with Balas's coefficients (D307);
        None restores the default."""
        self._m.set_mip_cover_lift(on)
        return self

    def set_mip_mir_rounds(self, rounds):
        """Rounds of MIR cuts on the model's rows at the root (D309); 0 for
        none, negative for the default."""
        self._m.set_mip_mir_rounds(rounds)
        return self

    def set_mip_dive_backtrack(self, times):
        """How many times a dive may resume from its stack (D308); 0 is
        D289's dive, negative the default."""
        self._m.set_mip_dive_backtrack(times)
        return self

    def set_mip_dive_gap(self, fraction):
        """The dive resumes only while the sibling is within `fraction` of
        the best open bound (D311); 0 for no bound, negative the default."""
        self._m.set_mip_dive_gap(fraction)
        return self

    def set_mip_node_mir(self, on=True):
        """MIR cuts at the nodes beside the Gomory round (D310); None
        restores the default."""
        self._m.set_mip_node_mir(on)
        return self

    def set_mip_mir_aggregate(self, rows):
        """Rows a MIR cut may absorb before it is rounded (D312); 0 is the
        single-row form, negative the default."""
        self._m.set_mip_mir_aggregate(rows)
        return self

    def set_mip_dive_heuristic(self, solves):
        """Relaxations a dive for a first incumbent may solve at the root
        (D313); 0 is off, negative the default."""
        self._m.set_mip_dive_heuristic(solves)
        return self

    def set_mip_dive_heuristic_depth(self, depth):
        """Deepest node the dive heuristic runs at (D314); 0 is the root
        alone and the default, negative restores it."""
        self._m.set_mip_dive_heuristic_depth(depth)
        return self

    def set_mip_rins(self, solves):
        """Relaxations a RINS dive may solve at a node with an incumbent
        (D315); 0 is off and the default, negative restores it."""
        self._m.set_mip_rins(solves)
        return self

    def set_mip_feaspump(self, rounds):
        """Rounds the feasibility pump may run at the root (D318); 0 is off
        and the default, negative restores it."""
        self._m.set_mip_feaspump(rounds)
        return self

    def set_mip_pump_general(self, on):
        """Whether the pump's distance carries an auxiliary column per
        general integer column; off by default, negative restores it."""
        self._m.set_mip_pump_general(on)
        return self

    def set_mip_pump_obj(self, decay):
        """The objective pump's decay, a fraction below 1; 0 is the plain
        pump, 0.5 the default, negative restores it."""
        self._m.set_mip_pump_obj(decay)
        return self

    def set_mip_pump_always(self, on):
        """Whether the pump runs at the root where an incumbent already
        exists (D322); off by default, negative restores it."""
        self._m.set_mip_pump_always(on)
        return self

    def set_mip_rcfix(self, on):
        """Reduced-cost fixing of integer bounds at the root (D323); on by
        default, negative restores it."""
        self._m.set_mip_rcfix(on)
        return self

    def set_mip_tighten(self, on):
        """Coefficient tightening of binary columns in one-sided rows at
        the root; on by default, negative restores it."""
        self._m.set_mip_tighten(on)
        return self

    def set_mip_probing(self, on):
        """Probing of the binary columns fractional at the root; off by
        default, negative restores it."""
        self._m.set_mip_probing(on)
        return self

    def set_mip_probing_cap(self, multiple):
        self._m.set_mip_probing_cap(multiple)
        return self

    def set_mip_clique_fix(self, on):
        """Fixing by the root's clique table at each node; off by
        default, negative restores it."""
        self._m.set_mip_clique_fix(on)
        return self

    def set_mip_conflicts(self, on):
        """Conflict analysis at infeasible nodes; on by default,
        negative restores it."""
        self._m.set_mip_conflicts(on)
        return self

    def set_mip_symmetry(self, on):
        """Symmetry detection at the root; off by default, negative
        restores it."""
        self._m.set_mip_symmetry(on)
        return self

    def set_mip_orbital(self, on):
        """Orbital branching and fixing; on by default, negative
        restores it."""
        self._m.set_mip_orbital(on)
        return self

    def set_mip_propagate(self, rounds):
        """Passes of bound propagation at each node (D324); 0 is off and a
        negative value restores the default."""
        self._m.set_mip_propagate(rounds)
        return self

    def set_mip_propagate_depth(self, depth):
        """The deepest node propagation runs at, the root being 0 (D324);
        negative, the default, is every node."""
        self._m.set_mip_propagate_depth(depth)
        return self

    def set_mip_dive_degrade(self, frac):
        """How far a node's bound may fall from its parent's and still dive
        (D316); 0, the default, puts no bound on it."""
        self._m.set_mip_dive_degrade(frac)
        return self

    def set_mip_heuristics(self, on=True):
        """Whether every fractional node is rounded for an incumbent (D290);
        on by default."""
        self._m.set_mip_heuristics(on)
        return self

    def set_mip_node_limit(self, nodes):
        self._m.set_mip_node_limit(nodes)
        return self

    def set_mip_branching(self, rule):
        self._m.set_mip_branching(rule)
        return self

    def set_algorithm(self, alg):
        self._m.set_algorithm(alg)
        return self

    @property
    def algorithm(self):
        return self._m.algorithm

    def set_option(self, name, value):
        self._m.set_option(name, value)
        return self

    def get_option(self, name):
        return self._m.get_option(name)

    def read_options(self, path):
        self._m.read_options(path)
        return self

    def set_mip_reliability(self, branches):
        self._m.set_mip_reliability(branches)
        return self

    def set_mip_probe_cap(self, multiple):
        self._m.set_mip_probe_cap(multiple)
        return self

    def set_mip_dive_child(self, rule):
        self._m.set_mip_dive_child(rule)
        return self

    def set_mip_cut_drop(self, on=True):
        self._m.set_mip_cut_drop(on)
        return self

    def set_mip_probe_depth(self, depth):
        self._m.set_mip_probe_depth(depth)
        return self

    def set_mip_pool_size(self, size):
        self._m.set_mip_pool_size(size)
        return self

    def set_incumbent_callback(self, fn):
        """Like Model.set_incumbent_callback, with `values` as a dict from
        variable to value (D291)."""
        if fn is None:
            self._m.set_incumbent_callback(None)
            return self
        vars_ = self._vars

        def wrap(inc):
            return fn(inc._replace(values={v: inc.values[i]
                                           for i, v in enumerate(vars_)}))
        self._m.set_incumbent_callback(wrap)
        return self

    def set_node_callback(self, fn):
        """Like Model.set_node_callback, with `event.values` as a dict
        from variable to value, `event.add(constraint)` taking a
        constraint built by comparison (it must hold for every solution
        of the problem), `event.branch_on(var)` naming the column to
        branch on, and `event.branch_var` the solver's own choice or
        None."""
        if fn is None:
            self._m.set_node_callback(None)
            return self
        problem = self

        class _Event:
            __slots__ = ("_ev", "node", "depth", "objective", "bound",
                         "values", "integral", "branch_var")

            def __init__(self, ev):
                self._ev = ev
                self.node = ev.node
                self.depth = ev.depth
                self.objective = ev.objective
                self.bound = ev.bound
                self.values = {v: ev.values[i]
                               for i, v in enumerate(problem._vars)}
                self.integral = ev.integral
                self.branch_var = (problem._vars[ev.branch_col]
                                   if ev.branch_col >= 0 else None)

            def add(self, cons):
                if not isinstance(cons, Constraint):
                    raise TypeError("add() wants a constraint, built like "
                                    " x + 2*y <= 5")
                index, value = [], []
                for v, c in cons._t.items():
                    if v._p is not problem:
                        raise ValueError(f"{v.name} belongs to a different "
                                         f"Problem")
                    if c != 0.0:
                        index.append(v._i)
                        value.append(c)
                self._ev.add_row(index, value, cons._lo, cons._hi)

            def branch_on(self, var):
                if var is None:
                    self._ev.branch_col = -1
                    return
                if var._p is not problem:
                    raise ValueError(f"{var.name} belongs to a different "
                                     f"Problem")
                self._ev.branch_col = var._i

        def wrap(ev):
            return fn(_Event(ev))
        self._m.set_node_callback(wrap)
        return self

    def presolve_report(self):
        """What presolve did on the last solve (D329): the sizes the
        simplex ran on, the rounds, and each family's count."""
        return self._m.presolve_report()

    def statistics(self):
        """What the problem is, counted (D327): sizes, row and column
        kinds, integer and binary counts, and magnitude ranges. Loads the
        problem first if it changed, the way write_mps does."""
        if self._pending():
            self._build_and_load()
        return self._m.statistics()

    def set_mip_start(self, point):
        """Hand the tree a point before it runs (D326).

        `point` is a mapping from variable to value, or None to clear. A
        variable left out is 0. The library checks the point at the root
        and runs without it when it is not feasible.
        """
        if point is None:
            self._m.set_mip_start(None)
            return self
        if self._pending():
            self._build_and_load()
        vals = [0.0] * len(self._vars)
        for v, x in point.items():
            vals[v._i] = float(x)
        self._m.set_mip_start(vals)
        return self

    def set_mip_cutoff(self, cutoff):
        """An objective the tree need not beat (D326); an infinity removes
        it. A cutoff tighter than the optimum ends the search
        INFEASIBLE."""
        self._m.set_mip_cutoff(cutoff)
        return self

    def write_proof(self, path):
        """Write the exact optimality proof to a file (D325).

        Needs a verify() that returned Proof.OPTIMAL. Raises while the
        problem is ahead of its last solve, because the proof on the model
        is then about a different problem."""
        self._settled()
        self._m.write_proof(path)
        return self

    def exact_certificate(self):
        """Derive the exact Farkas multipliers behind an INFEASIBLE
        answer; see `Model.exact_certificate`. Raises while the problem is
        ahead of its last solve."""
        self._settled()
        return self._m.exact_certificate()

    def exact_row_multiplier(self, con):
        """One constraint's exact Farkas multiplier, by Constraint or by
        index; see `Model.exact_row_multiplier`."""
        self._settled()
        return self._m.exact_row_multiplier(
            con._i if isinstance(con, Constraint) else int(con))

    def exact_unbounded_ray(self):
        """Derive the unbounded direction exactly; see
        `Model.exact_unbounded_ray`."""
        self._settled()
        return self._m.exact_unbounded_ray()

    def exact_col_direction(self, var):
        """One variable's exact ray component, by Var or by index; see
        `Model.exact_col_direction`."""
        self._settled()
        return self._m.exact_col_direction(
            var._i if isinstance(var, Var) else int(var))

    def check_proof(self, path):
        """Judge a proof file from this problem alone, over the rationals
        and with no tolerance (D325). Returns a ProofReport.

        It reads no basis and needs no solve, but it does need the model
        to exist, so a problem that changed since its last load is loaded
        first, the way write_mps does it."""
        if self._pending():
            self._build_and_load()
        return self._m.check_proof(path)

    def mip_report(self):
        """What the last branch and bound did: nodes, lp_solves,
        has_incumbent, incumbent, bound, cuts, heuristic_points,
        first_incumbent_node, fixed_cols, tightened. Raises while the
        problem is
        ahead of its last solve."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        return self._m.mip_report()

    def mip_incumbent(self):
        """The best integer point the last branch and bound found, proved or
        not, as (objective, {variable: value}). Raises when there is none."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        obj, x = self._m.mip_incumbent()
        return obj, {v: x[i] for i, v in enumerate(self._vars)}

    def mip_pool(self):
        """The solution pool after a branch and bound (D299): a list of
        (objective, {variable: value}), best first."""
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        return [(obj, {v: x[i] for i, v in enumerate(self._vars)})
                for obj, x in self._m.mip_pool()]

    def set_log_callback(self, fn, level=LogLevel.SUMMARY):
        self._m.set_log_callback(fn, level)
        return self

    def set_progress_callback(self, fn):
        self._m.set_progress_callback(fn)
        return self

    def write_mps(self, path):
        """Writes the problem as it stands, loading it first if it changed.
        A reload drops the basis, so writing a changed problem makes the
        next solve cold."""
        if self._pending():
            self._build_and_load()
        self._m.write_mps(path)
        return self

    def write_lp(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_lp(path)
        return self

    def write_nl(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_nl(path)
        return self

    def write_qplib(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_qplib(path)
        return self

    def write_osil(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_osil(path)
        return self

    def write_solution(self, path):
        self._m.write_solution(path)

    def read_solution(self, path):
        """Reads back a file write_solution wrote; see Model.read_solution."""
        return self._m.read_solution(path)

    def read_basis(self, path):
        """The basis out of a solution file of either kind; see
        Model.read_basis."""
        if self._pending():
            self._build_and_load()
        return self._m.read_basis(path)

    def write_mps_basis(self, path):
        """Writes the last solve's basis as an MPS basis file; see
        Model.write_mps_basis."""
        self._m.write_mps_basis(path)

    def read_mps_basis(self, path):
        """The basis in an MPS basis file; see Model.read_mps_basis."""
        if self._pending():
            self._build_and_load()
        return self._m.read_mps_basis(path)

    def write_point(self, path):
        """Writes the answer's point as a point file; see
        Model.write_point."""
        self._m.write_point(path)

    def write_point_values(self, path, col_value):
        """The same file from values the caller has, in the order the
        variables were added; see Model.write_point_values."""
        if self._pending():
            self._build_and_load()
        self._m.write_point_values(path, col_value)

    def write_duals(self, path):
        """The answer's row multipliers as a duals file; see
        Model.write_duals."""
        self._m.write_duals(path)

    def write_dual_values(self, path, row_dual):
        """The same file from multipliers the caller has, in the order the
        constraints were added; see Model.write_dual_values."""
        if self._pending():
            self._build_and_load()
        self._m.write_dual_values(path, row_dual)

    def read_point(self, path):
        """The column values in a point file, in the order the variables
        were added; see Model.read_point."""
        if self._pending():
            self._build_and_load()
        return self._m.read_point(path)

    def read_duals(self, path):
        """The row multipliers in a file of the same shape, in the order
        the constraints were added; see Model.read_duals."""
        if self._pending():
            self._build_and_load()
        return self._m.read_duals(path)

    @property
    def work_units(self):
        return self._m.work_units

    @property
    def iterations(self):
        return self._m.iterations

    @property
    def solve_time(self):
        """Seconds, a development number; see Model.solve_time."""
        return self._m.solve_time

    def __repr__(self):
        return (f"<jaos.Problem {len(self._cons)}x{len(self._vars)}, "
                f"{self.status.name.lower()}>")
