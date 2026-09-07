# SPECS — everything JAOS must have

JAOS is a linear and mixed-integer programming solver written from scratch in
C23. No dependencies, Apache 2.0, Linux/GCC. This file is the whole target:
one row per feature, with where it stands. **done**, **partial** (says what
is missing), **missing**, or **out of scope**. A row changes status when its
feature lands; nothing else changes here.

Two rules hold on every commit. The answer is bit-identical on every machine
and every run. An independent checker, sharing no code with the solver,
judges every answer against the model as loaded.

`docs/feature-matrix.md` puts these rows beside HiGHS, SoPlex, Clp, SCIP,
Gurobi and Hexaly. An empty JAOS cell there is a row here that is not done.

## 1. Problem classes

| | status | |
|---|---|---|
| Linear programming | **done** | |
| Mixed-integer linear | **partial** | missing: SOS, indicator, semi-continuous (§4) |
| Convex quadratic (QP) | **missing** | |
| Quadratically constrained, second-order cone | **missing** | |
| Mixed-integer quadratic | **missing** | |
| Nonlinear, mixed-integer nonlinear | **out of scope** | |
| Constraint programming, black-box | **out of scope** | |

## 2. LP algorithms

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing, Harris two-pass ratio test with bound flipping, phase 1 by artificial bounds, Bland fallback on a stall |
| Primal simplex | **partial** | Dantzig pricing, composite phase 1, Harris ratio test; behind a development switch. Missing: Devex pricing, caller-selectable |
| Barrier (interior point) | **missing** | needs a deterministic sparse Cholesky written here |
| Crossover from an interior point | **missing** | |
| First-order method (PDLP) | **missing** | |
| Concurrent solve, deterministic | **missing** | |
| GPU | **out of scope** | |

## 3. Preparing and handling the model

| | status | |
|---|---|---|
| Presolve | **partial** | empty rows and columns, singleton rows, cost-0 singleton columns, fixed columns, forcing and redundant rows, implied free column singletons. Missing: duplicate rows and columns, dominated columns, bound tightening, dual fixing (each measured once and refused, `bench/refusals.txt`) |
| Postsolve to the caller's indices, statuses and duals | **done** | |
| Scaling | **done** | Curtis-Reid, powers of two |
| Sparse LU, Markowitz pivoting, Forrest-Tomlin update | **done** | |
| Hyper-sparse triangular solves | **partial** | BTRAN yes. Missing: FTRAN still traverses every slot |
| Modify bounds, costs, coefficients, objective sense and constant | **done** | each reads back |
| Add and delete rows and columns | **done** | |
| Copy a model | **done** | |
| Row, column and objective names | **done** | |
| Warm start from the previous basis | **done** | |
| Read and write a basis in the MPS basis format | **done** | |
| Resume after a limit, in-process and from a file | **done** | |
| Model statistics | **done** | `jaos_model_statistics`, `jaos stats` |
| Presolve statistics | **done** | |

## 4. Mixed-integer machinery

| | status | |
|---|---|---|
| Branch and bound | **done** | best bound first, one private copy per node warm from its parent |
| Pseudocost branching | **done** | most-fractional as an option |
| Strong branching | **partial** | exists behind a switch and is off: measured worse |
| Gomory, knapsack cover and MIR cuts | **done** | at the root and to depth 3, dropped when the slack goes basic |
| Clique cuts | **missing** | |
| Flow cover, zero-half, lifted cover cuts | **missing** | lifted covers exist behind a switch and are off |
| Rounding heuristic, root dive, feasibility pump | **done** | |
| RINS, local branching, other improvement heuristics | **partial** | RINS exists behind a switch and is off |
| Solution pool | **done** | |
| MIP start and cutoff | **done** | |
| Node limit, incumbent callback | **done** | |
| Bound propagation, reduced-cost fixing | **partial** | both exist behind switches and are off |
| MIP presolve: probing, clique table, coefficient tightening | **missing** | |
| Semi-continuous variables | **missing** | |
| SOS1 and SOS2 constraints | **missing** | |
| Indicator constraints | **missing** | |
| Symmetry detection | **missing** | |
| Conflict analysis | **missing** | |
| Deterministic parallel tree search | **missing** | |

## 5. Parallelism

| | status | |
|---|---|---|
| Parallel LP solve | **missing** | |
| Parallel MIP solve | **missing** | |
| Deterministic under parallelism | **required** | the same tree and the same answer at any thread count |

## 6. Correctness and verification

| | status | |
|---|---|---|
| Bit-identical across machines | **done** | |
| Independent checker shipped with the solver | **done** | judges against the original, unscaled model |
| Exact rational proof of the final basis | **done** | reaches the bases whose numbers fit the limb budget |
| Exact rational values of a proved basis | **done** | |
| Proof file, written and checked from the model alone | **done** | optimal, infeasible and unbounded |
| Certified bound on suboptimality | **partial** | sound; alone it cannot separate a wrong vertex from a right one |
| Infeasibility and unboundedness certificates, floating and exact | **done** | |
| Irreducible infeasible subsystem | **done** | |
| The IIS written out as a model | **done** | |
| Feasibility relaxation | **done** | |
| Prove a basis another solver produced | **done** | |
| Check a point another solver produced | **done** | |
| Exact solving with no tolerances | **missing** | |

## 7. Input and output

| | status | |
|---|---|---|
| Read fixed and free MPS | **done** | `OBJNAME`, `RANGES`, all bound types, `MARKER` for integers |
| Read LP | **partial** | CPLEX-style core. Missing: the constructs `docs/format-support.md` lists as unsupported |
| Read and write gzip | **done** | inflate and deflate written here |
| Direct load from arrays | **done** | |
| Write MPS | **done** | |
| Write LP | **partial** | refuses a name LP cannot spell and a free row; `convert --positional` is the escape |
| Own solution file, written and read | **done** | |
| Point and duals files | **done** | the smallest exchange format; `check --point` |
| Read other solvers' solution files | **missing** | |
| Reject unsupported constructs with a line number | **done** | |
| `diff` and `show` commands | **done** | |
| SOS, indicator and semi-continuous in MPS and LP | **missing** | |
| Other formats (`.nl`, OSiL, QPLIB) | **missing** | |

## 8. Using it from another language

| | status | |
|---|---|---|
| C API, one header | **done** | |
| Command-line tool | **done** | `docs/cli.md` |
| Python: ctypes wrapper and modeling layer | **done** | standard library only |
| Python package installable with pip | **missing** | |
| Julia | **missing** | |
| Java, .NET | **missing** | |
| R, MATLAB | **missing** | |
| Modelling-system links (JuMP, Pyomo, AMPL, GAMS) | **missing** | |
| `make install` and pkg-config | **done** | |
| CMake package | **missing** | |
| Windows and macOS builds | **missing** | |

## 9. Controlling a solve

| | status | |
|---|---|---|
| Work limit and time limit | **done** | |
| Primal and dual tolerances | **done** | |
| Logging with levels | **done** | |
| Progress callback that can stop | **done** | |
| Choose the algorithm | **missing** | |
| Options as name-value strings, parameter file | **missing** | |
| Steering callbacks: user cuts, lazy constraints, branching | **missing** | |
| Thread count | **missing** | |
| Sensitivity and ranging | **done** | |

## 10. Licence and distribution

| | status | |
|---|---|---|
| Apache 2.0, free for commercial use, no dependencies | **done** | |

## The bars

- Netlib: 94 standard, 16 Kennington, 29 infeasible. Every answer checked,
  every objective against the Koch reference, two solves identical.
  `make netlib netlib-infeas netlib-kennington J=12`; results in
  `bench/results/`, baselines in `bench/*.baseline`.
- MIPLIB 3: 24 instances to the catalogue optimum. `make miplib`.
- Speed against HiGHS, SoPlex and Clp: `bench/compare/results/P0.txt`,
  `make compare COMPARE_ARGS='-t P0'`.
- MIPLIB 2017 easy and benchmark subsets: not started.
