import ctypes
import ctypes.util
import enum
import math
import os
import sys
import threading
from collections import namedtuple
from fractions import Fraction

from ._native import (
    Algorithm,
    Basis,
    BasisStatus,
    BoundRanging,
    Branching,
    CallbackAction,
    CertificateReport,
    CheckReport,
    ConeType,
    CostRanging,
    DiveChild,
    ExactRayReport,
    GapRule,
    IIS,
    IISReport,
    IISSide,
    INFINITY,
    Incumbent,
    JaosError,
    LogLevel,
    MipReport,
    ModelStats,
    NAME_MAX,
    NodeEvent,
    ObjSense,
    PresolveReport,
    Progress,
    Proof,
    ProofKind,
    ProofReport,
    ProofStage,
    RayReport,
    RelaxReport,
    RelaxScope,
    Relaxation,
    Solution,
    SolveStatus,
    Status,
    VerifyReport,
    _CS,
    _CertificateReport,
    _CheckReport,
    _D,
    _ExactRayReport,
    _I64,
    _IISReport,
    _INCUMBENT_FN,
    _Incumbent,
    _LIB_PATH,
    _LOG_FN,
    _MipReport,
    _ModelStats,
    _NODE_FN,
    _Node,
    _P,
    _PROGRESS_FN,
    _PresolveReport,
    _Progress,
    _ProofReport,
    _RayReport,
    _RelaxReport,
    _VP,
    _VerifyReport,
    _doubles,
    _find_library,
    _int64s,
    _lib,
    _library_names,
    _path,
    _sig,
    library_path,
    version,
)

_solving = threading.local()

_prior_unraisablehook = None

def _catch_unraisable(unraisable):
    """Keeps an exception that ctypes could not hand back from a callback,
    for the solve running on this thread to raise once it returns. The
    exception a signal handler raises as a callback starts is one: it
    comes before the callback's own try."""
    held = getattr(_solving, "held", None)
    if (held is not None and unraisable.exc_value is not None
            and "ctypes callback" in (unraisable.err_msg or "")):
        if held[0] is None:
            held[0] = unraisable.exc_value
        return
    _prior_unraisablehook(unraisable)

def _watch_unraisable():
    global _prior_unraisablehook
    if sys.unraisablehook is not _catch_unraisable:
        _prior_unraisablehook = sys.unraisablehook
        sys.unraisablehook = _catch_unraisable

def _hold(held, exc):
    if held[0] is None:
        held[0] = exc

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
        self._held = [None]

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
        """Reads a QPLIB file: a linear or quadratic objective, linear
        or quadratic rows, bounds, integer columns and the names. gzip
        is accepted here too."""
        self._check(_lib.jaos_read_qplib(self._handle(), _path(path)))
        return self

    def write_qplib(self, path):
        """Writes the model in the QPLIB format; SOS sets, semi-continuous
        columns, indicator rows and cones are refused, write MPS for
        those."""
        self._check(_lib.jaos_write_qplib(self._handle(), _path(path)))
        return self

    def read_cbf(self, path):
        """Reads a Conic Benchmark Format file (versions 1 to 3): variables
        and constraints in the F, L+, L-, L=, Q and QR cones, integer marks
        and the objective. A constraint block in Q or QR becomes new free
        columns equal to its affine expressions, in a cone, unless each of
        its rows picks one variable. Semidefinite, exponential and power
        cones are refused by line; a CHANGE ends the reading. gzip is
        accepted here too."""
        self._check(_lib.jaos_read_cbf(self._handle(), _path(path)))
        return self

    def write_cbf(self, path):
        """Writes the model in the Conic Benchmark Format, version 3. The
        names are not carried; a bound CBF's variable cones cannot hold
        becomes a constraint row. A quadratic objective, quadratic rows,
        SOS sets, semi-continuous columns and indicator rows are refused;
        write MPS for those."""
        self._check(_lib.jaos_write_cbf(self._handle(), _path(path)))
        return self

    def read_osil(self, path):
        """Reads an OSiL XML file: variables with bounds and types, one
        objective with its constant and its quadratic terms, constraints
        with theirs, and the matrix in either the column-wise or the
        row-wise layout. A nonlinear block and a second objective are
        refused by line. gzip is accepted here too."""
        self._check(_lib.jaos_read_osil(self._handle(), _path(path)))
        return self

    def write_osil(self, path):
        """Writes the model as OSiL XML: variables with bounds and types,
        one objective and the rows with their quadratic terms, and the
        column-wise matrix. Cones are refused; write MPS for those."""
        self._check(_lib.jaos_write_osil(self._handle(), _path(path)))
        return self

    def write_solution(self, path):
        self._check(_lib.jaos_write_solution(self._handle(), _path(path)))

    def write_sol_ampl(self, path, message=None):
        """Writes the last solve as the .sol file an AMPL solver hands
        back: the message (by default the status and the objective), the
        options of the .nl file the model was read from, the row duals of
        a continuous optimum and the column values, then the objno line
        with AMPL's result code (0 solved, 200 infeasible, 300
        unbounded, 400 or 401 a limit with or without a point, 500
        failure)."""
        msg = None if message is None else str(message).encode("utf-8")
        self._check(_lib.jaos_write_sol_ampl(self._handle(), _path(path),
                                             msg))

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

    def read_cone_duals(self, path):
        """The cone records of a solution file, one list per cone: an
        optimum's cone duals, or the cone part of an INFEASIBLE
        certificate."""
        sizes = [len(self.cone(k)[1]) for k in range(self.num_cones())]
        z = (_D * max(sum(sizes), 1))()
        self._check(_lib.jaos_read_cone_duals(self._handle(), _path(path),
                                              z))
        out, at = [], 0
        for n in sizes:
            out.append(list(z[at:at + n]))
            at += n
        return out

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

    def set_quadratic(self, entries):
        """The whole quadratic objective at once, as (row, col, value)
        triples: the objective becomes c'x + 1/2 x'Qx. Q is symmetric, so
        each off-diagonal pair is given once, as (i, j) or as (j, i), and
        a pair given twice raises. An empty list clears it. A pair needs
        the barrier, which takes it on the augmented system; the two
        simplexes, PDLP, the concurrent solve and branch and bound refuse
        a quadratic objective as they always did."""
        entries = list(entries)
        n = len(entries)
        rows, _ = _int64s([int(e[0]) for e in entries], "rows", n)
        cols, _ = _int64s([int(e[1]) for e in entries], "cols", n)
        vals, _ = _doubles([float(e[2]) for e in entries], "values", n)
        self._check(_lib.jaos_set_quadratic(self._handle(), n, rows, cols,
                                            vals))
        return self

    def quadratic_nz(self):
        """How many entries the lower triangle of Q holds, the diagonal
        included."""
        return _lib.jaos_quadratic_nz(self._handle())

    def quadratic(self):
        """Q back as (row, col, value) triples, the lower triangle with
        the diagonal, in column order."""
        n = self.quadratic_nz()
        if n == 0:
            return []
        rows = (_I64 * n)()
        cols = (_I64 * n)()
        vals = (_D * n)()
        self._check(_lib.jaos_quadratic(self._handle(), rows, cols, vals))
        return [(rows[k], cols[k], vals[k]) for k in range(n)]

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

    def add_cone(self, cone_type, cols):
        """A second-order cone over `cols`, in order. QUADRATIC over
        (t, x1, ...) holds t >= ||x||; ROTATED over (u, v, x1, ...) holds
        2 u v >= ||x||**2 with u, v >= 0. A model with a cone solves by
        the conic interior point."""
        cols = [int(c) for c in cols]
        n = len(cols)
        ca = (_I64 * max(n, 1))(*cols)
        self._check(_lib.jaos_add_cone(self._handle(), int(cone_type), n, ca))
        return self

    def num_cones(self):
        return int(_lib.jaos_num_cones(self._handle()))

    def cone(self, k):
        """(ConeType, columns) of cone `k`."""
        t = ctypes.c_int()
        n = _I64()
        self._check(_lib.jaos_cone(self._handle(), int(k), ctypes.byref(t),
                                   ctypes.byref(n), None))
        ca = (_I64 * max(n.value, 1))()
        self._check(_lib.jaos_cone(self._handle(), int(k), None, None, ca))
        return ConeType(t.value), list(ca)[:n.value]

    def delete_cones(self, cones):
        cones = [int(k) for k in cones]
        ka = (_I64 * max(len(cones), 1))(*cones)
        self._check(_lib.jaos_delete_cones(self._handle(), len(cones), ka))
        return self

    def set_row_quadratic(self, row, entries):
        """The quadratic part of row `row`, as (row, col, value) triples
        over columns: the row's activity becomes a'x + 1/2 x'Qx. Each
        off-diagonal pair is given once. An empty list clears it. A row
        with a quadratic part has one finite side, and Q must be convex
        on that side."""
        entries = list(entries)
        n = len(entries)
        rows, _ = _int64s([int(e[0]) for e in entries], "rows", n)
        cols, _ = _int64s([int(e[1]) for e in entries], "cols", n)
        vals, _ = _doubles([float(e[2]) for e in entries], "values", n)
        self._check(_lib.jaos_set_row_quadratic(self._handle(), int(row), n,
                                                rows, cols, vals))
        return self

    def row_quadratic_nz(self, row):
        return int(_lib.jaos_row_quadratic_nz(self._handle(), int(row)))

    def row_quadratic(self, row):
        """Row `row`'s Q back as (row, col, value) triples, the lower
        triangle with the diagonal, in column order."""
        n = self.row_quadratic_nz(row)
        if n == 0:
            return []
        rows = (_I64 * n)()
        cols = (_I64 * n)()
        vals = (_D * n)()
        self._check(_lib.jaos_row_quadratic(self._handle(), int(row), rows,
                                            cols, vals))
        return [(rows[k], cols[k], vals[k]) for k in range(n)]

    def cone_dual(self, k):
        """The dual vector of cone `k` after an OPTIMAL conic solve, one
        value per member. After INFEASIBLE it is the cone's part of the
        certificate."""
        _, cols = self.cone(k)
        z = (_D * max(len(cols), 1))()
        self._check(_lib.jaos_cone_dual(self._handle(), int(k), z))
        return list(z[:len(cols)])

    def _cone_vector(self, parts, name):
        """One flat array from one list per cone, checked against the
        cones' sizes."""
        nk = self.num_cones()
        parts = list(parts)
        if len(parts) != nk:
            raise ValueError(f"{name} has {len(parts)} cones, expected {nk}")
        flat = []
        for k, part in enumerate(parts):
            part = [float(v) for v in part]
            size = len(self.cone(k)[1])
            if len(part) != size:
                raise ValueError(f"{name}[{k}] has {len(part)} entries, "
                                 f"expected {size}")
            flat.extend(part)
        return (_D * max(len(flat), 1))(*flat)

    def set_mip_gap(self, gap):
        """The gap that closes a branch and bound, 1e-6 by default. 0 means
        zero: the search ends only when no open node's bound beats the
        incumbent. `set_mip_gap_rule` says what the gap is measured
        against."""
        self._check(_lib.jaos_set_mip_gap(self._handle(), float(gap)))

    def set_mip_gap_rule(self, rule):
        """A `GapRule`. SHIFTED, the default, closes the search when no open
        node's bound beats the incumbent by more than
        gap * (1 + |incumbent|); RELATIVE by more than gap * |incumbent|,
        SCIP's and HiGHS's |incumbent - bound| / |incumbent|."""
        self._check(_lib.jaos_set_mip_gap_rule(self._handle(),
                                               int(GapRule(rule))))
        return self

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

    def set_mip_local_branching(self, size):
        """Binaries a small tree may flip around each new incumbent.

        The tree solves the model with one more row, which keeps the
        binaries within ``size`` flips of the incumbent, and takes any
        better point it finds. 0, the default, is off; a negative value
        restores it.
        """
        self._check(_lib.jaos_set_mip_local_branching(self._handle(),
                                                      int(size)))

    def set_mip_node_select(self, rule):
        """Which open node the tree takes next.

        0 is the lowest bound; 1, the default, is the lowest pseudocost
        estimate, with the lowest bound every fifth pick. A negative
        value restores the default.
        """
        self._check(_lib.jaos_set_mip_node_select(self._handle(),
                                                  int(rule)))

    def set_mip_restart(self, on):
        """Start the tree again from the root when the root incumbent's
        reduced costs fix a fifth of the integer columns.

        Off by default; a negative value restores the default.
        """
        self._check(_lib.jaos_set_mip_restart(self._handle(),
                                              -1 if on is None or on < 0
                                              else int(bool(on))))

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

    def set_mip_tree_batch(self, nodes):
        """How many open nodes a branch and bound takes in one round,
        solved on up to `set_threads` threads; 1, the default, is one node
        at a time. Above 1 the search changes, and the thread count still
        does not change the answer."""
        self._check(_lib.jaos_set_mip_tree_batch(self._handle(), int(nodes)))

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

        held = self._held

        def trampoline(p, _user):
            if held[0] is not None:
                return int(CallbackAction.STOP)
            try:
                c = p.contents
                vals = [c.col_value[i] for i in range(c.num_col)]
                r = fn(Incumbent(c.node, c.objective, c.bound, vals,
                                 bool(c.by_rounding)))
                return int(CallbackAction.CONTINUE if r is None else r)
            except BaseException as e:
                _hold(held, e)
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

        held = self._held

        def trampoline(p, _user):
            if held[0] is not None:
                return int(CallbackAction.STOP)
            ev = None
            try:
                ev = NodeEvent(p.contents)
                r = fn(ev)
                b = ev.branch_col
                p.contents.branch_col = -1 if b is None else int(b)
                return int(CallbackAction.CONTINUE if r is None else r)
            except BaseException as e:
                _hold(held, e)
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
        on, the round count, and how many of each family fired. A column
        a forcing row fixes counts as a fixed column, so the columns left
        are the columns loaded less every column count, and the rows left
        are the rows loaded less every row count, a free column singleton
        and an implied free column taking one of each. All zero before a
        solve, all zero when presolve hands the model back whole, and all
        zero under a build with presolve compiled out, which is what it
        did."""
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

    def has_integer(self):
        """Whether a solve runs the branch and bound: an integer column, an
        SOS set, or a semi-continuous column whose lower bound is above
        zero. The one rule the tree reads, so it can differ from
        `statistics()`, which counts a semi-continuous column by its mark
        alone."""
        return bool(_lib.jaos_model_has_integer(self._handle()))

    def set_mip_start(self, col_value):
        """Hand the tree an integer point before it runs, or None to clear.

        `col_value` is a sequence by column index or a dict from column
        index to value. A column the start does not give (a None or NaN
        entry, a key the dict lacks, or an index past the sequence's end)
        is left open: at the root a small tree with the given integer
        columns fixed completes the point. The library checks the point
        and runs without it when it is not a feasible integer point, so a
        wrong point is never published as an answer;
        `mip_report().start_accepted` says whether it was taken.
        """
        if col_value is None:
            self._check(_lib.jaos_set_mip_start(self._handle(), None))
            return self
        nc = self.num_col
        vals = [math.nan] * nc
        items = (col_value.items() if isinstance(col_value, dict)
                 else enumerate(col_value[:nc]))
        for j, v in items:
            if not 0 <= int(j) < nc:
                raise IndexError(f"column {j} is outside 0..{nc - 1}")
            vals[int(j)] = math.nan if v is None else float(v)
        buf = (_D * max(nc, 1))(*vals)
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
        m._log_cb = self._log_cb
        m._progress_cb = self._progress_cb
        m._incumbent_cb = self._incumbent_cb
        m._node_cb = getattr(self, "_node_cb", None)
        m._held = self._held
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
        """The thread count, 1 by default; 0 takes every core the machine
        has. Four things use more than one: Algorithm.CONCURRENT, the
        barrier's Cholesky factor, and the rounds of nodes of both
        branch and bounds when `set_mip_tree_batch` is above 1. The
        answer and the work units are the same at any count; the wall
        clock is not. A negative count raises."""
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

        held = self._held

        def trampoline(_user, lvl, line):
            if held[0] is not None:
                return
            try:
                fn(LogLevel(lvl), line.decode("utf-8", "replace"))
            except BaseException as e:
                _hold(held, e)

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

        An LP calls every 64 iterations, from iteration 0, with the work
        so far and the best total primal infeasibility seen so far, which
        is infinite while the dual's phase 1 runs. A MIP calls from every
        relaxation it solves, with the tree's running totals of iterations
        and work, so the numbers never go back within one solve. A branch
        and bound also calls once per node it solves, and once more when
        it ends. `nodes` counts the nodes so far, `bound` is the best
        bound in the model's own sense (minus infinity when minimising
        and nothing is known yet, and always outside a branch and bound),
        and `incumbent` is the best objective found, or None.

        An exception in `fn`, KeyboardInterrupt included, stops the solve,
        and solve() raises it once the C call has returned. So does an
        exception a signal handler raises while the solve runs.
        """
        if fn is None:
            self._progress_cb = None
            self._check(_lib.jaos_set_progress_callback(
                self._handle(), ctypes.cast(None, _PROGRESS_FN), None))
            return self

        held = self._held

        def trampoline(p, _user):
            if held[0] is not None:
                return int(CallbackAction.STOP)
            try:
                c = p.contents
                r = fn(Progress(c.iterations, c.work_units,
                                c.primal_infeasibility, c.nodes, c.bound,
                                c.incumbent if c.has_incumbent else None))
                return int(CallbackAction.CONTINUE if r is None else r)
            except BaseException as e:
                _hold(held, e)
                return int(CallbackAction.STOP)

        self._progress_cb = _PROGRESS_FN(trampoline)
        self._check(_lib.jaos_set_progress_callback(self._handle(),
                                                    self._progress_cb, None))
        return self

    def solve(self):
        """Runs the solve. Returns the outcome.

        A return of OK from the C call means the solve ran, not that it
        found an optimum, so the outcome is what comes back here.

        An exception raised inside a callback, or by a signal handler while
        the solve runs (Ctrl-C's KeyboardInterrupt, a task queue's soft
        time limit), stops the solve and is raised here once the C call
        has returned; the model then holds the INTERRUPTED solve. On the
        main thread with no progress callback of its own, the model sets
        one that only stops, so that a signal handler gets to run every 64
        iterations and at every node.
        """
        self._run(_lib.jaos_solve, self._handle())
        return self.status

    def _run(self, fn, *args):
        held = self._held
        held[0] = None
        _watch_unraisable()
        prior = getattr(_solving, "held", None)
        _solving.held = held
        guard = None
        if (self._progress_cb is None and
                threading.current_thread() is threading.main_thread()):
            def stop_if_held(_p, _user):
                return int(CallbackAction.STOP if held[0] is not None
                           else CallbackAction.CONTINUE)
            guard = _PROGRESS_FN(stop_if_held)
            self._guard_cb = guard
            self._check(_lib.jaos_set_progress_callback(self._handle(),
                                                        guard, None))
        try:
            rc = fn(*args)
        finally:
            if guard is not None:
                _lib.jaos_set_progress_callback(
                    self._handle(), ctypes.cast(None, _PROGRESS_FN), None)
            _solving.held = prior
        exc, held[0] = held[0], None
        if exc is not None:
            raise exc
        self._check(rc)

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
        bounds are its proof, and a MIP has one only when its relaxation
        is infeasible, since integrality alone leaves no ray
        (jaos_certificate in jaos.h)."""
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

    def check_conic_solution(self, col_value, row_dual, cone_dual, tol=1e-7):
        """check_solution for a model with cones: `cone_dual` is one list
        per cone, as cone_dual(k) returns them. Pass row_dual=None to
        check the primal side only."""
        cv, _ = _doubles(col_value, "col_value", self.num_col)
        rd = None
        if row_dual is not None:
            rd, _ = _doubles(row_dual, "row_dual", self.num_row)
        cz = self._cone_vector(cone_dual, "cone_dual")
        rep = _CheckReport()
        self._check(_lib.jaos_check_conic_solution(
            self._handle(), cv, rd, cz, float(tol), ctypes.byref(rep)))
        return CheckReport(*(getattr(rep, f)
                             for f, _ in _CheckReport._fields_))

    def check_conic_certificate(self, row_ray, cone_ray, tol=1e-7):
        """check_certificate for a model with cones: `cone_ray` is one
        list per cone, as cone_dual(k) returns them after INFEASIBLE."""
        y, _ = _doubles(row_ray, "row_ray", self.num_row)
        cz = self._cone_vector(cone_ray, "cone_ray")
        rep = _CertificateReport()
        self._check(_lib.jaos_check_conic_certificate(
            self._handle(), y, cz, float(tol), ctypes.byref(rep)))
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
        self._run(_lib.jaos_iis, self._handle(), rs, cs, ctypes.byref(rep))
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
        self._run(_lib.jaos_feasrelax, self._handle(), int(scope), rm, cm,
                  ctypes.byref(rep))
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
