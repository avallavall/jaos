# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## 1. The postsolve defect — the gate is red, on a dual term only

**Five standard-set answers are refused by the checker and no Kennington one
is: `25fv47`, `bnl1`, `bnl2`, `e226`, `vtp-base`.** This file owns that
count; other documents point here rather than restating it. Per-instance
terms: `bench/measurements/02-05/gate/final-netlib.txt`.

Every failing term is dual. The primal point is certified on all 94, all
objectives match Koch's reference and all 139 instances are deterministic —
so what is left is the multipliers, not the reconstruction. The row-residual
half was a different defect and is closed (D99): it was the bounded singleton
column judged against bounds that had stopped describing its row, and fixing
it moved 14 instances to ok without moving a single work unit.

The defect is in presolve's own dual recovery, and that is measured rather
than inferred. Compiled with `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` — canary: the
`presolve=` field must come back unreduced, and it does — all five are
checker ok with `dual=0` and rows at 1e-13
(`bench/measurements/02-05/no-presolve/`). With presolve on they carry `dual`
0.0705, 6.12, 9.81, 1.16 and 1.32e+03.

`bnl1`, `bnl2` and `e226` are bit-identical across D99's fix, digest
included, so nothing that change touched is involved in them. On `25fv47` and
`vtp-base` the primal residual collapsed (4.32 to 3.5e-13, 1.9e+04 to
2.23e-11) while the dual stayed identical to the digit.

The whole fall is still inside 02-03's diff (`9aba410`). Bisection, each tree
built detached and run against the committed baseline:

| tree | checker ok | rejected |
|---|---|---|
| `03c28c9` — end of 02-02 | 93/94 | 1 (`finnis`, D94) |
| `8425acc` — end of 02-03 | 78/94 | 16 |
| `d861b22` — end of 02-04 | 79/94 | 15 |
| `541f7dd` — D99 | 89/94 | 5 |

The approach, in order:

1. Four sites can produce a `max_dual_violation` here and the label matters,
   because each leads to a different repair: the singleton row's recovery,
   the forcing row's derivation, the `d_j = -a_ij * y_i` the singleton column
   publishes, and the duals the simplex publishes on surviving rows. Separate
   them before repairing.
2. The recorded lead, from the attribution
   (`bench/measurements/02-05/attribution-02-03/`): on `bnl1`, `bnl2`, `e226`
   and `vtp-base` the worst dual violation sits on a row the arena removed as
   `JM_PS_SINGLETON_ROW`, with the kept column strictly interior, reduced
   cost 0 and status 0. The `zero_works` test at `src/presolve.c` reads
   `sol_redcost[j]` as the reduced solve's, and three other producers can
   have written it first — a second instance of the stale-read class 02-04
   repaired once and D99 found again in the primal.
3. Reading the flags is necessary and not sufficient: `finnis` had
   `row_tightens_hi` set and its multiplier was still wrong, because two rows
   tightened and only the tighter one is responsible.
4. The fix, with a test shown failing on the pre-fix code first, and the
   dual-side case the checker must reject built deliberately.

Prohibited: widening any tolerance, clamping a recovered value, reverting a
reduction family. The objectives are already right; a tolerance that admits a
wrong multiplier admits every wrong multiplier of that size.

## 2. Presolve, to finish (REQ-presolve)

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
  infeasible (`bgindy`) + 4 Kennington (D96), minus whatever #1's fix changes.
  No rewrite while the gate is red.
- Presolve's measured number (the D-15 figure): work ratio and `J=1` time
  ratio reported separately, negative control beside them, raw readings to
  `bench/measurements/`, judged by `jaos-measurer`. The runner prints seconds
  as `%8.3f` and must be fixed first — see standing debts.
- Close-out: CHANGELOG entry updated, SPECS presolve row updated, this
  section deleted.

## 3. After presolve — the rest of M2, in order

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

## 4. After M2 — feature expansion (decided 2026-08-13)

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
