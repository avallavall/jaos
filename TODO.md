# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## 1. Presolve, to finish (REQ-presolve)

The last three families are deferred, not dropped: they have 0.15% left to
remove on these 139 models, and that count says nothing about the model
population where the literature reports them paying (D101). The deferrals
table below carries the reopen condition, and the counter that tests it is
committed at `bench/measurements/02-07/`. Presolve is complete at five
families.

Done, and not to be re-costed: the whole diff was read as one by
`numerics-reviewer` and produced D103, which repaired two defects it found and
one it introduced. All three sets ran under both settings with the
presolve-off control passing, and presolve's measured number (the D-15 figure)
is in D103 and `bench/measurements/02-09/`.

What is left:

- **The deliberate three-baseline rewrite** (`make netlib-baseline` and
  siblings), confirmed by a following gate run. Known by-design movements at
  `d861b22`: 26 netlib + 1 infeasible (`bgindy`) + 4 Kennington (D96). The
  gate has been green since D100 and the records have been bit-identical
  across D103, so nothing blocks it; it stays a deliberate act taken after the
  change is read and accepted, never a side effect.
- A verdict from `jaos-measurer` on D103's campaign, read in a context that
  did not produce the numbers.
- Close-out: SPECS presolve row updated, this section deleted.

## 1b. Presolve makes `grow22` and `grow7` far worse (opened by D103)

Not a regression from D103 — the records either side of it are bit-identical —
so this arrived with presolve and has never been asked about.

```
grow22   4.4e7 -> 4.9e8 work   11.16x    2180 -> 16382 iterations   7.51x
grow7    6.4e6 -> 5.5e7 work    8.56x     545 ->  4805 iterations   8.82x
```

They are the only two instances of 94 past 2x, and they set the standard set's
worst case while its geometric mean is 0.810x. The question is why a reduced
model is so much worse for the simplex than the model it replaced: the same
reductions on the other 92 instances cost nothing like this, so it is a
property of what these two become rather than of a family being expensive.
Both are `grow` models, which is the first thing to check.

Nothing here is a candidate repair yet. The first move is to find which
family produces the reduction that costs them, which the per-family counters
already report, and whether the extra iterations start immediately or after a
particular round.

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
  unset and is measured, not guessed. The ladder is recalibrated and the
  question is closed: **P0** is the rung, presolve on both sides, and T0 keeps
  its definition and its record as a historical rung
  (`bench/compare/README.md`). Standing at P0: **4.13x HiGHS, 1.18x SoPlex,
  3.50x Clp**, on 2.29x / 1.73x / 2.39x the cost of an iteration. The
  per-iteration figure is what M2 is aimed at and it has barely moved: three
  independently written dual simplexes still put JAOS's iteration between
  1.7x and 2.4x theirs.
- **HiGHS presolves what dominates this set and JAOS does not.** Opened by the
  P0 rung. Each solver's own presolve is worth about the same overall — JAOS
  0.739x, HiGHS 0.692x, Clp 0.670x, SoPlex 0.906x — but on `maros-r7` HiGHS
  reads 0.378x and halves its iteration count while JAOS reads 1.065x and
  removes nothing, and `stocfor3` is 0.198x against 0.965x. Those two are the
  worst instances in the whole comparison. Measured, in
  `bench/measurements/02-10/`: on `maros-r7` HiGHS removes 984 rows, 2803
  columns and **45% of the nonzeros**, where JAOS removes not one of any.

## 1c. Doubleton equalities: a family nobody has counted (opened by D104)

An `==` row with exactly two entries. One variable is substituted out, the row
goes, and every nonzero the substituted column had goes with it. It is not one
of the five live families and not one of the three D101 deferred, so no
measurement in this repository has ever counted it. On the model as loaded:

| set | doubleton equality rows | of all rows | instances carrying one |
|---|---|---|---|
| netlib | 6504 | **7.53%** | 67 of 94 |
| Kennington | 72459 | **28.15%** | 12 of 16 |

`ken-18` alone carries 48276, and it is the slowest instance in that set.
Against D101's 0.15% for the three deferred families this is fifty to a
hundred and ninety times larger, which makes it a different proposition rather
than a fourth deferral.

**Measured at presolve's exit, and that is where it stops being simple.**
The five live families barely touch these rows — 6504 to 6153 on netlib,
72459 to 60382 on Kennington — so the population survives to where a sixth
family would run. But a doubleton is substituted by eliminating one endpoint,
and unless that endpoint is free its bounds must be transferred onto the
survivor, which is bound tightening.

| set | surviving | share of live rows | with a free endpoint |
|---|---|---|---|
| netlib | 6153 | 8.55% | **19** |
| Kennington | 60382 | 29.36% | **0** |

**99.7% of the family is behind D97**, which refused bound tightening in six
designs, every one returning INFEASIBLE on a model that has an optimum. What
is buildable today is 19 rows across six netlib instances.

So this is not "build doubletons next". It is that **the prize behind D97 is
much larger than D97 knew**: it weighed bound tightening on its own, and this
is a second family that cannot exist without it. D97's reopen condition
already says what would settle it — derive the over-tightening on
`pilot`/`pilot87`/`agg`/`maros`, and have a dual postsolve for an imposed
bound. That work now buys two families instead of one.

The remaining cost question is unchanged and still unmeasured: a substitution
adds the eliminated column's terms into the survivor's rows, so it creates
fill. `subnz` says a non-interacting pass would remove 2.65% of netlib's
nonzeros and 5.13% of Kennington's, against row shares three times larger,
and neither figure accounts for fill.

## 1d. What HiGHS finds in `maros-r7` is none of the eight families

Open, and stated as unanswered rather than guessed at. `maros-r7` and `truss`
are indistinguishable on every structural count taken so far: 100% equality
rows, all of degree 4 or more, zero doubletons, zero degree-2 rows, zero free
columns. HiGHS reduces `maros-r7` by 31% of its rows and `truss` by nothing.
The 02-07 counter reads `remrow=0 remcol=0 dualfix=0` on `maros-r7`, so it is
not a duplicate row, a duplicate column or a dominated column either.

Whatever it is, JAOS has never scoped it. The next move is `literature-scout`
on what a published presolve does to a model of that shape — every column
half-bounded, every row an equality of degree 4 or more — with citations
checked, rather than another guess measured.

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
| D101 | duplicate rows, duplicate columns, dominated columns — 0.15% left to remove on these 139 models | a model population where `bench/measurements/02-07/`'s counter reports a non-trivial share. The condition is executable, not a matter of opinion. Three pieces of the work have no published source and would have to be derived with their own tests |
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | the over-tightening on `pilot`/`pilot87`/`agg`/`maros` is derived, AND a dual postsolve for an imposed bound exists; then only under a campaign. **The condition is unchanged and the prize is not**: doubleton substitution needs the same machinery, and it is 8.55% of netlib's live rows and 29.36% of Kennington's, of which 19 rows in total can be built without it (§1c). D97 weighed this feature alone; it now unlocks two |
| SPECS §3 | crash basis — destroys the exact slack-basis steepest-edge weights | pricing stops starting from exact steepest-edge weights; REQ-devex-pricing landing is the trigger |
| D74 | removing the re-entry loan — 2.372x `pilot87` iterations for 0.980x `pilot` | the oscillation mechanism itself changes (phase 4's investigation) |
| D63 | restarting weights to exact instead of 1.0 | the pricing rule changes; Devex would replace the question |
| D95 | eliminating nonzero-cost singleton columns | a dual-informed elimination design exists (the lift condition is in the entry) |
| D93 | the 4.2% time bar — unmeasurable on this host | a controlled host that satisfies D17 |
| D92/backlog | `pilot87`'s suboptimality bound, not understood | it blocks a gate (trigger already recorded) |
| D82, D84 | partial and multiple pricing | nothing scheduled — refused on wrong answers, not on a trade; a new scheme is a new decision, not a retry |
| D34, D11, D2 | `long double`, GMP, any external code | never, while the two absolute premises stand (locked 2026-08-13) |

## Standing debts — small, real, none blocks the sections above

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
- **The `warm` record predates presolve.** `bench/results/warm.txt` and its
  Kennington sibling were last written at `44c0ef6`, an 01-03 commit, so a
  diff against them reports the whole of presolve and cannot isolate a later
  change. It read 92 of 98 instances moved and flagged `scrs8` as a
  regression, where the movement against HEAD was five lines and no
  regression. Rewriting it is the same deliberate act as rewriting a gate
  baseline, with the same precondition.
- **A collapsed fold leaves a bound no record owns.** When a singleton row's
  intersection collapses inside the fold's rounding window, `src/presolve.c` puts
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
  **D103 gave this a stated size, and it is not bounded.** The midpoint is
  unclamped, so the published value can sit up to half the window outside a
  bound the caller stated: `4 * DBL_EPSILON * row_traffic[i] / |a|`. The
  traffic term is new with D103 and nothing caps it. A row whose fixed column
  left at `a*v = 1e9` and whose surviving singleton has `a = 1e-6` reads 0.89.
  `tests/test_presolve.c`'s `test_a_fold_onto_the_box_at_scale_still_collapses`
  documents the shape with the measured 2.4e-7 rather than asserting
  containment, and says in as many words that its own bound holds only because
  that model's traffic is zero.
- **The other half of `assert(want_lo <= want_hi)`: an empty intersection of
  an ulp.** D102 closed the half where the model really is infeasible. The
  half that remains is rounding: 11 of the 94 standard instances reach
  `ps_replay_one` with `want_lo` above `want_hi` by 2.2e-16 to 1.3e-15, and
  the replay publishes `want_lo`, which is that far outside the column's own
  box. `bnl1` row 581 wants 2.1850000000000005 from a column whose upper bound
  is 2.1850000000000001; the others are `finnis`, `80bau3b`, `bandm`, `cycle`,
  `dfl001`, `nesm`, `perold`, `pilot-ja`, `pilot`, `pilotnov`. The checker's
  tolerance absorbs it, so no answer is wrong today, but a declared bound is
  being published outside and the assert cannot be enabled while this stands.
  The repair is to clamp the published value into `[rec->lo, rec->hi]`, and it
  had to come after D102: clamping first would have masked the gap of 93 as
  though it were rounding. Measured 2026-08-14, readings in
  `bench/measurements/02-08/`.
- **`row_traffic[i]` saturates to `+inf` and never recovers.** The relaxation
  at `src/presolve.c` adds `max(|cmax|, |cmin|)` to it, and a column with a
  half-infinite box makes that infinite. Measured: all 117 standard-set rows
  that reach the frozen-row test at exactly zero margin carry
  `row_traffic == inf`. What should accumulate is the finite part actually
  subtracted from `cur_rl`/`cur_ru`. Fixing it changes the frozen-row test's
  behaviour on existing reductions, so it needs its own measurement rather
  than riding along. Found by `numerics-reviewer`, refused as out of scope for
  D102 with that reason.
  **D103 did not change its severity and the source now says why.** The two
  sites that read the traffic guard against an infinite value, and both guards
  are unreachable: the only site that can saturate it sets `row_frozen[i]`
  four lines later, `row_frozen` is never cleared, and the round loop's row
  pass skips a frozen row. They are guards against a sixth family that relaxes
  a row without freezing it, and they are labelled as that in the file.
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
