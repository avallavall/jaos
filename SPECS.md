# SPECS — what JAOS is built to be

A mathematical-programming solver written from scratch in C23, competitive
with the serious open solvers and usable as a library by someone who did not
write it.

This file is the target. `TODO.md` is the order the work happens in,
`CHANGELOG.md` what has landed, `DECISIONS.md` why. Nothing is in scope that
is not on this page.

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
grows, because both were about to become expensive:

- **The first two premises are absolute, with no exceptions.** Everything
  planned from here is costed under them. A barrier method needs a sparse
  Cholesky factorization: it is written here, and it is deterministic. Reading
  compressed input needs an inflate written here, or the feature stays absent.
  Any parallelism is deterministic by design rather than by luck. A feature
  that cannot be built under these two rules is not built. This is what makes
  each feature two to five times more expensive than it is elsewhere, and it is
  accepted deliberately.

- **The goal is the best open solver that returns identical results on every
  machine and ships its own independent checker.** Not matching Gurobi, which
  is not reachable. The stated goal is reachable and nobody occupies it today:
  Gurobi's documentation states it is deterministic on one machine but not
  across machines, and none of the solvers in `docs/feature-matrix.md` ships a
  checker. Breadth of features serves that goal; it does not replace it.

Status is **done**, **partial** or **missing**. "Partial" always says what is
missing.

---

## 0. The target, and where JAOS stands against it

The sections below say what JAOS has. They do not say what a general-purpose
solver is expected to have, so on their own they cannot tell anyone whether the
target is the right target. `docs/feature-matrix.md` is the other half: the
feature list the field is measured on, with JAOS, HiGHS, SoPlex, Clp, SCIP,
Gurobi and Hexaly side by side. It is written from what the field offers, not
from what JAOS has, so most of it is empty for JAOS on purpose.

**Read it at the close of every phase.** If a phase closed and moved no cell on
that page, the phase improved the code and not the product. That is sometimes
the right thing to do, but it should be known rather than assumed.

What the matrix says today, in short:

**JAOS is an LP solver.** Nine areas are measured — problem classes, LP
algorithms, model handling, mixed-integer machinery, parallelism, correctness
and verification, input and output, language bindings, and solve control. JAOS
is present in four of them and absent from the rest. That is expected at
0.1.0-dev.

**Three things JAOS has that the field mostly does not.** Bit-identical results
across machines, which Gurobi explicitly does not promise. An independent
checker shipped with the solver, which none of the others ship. A budget counted
in reproducible work units rather than seconds. All three come from the same
premise, and together they are the project's actual distinguishing feature.

**One place where JAOS is behind where it believed it was ahead.** Verification
is this project's own subject, and two solvers are further along it. SoPlex has
solved LPs exactly over the rationals since version 2.1 and added precision
boosting in 6.0. SCIP 10.0 solves mixed-integer problems with no numerical
tolerances at all and emits a certificate in VIPR format that an external
program verifies in exact rational arithmetic. JAOS's checker is a good
floating-point checker judging against tolerances. It is not a proof. Section 5
below lists exact rational verification as missing; this is what it is missing
against, and emitting a VIPR-format certificate would put JAOS in a club of two
for much less work than the mixed-integer sections would cost.

**The current milestone moves one row of that page.** M2 is about speed, and its
success criterion is a time ratio. Only the presolve rows change when it closes.
That is a deliberate choice and not an oversight, but it means the roadmap after
M2 has to state whether breadth becomes the goal, because M2's own criterion
does not measure it.

---

## 1. Reading a problem

| | status | |
|---|---|---|
| Fixed MPS | **done** | |
| Free MPS, autodetected | **done** | |
| `RANGES` with per-row-type semantics | **done** | |
| All `BOUNDS` types, `OBJSENSE`, objective constant | **done** | |
| Unsupported constructs rejected with a line number | **done** | SOS and indicators, never silently skipped |
| LP format | **partial** | CPLEX-style core; the documented subset is in `docs/format-support.md` |
| Locale-independent number parsing | **done** | own, because `strtod` under a comma-decimal locale corrupts instances |
| Direct load from CSC arrays | **done** | `jaos_load_lp` |
| Compressed input (`.gz`) | **missing** | handled outside the library today |

## 2. Holding and changing a problem

| | status | |
|---|---|---|
| Dimension and nonzero queries | **done** | |
| Read a bound or a cost back | **done** | `jaos_col_cost`, `jaos_col_bounds`, `jaos_row_bounds`; a caller who read the model from a file could otherwise change a bound it cannot see |
| Add or delete rows and columns after load | **done** | `jaos_add_rows`, `jaos_add_cols`, `jaos_delete_rows`, `jaos_delete_cols`. Additions append so existing indices never move; deletion takes a set, because deleting one at a time makes the caller track the renumbering (D77) |
| Change a bound or a cost | **done** | `jaos_set_col_cost`, `jaos_set_col_bounds`, `jaos_set_row_bounds`; each discards the answer the model was holding |
| Change a coefficient | **done** | `jaos_set_coefficient`; zero deletes the entry, a new index inserts one, and both derived copies are rebuilt |
| Re-solve warm from the previous basis | **done** | automatic: an optimum leaves its basis on the model, a modification keeps it, a load drops it. `jaos_clear_basis` asks for a cold solve (D68) |
| Load a starting basis | **done** | `jaos_set_basis`, the write side of `jaos_basis`. A wrong basis costs iterations and never the answer |
| Resume from where a work or time limit stopped | **done** | the basis a stopped solve reached is kept where the next one starts; a numerical failure is the one outcome that leaves none (D70) |

## 3. Solving

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing [8], Harris two-pass ratio test with bound flipping [7][19], dual phase 1 by artificial bounds [21], Bland fallback on a detected stall. What looked like Bland failing to finish off a cycle (D72) was the factorization it was pivoting on having stopped describing the basis; the trigger above is the repair (D86). The ratio test's dense branch enumerates its candidates from a maintained nonbasic set rather than from the whole model — the same candidates in the same order, which is what keeps it out of what Harris's two passes guarantee, and it is not established that it buys time (D93) |
| Sparse LU, Markowitz threshold pivoting | **done** | [4][6][20], Forrest-Tomlin updates [5], singular-basis repair |
| Stability trigger on the triangular solves | **done** | the pivot element is computed twice each iteration, by BTRAN and by FTRAN; past `LU_AGREE_TOL` the pivot is declined unbilled and the factorization rebuilt. Free — both numbers are already paid for (D86) |
| Scaling | **done** | Curtis-Reid [11], geometric-mean equilibration as an option |
| Hyper-sparsity in the triangular solves | **partial** | [9]: both solves report their pattern, the passes billed for every slot are not all reduced |
| Presolve | **partial** | five families live behind a cascading round loop: empty rows and columns, singleton rows, cost-0 singleton columns (D95), fixed columns, forcing and redundant rows — both caps measured (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_TIGHTEN_EPS = 1e-9`, `docs/tolerances.md`). `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` compiles it out and reproduces the pre-presolve baselines bit for bit (D96). Deferred, with the count that defers them and an executable reopen condition: duplicate rows and columns, dominated columns, worth 0.15% of rows and columns on these 139 models (D101). Refused: bound tightening, measured six ways (D97). The postsolve defect is closed in both halves, the row residual (D99) and the dual (D100), and all three sets pass with 94/94, 29/29 and 16/16 checker ok. A relaxed row is tested for feasibility once the boxes are final, so an infeasible model is no longer published OPTIMAL (D102). Open, and none of it blocks the gate: the published basis breaks `jaos.h`'s row-count promise, a collapsed fold leaves a bound no record owns, and the same replay can still produce an empty intersection of an ulp on 11 of the 94 — `TODO.md`'s standing debts own these. Field value: 1.42x to HiGHS, 1.14x to SoPlex (D81) |
| Primal simplex | **missing** | needed for crossover and for the warm starts the dual cannot serve. No longer needed for carried defect 4: the primal clean-up already owns a ratio test and a basis change, and reading the reduced cost's sign rather than the status is all a nonbasic free variable ever needed from it (D85). **Not a speed argument:** given free choice both rivals ran the dual on every instance, with iteration counts identical to being forced (D81) |
| Crash basis | **missing** | [12]; measured once and refused: it destroys the exact starting steepest-edge weights the slack basis gives |
| Partial and multiple pricing | **measured and refused** | [1]: both halves built and swept. The leaving-row sweep is the wrong thing to make cheaper — its units are the cheapest in the solver, and every scheme for scanning it less often pays in trajectory and in wrong answers (D82, D84) |
| Barrier and crossover | **missing** | not optional at large scale |
| MILP: branch and bound, cuts, heuristics | **missing** | [14][15][16][17][18] |
| Deterministic parallelism | **missing** | [10][13] |

Citation numbers are the bibliography in `docs/archive/PLAN.md`.
Implementation works from those and their kin only — never another solver's
source (D12).

## 4. Controlling a solve

**This section has landed.** It was the gap a user felt first — the whole API
configured nothing at all.

**What is controllable is the contract, not the method.** A caller sets what
depends on their problem and which the solver cannot know: how much precision
their data deserves, how long they will wait, where log lines go. How the
problem is solved is the solver's to decide — the pricing rule, when a
carried weight stops being worth keeping, when to refactorize, whether a
sparse or dense path is cheaper. Nobody linking this library can be expected
to know whether their model wants Devex or steepest edge, and an option that
asks them is a problem handed back to the caller.

| | status | |
|---|---|---|
| Work limit, time limit | **done** | |
| Set the primal and dual tolerances | **done** | `jaos_set_primal_tolerance`, `jaos_set_dual_tolerance`; 0 restores the default |
| Logging and verbosity | **done** | `jaos_set_log_callback`, `jaos_set_log_level`; four levels, silent until a callback is installed |
| Callbacks | **done** | `jaos_set_progress_callback`; a watcher may look and may stop a solve, never steer one. Asked on a fixed iteration count, so *when* it is asked is reproducible; a stop is `JAOS_SOLVE_INTERRUPTED` and keeps its basis, so the next solve resumes (D79) |
| Choose the algorithm | **out of scope** | the solver picks; see above |
| Turn scaling off or pick the mode | **out of scope** | same, and it is a method question |

## 5. Reading an answer

| | status | |
|---|---|---|
| Status, objective, values, activities, duals, reduced costs | **done** | |
| Where every variable rests in the basis | **done** | `jaos_basis` |
| Iterations and work units | **done** | |
| **Solve time** | **done** | `jaos_solve_time`, seconds of the last solve. The only number JAOS reports that is not reproducible, and the header says so and says not to diff it |
| Independent solution checker | **done** | it bounds unbounded variables by what the rows imply, propagated to a fixed point, and reads the verdict off only the terms from bounds the model declared — so a slack implied bound cannot cost a correct answer its verdict. 64 of 110 accepted answers are now certified against 12 before, `etamacro` among them, and the largest term the identity still cannot take is 3e-08 against 2.25e-07 (D91). `relative_suboptimality` reads 6.9e-05 where `pilot` is right and 5.02e-03 where it is wrong; the gate watches it, and no verdict reads it on one instance's worth of evidence |
| A certified lower bound on suboptimality | **partial** | `certified_suboptimality`, sound and never overclaiming, but it reads the same ~1e-25 on answers known to be 1.04e-3 wrong as on correct ones — the step it uses cannot move at a vertex (D73) |
| Sensitivity and ranging | **missing** | |
| Infeasibility and unboundedness certificates | **missing** | |
| Exact rational verification of a final basis | **missing** | |

## 6. Writing

| | status | |
|---|---|---|
| Write MPS | **missing** | |
| Write LP | **missing** | |
| Write a solution file | **missing** | |

## 7. Using it from another language

| | status | |
|---|---|---|
| C API | **done** | `include/jaos.h`, the only header |
| Python | **missing** | |
| Anything else | **missing** | |

---

## 8. The bars it has to clear

| | status |
|---|---|
| Netlib standard set, 94 instances: optimal, objective within tolerance, checker green | **red at HEAD** — 94 solved, 94 objectives match Koch, 89/94 checker ok; what remains is a dual-recovery defect in postsolve, and the primal point is certified on all 94 (`TODO.md` #1). The committed baseline record reads 93/94 |
| Kennington subset, 16 instances | **green at HEAD** — 16 solved, 16 objectives ok, 16/16 checker ok, and no field differs from the committed baseline (D99) |
| Netlib infeasible subset, 29 instances: refused, no false optima | **pass** |
| Determinism across two solves and across runs, all 139 | **pass** — the second solve clears the basis first, or it would be a warm re-solve and would measure a sequence of calls rather than the solver (D68) |
| Warm re-solve against cold, one branching step per instance | **measured: 0.0052 of the iterations, 0.0164 of the work** on 92 of the standard 94; 0.0006 and 0.0041 on 11 of Kennington's 16 (D69, improved by D90). Both answers go through the independent checker, and 0 of either set is refused — the cold half of that was added by D92, which is what it caught. The standard-set work figure read 0.0162 before D93 redefined what the dense ratio test charges, and rose because a cold solve makes thousands of dense calls where a warm re-solve makes a handful, so the saving comes mostly off the denominator |
| Full suite clean under ASan and UBSan | **pass** |
| Reader robustness under fuzzing | **pass** |
| Competitive gap at tier T0 vs **HiGHS 1.15.1** | **measured: 3.72x slower** (D52, D53, D60, re-taken with three competitors in D83) |
| Competitive gap at tier T0 vs **SoPlex 8.0.3** | **measured: 1.34x slower**, faster on 10 of 22 |
| Competitive gap at tier T0 vs **Clp 1.17.11** | **measured: 3.77x slower**, on 1.67x the iterations and 2.26x per iteration (D83) |
| Rungs T1–T3, which price presolve and algorithm choice | **measured: presolve is worth 1.42x to HiGHS and 1.14x to SoPlex; free algorithm choice is worth nothing, on identical iteration counts** (D81) |
| MIPLIB 2017 easy subset | not started |
| MIPLIB 2017 benchmark subset | not started |

**Everything above these is correctness, and correctness is table stakes.**
The gap now has numbers, and they decompose the same way against both rivals:

| tier T0 | vs HiGHS | vs SoPlex | vs Clp |
|---|---|---|---|
| time per solve | 3.72x | 1.34x | 3.77x |
| iterations | 1.47x | **0.70x** | 1.67x |
| **time per iteration** | **2.54x** | **1.92x** | **2.26x** |
| JAOS faster on | 0 of 18 | 10 of 22 | 0 of 17 |

JAOS takes **30% fewer iterations than SoPlex** and is still slower. The
search is competitive; each iteration costs two to three times what it
should. **And three separately written dual simplexes agree on that while
disagreeing about everything else** — Clp takes 1.67x JAOS's iterations where
SoPlex takes 0.70x — so the per-iteration cost is a property of JAOS and not
an artefact of one rival (D83).

**It is two targets, and the set mean hides which is which (D63).** Per
instance the tail splits in half:

| | time | iterations | per iteration |
|---|---|---|---|
| `maros-r7` | 25.6x | 2.3x | **11.0x** |
| `pilot` | 13.4x | **4.7x** | 2.9x |
| `pilot87` | 13.2x | **4.6x** | 2.9x |
| `greenbea` | 8.1x | **2.9x** | 2.8x |

**A cheaper iteration** is `maros-r7`'s problem, and it is coming down:
`fit2p` was 17.5x per iteration and is 5.0x (D55, D56), `maros-r7` was 16.5x
and is 11.0x (D58, D59). All four of those entries were work the counter
never billed, found by profiling the build that ships.

**Fewer iterations** is what `pilot`, `pilot87`, `25fv47` and `greenbea`
need, and the cause is measured: steepest-edge weights are discarded on
80–93% of their iterations, which prices them by largest infeasibility
rather than by steepest edge. The threshold that discards them is bounded on
both sides by correctness and cannot simply be loosened (D63).
