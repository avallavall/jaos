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

    import jaos

    m = jaos.Model()
    m.read_mps("model.mps")
    m.solve()
    if m.status is jaos.SolveStatus.OPTIMAL:
        print(m.objective(), m.solution().col_value)

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
    "version", "library_path",
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

_LOG_FN = ctypes.CFUNCTYPE(None, _VP, ctypes.c_int, _CS)
_PROGRESS_FN = ctypes.CFUNCTYPE(ctypes.c_int, _VP, _VP)


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
_sig("jaos_set_col_cost", ctypes.c_int, _VP, _I64, _D)
_sig("jaos_set_col_bounds", ctypes.c_int, _VP, _I64, _D, _D)
_sig("jaos_set_row_bounds", ctypes.c_int, _VP, _I64, _D, _D)
_sig("jaos_set_coefficient", ctypes.c_int, _VP, _I64, _I64, _D)
_sig("jaos_read_mps", ctypes.c_int, _VP, _CS)
_sig("jaos_read_lp", ctypes.c_int, _VP, _CS)
_sig("jaos_write_mps", ctypes.c_int, _VP, _CS)
_sig("jaos_write_lp", ctypes.c_int, _VP, _CS)
_sig("jaos_write_solution", ctypes.c_int, _VP, _CS)
_sig("jaos_set_work_limit", ctypes.c_int, _VP, _I64)
_sig("jaos_set_time_limit", ctypes.c_int, _VP, _D)
_sig("jaos_set_primal_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_dual_tolerance", ctypes.c_int, _VP, _D)
_sig("jaos_set_log_callback", ctypes.c_int, _VP, _LOG_FN, _VP)
_sig("jaos_set_log_level", ctypes.c_int, _VP, ctypes.c_int)
_sig("jaos_solve", ctypes.c_int, _VP)
_sig("jaos_status_of", ctypes.c_int, _VP)
_sig("jaos_objective", ctypes.c_int, _VP, _P(_D))
_sig("jaos_solution", ctypes.c_int, _VP, _P(_D), _P(_D), _P(_D), _P(_D))
_sig("jaos_basis", ctypes.c_int, _VP, _P(ctypes.c_int), _P(ctypes.c_int))
_sig("jaos_clear_basis", None, _VP)
_sig("jaos_work_units", _I64, _VP)
_sig("jaos_iterations", _I64, _VP)
_sig("jaos_solve_time", _D, _VP)

INFINITY = _lib.jaos_infinity()


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
        return self

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

    # -- changing it -------------------------------------------------------

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

    def clear_basis(self):
        """Asks the next solve to start cold."""
        _lib.jaos_clear_basis(self._handle())
        return self

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
