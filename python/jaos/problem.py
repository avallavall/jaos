import ctypes
import ctypes.util
import enum
import os
import sys
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
from .model import (
    Model,
)

def quicksum(terms):
    """One expression from an iterable of variables, expressions and
    numbers, built in a single pass. `sum()` also works, but it builds one
    intermediate expression per term, which is quadratic in their count."""
    t, c, p, q = {}, 0.0, None, {}
    for o in terms:
        e = _as_expr(o)
        if e is None:
            raise TypeError(f"cannot sum {o!r} into a linear expression")
        p = _merge_problem(p, e._p)
        for v, k in e._t.items():
            t[v] = t.get(v, 0.0) + k
        for pair, k in e._q.items():
            q[pair] = q.get(pair, 0.0) + k
        c += e._c
    return LinExpr(t, c, p, q)

def _pair(a, b):
    """The key of the quadratic term a*b: the two variables in index
    order, so x*y and y*x are one term."""
    return (a, b) if a._i <= b._i else (b, a)

def _product(a, b):
    """The product of two expressions with no quadratic part, expanded
    into a quadratic, linear and constant part."""
    if a._q or b._q:
        raise TypeError(_NOT_QUADRATIC)
    p = _merge_problem(a._p, b._p)
    q = {}
    for u, ku in a._t.items():
        for v, kv in b._t.items():
            key = _pair(u, v)
            q[key] = q.get(key, 0.0) + ku * kv
    t = {}
    for v, k in b._t.items():
        t[v] = t.get(v, 0.0) + a._c * k
    for u, k in a._t.items():
        t[u] = t.get(u, 0.0) + b._c * k
    return LinExpr(t, a._c * b._c, p, q)

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

_NOT_QUADRATIC = ("an expression here is at most quadratic: a product of "
                  "two linear expressions, or a square; a product with a "
                  "quadratic expression, or a power other than 2, is not")

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
        return _as_expr(self) * o
    __rmul__ = __mul__

    def __pow__(self, n):
        return _as_expr(self) ** n

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
        if isinstance(o, (Var, LinExpr)):
            return _product(self, _as_expr(o))
        if not isinstance(o, (int, float)):
            return NotImplemented
        k = float(o)
        return LinExpr({v: c * k for v, c in self._t.items()},
                       self._c * k, self._p,
                       {v: c * k for v, c in self._q.items()})
    __rmul__ = __mul__

    def __pow__(self, n):
        if n != 2:
            raise TypeError(_NOT_QUADRATIC)
        return _product(self, self)

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
        lo = -d._c if lower else -INFINITY
        hi = -d._c if upper else INFINITY
        return Constraint(d._p, d._t, lo, hi,
                          {v: c for v, c in d._q.items() if c != 0.0})

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
                + sum(c * col[a._i] * col[b._i]
                      for (a, b), c in self._q.items()))

    def __repr__(self):
        parts = [f"{c:g}*{v.name}" for v, c in self._t.items()]
        parts += [f"{c:g}*{a.name}**2" if a is b
                  else f"{c:g}*{a.name}*{b.name}"
                  for (a, b), c in self._q.items()]
        if self._c or not parts:
            parts.append(f"{self._c:g}")
        return " + ".join(parts)

class Constraint:
    """One constraint. Made by comparing expressions; a row of the
    problem once Problem.add has taken it. Quadratic terms in it, squares
    or products of two variables, make it a quadratic row, which takes one
    finite side and solves by the conic interior point:
    x**2 + y**2 <= 1, or (x + y)**2 <= 4.

    Its bounds stay writable afterwards: setting `lb` or `ub` on an added
    constraint is how a right-hand side is moved between solves, and only
    the bound crosses to the C side, so the next solve resumes warm.
    """

    __slots__ = ("_p", "_t", "_lo", "_hi", "_i", "_q", "name")

    def __init__(self, problem, terms, lo, hi, quad=None):
        self._p = problem
        self._t = dict(terms)
        self._lo = float(lo)
        self._hi = float(hi)
        self._q = dict(quad) if quad else {}
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
        e = repr(LinExpr(self._t, 0.0, self._p, self._q))
        if self._lo == self._hi:
            return f"{e} == {self._hi:g}"
        if self._lo == -INFINITY and self._hi == INFINITY:
            return f"{e} free"
        if self._lo == -INFINITY:
            return f"{e} <= {self._hi:g}"
        if self._hi == INFINITY:
            return f"{e} >= {self._lo:g}"
        return f"{self._lo:g} <= {e} <= {self._hi:g}"

class Cone:
    """One second-order cone of a Problem. Made by Problem.add_cone."""

    __slots__ = ("_p", "_k", "variables", "rotated")

    def __init__(self, problem, index, variables, rotated):
        self._p = problem
        self._k = index
        self.variables = list(variables)
        self.rotated = bool(rotated)

    @property
    def dual(self):
        """The cone's dual vector in the held solution, one value per
        member."""
        self._p._solution()
        return self._p._m.cone_dual(self._k)

    def __repr__(self):
        names = ", ".join(v.name for v in self.variables)
        return f"{'rotated ' if self.rotated else ''}cone({names})"

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
        self._cones = []
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
        self._dirty_qpairs = False
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
                                   float(lo) - e._c, float(hi) - e._c,
                                   e._q), name)

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
        newq = {(a, b): (2.0 if a is b else 1.0) * float(c)
                for (a, b), c in e._q.items() if c != 0.0}
        self._sol = None
        if self._loaded and not self._structural:

            if sense is not self._sense or float(e._c) != self._obj_c:
                self._dirty_objective = True
            for v in set(self._obj) | set(new):
                if self._obj.get(v, 0.0) != new.get(v, 0.0):
                    self._dirty_costs.add(v._i)
            for key in set(self._objq) | set(newq):
                if self._objq.get(key, 0.0) != newq.get(key, 0.0):
                    self._dirty_quad.add(key[0]._i)
                    if key[0] is not key[1]:
                        self._dirty_qpairs = True
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

    def add_cone(self, variables, rotated=False):
        """A second-order cone over `variables`, in order. By default
        (t, x1, x2, ...) holds t >= ||x||. With rotated=True,
        (u, v, x1, ...) holds 2 u v >= ||x||**2 with u, v >= 0. The
        problem then solves by the conic interior point. Returns the
        Cone, whose `dual` reads the cone's dual after a solve."""
        variables = list(variables)
        for v in variables:
            if not isinstance(v, Var) or v._p is not self:
                raise ValueError(f"{v!r} is not a variable of this Problem")
        c = Cone(self, len(self._cones), variables, rotated)
        self._cones.append(c)
        self._touch_structure()
        return c

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
        self._load_objective_q()
        for t, vs, ws in self._sos:
            self._m.add_sos(t, [v._i for v in vs], ws)
        for c, z, v in self._ind:
            self._m.set_row_indicator(c._i, z._i, v)
        for c in self._cons:
            self._m.set_row_name(c._i, c.name)
            if c._q:
                self._m.set_row_quadratic(
                    c._i, [(a._i, b._i, (2.0 if a is b else 1.0) * k)
                           for (a, b), k in c._q.items()])
        for c in self._cones:
            self._m.add_cone(
                ConeType.ROTATED if c.rotated else ConeType.QUADRATIC,
                [v._i for v in c.variables])
        self._dirty_var_bounds.clear()
        self._dirty_costs.clear()
        self._dirty_row_bounds.clear()
        self._dirty_objective = False
        self._structural = False
        self._loaded = True

    def _load_objective_q(self):
        """Puts the objective's quadratic part on the model: column by
        column while it holds only squares, as one Q once it holds a
        product of two variables."""
        if any(a is not b for a, b in self._objq):
            self._m.set_quadratic([(a._i, b._i, q)
                                   for (a, b), q in self._objq.items()])
        else:
            self._m.set_quadratic([])
            for (v, _), q in self._objq.items():
                self._m.set_col_quadratic(v._i, q)
        self._dirty_quad.clear()
        self._dirty_qpairs = False

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
            if self._dirty_qpairs:
                self._load_objective_q()
            else:
                for i in self._dirty_quad:
                    v = self._vars[i]
                    self._m.set_col_quadratic(i, self._objq.get((v, v), 0.0))
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
        if self._cones:
            return self._m.check_conic_solution(
                s.col_value, s.row_dual,
                [c.dual for c in self._cones], tol)
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

    def set_mip_local_branching(self, size):
        """Binaries a small tree may flip around each new incumbent; 0 is
        off and the default, negative restores it."""
        self._m.set_mip_local_branching(size)
        return self

    def set_mip_node_select(self, rule):
        """Which open node the tree takes next: 0 the lowest bound, 1 the
        lowest pseudocost estimate, the default; negative restores it."""
        self._m.set_mip_node_select(rule)
        return self

    def set_mip_restart(self, on):
        """Restart the tree from the root when the root incumbent's reduced
        costs fix a fifth of the integer columns; off by default."""
        self._m.set_mip_restart(on)
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

    def set_mip_tree_batch(self, nodes):
        self._m.set_mip_tree_batch(nodes)
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
        of the problem), `event.hand(point)` handing the tree a dict
        from every variable to its value as a candidate incumbent,
        `event.branch_on(var)` naming the column to branch on, and
        `event.branch_var` the solver's own choice or None."""
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

            def hand(self, point):
                values = []
                for v in problem._vars:
                    if v not in point:
                        raise ValueError(f"hand() wants a value for every "
                                         f"variable, {v.name} has none")
                    values.append(point[v])
                self._ev.add_solution(values)

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

    def has_integer(self):
        """Whether solve() runs the branch and bound. Loads the problem
        first if it changed, the way statistics does."""
        if self._pending():
            self._build_and_load()
        return self._m.has_integer()

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

    def write_cbf(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_cbf(path)
        return self

    def write_osil(self, path):
        if self._pending():
            self._build_and_load()
        self._m.write_osil(path)
        return self

    def write_solution(self, path):
        self._m.write_solution(path)

    def write_sol_ampl(self, path, message=None):
        """The last solve as an AMPL .sol file; see Model.write_sol_ampl."""
        self._m.write_sol_ampl(path, message)

    def read_solution(self, path):
        """Reads back a file write_solution wrote; see Model.read_solution."""
        return self._m.read_solution(path)

    def read_cone_duals(self, path):
        """The cone records of a file write_solution wrote, one list per
        cone in the order add_cone made them; see Model.read_cone_duals."""
        if self._pending():
            self._build_and_load()
        return self._m.read_cone_duals(path)

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
