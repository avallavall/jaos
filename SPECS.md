# SPECS — everything JAOS must have

JAOS is a solver for linear, mixed-integer, convex quadratic and conic
programs, written from scratch in C23. No dependencies, Apache 2.0,
Linux and GCC, with CI builds on Windows (clang-cl) and macOS (GCC 14).
This file is the whole target:
one row per feature, with where it stands. **done**, **partial** (says what
is missing), **missing**, or **out of scope**. A row changes status when its
feature lands; nothing else changes here. The readings behind a row are in
the `bench/measurements/` folders it names.

Two rules hold on every commit. The answer is bit-identical on every machine
and every run. An independent checker, sharing no algorithm with the
solver, judges every answer against the model as loaded.

`docs/feature-matrix.md` puts the rows the field compares on beside HiGHS,
SoPlex, Clp, SCIP, Gurobi and Hexaly. The rows about JAOS's own tooling
(copy a model, names, statistics, presolve statistics, whether the model is
a MIP, `diff` and `show`) have no counterpart there. A JAOS cell at ◐ or ○
there is a row here that is not done.

## 1. Problem classes

| | status | |
|---|---|---|
| Linear programming | **done** | `min` or `max c'x + offset` over `rl <= Ax <= ru` and `cl <= x <= cu`, loaded from arrays, built by rows and columns, or read from MPS, LP, `.nl`, OSiL, QPLIB or CBF. Solved by the dual simplex by default, or by the primal, the barrier, PDLP or the concurrent solve. The answer is a point, row activities, duals, reduced costs and a basis, or a Farkas ray or an unbounded direction. The Netlib, Kennington and infeasible gates read it (`bench/results/`) |
| Mixed-integer linear | **done** | integer, binary and semi-continuous columns, SOS1/SOS2, indicator constraints |
| Convex quadratic (QP) | **partial** | `c'x + ½ x'Qx` with a full `Q` (`jaos_set_col_quadratic`, `jaos_set_quadratic`), read and written in MPS, LP, QPLIB, OSiL and `.nl` (degree two); convexity is checked by a quasi-definite LDL and an indefinite `Q` is refused by name. Solved by the barrier; the push (`qp_push`) sets the active variables on their bounds and solves the equality-constrained QP on the rest, a push that does not settle is tried once more from the variables the barrier reads at a bound, and a refused barrier point is polished on its active set before it is given up. A QP the barrier cannot finish goes to the conic interior point. An unbounded QP ends `UNBOUNDED` with a ray the checker confirms. Every answer is put to the checker before it is called optimal (`bench/measurements/02-248/`, `02-318/`, `02-365/`, `02-366/`, `02-367/`). Missing: 7 of QPLIB's 8 largest convex QPs, which reach 1e11 work units; `values` of Maros-Meszaros, refused as not convex |
| Quadratically constrained, second-order cone | **partial** | cones over columns (`jaos_add_cone`, quadratic and rotated) and convex quadratic rows with one finite side (`jaos_set_row_quadratic`); MPS `QCMATRIX`/`QSECTION`/`CSECTION`, LP, QPLIB, OSiL and CBF read and write them where the format can. Solved by the conic interior point (`src/conic.c`): the homogeneous self-dual embedding with Nesterov-Todd scaling, then a Newton finish on the active constraints and a settle of a refused optimum on its active rows. Infeasibility and unboundedness are published only with a certificate or ray the checker confirms. Integer columns, SOS sets, semi-continuous columns and indicator rows beside cones go through the conic branch and bound (`src/conictree.c`). `make cblib` reads CBLIB's 29 continuous instances (`bench/measurements/02-253/`, `02-254/`, `02-273/`, `02-319/`). Missing: the duals of QPLIB_2456, QPLIB_3105 and QPLIB_2468; QPLIB's mixed-integer QCQPs; cuts and warm starts in the conic tree; a certificate for an infeasibility whose free column with no curvature needs its coefficient to vanish exactly; a quadratic row over more than `CONIC_QC_DENSE` columns |
| Mixed-integer quadratic | **partial** | the linear tree with the barrier at the root and every node, the root heuristics of the conic tree while no incumbent is known, bound propagation at every node, and the incumbent judged with the quadratic gradient (`bench/measurements/02-256/`, `02-259/`). Missing: 13 of QPLIB's 17 convex MIQPs do not finish within 1e11 work units |
| Nonlinear, mixed-integer nonlinear | **out of scope** | |
| Constraint programming, black-box | **out of scope** | |

## 2. LP algorithms

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing (dual Devex once a weight drifts, outside the MIP tree), Harris two-pass ratio test with bound flipping, phase 1 by artificial bounds, a cost perturbation on the first stall and Bland's rule after. A solve whose perturbation stalls restarts once without it, and a numerical error on the aggregated model is solved again without the aggregator (`bench/measurements/02-278/`, `02-280/`) |
| Primal simplex | **partial** | steepest-edge pricing (Devex and Dantzig behind switches), composite phase 1, Harris ratio test; `jaos_set_algorithm` selects it. Missing: 6 of the 94 standard instances run past 10x the dual's work (`bench/results/primal.txt`); seven remedies are refused (`bench/refusals.txt`, the primal-* lines) |
| Barrier (interior point) | **partial** | Mehrotra's predictor-corrector with bounds (`src/barrier.c`), the normal equations or the augmented system factored by the deterministic sparse Cholesky and quasi-definite LDL of `src/chol.c` (minimum degree, or nested dissection when it costs fewer operations); dense columns handled by a Sherman-Morrison-Woodbury correction. `--algorithm barrier`. Infeasible and unbounded models get their verdict from the dual simplex (`bench/results/barrier.txt`, `barrier-infeas.txt`, `bench/measurements/02-344/`). Missing: a rule that picks the augmented system for an LP, cheaper than the symbolic factorisation; a crossover cheaper than a crash (the row below) |
| Crossover from an interior point | **partial** | a basis guess ranked by distance against dual slack, repaired by the LU, then the primal push puts every nonbasic column on a bound and the primal simplex finishes (`crash_basis`, `push_basis`; `bench/measurements/02-287/`). Missing: 14 of the 94 standard instances run past 10x the dual's work (`bench/results/barrier.txt`) |
| First-order method (PDLP) | **partial** | primal-dual hybrid gradient on the scaled model (`src/pdlp.c`): Ruiz and Pock-Chambolle scaling, adaptive steps, restarts and primal-weight updates as in Applegate et al. (2021), deterministic, finished by the crash basis and the dual simplex; infeasible, unbounded or stalled models go to the dual simplex. `--algorithm pdlp`; plain LPs only (`bench/results/pdlp.txt`, `bench/measurements/02-222/`). Missing: a set too large to factor, where it would pay; feasibility polishing |
| Concurrent solve, deterministic | **partial** | the dual, the primal and the barrier on three copies, in rounds of growing work budgets, the first to answer winning; the work billed is the sum, and no clock decides (`src/concurrent.c`, `--algorithm concurrent`, `bench/results/concurrent.txt`). Plain LPs only. Missing: a set where the barrier is the arm that wins |
| GPU | **out of scope** | |

## 3. Preparing and handling the model

| | status | |
|---|---|---|
| Presolve | **partial** | empty rows and columns, singleton rows, cost-0 singleton columns, fixed columns, forcing and redundant rows, implied free column singletons, and the aggregator, which substitutes an implied free column out of an equality of two or three entries (`src/aggregate.c`, `bench/measurements/02-285/`). Presolve answers infeasible on its own and never unbounded. The aggregator runs on continuous LPs only. Missing: duplicate rows and columns, dominated columns, bound tightening, dual fixing, and the substitution of a column the equality does not imply free (each measured and deferred: D101, D246, D97, `presolve-duplicate-rows`, `agg-doubleton-moves` in `bench/refusals.txt`) |
| Postsolve to the caller's indices, statuses and duals | **done** | every removed row and column comes back at its loaded index with the status and dual it would have had (`bench/measurements/02-229/`) |
| Scaling | **done** | Curtis-Reid, powers of two, each factor clamped to 2^±20 (`EXP_LIMIT`). The conic interior point uses its own Ruiz scaling |
| Sparse LU, Markowitz pivoting, Forrest-Tomlin update | **done** | `src/lu.c`: Markowitz ordering under a threshold, Forrest-Tomlin updates, refactoring when the chain grows or a solve disagrees with itself, hyper-sparse triangular solves. Constants in `docs/tolerances.md`, charges in `docs/work-units.md` |
| Hyper-sparse triangular solves | **done** | both directions, FTRAN behind a density prediction per kind of vector |
| Modify bounds, costs, coefficients, objective sense and constant | **done** | each reads back |
| Add and delete rows and columns | **done** | SOS sets, indicator rows and cones follow the renumbering; deleting a column in a cone or a column that switches an indicator row is refused by name |
| Copy a model | **done** | `jaos_model_copy` gives back a model that holds what the source held and shares nothing with it (`bench/measurements/02-229/`) |
| Row, column and objective names | **done** | names of 1 to 255 bytes with no whitespace or control character, set and looked up by name; unnamed ones are `C<j+1>`, `R<i+1>` and `COST`. Every format carries the names it can, and every writer that carries names refuses two of one name (`bench/measurements/02-245/`) |
| Warm start from the previous basis | **done** | the basis a solve leaves starts the next solve unless `jaos_clear_basis` drops it; a basis a few basic variables short after presolve is repaired, and a model whose mapped basis is complete is aggregated too (`bench/results/warm.txt`) |
| Read and write a basis in the MPS basis format | **done** | `jaos_write_mps_basis`, `jaos_read_mps_basis`, `--write-basis`, `--basis` |
| Resume after a limit, in-process and from a file | **done** | a solve stopped by a limit or a callback parks its simplex state, and the next solve goes on from exactly there; an edit drops the state and the next solve starts warm. From a file through the basis (`bench/measurements/02-247/`) |
| Model statistics | **done** | `jaos_model_statistics`, `jaos stats` |
| Presolve statistics | **done** | `jaos_presolve_result`, the `presolve_*` lines of `solve`, `presolve_report()` in Python: the sizes the simplex ran on and how many of each reduction fired (`bench/measurements/02-233/`) |
| Whether the model is a MIP | **done** | `jaos_model_has_integer`, `has_integer()` in Python: an integer column, an SOS set, or a semi-continuous column with a lower bound above zero |

## 4. Mixed-integer machinery

| | status | |
|---|---|---|
| Branch and bound | **done** | best estimate first with the best bound every fifth pick, each node warm from its parent's basis on one private copy of the model. Stops at the gap `mip_gap`, absolute or relative (`jaos_set_mip_gap_rule`). Rounds a bound up to the objective's step when every cost sits on one. A failed node relaxation is set aside with its bound. An unbounded relaxation ends `UNBOUNDED` only when an integer point exists, with SOS sets and indicator rows split first (`bench/measurements/02-326/`, `02-328/`, `02-352/`) |
| Pseudocost branching | **done** | most-fractional as an option (`--branching`, `jaos_set_mip_branching`) |
| Strong branching | **partial** | exists behind a switch and is off by measurement (`--reliability N`, `--probe-cap`, `--probe-depth`; D293 in `bench/refusals.txt`). Missing: a form that pays |
| Gomory, knapsack cover and MIR cuts | **done** | Gomory cuts at the root and to depth 3; knapsack covers and MIR at the root, the MIR rounds going on while they lift the bound; aggregated MIR with Marchand and Wolsey's bound flips on rows with a continuous column; a network mode with c-MIR, variable-bound substitution and a cut pool for fixed-charge networks; hull cuts on short all-integer rows (`--hull-rounds`). Slack cuts are dropped (`bench/measurements/02-328/`, `02-341/`, `02-342/`, `02-347/`, `02-356/`, `02-357/`) |
| Clique cuts | **done** | four rounds at the root from the conflicts of the all-binary rows (`--clique-rounds`) |
| Flow cover, zero-half, lifted cover cuts | **partial** | flow covers on at 5 rounds (`bench/measurements/02-317/`); zero-half (`--zero-half-rounds`) and lifted covers (`--cover-lift`) behind switches, off by measurement (`bench/measurements/02-361/`). Missing: a reading that lands zero-half or lifted covers on |
| Rounding heuristic, root dive, feasibility pump | **done** | the relaxation rounded, a dive at the root (`--dive-heuristic`), and the feasibility pump (`--feaspump`); every point passes the incumbent's acceptance |
| RINS, local branching, other improvement heuristics | **done** | RINS and RENS as sub-MIPs at the root and every 100 nodes, lock rounding, a feasibility jump at a root with no incumbent, and a dive once 1000 nodes pass with no incumbent; local branching behind `--local-branching`, off by measurement (`bench/measurements/02-325/`, `02-328/`, `02-329/`, `02-355/`) |
| Node selection beyond best bound | **done** | lowest pseudocost estimate with the lowest bound every fifth pick (`--node-select`); the plunge behind `--dive` is off by measurement (D289-dive) (`bench/measurements/02-286/`) |
| MIP restarts | **partial** | a restart once the root's reduced costs fix a share of the integer columns, carrying the first root's cuts; on in network mode, behind `--restart` elsewhere (`bench/measurements/02-345/`). Missing: a MIP presolve that removes what the restart fixed, and a reading that lands it outside network mode |
| Solution pool | **done** | `jaos_set_mip_pool_size`, `--pool-size`, `--pool-out`: best first, no two entries with the same integer assignment (`bench/measurements/02-246/`) |
| MIP start and cutoff | **done** | `jaos_set_mip_start`, `jaos_set_mip_cutoff`, `--mip-start`, `--cutoff`; a partial start (NaN entries, `--partial-start`) is completed by a small tree; `start_accepted` says whether the start was taken |
| Node limit, incumbent callback | **done** | `jaos_set_mip_node_limit`, `--node-limit`; `jaos_set_incumbent_callback` hands every new incumbent to the caller, who may stop the solve (`bench/measurements/02-231/`) |
| Bound propagation, reduced-cost fixing | **partial** | reduced-cost fixing on (`--rcfix`); bound propagation behind `--propagate`, off for a linear MIP by measurement (D324), on for a quadratic objective (`bench/measurements/02-335/`). Missing: a reading that lands propagation on for a linear MIP |
| MIP presolve: probing, clique table, coefficient tightening | **partial** | coefficient tightening with implied bounds and a parity step mod 2 (`--tighten`); a clique table with fixing by conflict at each node; a MIP presolve that merges two continuous columns an equality makes equal (`--mip-presolve`); probing behind `--probing`, off (`bench/measurements/02-325/`, `02-328/`, `02-331/`, `02-337/`). Missing: a reading that lands probing on; singleton rows, fixed and dominated columns and other doubleton equations in the MIP presolve; a MIP presolve the restart can run again |
| Semi-continuous variables | **done** | `jaos_set_col_semicontinuous`; MPS `SC` and `SI`, LP `Semi-continuous`, both writers |
| SOS1 and SOS2 constraints | **done** | `jaos_add_sos`, `jaos_sos`; MPS and LP `SOS` sections, both writers; Python `add_sos` |
| Indicator constraints | **done** | `jaos_set_row_indicator`: a row that holds only while an integer column equals 0 or 1. The tree keeps it free until the column is fixed. LP `z = 1 ->`, MPS `INDICATORS`, both writers, Python `add_indicator` |
| Symmetry detection | **done** | automorphisms by colour refinement and a partition search under a work cap (`src/symmetry.c`); orbital branching and fixing on by default (`--orbital`), `--symmetry` to detect and report |
| Conflict analysis | **done** | an infeasible node's Farkas proof gives a row over the binaries the proof needs, kept for the rest of the search; on by default (`--conflicts`) |
| Deterministic parallel tree search | **done** | both trees can take their open nodes in rounds (`--tree-batch N`), each node on its own copy, the answers taken in the round's order, so the tree and its work are the same at any thread count; off by default by measurement (`bench/measurements/02-264/`, `02-290/`) |

## 5. Parallelism

| | status | |
|---|---|---|
| Parallel LP solve | **partial** | the concurrent solve on `--threads N`, and the barrier's Cholesky factor on threads, bit-identical at any N (`bench/measurements/02-288/`). Missing: a parallel simplex |
| Parallel MIP solve | **partial** | rounds of nodes on threads (the parallel tree search row), off by default; a race of tree setups was refused (`mip-race`). Missing: a round cheap enough to be the default where a node takes a few pivots |
| Deterministic under parallelism | **done** | the concurrent solve, the parallel Cholesky factor and the rounds of nodes give the same answer and work at any thread count, and two models solved in two threads at once do not share state (`tests/test_chol.c`, `test_conic.c`, `test_mip.c`, `test_symmetry.c`) |

## 6. Correctness and verification

| | status | |
|---|---|---|
| Bit-identical across machines | **done** | no clock decides anything, no order depends on an address, no floating point is reassociated (`-ffp-contract=off`), no randomness is unseeded, and rounding avoids the C library's `round`. `tests/windows.sh` compares the mingw-w64 build with Linux byte for byte (`bench/measurements/02-234/`, `02-251/`). Not read: a different processor |
| Independent checker shipped with the solver | **done** | judges an answer against the original, unscaled model; on a model with integer structure it judges the primal half and reports `checked_duals` false |
| Exact rational proof of the final basis | **done** | `jaos_verify` proves a basis optimal over the rationals, and refuses only when a number outgrows the limbs; a quadratic objective and a MIP are refused by name (`bench/measurements/02-358/`) |
| Exact rational values of a proved basis | **done** | `jaos_exact_col_value`, `jaos_exact_row_dual`, `jaos_exact_objective`; `jaos_write_proof` writes the proof file and `jaos_check_proof` judges it from the model alone (`bench/measurements/02-237/`) |
| Proof file, written and checked from the model alone | **done** | optimal, infeasible and unbounded |
| Certified bound on suboptimality | **partial** | sound. Missing: alone it cannot separate a wrong vertex from a right one |
| Infeasibility and unboundedness certificates, floating and exact | **done** | `jaos_certificate`, `jaos_unbounded_ray`, their checkers and their exact forms; a MIP carries the certificate of an infeasible root relaxation (`bench/measurements/02-235/`) |
| Irreducible infeasible subsystem | **done** | `jaos_iis`, the CLI's `iis`; a subsystem of row and column bound sides of the linear relaxation |
| The IIS written out as a model | **done** | `jaos_iis_model`, `jaos iis --write OUT` (`bench/measurements/02-236/`) |
| Feasibility relaxation | **partial** | `jaos_feasrelax` and the CLI's `relax`, over the rows, the columns or both, carrying SOS sets, semi-continuous marks and indicator rows. Over the columns a freed integer column is held in a box that grows, under caps; when the caps stop the search, a Hermite normal form over the equality rows, or the gcd of one ranged row over integer columns, may prove that no box holds a point (`bench/measurements/02-230/`, `02-297/`, `02-362/`, `02-369/`). Missing: a proof where several inequality rows together leave no integer point |
| Prove a basis another solver produced | **done** | `jaos_verify_basis`, `jaos verify FILE --basis BAS`; `broken` names the stage and the first row or column that breaks it (`bench/measurements/02-238/`) |
| Check a point another solver produced | **done** | `jaos_read_point`, `jaos_read_duals`, `jaos check FILE --point POINT [--duals DUALS]`, with the solution shapes of Gurobi, MIPLIB, SCIP, HiGHS and CPLEX (`bench/measurements/02-239/`) |
| Exact solving with no tolerances | **partial** | LPs: `jaos_set_exact`, the option `exact`, `jaos solve --exact`. The floating-point answer is proved over the rationals and repaired by exact pivots where the proof breaks; data infeasible over the rationals end `infeasible` with an exact certificate (`bench/measurements/02-358/`). Missing: MIPs, QPs and cones; data read as exact decimals; numbers past the limb budget without a wider build |

## 7. Input and output

| | status | |
|---|---|---|
| Read fixed and free MPS | **done** | `OBJNAME`, `RANGES`, all bound types, `MARKER` for integers |
| Read LP | **done** | the CPLEX-style dialect: objective, constraints, ranges, bounds, General, Binary, Semi-continuous, SOS, indicators, lazy constraints, user cuts, and the quadratic block (`docs/format-support.md`) |
| Read and write gzip | **done** | inflate and deflate written here |
| Direct load from arrays | **done** | `jaos_load_lp`, compressed columns (`bench/measurements/02-229/`) |
| Write MPS | **done** | every field the model holds that MPS can carry; a semi-continuous column with no upper bound is refused by name |
| Write LP | **done** | names LP cannot spell go under positional names with a comment map; free and ranged rows in the forms HiGHS reads (`bench/measurements/02-252/`) |
| Own solution file, written and read | **done** | `jaos_write_solution`, `jaos_read_solution`; every value reads back the same double |
| Point and duals files | **done** | `jaos_write_point`, `jaos_write_duals`, `--write-point`, `--write-duals`, read back by `jaos_read_point` and `jaos_read_duals` |
| Read other solvers' solution files | **done** | Gurobi, MIPLIB, SCIP, HiGHS and CPLEX XML, detected from the file |
| Reject unsupported constructs with a line number | **done** | every reader names the line of a construct it does not take; the tool exits 5 (`bench/measurements/02-240/`) |
| `diff` and `show` commands | **done** | `diff A B` lists every difference between two models; `show FILE --row NAME` or `--col NAME` prints one (`bench/measurements/02-241/`) |
| Indicator constraints in MPS and LP | **done** | MPS `INDICATORS`, LP `name: z = 1 -> ...`, read and written; `.nl`, OSiL and QPLIB refuse them by name |
| Other formats (`.nl`, OSiL, QPLIB, CBF) | **done** | `.nl` text form read and written, with bodies of degree two read; QPLIB read and written with quadratic objectives and rows; OSiL read and written; CBF read and written with its cones. What each takes and refuses is in `docs/format-support.md` |

## 8. Using it from another language

| | status | |
|---|---|---|
| C API, one header | **done** | `include/jaos.h`, 206 functions, each returning a `jaos_status` the caller must read or a plain value; the model an opaque `jaos_model`, its last message in `jaos_model_error`. C23, linking libc and libm only (`docs/api.md`) |
| Command-line tool | **done** | `docs/cli.md` |
| Python: ctypes wrapper and modeling layer | **done** | `python/jaos/`, standard library only: `Model` over every C call and `Problem` on top, which edits the model in place and re-solves warm |
| Python package installable with pip | **done** | `pyproject.toml` and `setup.py`; a tag builds the wheels and the sdist into a GitHub Release, and the PyPI upload is off until the project is registered (`docs/build.md`) |
| Julia | **done** | `julia/JAOS`: every C call through `ccall`, and `JAOS.Optimizer` for MathOptInterface and JuMP, with lazy and user-cut callbacks; `make julia-test` runs MathOptInterface's conformance suite. Callbacks run on the calling thread (`docs/api.md`, Threads) |
| Java, .NET | **done** | `java/src/org/jaos` (foreign-function API, Java 22+) and `dotnet/Jaos` (.NET 8), every C call, callbacks, and a `Problem` layer; `make java-test`, `make dotnet-test` |
| R | **done** | `R/jaos`: every C call, directly or through the options by name; `jaos_solve_lp` takes dense and sparse matrices; `make r-test` |
| Modelling-system links (JuMP, Pyomo, AMPL, GAMS) | **partial** | AMPL's solver protocol (`jaos STUB -AMPL`, `jaos_write_sol_ampl`), read back by Pyomo and JuMP's AmplNLWriter; JuMP also through `JAOS.Optimizer` (`bench/measurements/02-257/`). Missing: GAMS, which needs its own link library; `.nl` bodies above degree two |
| `make install` and pkg-config | **done** | `make install` and `make uninstall` under `PREFIX`, honouring `DESTDIR`; `jaos.pc`; `tests/install.sh` builds a consumer against the install |
| CMake package | **done** | `CMakeLists.txt` with `jaosConfig.cmake`; `tests/cmake.sh` checks a `find_package` consumer |
| Windows and macOS builds | **done** | POSIX calls behind `src/jaos_sys.h`; mingw-w64 and wine in `tests/windows.sh`, native Windows runs, and CI on clang-cl and macOS GCC 14 (`docs/build.md`) |

## 9. Controlling a solve

| | status | |
|---|---|---|
| Work limit and time limit | **done** | `jaos_set_work_limit`, `jaos_set_time_limit`, `--work-limit`, `--time-limit`; every sub-solve of a tree runs under what is left (`bench/measurements/02-242/`) |
| Primal and dual tolerances | **done** | `jaos_set_primal_tolerance`, `jaos_set_dual_tolerance`, `--primal-tol`, `--dual-tol`; zero means the default (`docs/tolerances.md`, `bench/measurements/02-244/`) |
| Logging with levels | **done** | `jaos_set_log_callback`, `jaos_set_log_level` (`off`, `summary`, `progress`, `detail`), `--log`, `--quiet` (`bench/measurements/02-243/`) |
| Progress callback that can stop | **done** | `jaos_set_progress_callback`: iterations, work, primal infeasibility, and for a MIP nodes, bound and incumbent; a `STOP` ends the solve `INTERRUPTED` and the next solve continues (`bench/measurements/02-232/`) |
| Choose the algorithm | **done** | `jaos_set_algorithm`, `--algorithm dual|primal|barrier|pdlp|concurrent` |
| Options as name-value strings, parameter file | **done** | `jaos_set_option`, `jaos_get_option`, `jaos_read_options`; 61 options; `--opt NAME=VALUE`, `--params FILE`, `jaos options` |
| Steering callbacks: user cuts, lazy constraints, branching, solutions | **done** | `jaos_set_node_callback`: rows through `jaos_node_add_row`, the branching column, and points through `jaos_node_add_solution` |
| Thread count | **done** | `jaos_set_threads`, `--threads N`; 0 means every core; the answer is the same at every count |
| Sensitivity and ranging | **done** | `jaos_cost_ranging`, `jaos_rhs_ranging`, `jaos_bound_ranging`, the CLI's `ranging`; a QP and a MIP are refused by name |

## 10. Licence and distribution

| | status | |
|---|---|---|
| Apache 2.0, free for commercial use, no dependencies | **done** | `LICENSE` and an SPDX line in every source file; libc and libm only; no code from another solver. The one vendored file is Unity, used by the tests alone |

## The bars

- Netlib: 94 standard, 16 Kennington, 29 infeasible. Every answer checked,
  every objective against the Koch reference, two solves identical.
  `make netlib netlib-infeas netlib-kennington J=12`; results in
  `bench/results/`, baselines in `bench/*.baseline`.
- MIPLIB 3: 24 instances to the catalogue optimum. `make miplib`.
- Speed against HiGHS, SoPlex and Clp: `bench/compare/results/P0.txt`,
  `make compare COMPARE_ARGS='-t P0'`.
- MIPLIB 2017: the 30 smallest benchmark instances with a proven optimum,
  each stopped at 1e10 work units. A reading, not a gate:
  `make miplib2017`, `bench/results/miplib2017.txt`.
- MIP speed against HiGHS and SCIP: `bench/compare/results/mip-*.txt`,
  `bench/compare/run-mip.sh`.
