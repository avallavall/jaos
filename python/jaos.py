"""JAOS from Python.

A ctypes wrapper over `libjaos.so`. The standard library and nothing else,
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
2. ``libjaos.so`` beside this file;
3. ``build/release/libjaos.so`` under the current directory;
4. the system loader's search path.

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

__all__ = [
    "Model", "JaosError", "Status", "SolveStatus", "ObjSense", "LogLevel",
    "BasisStatus", "CallbackAction", "Solution", "Basis", "INFINITY",
    "NAME_MAX", "version", "library_path",
    "Problem", "Var", "LinExpr", "Constraint", "quicksum",
    "CheckReport", "CertificateReport", "RayReport", "Progress",
    "IISSide", "IISReport", "IIS",
    "Proof", "ProofStage", "VerifyReport",
]


# --------------------------------------------------------------------------
# The enumerations, mirroring include/jaos.h
# --------------------------------------------------------------------------

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
# Ranging: each field is a list of one interval end per column or per row.
CostRanging = namedtuple("CostRanging", "lower upper")
BoundRanging = namedtuple("BoundRanging",
                          "lower_lo lower_hi upper_lo upper_hi")

# What the solve reports about itself mid-run. See jaos_progress in jaos.h:
# there is deliberately no objective in it.
Progress = namedtuple("Progress",
                      "iterations work_units primal_infeasibility")


# --------------------------------------------------------------------------
# Loading the library
# --------------------------------------------------------------------------

def _find_library():
    env = os.environ.get("JAOS_LIBRARY")
    if env:
        if not os.path.exists(env):
            raise OSError(f"JAOS_LIBRARY is set to {env!r}, which does not "
                          f"exist")
        return env
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "libjaos.so")
    if os.path.exists(here):
        return here
    built = os.path.join(os.getcwd(), "build", "release", "libjaos.so")
    if os.path.exists(built):
        return built
    found = ctypes.util.find_library("jaos")
    if found:
        return found
    raise OSError(
        "libjaos.so not found. Build it with `make shared`, then either run "
        "from the repository root or set JAOS_LIBRARY to its full path.")


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
    ]


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

_LOG_FN = ctypes.CFUNCTYPE(None, _VP, ctypes.c_int, _CS)
_PROGRESS_FN = ctypes.CFUNCTYPE(ctypes.c_int, _P(_Progress), _VP)


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
_sig("jaos_write_mps", ctypes.c_int, _VP, _CS)
_sig("jaos_write_lp", ctypes.c_int, _VP, _CS)
_sig("jaos_write_solution", ctypes.c_int, _VP, _CS)
_sig("jaos_read_solution", ctypes.c_int, _VP, _CS, _P(_D),
     _P(_D), _P(_D), _P(ctypes.c_int),
     _P(_D), _P(_D), _P(ctypes.c_int))
_sig("jaos_read_certificate", ctypes.c_int, _VP, _CS, _P(ctypes.c_int),
     _P(_D), _P(_D))
_sig("jaos_solution_file_status", ctypes.c_int, _VP, _CS, _P(ctypes.c_int))
_sig("jaos_set_work_limit", ctypes.c_int, _VP, _I64)
_sig("jaos_set_time_limit", ctypes.c_int, _VP, _D)
_sig("jaos_set_primal_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_dual_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_log_callback", ctypes.c_int, _VP, _LOG_FN, _VP)
_sig("jaos_set_log_level", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_set_progress_callback", ctypes.c_int, _VP, _PROGRESS_FN, _VP)
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
_sig("jaos_iis", ctypes.c_int, _VP, _P(ctypes.c_int), _P(ctypes.c_int),
     _P(_IISReport))
_sig("jaos_cost_ranging", ctypes.c_int, _VP, _P(_D), _P(_D))
_sig("jaos_rhs_ranging", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_bound_ranging", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_verify", ctypes.c_int, _VP, _P(_VerifyReport))
_sig("jaos_work_units", _I64, _VP)
_sig("jaos_iterations", _I64, _VP)
_sig("jaos_solve_time", _D, _VP)

INFINITY = _lib.jaos_infinity()

# JAOS_NAME_MAX in jaos.h: the longest name a row, column or objective may
# carry, in bytes.
NAME_MAX = 255


def version():
    """The library's version string, from the library rather than from here.

    Two version numbers in one project drift, so this module keeps none of
    its own.
    """
    return _lib.jaos_version().decode("utf-8")


# --------------------------------------------------------------------------
# Turning Python sequences into C arrays
# --------------------------------------------------------------------------

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


# --------------------------------------------------------------------------
# The model
# --------------------------------------------------------------------------

class Model:
    """One problem, and the answer to it.

    Not thread-safe, which is the C library's rule and not this module's:
    one model is used by one thread at a time, and distinct models are
    independent.
    """

    def __init__(self):
        handle = _VP()
        rc = _lib.jaos_model_new(ctypes.byref(handle))
        if rc != Status.OK:
            raise JaosError(rc, "could not allocate a model")
        self._m = handle
        # ctypes does not keep a callback alive on the C side's behalf, and
        # a collected trampoline is a crash rather than an error. The two
        # references below are what stop that.
        self._log_cb = None
        self._progress_cb = None

    # -- lifetime ----------------------------------------------------------

    def close(self):
        """Frees the model. Safe to call twice; the object is unusable
        afterwards."""
        if getattr(self, "_m", None) is not None:
            _lib.jaos_model_free(self._m)
            self._m = None
            self._log_cb = None
            self._progress_cb = None

    def __del__(self):
        try:
            self.close()
        except Exception:                     # interpreter teardown
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

    # -- loading -----------------------------------------------------------

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

    def write_mps(self, path):
        self._check(_lib.jaos_write_mps(self._handle(), _path(path)))
        return self

    def write_lp(self, path):
        self._check(_lib.jaos_write_lp(self._handle(), _path(path)))
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

    # -- reading the problem back ------------------------------------------

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

    # -- names (D284) ------------------------------------------------------
    #
    # Every row and column has a name: the file's, or one set here, or its
    # position -- R<i+1>, C<j+1>, COST for the objective -- where nobody
    # gave one. None or "" takes a name away.

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

    def _entries(self, fn, k):
        # Two calls, as the header describes: the count, then the arrays.
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

    # -- changing it -------------------------------------------------------

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

    # -- growing and shrinking it ------------------------------------------

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

    # -- limits, tolerances and output -------------------------------------

    def set_work_limit(self, units):
        self._check(_lib.jaos_set_work_limit(self._handle(), int(units)))
        return self

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
            # An exception thrown through a C frame is undefined behaviour,
            # so it is caught here and reported rather than propagated.
            try:
                fn(LogLevel(lvl), line.decode("utf-8", "replace"))
            except Exception:                 # pragma: no cover - defensive
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

    # -- solving -----------------------------------------------------------

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
        vals = {f: getattr(rep, f) for f, _ in _VerifyReport._fields_}
        vals["status"] = Proof(vals["status"])
        vals["stage"] = ProofStage(vals["stage"])
        return VerifyReport(**vals)

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


# --------------------------------------------------------------------------
# The modeling layer
# --------------------------------------------------------------------------
#
# Sugar over Model, and nothing but sugar: it builds the same arrays load()
# takes, in one place, and the C side validates them again. It is built in
# Python and handed over whole at solve time rather than mirrored into C
# call by call, because that is the shape jaos_load_lp already wants, it
# crosses the ctypes boundary once instead of once per coefficient, and one
# build step is one place to keep deterministic.
#
# After a first solve, a change that only moves bounds or objective
# coefficients is applied through the three C setters instead, so the next
# solve resumes warm from the basis it has — which is the case the dual
# simplex is best at, as jaos.h says at jaos_set_col_cost. Adding a variable
# or a constraint rebuilds, and the next solve runs cold.


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


class Var:
    """One variable of a Problem. Made by add_var, never directly.

    Arithmetic on it builds a LinExpr; comparing it builds a Constraint.
    `==` between variables therefore means an equality constraint, not
    identity — use `is` to ask whether two names are the same variable.
    """

    # Identity hashing, kept explicitly because __eq__ is overridden below.
    # It is safe as a dict key this way: object.__hash__ gives two live
    # objects two different hashes, so a dict never has to call the
    # constraint-building __eq__ to tell two variables apart.
    __hash__ = object.__hash__

    __slots__ = ("_p", "_i", "_lb", "_ub", "name")

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
        return _as_expr(self) * o
    __rmul__ = __mul__

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

    __slots__ = ("_t", "_c", "_p")

    def __init__(self, terms=None, constant=0.0, problem=None):
        self._t = dict(terms) if terms else {}
        self._c = float(constant)
        self._p = problem

    def __add__(self, o):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        p = _merge_problem(self._p, e._p)
        t = dict(self._t)
        for v, k in e._t.items():
            t[v] = t.get(v, 0.0) + k
        return LinExpr(t, self._c + e._c, p)
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
        if isinstance(o, (Var, LinExpr)):
            raise TypeError(_NOT_LINEAR)
        if not isinstance(o, (int, float)):
            return NotImplemented
        k = float(o)
        return LinExpr({v: c * k for v, c in self._t.items()},
                       self._c * k, self._p)
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

    # A comparison builds the constraint  (self - other)  against the
    # constant it leaves behind. The expression's own constant is folded
    # into the bounds, so  x + 1 <= 4  and  x <= 3  build the same row.

    def _rel(self, o, lower, upper):
        e = _as_expr(o)
        if e is None:
            return NotImplemented
        d = self - e
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
        return self._c + sum(c * col[v._i] for v, c in self._t.items())

    def __repr__(self):
        parts = [f"{c:g}*{v.name}" for v, c in self._t.items()]
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
        self._vars = []            # every Var, in index order
        self._cons = []            # every added Constraint, in row order
        self._obj = {}             # Var -> objective coefficient
        self._obj_c = 0.0
        self._sense = ObjSense.MINIMIZE
        self._loaded = False       # the Model holds the current structure
        self._structural = False   # it no longer does; reload before solving
        self._sol = None
        # Value-only changes since the load, by index. Sets, because
        # applying them commutes: each index owns its own slot in the C
        # model and no floating point accumulates across them.
        self._dirty_var_bounds = set()
        self._dirty_costs = set()
        self._dirty_objective = False   # the sense or the constant moved
        self._dirty_row_bounds = set()

    # -- lifetime ----------------------------------------------------------

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

    # -- writing the problem -----------------------------------------------

    def add_var(self, lb=0.0, ub=INFINITY, name=None):
        """A new variable, bounded below at zero unless said otherwise."""
        v = Var(self, len(self._vars), lb, ub,
                name if name is not None else f"x{len(self._vars)}")
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
        self._sol = None
        if self._loaded and not self._structural:
            # Every part of the objective has a C setter (D283), so none of
            # this is structural and the next solve resumes from the basis.
            if sense is not self._sense or float(e._c) != self._obj_c:
                self._dirty_objective = True
            for v in set(self._obj) | set(new):
                if self._obj.get(v, 0.0) != new.get(v, 0.0):
                    self._dirty_costs.add(v._i)
        self._obj = new
        self._obj_c = float(e._c)
        self._sense = sense
        return self

    # -- change tracking ----------------------------------------------------

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
                or bool(self._dirty_row_bounds))

    # -- solving ------------------------------------------------------------

    def _build_and_load(self):
        nc, nr = len(self._vars), len(self._cons)
        cost = [0.0] * nc
        for v, c in self._obj.items():
            cost[v._i] = c
        # One bucket per column. The outer loop walks rows in order, so
        # each bucket's row indices come out ascending by construction —
        # the same layout load() documents.
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
        self._dirty_var_bounds.clear()
        self._dirty_costs.clear()
        self._dirty_row_bounds.clear()
        self._dirty_objective = False
        self._structural = False
        self._loaded = True

    def solve(self):
        """Loads what changed, runs the solve, returns the outcome."""
        self._sol = None
        if not self._loaded or self._structural:
            self._build_and_load()
        else:
            if self._dirty_objective:
                self._m.set_sense(self._sense)
                self._m.set_obj_offset(self._obj_c)
                self._dirty_objective = False
            for i in self._dirty_costs:
                self._m.set_col_cost(i, self._obj.get(self._vars[i], 0.0))
            for i in self._dirty_var_bounds:
                v = self._vars[i]
                self._m.set_col_bounds(i, v._lb, v._ub)
            for i in self._dirty_row_bounds:
                c = self._cons[i]
                self._m.set_row_bounds(i, c._lo, c._hi)
            self._dirty_costs.clear()
            self._dirty_var_bounds.clear()
            self._dirty_row_bounds.clear()
        return self._m.solve()

    def _solution(self):
        if self._pending():
            raise ValueError("the problem changed since the last solve; "
                             "call solve() before reading values")
        if self._sol is None:
            self._sol = self._m.solution()
        return self._sol

    # -- reading the answer -------------------------------------------------

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

    # -- pass-through -------------------------------------------------------

    def set_work_limit(self, units):
        self._m.set_work_limit(units)
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

    def write_solution(self, path):
        self._m.write_solution(path)

    def read_solution(self, path):
        """Reads back a file write_solution wrote; see Model.read_solution."""
        return self._m.read_solution(path)
        return self

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
