# SPECS — what JAOS is built to be

A mathematical-programming solver written from scratch in C23, competitive
with the serious open solvers and usable as a library by someone who did not
write it.

This file is the target and the present state, in the present tense. It says
what exists, what is partial and what is missing. It does not say why, and it
does not say how anything got here: `DECISIONS.md` owns the why and the
measurements, `CHANGELOG.md` owns what landed when, `TODO.md` owns what is
next. A number appears here only with the record that owns it beside it.
Nothing is in scope that is not on this page.

The core value, unchanged since M1: a correct answer, bit-identical on every
machine and every run, proved by a checker that had no access to the solver
that produced it.

## Premises

Non-negotiable, and they shape everything below.

- **No external code.** Two exceptions, both closed and neither extended:
  netlib's `emps` as a dev-time instance converter, fetched and checksummed
  and never redistributed; and Unity for the test suite. Papers, theses and
  textbooks are the only other input — never another solver's source.
- **Bit-identical results on every machine and every run.** No clock decides
  anything, no iteration order depends on an address, no reassociated
  floating point, no unseeded randomness.
- **Every number needs a measurement on both sides.** A tolerance, a
  threshold, an interval. Fitting a constant to one instance is how this
  project loses weeks.
- **Deterministic work units are the unit of cost, and every run also
  reports wall-clock time.** The units make regressions detectable across
  machines; the seconds say whether the units bought anything. Seconds are
  development numbers and are labelled as such — they never enter a baseline.

Two decisions taken on 2026-08-13 fix how the premises apply as the project
grows:

- **The first two premises are absolute, with no exceptions.** Everything
  planned from here is costed under them. A barrier method needs a sparse
  Cholesky factorization: it is written here, and it is deterministic. Reading
  compressed input needs an inflate written here, or the feature stays absent
  — and it was written here, in `src/inflate.c` (D240).
  Any parallelism is deterministic by design rather than by luck. A feature
  that cannot be built under these two rules is not built. This makes each
  feature two to five times more expensive than it is elsewhere, and it is
  accepted deliberately.

- **The goal is the best open solver that returns identical results on every
  machine and ships its own independent checker.** Not matching Gurobi, which
  is not reachable. Gurobi's documentation states it is deterministic on one
  machine but not across machines, and none of the solvers in
  `docs/feature-matrix.md` ships a checker. Breadth of features serves that
  goal; it does not replace it.

Status is **done**, **partial** or **missing**. "Partial" always says what is
missing.

---

## 0. The target, and where JAOS stands against it

The sections below say what JAOS has. `docs/feature-matrix.md` is the other
half: the feature list the field is measured on, with JAOS, HiGHS, SoPlex,
Clp, SCIP, Gurobi and Hexaly side by side. It is written from what the field
offers, not from what JAOS has, so most of it is empty for JAOS on purpose.

**Read it at the close of every phase.** A phase that moved no cell on that
page improved the code and not the product.

**JAOS is an LP solver.** Of the nine areas the matrix measures, JAOS is
present in four. That is expected at 0.2.0.

**Three things JAOS has that the field mostly does not.** Bit-identical
results across machines, which Gurobi explicitly does not promise. An
independent checker shipped with the solver, which none of the others ship. A
budget counted in reproducible work units rather than seconds.

**One place where JAOS is behind where it believed it was ahead.** SoPlex
solves LPs exactly over the rationals and SCIP emits a VIPR certificate an
external program verifies in exact arithmetic. JAOS's checker is a
floating-point checker judging against tolerances. It is not a proof. Section
5 lists exact rational verification as missing; this is what it is missing
against.

**The current milestone is M2, and it is about speed.** Its success criterion
is a time ratio (section 8), and only the presolve and solving rows change
when it closes.

---

## 1. Reading a problem

| | status | |
|---|---|---|
| Fixed MPS | **done** | |
| Free MPS, autodetected | **done** | |
| `RANGES` with per-row-type semantics | **done** | |
| All `BOUNDS` types, `OBJSENSE`, objective constant | **done** | |
| Unsupported constructs rejected with a line number | **done** | SOS and indicators, never silently skipped |
| LP format | **partial** | CPLEX-style core. **Missing:** everything outside the subset `docs/format-support.md` lists |
| Locale-independent number parsing | **done** | own, because `strtod` under a comma-decimal locale corrupts instances |
| Direct load from CSC arrays | **done** | `jaos_load_lp` |
| Compressed input (`.gz`) | **done** | gzip over DEFLATE, written here in `src/inflate.c`; both readers detect it from the file itself, not from the name |

## 2. Holding and changing a problem

| | status | |
|---|---|---|
| Dimension and nonzero queries | **done** | |
| Read a bound or a cost back | **done** | `jaos_col_cost`, `jaos_col_bounds`, `jaos_row_bounds` |
| Add or delete rows and columns after load | **done** | `jaos_add_rows`, `jaos_add_cols`, `jaos_delete_rows`, `jaos_delete_cols`. Additions append, so existing indices never move; deletion takes a set (D77) |
| Change a bound or a cost | **done** | `jaos_set_col_cost`, `jaos_set_col_bounds`, `jaos_set_row_bounds`; each discards the answer the model holds (D66) |
| Change a coefficient | **done** | `jaos_set_coefficient`; zero deletes the entry, a new index inserts one, both derived copies are rebuilt (D67) |
| Re-solve warm from the previous basis | **done** | automatic: an optimum leaves its basis on the model, a modification keeps it, a load drops it. `jaos_clear_basis` asks for a cold solve (D68) |
| Load a starting basis | **done** | `jaos_set_basis`, the write side of `jaos_basis`. A wrong basis costs iterations and never the answer |
| Resume from where a work or time limit stopped | **done** | the basis a stopped solve reached is where the next one starts; a numerical failure is the one outcome that leaves none (D70) |

## 3. Solving

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing [8], Harris two-pass ratio test with bound flipping [7][19], dual phase 1 by artificial bounds [21], Bland fallback on a detected stall (D26). The right-hand side and the refinement residual are Neumaier-compensated sums (D168, D171). The dense ratio-test branch enumerates its candidates from a maintained nonbasic set (D93) |
| Sparse LU, Markowitz threshold pivoting | **done** | [4][6][20], Forrest-Tomlin updates [5], singular-basis repair |
| Stability trigger on the triangular solves | **done** | the pivot element is computed twice each iteration, by BTRAN and by FTRAN; past `LU_AGREE_TOL` the pivot is declined unbilled and the factorization rebuilt (D86) |
| Scaling | **done** | Curtis-Reid [11]; geometric-mean equilibration exists as an internal alternative mode, not a caller option |
| Hyper-sparsity in the triangular solves | **partial** | [9]: both solves report their pattern (D38, D43, D44), and both of BTRAN's triangular passes compute only their reachable slots (D36, D253). **Missing:** FTRAN's passes still traverse every slot — they skip the arithmetic of zero slots and bill per nonzero, so work units cannot see the traversal, only an instruction count can |
| Presolve | **partial** | six families behind a cascading round loop: empty rows and columns, singleton rows, cost-0 singleton columns (D95), fixed columns, forcing and redundant rows, the implied free column singleton (D105, D106). Every cap and window is measured (`docs/tolerances.md`). `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` compiles it out and reproduces the pre-presolve baselines bit for bit (D96). **Missing:** duplicate rows and columns and dominated columns, deferred with an executable reopen condition (D101), which `make refusals` runs and which reports **zero** removable rows and columns on all 15 plato instances (D242); bound tightening, refused and reopening with a crossover (D97); dual fixing, measured and refused at 0.67% of netlib's and 1.09% of fome's live columns against 02-154's 5% bar, on a counter arm calibrated to a known answer (D246). **Open:** `grow22`, `grow7` and `greenbeb` cost more with presolve on than off (`TODO.md`). The published basis had broken `jaos.h`'s row-count promise on 46 of netlib's 188 solves (D167) until every postsolve status was decided from the reduction's structure (D257). Worth, presolve on over off: work 0.810x standard, 0.651x Kennington, 0.084x infeasible (D103), and D106 a further 0.9527x on the standard set |
| Primal simplex | **partial** | Dantzig pricing, sharing `pivot()` and the pricing row with the dual, behind `cfg.force_primal`, a development switch and not an option (D188). A composite short-step phase 1 works from any given basis (D190). The entering column stops at its own bound (D189). A pivot must stand above one ulp of its own column's largest entry, not an absolute floor (D207). Harris's two-pass ratio test in primal form, at a width of half `primal_tol` (D212, D213). Reach on the standard set: 77 of 94 agree with the dual, 11 overrun a 10x budget, 6 disagree and none trips an internal guard (`bench/results/primal.txt`; D193, D205, D212, D218, D245, D249, D250, D251). Phase 1 refuses a basis whose own infeasibility doubles, which is what moved one instance from the overrun column to the disagreeing one (D218). Most of that campaign's iterations belong to the dual's settling re-entry rather than to the primal, so the agreement count does not mean what it reads as; `make primal` prints the three-way split itself and the record's own `iterations by method` line is where the figures live, because a percentage restated here drifts with every campaign (D194, D197, D204). It declares an unbounded ray it meets in phase 2, on the same D19 proof the dual's verdict already used, and refuses there only while a borrowed cost is outstanding (D241). The shared lent-bound verdict both methods finish through also proves a ray that needs several columns at once, by moving every held column together at unit rate; a subset or unequal-rate ray still reaches the refusal, which says both directions were tried (D247). **Missing:** Devex pricing — `TODO.md` section 0 stage 5, blocked on a paywalled source. Blocks crossover, which blocks D97 |
| Crash basis | **missing** | [12]; measured and refused: it destroys the exact starting steepest-edge weights of the slack basis. Reopens when pricing stops starting from exact weights (`TODO.md` refusals) |
| Partial and multiple pricing | **measured and refused** | [1]: both built and swept. The leaving-row sweep's units are the cheapest in the solver, and every scheme for scanning it less often pays in trajectory and in wrong answers (D82, D84) |
| Barrier and crossover | **missing** | not optional at large scale. Crossover is what D97's ideal design needs at postsolve (`bench/measurements/02-88/`). Crossover as published starts from an interior point and JAOS has none; where the starting point comes from is undecided (`docs/research/primal-simplex.md` §5) |
| MILP: branch and bound, cuts, heuristics | **missing** | [14][15][16][17][18] |
| Deterministic parallelism | **missing** | [10][13] |

Citation numbers are the bibliography in `docs/archive/PLAN.md`.
Implementation works from those and their kin only — never another solver's
source (D12).

## 4. Controlling a solve

**What is controllable is the contract, not the method.** A caller sets what
depends on their problem and which the solver cannot know: how much precision
their data deserves, how long they will wait, where log lines go. How the
problem is solved is the solver's to decide (D64).

| | status | |
|---|---|---|
| Work limit, time limit | **done** | |
| Set the primal and dual tolerances | **done** | `jaos_set_primal_tolerance`, `jaos_set_dual_tolerance`; 0 restores the default |
| Logging and verbosity | **done** | `jaos_set_log_callback`, `jaos_set_log_level`; four levels, silent until a callback is installed (D65) |
| Callbacks | **done** | `jaos_set_progress_callback`; a watcher may look and may stop a solve, never steer one. Asked on a fixed iteration count, so *when* it is asked is reproducible; a stop is `JAOS_SOLVE_INTERRUPTED` and keeps its basis (D79). Phase 1 of the primal offers it too (D200) |
| Choose the algorithm | **out of scope** | the solver picks (D64) |
| Turn scaling off or pick the mode | **out of scope** | same, and it is a method question |

## 5. Reading an answer

| | status | |
|---|---|---|
| Status, objective, values, activities, duals, reduced costs | **done** | the objective is `obj_offset` plus a compensated sum of `c_j x_j` over the published values, on the model that publishes them. It is the correctly rounded exact objective of the published point on 110 of 110, worst 0.493 ulp (D173, `bench/measurements/02-83/`); this number is finished and is not to be reopened |
| Where every variable rests in the basis | **done** | `jaos_basis`. Exactly `num_row` basics on every gate solve, 188 of 188 netlib and 32 of 32 Kennington, and no published reduced cost contradicts its status (worst 4.09e-10): every postsolve status is decided from the reduction's structure, never from an equality test on a rounded value, and where the structure says a value rests on a bound the bound is published (D257, `bench/measurements/02-166/`); a column the caller fixed is named at the bound its reduced cost points into, in the simplex and at postsolve, so the statuses are dual feasible as a set (D258). A nonbasic status names a bound the model declared: a column left resting on one the solve lent itself is walked off it before the answer is published, which takes the published answers carrying such a status from 1717 to 0 over 200000 small models and lands `finnis`'s objective on the netlib reference to 3e-8 (D261, `bench/measurements/02-168/`). Before that 46 netlib solves broke the count and five instances carried a reduced cost their status forbids (D167, D170). `build_warm_basis` repairs a mapped count short by at most `WARM_REPAIR_MAX_SHORT` and falls back to cold beyond that; a long count is refused (D151, D186) |
| Iterations and work units | **done** | |
| Solve time | **done** | `jaos_solve_time`, seconds of the last solve. The only number JAOS reports that is not reproducible, and the header says so |
| Independent solution checker | **done** | it bounds unbounded variables by what the rows imply, propagated to a fixed point, and reads the verdict off only the terms from bounds the model declared (D87, D91). 64 of 110 accepted answers are certified (D91). The gate holds every solve to an absolute suboptimality bar, `RSUB_CEILING` (D185) |
| A certified lower bound on suboptimality | **partial** | `certified_suboptimality` is sound and never overclaims. **Missing:** on its own it cannot separate a wrong vertex from a right one (D73); the gate's absolute bar is what makes it a verdict (D185) |
| Sensitivity and ranging | **done** | `jaos_cost_ranging`, `jaos_rhs_ranging`, `jaos_bound_ranging`: for every cost, every row bound and every column bound, the interval it may take with the basis behind the last optimum staying optimal, the textbook ratio tests on that basis refactored over the model as loaded (D258). Presolve has no half in it: the published basis is a basis of the caller's model since D257, so no range is mapped back through a reduction. The solver is the oracle in `tests/test_ranging.c`: a number moved just inside its range re-solves warm for nothing, just outside it the re-solve pivots or the model is infeasible. Ranges 94 of 94 netlib and 16 of 16 Kennington instances with no refusal and no failed check, once no column is published on a bound the solve lent it (D261, `bench/measurements/02-168/`). Not billed to `jaos_work_units`; the cost is stated in `jaos.h` |
| Infeasibility and unboundedness certificates | **done** | `jaos_certificate` publishes the Farkas ray behind INFEASIBLE (D254), `jaos_unbounded_ray` the direction behind UNBOUNDED from all three simplex proof sites (D255), and `jaos_check_certificate` / `jaos_check_ray` verify both from the model alone, in the original space. A ray proved on a presolve-reduced model is lifted back through the reductions, and each presolve proof site seeds its own from the bound it refused (D256): 28 of the 29 reference infeasibles certify at 1e-7 under the default build and `gran` at 1e-9, because a site-seeded ray carries presolve's own eight-ulp margin (`bench/measurements/02-165/`). A model whose own bounds are inverted publishes no ray: the bounds are the proof, and the accessor refuses |
| Exact rational verification of a final basis | **missing** | |

## 6. Writing

| | status | |
|---|---|---|
| Write MPS | **done** | `jaos_write_mps`, free layout. Three refusals: a row whose lower bound is above its upper one and a bound at an infinity of the wrong sign, which MPS has no form for, and a ranged row that neither RANGES form reconstructs exactly, which the writer checks before it writes. No gate instance reaches any of them (D226) |
| Write LP | **partial** | `jaos_write_lp`, the dialect `jaos_read_lp` accepts. A ranged row is written as the two-sided form and read back as one row with two ends (D239). **Missing:** a free row and a row with no coefficients — each refused by name and pointing at `jaos_write_mps`, which has neither. **104 of the 139 gate instances round-trip through it; 35 are refused**, 34 of them for an empty row and 1 free (D239, re-run of `bench/measurements/02-138/run-lpcover.sh`). Neither closes with a reader change: LP has no way to write a constraint with no terms, and the two-sided form takes numbers rather than `inf` |
| Write a solution file | **done** | `jaos_write_solution`, JAOS's own line-oriented format (`docs/format-support.md`). Available only on an optimum, the rule `jaos_solution` and `jaos_basis` already apply, and only when every value in the answer is finite: an objective that overflowed would print a word the host libc spells, and the file would not be reproducible (D226) |

**What JAOS writes, JAOS reads back as the same model**, and that is the
contract all three are built to rather than a property they happen to have.
Values carry the shortest of 15, 16 or 17 significant digits that reads back
as the same double; the one construction the MPS reader rebuilds by
arithmetic, a ranged row, is checked against what the reader will make of it
before it is written; a format that cannot express a row or a column refuses
and names it; and a refused write removes the partial file. `tests/test_write.c`
checks the round trip field by field with `==`, and 139 of 139 gate instances
round-trip exactly (D226, `bench/measurements/02-138/`).

## 7. Using it from another language

| | status | |
|---|---|---|
| C API | **done** | `include/jaos.h`, the only header |
| Python | **done** | `python/jaos.py`: a ctypes wrapper covering every call in `jaos.h` (`make shared`), and a modeling layer on top — `Problem`, variables, expressions, constraints from ordinary comparisons, warm re-solve when only bounds or costs moved. Standard library only, so it needs no compiler, no header and no package index at install time — the same no-dependency rule the C library holds. The four certificate calls and their checkers are reached too, `Model.certificate` / `unbounded_ray` / `check_certificate` / `check_ray` and the same pair on `Problem`, and so are the three ranging calls, `cost_ranging` / `rhs_ranging` / `bound_ranging` on both (D258). 72 tests, `make python-test`; defect shapes armed in `bench/measurements/02-155/` (D243) and `bench/measurements/02-158/` |
| Anything else | **missing** | |

---

## 8. The bars it has to clear

| | status |
|---|---|
| Netlib standard set, 94 instances: optimal, objective within tolerance, checker green | **green at HEAD** — 94 solved, 94 objectives match Koch within `1e-6 * max(\|ref\|, 1)`, 94/94 checker ok, 94 deterministic, baseline `0 regressed, 0 improved, 0 new` (`bench/results/netlib.txt`). The four instances D173 found off the optimum inside that window are closed (D184) |
| Kennington subset, 16 instances | **green at HEAD** — 16 solved, 16 objectives ok, 16/16 checker ok (`bench/results/netlib-kennington.txt`) |
| Netlib infeasible subset, 29 instances: refused, no false optima | **pass** (`bench/results/netlib-infeas.txt`) |
| Determinism across two solves and across runs, all 139 | **pass** — the second solve clears the basis first, or it would be a warm re-solve (D68) |
| Warm re-solve against cold, one branching step per instance | **pass**; `disagreed=0, rejected=0` on both sets. The ratio lives in `bench/results/warm.txt` and `warm-kennington.txt`, never here (D92, D151) |
| Full suite clean under ASan and UBSan | **pass** |
| Reader robustness under fuzzing | **pass** |
| Competitive gap at rung **P0** vs **HiGHS 1.15.1** | **3.60x slower**, on 1.78x the iterations and 2.02x the cost of one, faster on 1 of 17 (`bench/compare/results/P0.txt`, taken 2026-08-30) |
| Competitive gap at rung **P0** vs **SoPlex 8.0.3** | **1.12x slower**, on **0.73x the iterations** and 1.52x the cost of one, faster on 10 of 21 (same record) |
| Competitive gap at rung **P0** vs **Clp 1.17.11** | **2.96x slower**, on 1.56x the iterations and 1.90x the cost of one, faster on 1 of 14 (same record) |
| Direction of the gap since 2026-08-17 | **wider on all three**: 3.15x → 3.60x, 0.95x → 1.12x, 2.57x → 2.96x. Consistent with D184 paying 1.0339x netlib and 1.0976x Kennington in work to remove four wrong answers, and with nothing since buying it back. No performance work has landed in that window |
| Rungs T1–T3, which price presolve and algorithm choice | presolve is worth 1.42x to HiGHS and 1.14x to SoPlex; free algorithm choice is worth nothing, on identical iteration counts (D81) |
| MIPLIB 2017 easy subset | not started |
| MIPLIB 2017 benchmark subset | not started |

P0 is each solver's own presolve on, the dual forced, no crash basis, one
thread (D104). `bench/compare/README.md` owns the per-instance decomposition
and the historical rungs.

**The diagnosis has been stable for weeks and the current reading sharpens
it: the algorithm is competitive and the iteration is not.** Against SoPlex
JAOS takes **0.73x the iterations** — fewer than SoPlex needs — and still
loses on total time. All three rivals disagree about the iteration count
(1.78x, 0.73x, 1.56x) and agree about the cost of one (2.02x, 1.52x, 1.90x).
That agreement is a property of JAOS and not of any one rival (D83), and it
says where the work is: making an iteration cheaper, not making fewer of them.

**Run `make compare COMPARE_ARGS='-t P0'`, never bare `make compare`.** The
bare form defaults to rung T0, which was taken when JAOS had no presolve;
against a presolving JAOS it puts presolve on one side only, reports a
flattering number, and overwrites T0's stored record on the way. It read
2.52x against HiGHS on 2026-08-30 where the honest rung reads 3.60x.
