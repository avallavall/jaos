# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## 1. Presolve, to finish (REQ-presolve)

- Duplicate rows, duplicate columns, dominated columns — the last three
  families. Both detection tolerances swept (`make clean` between settings, a
  canary that must move); the near-tie cases constructed and shown rejected,
  not hoped absent; the duplicate/dominance boundary stated in the source and
  tested on both sides. The duplicate-column split is the one postsolve record
  that returns two primal values from one.
- `numerics-reviewer` over the whole presolve/postsolve diff, after the last
  family lands and before the campaigns. Every finding gets a disposition:
  fixed with its commit, refused with its reason, or carried with its
  destination.
- All three sets plus the presolve-off negative control, then the deliberate
  three-baseline rewrite (`make netlib-baseline` and siblings), confirmed by a
  following gate run. Known by-design movements at `d861b22`: 26 netlib + 1
  infeasible (`bgindy`) + 4 Kennington (D96). The gate has been green since
  D100, so the rewrite is no longer blocked by it; it stays a deliberate act
  taken after the change is read and accepted, never a side effect.
- Presolve's measured number (the D-15 figure): work ratio and `J=1` time
  ratio reported separately, negative control beside them, raw readings to
  `bench/measurements/`, judged by `jaos-measurer`. The runner prints seconds
  as `%8.3f` and must be fixed first — see standing debts.
- Close-out: CHANGELOG entry updated, SPECS presolve row updated, this
  section deleted.

## 2. After presolve — the rest of M2, in order

- **Factorization** (REQ-lu-fill-and-markowitz, REQ-hyper-sparse-downstream):
  the stale live counts Markowitz chooses on, and the fill — factors carry
  2.673x the basis nonzeros (4.801 on `maros-r7`); keep sparse triangular
  results sparse downstream (`stocfor3`: 6.79x per iteration, solves 43%,
  memset/memcpy/malloc 18.8% against 11.3% on `dfl001`). Left-looking
  elimination is a rewrite and needs its own decision first. Struck off by
  measurement, do not re-cost: `compact_pivot_row`'s row-to-position lookup
  (<0.5% on `maros-r7`); per-column arrays vs one arena (0.73% + 0.30%; the
  locality argument needs a cache simulation before it is costed or dropped).
- **Search path** (REQ-devex-pricing — acceptance stated: full gate with
  iteration count and per-iteration cost reported separately;
  REQ-reentry-oscillation — investigative first: 0.24% on `pilot87` at
  interval 24, D51 names the mechanism, D74 closed the only proposed cure,
  D89 removed the consequence).
- **Close M2** (REQ-m2-competitive-gate): needs a controlled host — D17 says
  a WSL number cannot close a gate, and this machine is Windows/WSL with a
  measured repeatability of 6.27% (D93). The per-instance guard factor is
  unset and is measured, not guessed. T0 is presolve-off on both sides by
  definition; how the comparison ladder recalibrates once JAOS has a presolve
  is undecided (`bench/compare/README.md`). Standing: 3.72x HiGHS, 1.34x
  SoPlex, 3.77x Clp (D83).

## 3. After M2 — feature expansion (decided 2026-08-13)

Two decisions are locked: the two premises are absolute (no external code,
bit-identical everywhere; a feature that cannot be built under them is not
built), and the goal is the best open solver that is deterministic across
machines and ships its own checker — not matching Gurobi.
`docs/feature-matrix.md` is the scoreboard; read it at every close. Whether
M2 finishes as scoped is answered when presolve closes.

Proposed order: cheap breadth first (write MPS, write LP, write a solution
file, Python bindings, sensitivity and ranging, infeasibility certificates),
then primal simplex, then barrier with crossover, then MILP, then
QP/conic/NLP/MINLP. VIPR-format certificates are a cheap differentiator —
only SCIP 10.0 emits them and JAOS already ships a checker. For exact
rational verification, GMP is excluded (D11); the methods to weigh are
iterative refinement, interval arithmetic in double, or hand-rolled
rationals for the final basis only.

## Refusals and deferrals — what would reopen each

A refusal is a measurement, and a measurement is valid while its premises
hold. D24's reason expired when presolve landed, and it was caught by an
accident rather than a checklist (D94). This table is the checklist: when a
change satisfies a condition in the right column, re-ask that question. Until
then, do not — a refusal whose premise has not changed just fails again.

| decision | what was refused or deferred | reopens when |
|---|---|---|
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | the over-tightening on `pilot`/`pilot87`/`agg`/`maros` is derived, AND a dual postsolve for an imposed bound exists; then only under a campaign |
| SPECS §3 | crash basis — destroys the exact slack-basis steepest-edge weights | pricing stops starting from exact steepest-edge weights; REQ-devex-pricing landing is the trigger |
| D74 | removing the re-entry loan — 2.372x `pilot87` iterations for 0.980x `pilot` | the oscillation mechanism itself changes (phase 4's investigation) |
| D63 | restarting weights to exact instead of 1.0 | the pricing rule changes; Devex would replace the question |
| D95 | eliminating nonzero-cost singleton columns | a dual-informed elimination design exists (the lift condition is in the entry) |
| D93 | the 4.2% time bar — unmeasurable on this host | a controlled host that satisfies D17 |
| D92/backlog | `pilot87`'s suboptimality bound, not understood | it blocks a gate (trigger already recorded) |
| D82, D84 | partial and multiple pricing | nothing scheduled — refused on wrong answers, not on a trade; a new scheme is a new decision, not a retry |
| D34, D11, D2 | `long double`, GMP, any external code | never, while the two absolute premises stand (locked 2026-08-13) |

## Standing debts — small, real, none blocks the sections above

- `bench/run.c` prints seconds as `%8.3f`: 8 standard-set instances carry no
  time ratio at all and 42 read exactly 1.0000x. Two lines; legal now that no
  measurement is in flight.
- `preflight.sh` does not check committed records for `baseline: NOT
  COMPARED`; such a record sat committed once already.
- The `REFACTOR_EVERY` 16..256 trajectory sweep is manual; three of M1's four
  defect closures came from it and no target automates it.
- Test ceilings drift silently — the `<62000` one drifted 2800 units with
  nothing watching. Re-measure a ceiling's both sides when touching its
  subject.
- `pilot87`'s suboptimality bound is not understood (`gap_positive` moves
  0.0068–26.7 across D92's variants while every answer is inside tolerance).
  Deferred with a trigger: it re-enters the plan if it blocks a gate, and it
  already refused two of D92's three candidate repairs.
- Restricting the candidate set ahead of `bfrt_walk`/`jm_harris_pick` is open
  and not refused (D93); it puts Harris's guarantees at stake and needs its
  own decision before any code.
- `galenet` makes two `dual_ratio_test` calls in a one-iteration solve —
  calls are not iterations in any work-saved arithmetic (D93).
- **`assert(want_lo <= want_hi)` fires on `bnl1` and `finnis`.** With
  assertions live both abort inside `ps_replay_one`. It predates D100 and
  fires identically against the code that change replaced, so the dual
  recovery is not the cause. No gate can see it: the release build and
  `bench/run` carry `-DNDEBUG`, and `make sanitize` keeps assertions but runs
  no instances. Two of the 94 abort the build the project otherwise treats as
  the stricter of the two. Measured on both trees, 2026-08-14.
- **The `warm` record predates presolve.** `bench/results/warm.txt` and its
  Kennington sibling were last written at `44c0ef6`, an 01-03 commit, so a
  diff against them reports the whole of presolve and cannot isolate a later
  change. It read 92 of 98 instances moved and flagged `scrs8` as a
  regression, where the movement against HEAD was five lines and no
  regression. Rewriting it is the same deliberate act as rewriting a gate
  baseline, with the same precondition.
- **A collapsed fold leaves a bound no record owns.** When a singleton row's
  intersection collapses inside `PRESOLVE_TIGHTEN_EPS`, `src/presolve.c` puts
  the midpoint of the two ends into both folded bounds, and that midpoint is
  no row's implied bound. The record that collapsed carries it and can still
  be paid; the record that produced the other side keeps its own value and
  compares unequal for ever. When the reduced cost's sign points at that
  other side, no record pays and the cost is left on a column strictly inside
  its own box. `min x0 + x1 + x2 s.t. x0 >= 5, x0 <= 5 - 1e-13, x1 + x2 >= 3,
  x0 in [0, 10]` publishes `x0 = 4.9999999999999503` with
  `max_dual_violation = 1`. **Not a regression**: the pre-fix code refuses it
  by the same magnitude on row 1 rather than on the column, measured on both
  trees. The repair is a decision about what a collapsed record should record,
  not a patch — the midpoint is deliberate and symmetric in the two ends, and
  whatever replaces it has to keep that. Found by `numerics-reviewer` and
  re-run independently, 2026-08-14.
- **A frozen row is never rechecked, so an infeasible model can be published
  OPTIMAL.** `min x0 s.t. x0 + x1 = 100, x0 in [4,4], x1 in [0,3]` has no
  feasible point, and `-DJAOS_NO_PRESOLVE` says so. With presolve on, the
  singleton column relaxes row0 and freezes it; the row pass
  (`src/presolve.c:595`) and the activity pass (883) both skip a frozen row,
  so nothing revisits it, and the replay's intersection comes out empty. Under
  `-DNDEBUG`, which is how `bench/run` is built, it publishes `x1 = 96`
  against a box of `[0,3]` — OPTIMAL, `max_col_violation = 93`. With asserts
  live it aborts on `assert(want_lo <= want_hi)`. Reachable through both
  postsolve paths. Predates 02-05 (HEAD judged against the original pair and
  came out empty the same way). This is the catastrophe `netlib-infeas`
  exists to catch and none of its 29 instances has the shape: **the repair
  needs an instance of this shape added to that set**, or the set cannot see
  it. Found by review, 2026-08-14; the comment in `ps_replay_one` states the
  premise that fails.
- **The basis the singleton-column family publishes breaks the count
  promise.** `jaos.h` promises exactly `num_row` of the `num_col + num_row`
  statuses are basic. It does not hold when the replay recovers the column
  strictly inside its own box: the column is published basic, and so is the
  row it was relaxed out of, which is one basic too many. Minimum case, on
  the `jm_postsolve_expand` path: `min x0 s.t. x0 + x1 = 7, x0 in [0,20],
  x1 in [0,100] cost 0` publishes 2 basic against `num_row = 1`, where
  `-DJAOS_NO_PRESOLVE` publishes row0 `AT_LOWER` and 1. The two-row
  `jm_postsolve_solved` model in
  `test_singleton_col_between_two_removals_solved_path` publishes 3 against
  2, and that test pins the count so the repair announces itself. When the
  column lands on its own bound instead, the count is right —
  `make_singleton_col_model` is that case. So the discriminator is which
  bound determined the value, which `ps_replay_one` has already computed
  when it picks `want_lo`; `JM_PS_FREE_COL_SINGLETON` derives its row's
  status as the mirror of its column's, with the row-count argument written
  beside it (`src/presolve.c` 1619-1634), and that is the pattern. Cost is a
  lost warm start, not a wrong answer — `build_warm_basis` falls back to cold
  when the count does not hold, and no checker or digest reads a status — so
  the repair is measured on `make warm` and `make warm-kennington`, which is
  what it changes.
