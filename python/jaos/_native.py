import ctypes
import ctypes.util
import enum
import os
import sys
from collections import namedtuple
from fractions import Fraction

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

class ConeType(enum.IntEnum):
    """What a cone holds; jaos_cone_type. QUADRATIC over (t, x) is
    t >= ||x||. ROTATED over (u, v, x) is 2 u v >= ||x||**2 with u and
    v at least zero."""
    QUADRATIC = 1
    ROTATED = 2

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
        ("max_cone_violation", _D),
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
        ("aggregated_col", _I64),
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
        ("cone_set", _I64),
        ("quadratic_row", _I64),
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
        ("curvature", _D),
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

_sig("jaos_set_quadratic", ctypes.c_int, _VP, _I64, _P(_I64),
     _P(_I64), _P(_D))

_sig("jaos_quadratic_nz", _I64, _VP)

_sig("jaos_quadratic", ctypes.c_int, _VP, _P(_I64), _P(_I64),
     _P(_D))

_sig("jaos_col_quadratic", ctypes.c_int, _VP, _I64, _P(_D))

_sig("jaos_add_sos", ctypes.c_int, _VP, ctypes.c_int, _I64, _P(_I64), _P(_D))

_sig("jaos_num_sos", _I64, _VP)

_sig("jaos_set_row_indicator", ctypes.c_int, _VP, _I64, _I64, ctypes.c_int)

_sig("jaos_row_indicator", ctypes.c_int, _VP, _I64, _P(_I64), _P(ctypes.c_int))

_sig("jaos_sos", ctypes.c_int, _VP, _I64, _P(ctypes.c_int), _P(_I64),
     _P(_I64), _P(_D))

_sig("jaos_add_cone", ctypes.c_int, _VP, ctypes.c_int, _I64, _P(_I64))

_sig("jaos_num_cones", _I64, _VP)

_sig("jaos_cone", ctypes.c_int, _VP, _I64, _P(ctypes.c_int), _P(_I64),
     _P(_I64))

_sig("jaos_delete_cones", ctypes.c_int, _VP, _I64, _P(_I64))

_sig("jaos_set_row_quadratic", ctypes.c_int, _VP, _I64, _I64, _P(_I64),
     _P(_I64), _P(_D))

_sig("jaos_row_quadratic_nz", _I64, _VP, _I64)

_sig("jaos_row_quadratic", ctypes.c_int, _VP, _I64, _P(_I64), _P(_I64),
     _P(_D))

_sig("jaos_cone_dual", ctypes.c_int, _VP, _I64, _P(_D))

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

_sig("jaos_model_has_integer", ctypes.c_bool, _VP)

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

_sig("jaos_read_cbf", ctypes.c_int, _VP, _CS)

_sig("jaos_write_cbf", ctypes.c_int, _VP, _CS)

_sig("jaos_read_osil", ctypes.c_int, _VP, _CS)

_sig("jaos_write_osil", ctypes.c_int, _VP, _CS)

_sig("jaos_write_solution", ctypes.c_int, _VP, _CS)

_sig("jaos_write_sol_ampl", ctypes.c_int, _VP, _CS, _CS)

_sig("jaos_read_solution", ctypes.c_int, _VP, _CS, _P(_D),
     _P(_D), _P(_D), _P(ctypes.c_int),
     _P(_D), _P(_D), _P(ctypes.c_int))

_sig("jaos_read_certificate", ctypes.c_int, _VP, _CS, _P(ctypes.c_int),
     _P(_D), _P(_D))

_sig("jaos_read_cone_duals", ctypes.c_int, _VP, _CS, _P(_D))

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

_sig("jaos_set_mip_tree_batch", ctypes.c_int, _VP, _I64)

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

_sig("jaos_check_conic_solution", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D),
     _D, _P(_CheckReport))

_sig("jaos_check_conic_certificate", ctypes.c_int, _VP, _P(_D), _P(_D), _D,
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

def _path(p):
    return os.fspath(p).encode(sys.getfilesystemencoding())
