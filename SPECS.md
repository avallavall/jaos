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
| Mixed-integer linear | **done** | integer, binary and semi-continuous columns, SOS1/SOS2, indicator constraints |
| Convex quadratic (QP) | **missing** | |
| Quadratically constrained, second-order cone | **missing** | |
| Mixed-integer quadratic | **missing** | |
| Nonlinear, mixed-integer nonlinear | **out of scope** | |
| Constraint programming, black-box | **out of scope** | |

## 2. LP algorithms

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing, Harris two-pass ratio test with bound flipping, phase 1 by artificial bounds, Bland fallback on a stall |
| Primal simplex | **partial** | Devex pricing (Dantzig behind `cfg.primal_dantzig`), composite phase 1, Harris ratio test; `jaos_set_algorithm` selects it. Missing: 9 of the 94 standard instances overrun or disagree with the dual (`bench/results/primal.txt`): four stall in phase 1 (d6cube, degen3, dfl001, maros-r7), three run phase 2 past 10x the dual's work (bnl2, fit1d, fit2d, scsd8), pilot87's phase 1 ends in a numerical error |
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
| Hyper-sparse triangular solves | **done** | both directions, FTRAN behind a density prediction per kind of vector; closed on instruction counts |
| Modify bounds, costs, coefficients, objective sense and constant | **done** | each reads back |
| Add and delete rows and columns | **done** | |
| Copy a model | **done** | |
| Row, column and objective names | **done** | |
| Warm start from the previous basis | **done** | |
| Read and write a basis in the MPS basis format | **done** | |
| Resume after a limit, in-process and from a file | **done** | |
| Model statistics | **done** | `jaos_model_statistics`, `jaos stats`; sizes, row and column kinds, integer, binary and semi-continuous counts, SOS sets, indicator rows, magnitudes |
| Presolve statistics | **done** | |

## 4. Mixed-integer machinery

| | status | |
|---|---|---|
| Branch and bound | **done** | best bound first, one private copy per node warm from its parent |
| Pseudocost branching | **done** | most-fractional as an option |
| Strong branching | **partial** | exists behind a switch and is off: measured worse |
| Gomory, knapsack cover and MIR cuts | **done** | at the root and to depth 3, dropped when the slack goes basic |
| Clique cuts | **done** | four rounds at the root by default, from the conflicts each all-binary row puts between literals, grown greedily in a fixed order; 0.9697x the work over the MIP set, `gen` 0.62x and `p0282` 0.69x, none past 2x. `jaos_set_mip_clique_rounds`, `--clique-rounds`, `mip_clique_rounds` |
| Flow cover, zero-half, lifted cover cuts | **missing** | lifted covers exist behind a switch and are off |
| Rounding heuristic, root dive, feasibility pump | **done** | |
| RINS, local branching, other improvement heuristics | **partial** | RINS exists behind a switch and is off |
| Solution pool | **done** | |
| MIP start and cutoff | **done** | |
| Node limit, incumbent callback | **done** | |
| Bound propagation, reduced-cost fixing | **partial** | both exist behind switches and are off |
| MIP presolve: probing, clique table, coefficient tightening | **missing** | |
| Semi-continuous variables | **done** | `jaos_set_col_semicontinuous`; MPS `SC` and `SI`, LP `Semi-continuous`, both writers; the tree relaxes the floor to zero and branches on the zero side; the checker accepts zero |
| SOS1 and SOS2 constraints | **done** | `jaos_add_sos`, `jaos_num_sos`, `jaos_sos`; MPS `SOS` section and LP `SOS` section, both writers; the tree branches on the weighted split and the checker counts the excess nonzeros as an integrality violation; Python `add_sos` at both layers |
| Indicator constraints | **done** | `jaos_set_row_indicator`, `jaos_row_indicator`: a row that holds only while a binary column equals 0 or 1. The tree keeps the row free until the column is fixed by branching and branches on the column when the point breaks it; the checker ignores an inactive row. LP `z = 1 ->`, MPS `INDICATORS`, both writers, Python `add_indicator` |
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
| Read LP | **partial** | CPLEX-style linear dialect: objective, constraints, ranges, bounds, General, Binary, Semi-continuous, SOS, indicators, Lazy Constraints, User Cuts. Missing: quadratic terms, which are QP |
| Read and write gzip | **done** | inflate and deflate written here |
| Direct load from arrays | **done** | |
| Write MPS | **done** | |
| Write LP | **partial** | refuses a name LP cannot spell and a free row; `convert --positional` is the escape |
| Own solution file, written and read | **done** | |
| Point and duals files | **done** | the smallest exchange format; `check --point` |
| Read other solvers' solution files | **done** | `jaos_read_point` and `jaos_read_duals` detect and read Gurobi, MIPLIB, SCIP, HiGHS and CPLEX XML solution files, so `jaos check --point` judges them |
| Reject unsupported constructs with a line number | **done** | |
| `diff` and `show` commands | **done** | |
| Indicator constraints in MPS and LP | **done** | |
| Other formats (`.nl`, OSiL, QPLIB) | **missing** | |

## 8. Using it from another language

| | status | |
|---|---|---|
| C API, one header | **done** | |
| Command-line tool | **done** | `docs/cli.md` |
| Python: ctypes wrapper and modeling layer | **done** | standard library only |
| Python package installable with pip | **done** | `pyproject.toml` and `setup.py` build `libjaos.so` and install the `jaos` package; `python -m jaos solve FILE` |
| Julia | **missing** | |
| Java, .NET | **missing** | |
| R, MATLAB | **missing** | |
| Modelling-system links (JuMP, Pyomo, AMPL, GAMS) | **missing** | |
| `make install` and pkg-config | **done** | |
| CMake package | **done** | `CMakeLists.txt` beside the Makefile: archive, shared library, tool, `ctest`, install with `jaosConfig.cmake` and `jaos.pc`; `tests/cmake.sh` checks a `find_package` consumer |
| Windows and macOS builds | **partial** | the POSIX calls sit behind `src/jaos_sys.h`; Windows builds with mingw-w64, and `tests/windows.sh` runs `jaos.exe` under wine and requires the Linux build's answers byte for byte. Missing: a native Windows run, clang-cl, macOS |

## 9. Controlling a solve

| | status | |
|---|---|---|
| Work limit and time limit | **done** | |
| Primal and dual tolerances | **done** | |
| Logging with levels | **done** | |
| Progress callback that can stop | **done** | |
| Choose the algorithm | **done** | `jaos_set_algorithm`, `jaos_algorithm_of`, `--algorithm dual|primal`, Python `set_algorithm` |
| Options as name-value strings, parameter file | **done** | `jaos_set_option`, `jaos_get_option`, `jaos_read_options`, `jaos_num_options`, `jaos_option_name`; 42 options; `jaos solve --opt NAME=VALUE`, `--params FILE`; Python `set_option`, `get_option`, `read_options`, `Model.option_names()`; `jaos options` prints them all in the shape `--params` reads |
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
