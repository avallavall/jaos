# SPECS — what JAOS is built to be

A mathematical-programming solver written from scratch in C23, competitive
with the serious open solvers and usable as a library by someone who did not
write it.

This file is the target. `PLAN.md` is the order the work happens in,
`CHANGELOG.md` what has landed, `DECISIONS.md` why. Nothing is in scope that
is not on this page.

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

Status is **done**, **partial** or **missing**. "Partial" always says what is
missing.

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
| Add or delete rows and columns after load | **missing** | |
| Change a bound or a cost | **done** | `jaos_set_col_cost`, `jaos_set_col_bounds`, `jaos_set_row_bounds`; each discards the answer the model was holding |
| Change a coefficient | **done** | `jaos_set_coefficient`; zero deletes the entry, a new index inserts one, and both derived copies are rebuilt |
| Re-solve warm from the previous basis | **missing** | the dual simplex is the right method for it and none of the plumbing exists |
| Load a starting basis | **missing** | |

## 3. Solving

| | status | |
|---|---|---|
| Dual simplex | **done** | steepest-edge pricing [8], Harris two-pass ratio test with bound flipping [7][19], dual phase 1 by artificial bounds [21], Bland fallback on a detected cycle |
| Sparse LU, Markowitz threshold pivoting | **done** | [4][6][20], Forrest-Tomlin updates [5], singular-basis repair |
| Scaling | **done** | Curtis-Reid [11], geometric-mean equilibration as an option |
| Hyper-sparsity in the triangular solves | **partial** | [9]: both solves report their pattern, the passes billed for every slot are not all reduced |
| Presolve | **missing** | the largest single algorithmic gap |
| Primal simplex | **missing** | needed for crossover, for warm starts, and for models the dual handles badly |
| Crash basis | **missing** | [12]; measured once and refused: it destroys the exact starting steepest-edge weights the slack basis gives |
| Partial and multiple pricing | **missing** | [1] |
| Barrier and crossover | **missing** | not optional at large scale |
| MILP: branch and bound, cuts, heuristics | **missing** | [14][15][16][17][18] |
| Deterministic parallelism | **missing** | [10][13] |

Citation numbers are the bibliography in `PLAN.md`. Implementation works from
those and their kin only — never another solver's source (D12).

## 4. Controlling a solve

**Most of this section has landed.** It was the gap a user felt first — the
whole API configured nothing at all — and what is left of it is callbacks.

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
| Callbacks | **missing** | |
| Choose the algorithm | **out of scope** | the solver picks; see above |
| Turn scaling off or pick the mode | **out of scope** | same, and it is a method question |

## 5. Reading an answer

| | status | |
|---|---|---|
| Status, objective, values, activities, duals, reduced costs | **done** | |
| Where every variable rests in the basis | **done** | `jaos_basis` |
| Iterations and work units | **done** | |
| **Solve time** | **missing** | required by the premises above |
| Independent solution checker | **partial** | it certifies a suboptimality bound it cannot prove whenever the improving direction is unbounded (D47) |
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
| Netlib standard set, 94 instances: optimal, objective within tolerance, checker green | **pass** |
| Kennington subset, 16 instances | **pass** |
| Netlib infeasible subset, 29 instances: refused, no false optima | **pass** |
| Determinism across two solves and across runs, all 139 | **pass** |
| Full suite clean under ASan and UBSan | **pass** |
| Reader robustness under fuzzing | **pass** |
| Competitive gap at tier T0 vs **HiGHS 1.15.1** | **measured: 3.70x slower** (D52, D53, D60) |
| Competitive gap at tier T0 vs **SoPlex 8.0.3** | **measured: 1.31x slower**, faster on 11 of 22 |
| Competitive gap vs Clp | not measured |
| Rungs T1–T3, which price presolve and algorithm choice | not measured |
| MIPLIB 2017 easy subset | not started |
| MIPLIB 2017 benchmark subset | not started |

**Everything above these is correctness, and correctness is table stakes.**
The gap now has numbers, and they decompose the same way against both rivals:

| tier T0 | vs HiGHS | vs SoPlex |
|---|---|---|
| time per solve | 3.70x | 1.31x |
| iterations | 1.47x | **0.70x** |
| **time per iteration** | **2.52x** | **1.87x** |
| JAOS faster on | 0 of 18 | 11 of 22 |

JAOS takes **30% fewer iterations than SoPlex** and is still slower. The
search is competitive; each iteration costs two to three times what it
should. And the per-iteration ratio comes out the same whichever rival
measures it — `truss` 1.5x and 1.5x — so it is a property of JAOS rather than
of a comparison.

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
