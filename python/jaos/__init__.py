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
3. the library under the current directory's build directories:
   ``build/release`` then ``build/cmake`` on Linux and macOS, and
   ``build/cmake``, ``build/cmake/Release`` then ``build/release`` on
   Windows;
4. the system loader's search path.

The file name is ``libjaos.so`` on Linux, ``libjaos.dylib`` then
``libjaos.so`` on macOS, and ``jaos.dll`` then ``libjaos.dll`` on Windows.

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
    LibraryNotFound,
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
    build_commit,
    library_path,
    version,
)
from .model import (
    Model,
)
from .problem import (
    Cone,
    Constraint,
    LinExpr,
    Problem,
    Var,
    _NOT_LINEAR,
    _NOT_QUADRATIC,
    _as_expr,
    _merge_problem,
    quicksum,
)

__all__ = [
    "Model", "JaosError", "Status", "SolveStatus", "ObjSense", "LogLevel",
    "BasisStatus", "CallbackAction", "Solution", "Basis", "INFINITY",
    "NodeEvent",
    "NAME_MAX", "version", "build_commit", "library_path",
    "LibraryNotFound", "GapRule",
    "Problem", "Var", "LinExpr", "Constraint", "quicksum",
    "CheckReport", "CertificateReport", "RayReport", "Progress",
    "IISSide", "IISReport", "IIS",
    "RelaxScope", "RelaxReport", "Relaxation",
    "Proof", "ProofStage", "VerifyReport", "ExactRayReport",
    "MipReport", "ConeType", "Cone",
]
