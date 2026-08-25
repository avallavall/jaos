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
0.1.1.

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
| Dual simplex | **done** | steepest-edge pricing [8], Harris two-pass ratio test with bound flipping [7][19], dual phase 1 by artificial bounds [21], Bland fallback on a detected stall. What looked like Bland failing to finish off a cycle (D72) was the factorization it was pivoting on having stopped describing the basis; the trigger above is the repair (D86). The ratio test's dense branch enumerates its candidates from a maintained nonbasic set rather than from the whole model — the same candidates in the same order, which is what keeps it out of what Harris's two passes guarantee, and it is not established that it buys time (D93). **The right-hand side `x_B = -B^-1 (N x_N)` and the residual the refinement step subtracts from it are both built with Neumaier compensation** — it is a sum over the nonbasic columns in column order, and a row that met a large term before many small ones was dropping the small ones, which made `-DJAOS_NO_PRESOLVE` refuse a model whose feasible point is exactly representable (D168). The residual was refused once on the argument that its terms already carry the factorization's error, and the measurement overturned it: three `pds` instances published a column outside its own declared bound, worst 8.81e-13, and stop (D171) |
| Sparse LU, Markowitz threshold pivoting | **done** | [4][6][20], Forrest-Tomlin updates [5], singular-basis repair |
| Stability trigger on the triangular solves | **done** | the pivot element is computed twice each iteration, by BTRAN and by FTRAN; past `LU_AGREE_TOL` the pivot is declined unbilled and the factorization rebuilt. Free — both numbers are already paid for (D86) |
| Scaling | **done** | Curtis-Reid [11], geometric-mean equilibration as an option |
| Hyper-sparsity in the triangular solves | **partial** | [9]: both solves report their pattern, the passes billed for every slot are not all reduced |
| Presolve | **partial** | six families live behind a cascading round loop: empty rows and columns, singleton rows, cost-0 singleton columns (D95), fixed columns, forcing and redundant rows, and the implied free column singleton (D105, D106) — every cap and window measured (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_ROUND_ULPS = 8`, `PRESOLVE_IMPLIED_FREE_ULPS = 8`, `docs/tolerances.md`). `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` compiles it out and reproduces the pre-presolve baselines bit for bit (D96). Deferred, with the count that defers them and an executable reopen condition: duplicate rows and columns, dominated columns, worth 0.15% of rows and columns on these 139 models (D101). Refused: bound tightening, measured six ways (D97) — and D106 lands beside it rather than behind it, because an implied bound used as a predicate narrows nothing. The postsolve defect is closed in both halves, the row residual (D99) and the dual (D100); an infeasible model is no longer published OPTIMAL (D102); presolve reads the model's sense and the three windows that judge a residue are a count of roundings rather than a tunable (D103); and a removed column now owes its share to every row it touches, which two families had never paid (D106). All three sets pass with 94/94 and 16/16 checker ok and 29/29 correctly refused (the infeasible set carries no checker verdict and no digest). **What it is worth**, presolve on over off, geometric mean of per-instance ratios: work 0.810x standard, 0.651x Kennington, 0.084x infeasible (D103), and D106 took a further 0.9527x off the standard set on top of that — `maros-r7` alone goes from 21010708013 work units to 328053926. Open, and none of it blocks the gate: the published basis breaks `jaos.h`'s row-count promise on 48 of netlib's 188 solves and on none of Kennington's, down from 132 and 24 (D131, repaired by D138 and D139, re-counted by D167), presolve makes `grow22` and `grow7` 11.16x and 8.56x more expensive, and D106's own family makes `greenbeb` 1.5126x — `TODO.md` owns these. **Closed since**: the row bounds `cur_rl`/`cur_ru` keep their residue through a Neumaier accumulator, which was the last uncompensated running sum in the file and the source of three wrong answers judged by four windows that did not know how it was computed (D162 to D166; the counts those entries added were removed again once the drift was gone). Also: the replay's empty intersection of an ulp, which put a value outside a declared bound on `bnl1` and `finnis` and stopped eleven of the 94 from running under `-UNDEBUG` (D152), and every debug build now verifies each published row activity against the published columns (D153). Field value: 1.42x to HiGHS, 1.14x to SoPlex (D81) |
| Primal simplex | **partial — phase 2 only** | **Phase 2 landed 2026-08-25 behind `cfg.force_primal`, a development switch and not an option (D188).** Dantzig pricing, sharing `pivot()` and the pricing row with the dual. **A composite phase 1 landed the same day and 55 of 94 END in agreement with the dual**, at a work geometric mean of 3.9023x over that set — but the primal's phase 2 runs **97 iterations across the entire set** — one iteration on 80 of the 94, none more than ten — while **60.5% of every iteration is a DUAL iteration** and 39.5% is phase 1, so that figure is not the primal solving 55 models (D194, corrected by D195); guarding phase 2 as well leaves 17 optimal (D194), and which number this row should carry is an open decision in `TODO.md` §0; 7 more overrun a 10x budget and 32 fail where the dual succeeds. D190 published 64, D191 corrects it to 54 — a guard it documented at two sites was applied at one — and D193 takes it to 55 by guarding a third site nobody had named. What is left is in `TODO.md` §0's build order — phase 1 is stage 4, Devex stage 5, the unboundedness verdict stage 7. It is the only missing feature anything in `TODO.md` waits on: it blocks crossover, crossover blocks D97, and D97 unlocks 8.55% of netlib's live rows and 29.36% of Kennington's. Building it also makes `can_move`'s units conflation live, which D184 measured as dead only because the primal pivot path does not exist. Needed for crossover and for the warm starts the dual cannot serve. **A third dependent, found 2026-08-25:** `classify_optimum` refuses out loud when a column is held by an invented bound and stopped by a real constraint, because lifting the loan and re-solving needs a primal pivot; D19 owns the population that reaches it, and no instance in `bench/results/*.txt` does. No longer needed for carried defect 4: the primal clean-up already owns a ratio test and a basis change, and reading the reduced cost's sign rather than the status is all a nonbasic free variable ever needed from it (D85). **Not a speed argument:** given free choice both rivals ran the dual on every instance, with iteration counts identical to being forced (D81) |
| Crash basis | **missing** | [12]; measured once and refused: it destroys the exact starting steepest-edge weights the slack basis gives |
| Partial and multiple pricing | **measured and refused** | [1]: both halves built and swept. The leaving-row sweep is the wrong thing to make cheaper — its units are the cheapest in the solver, and every scheme for scanning it less often pays in trajectory and in wrong answers (D82, D84) |
| Barrier and crossover | **missing** | not optional at large scale. **D97 is a second dependent, found 2026-08-21**: its ideal design detects an imposed-bound collision at postsolve, and the remedy there is a crossover, so without one D97's first version must decline 20-35% of the reduction to avoid a hazard measured at 0.012% (`bench/measurements/02-88/`). **A gap in the chain, found 2026-08-25:** crossover as published starts from an interior point and JAOS has none, so `TODO.md` §0's chain assumes a starting point that does not exist. Megiddo and Bixby–Saltzman still apply from a feasible non-basic point plus a ranking, but where that point comes from has to be decided before the crossover is designed (`docs/research/primal-simplex.md` §5) |
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
| Status, objective, values, activities, duals, reduced costs | **done** | The objective is `obj_offset` plus a compensated sum of `c_j x_j` over the published values, taken on the model that publishes them — `jaos.h` promises the objective OF the solution held, and until D169 the sum was naive and the presolve paths reported a reduced model's number instead. **It is the correctly rounded EXACT objective of the published point on 110 of 110**, worst 0.493 ulp, measured against an oracle that rounds nowhere (D173, `bench/measurements/02-83/`) — this number is finished and should not be reopened. 109 of the 110 also agree bit for bit with `jaos_check_solution`, against 34 of 94 before D169, and each product's own rounding is recovered with Dekker's split (D172). **The one that differs is `finnis`, and D173 settled which of the two is wrong**: the published objective is 0.338 ulps from exact and the checker is 790, because `long double` cannot hold a binary64 product's 106 bits. Reading that gap as mutual uncertainty inside the checker's own error was too generous to the checker (D169, D172, D173) |
| Where every variable rests in the basis | **partial** | `jaos_basis`. The statuses are right; the **count** was not. `jaos.h` promises exactly `num_row` of them are basic, and 70% of solves broke it (D131). Two postsolve repairs closed most of it: **Kennington publishes a valid basis on every solve**, netlib on 140 of 188 (D138, D139, re-counted by D167). The 48 that remain are three named shapes in `TODO.md`. No answer was ever wrong, and the cost is more than a lost warm start: on five netlib instances a column published BASIC carries a published reduced cost its own status forbids, worst 15018.5 on `nesm`, so `col_dual` and `jaos_basis` contradict each other (D170). Nothing reads the basis back except `build_warm_basis`, which since D151 **repairs** a mapped count short by at most `WARM_REPAIR_MAX_SHORT = 4` by promoting logicals and falls back to a cold start beyond that. A long count is still refused |
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
| Netlib standard set, 94 instances: optimal, objective within tolerance, checker green | **green at HEAD** — 94 solved, 94 shape ok, 94 objectives match Koch, 94/94 checker ok, 94 deterministic, and the committed baseline reads `0 regressed, 0 improved, 0 new`. The dual-recovery defect this row described was closed in both halves (D99, D100), the sense and window defects with it (D103). **"Match Koch" means within `1e-6 * max(\|ref\|, 1)` and four instances are measurably off the optimum inside that window** (D173): `pilot` by 2.31e-05, `pilot87` 1.04e-07, `scsd6` 1.12e-09, `etamacro` 1.31e-08, where 68 of the 93 Koch-referenced instances sit within half a unit of the arithmetic floor. `pilot` is the only netlib instance HiGHS, SoPlex and Clp all beat, and D174 attributes all four to `DUAL_TOL` |
| Kennington subset, 16 instances | **green at HEAD** — 16 solved, 16 objectives ok, 16/16 checker ok, and no field differs from the committed baseline (D99) |
| Netlib infeasible subset, 29 instances: refused, no false optima | **pass** |
| Determinism across two solves and across runs, all 139 | **pass** — the second solve clears the basis first, or it would be a warm re-solve and would measure a sequence of calls rather than the solver (D68) |
| Warm re-solve against cold, one branching step per instance | **pass, and the figures live in `bench/results/warm.txt` and `warm-kennington.txt`** rather than here — they have moved four times in two days and a copy in this table is what goes stale. Both answers go through the independent checker and `disagreed=0, rejected=0` on both sets; the cold half of that was added by D92, which is what it caught. **The figures are not comparable across presolve**: the 0.0164 this row used to carry was pre-presolve, D129 re-measured it at 0.0696 and named the basis-count defect behind it, and D151's capped repair is the current state. Read the records, not a remembered number (D69, D90, D129, D143, D151) |
| Full suite clean under ASan and UBSan | **pass** |
| Reader robustness under fuzzing | **pass** |
| Competitive gap at rung **P0** vs **HiGHS 1.15.1** | **measured: 3.15x slower**, on 2.04x the cost of an iteration (re-taken 2026-08-17 after D106; 4.13x at D104) |
| Competitive gap at rung **P0** vs **SoPlex 8.0.3** | **measured: 0.95x — faster per solve**, on 1.51x per iteration, faster on 11 of 20 (re-taken 2026-08-17; 1.18x at D104) |
| Competitive gap at rung **P0** vs **Clp 1.17.11** | **measured: 2.57x slower**, on 1.95x per iteration (re-taken 2026-08-17; 3.50x at D104) |
| All three, re-taken after D106 | **done 2026-08-17.** `maros-r7` fell from 72.5x HiGHS to 1.33x; the worst instance is now `stocfor3` at 30.0x. Record: `bench/compare/results/P0.txt`; the pre-D106 reading is kept at `results/P0-2026-08-14.txt` |
| Rungs T1–T3, which price presolve and algorithm choice | **measured: presolve is worth 1.42x to HiGHS and 1.14x to SoPlex; free algorithm choice is worth nothing, on identical iteration counts** (D81) |
| MIPLIB 2017 easy subset | not started |
| MIPLIB 2017 benchmark subset | not started |

**Everything above these is correctness, and correctness is table stakes.**
The gap now has numbers, and they decompose the same way against both rivals:

| rung P0, 2026-08-17 | vs HiGHS | vs SoPlex | vs Clp |
|---|---|---|---|
| time per solve | 3.15x | **0.95x** | 2.57x |
| **time per iteration** | **2.04x** | **1.51x** | **1.95x** |
| JAOS faster on | 1 of 17 | 11 of 20 | 2 of 13 |

P0 is each solver's own presolve on, the dual forced, no crash basis, one
thread — the rung M2's gate is judged on since JAOS gained a presolve (D104).
It was re-taken 2026-08-17 after D106; the pre-D106 reading (4.13x / 1.18x /
3.50x) is kept at `results/P0-2026-08-14.txt`. T0 keeps its definition and
its record as a historical rung, at 3.72x, 1.34x and 3.77x;
`bench/compare/README.md` owns both tables and this one cites it.

JAOS takes **37% fewer iterations than SoPlex** at the re-taken P0 and is now
marginally faster per solve; against HiGHS and Clp it is slower on both
counts. The search is competitive; each iteration costs between 1.5x and 2.0x
what it should. **And three separately written dual simplexes agree on that
while disagreeing about everything else** — at T0, Clp took 1.67x JAOS's
iterations where SoPlex took 0.70x — so the per-iteration cost is a property
of JAOS and not an artefact of one rival (D83). That per-iteration figure is
what M2 is aimed at; the re-take reads 2.04x / 1.51x / 1.95x against
2.29x / 1.73x / 2.39x before D106.

**It is two targets, and the set mean hides which is which (D63).** Per
instance the tail splits in half. The table below is the T0 reading and is
historical. At the 2026-08-17 P0 re-take `maros-r7` reads 1.33x on 1.05x the
iterations, and the cheaper-iteration half's owner is now `stocfor3`: 30.0x
on 2.9x the iterations, 10.4x per iteration. `pilot` and `pilot87` stay in
the fewer-iterations half, at 12.5x and 13.6x on 4.8x the iterations each.

| | time | iterations | per iteration |
|---|---|---|---|
| `maros-r7` | 25.6x | 2.3x | **11.0x** |
| `pilot` | 13.4x | **4.7x** | 2.9x |
| `pilot87` | 13.2x | **4.6x** | 2.9x |
| `greenbea` | 8.1x | **2.9x** | 2.8x |

**A cheaper iteration** was `maros-r7`'s problem, and D106 closed most of it:
`fit2p` was 17.5x per iteration and is 5.0x (D55, D56), `maros-r7` was 16.5x,
then 11.0x (D58, D59), and reads 1.27x at the 2026-08-17 P0 re-take. All four
of those entries were work the counter never billed, found by profiling the
build that ships. `TODO.md` §1e still owes the factorization measurement that
explains the D106 drop.

**Fewer iterations** is what `pilot`, `pilot87`, `25fv47` and `greenbea`
need, and the cause is measured: steepest-edge weights are discarded on
80–93% of their iterations, which prices them by largest infeasibility
rather than by steepest edge. The threshold that discards them is bounded on
both sides by correctness and cannot simply be loosened (D63).
