# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## 1. Implied free column singletons — the inequality rows, and three instances it hurt

**The equality-row half landed 2026-08-15 (D106).** It removes 1041 rows,
2040 columns and 47043 nonzeros over 17 of the 94 standard instances, and
**980 rows from `maros-r7`**, the figure stated before the code existed.
Kennington is bit-identical. Work over the standard set is a geometric mean
of 0.9527x. `maros-r7` alone goes from 21010708013 work units to 328053926
and from 10479 iterations to 2576.

Three things are open and they are in this order.

### 1a. Inequality rows — two thirds of the counted opportunity

`bench/measurements/02-10/`'s counter reads **3315 rows over 56 of the 94
standard instances** on the model as loaded. The shipped family reaches 1041
over 17, because TODO's own plan said equality rows only to start and because
a column's ORIGINAL degree must be 1.

An inequality row is harder for two reasons, both of them about the dual:

- `x_j` is not determined by its own row any more, so the postsolve has to
  pick a value rather than compute one. The row's implied box says which
  values are feasible; nothing yet says which is optimal.
- An inequality row's multiplier has a sign condition, and `y_i = c_j / a_ij`
  has no reason to satisfy it. When it does not, the reduction is unsound and
  the family has to decline — which is a test that does not exist today.

Count what a sign-respecting version would actually reach before building it.
The 3315 is an upper bound on it, not a forecast.

### 1b. The margin's absolute floor removes 1353 rows for nothing the set can see

`PRESOLVE_IMPLIED_FREE_ULPS` is a switch, not a dial. Swept 0, 1, 8, 64, 4096
with `make clean` between settings and five distinct md5s of `presolve.o`:
rows removed set-wide read **9992, 8639, 8639, 8639, 8639**. One step, at
zero, and four decades of nothing above it.

At zero the family removes 2394 rows instead of 1041, and the standard set
still reads **94 objective ok against Koch's exact rationals** and 94 checker
ok. An objective that is too good because a real bound was dropped is exactly
what that predicate catches, and it does not fire.

What stops zero from shipping is cost, not correctness: geometric mean
0.9627x against 8, `bore3d` 0.2524x, and **`d2q06c` 2.2163x**, which crosses
`bench/run.c`'s own 2.0x work bar and would make the gate report a
regression.

So the open question is narrower than "what should the constant be". The
window is `ULPS * DBL_EPSILON * max(1, scale)`, and it is the `max(1, ...)`
floor that declines the exact-equality cases — where `l_j` and the implied
end are both exactly representable and nothing cancelled, so the comparison
carries no error to protect against. Removing the floor would take those and
leave the rest declined. It needs `d2q06c` explained first: 2.2163x from
removing more rows is the same unexplained shape as §1c and §2.

### 1c. The margin covers the forward sum's error and not the recovery's

`PRESOLVE_IMPLIED_FREE_ULPS` is sized on the error in `ilo`/`iup`, which
`numerics-reviewer` confirms it covers with about 4x slack in every regime
asked of it. The recovery is a different quantity and nothing sizes it.

`x_j` comes back from `sol_row[i]`, accumulated in plain `double` with no
compensation and in replay order. For a row of `n` live terms that error
reaches about `n * eps * traffic / |a|`, while the margin promises
`8 * eps * traffic / |a|`. On a row of 500 entries with traffic 1e8 that is
**1.1e-5 against 1.8e-8**.

The symptom is not the silent one. The model is not relaxed and the objective
is not too good; `x_j` lands a little outside a bound that was supposed to be
non-binding, so it shows as a small COLUMN violation. It also needs the
optimum at the vertex where every other column sits on the extreme bound
`ilo` was computed from, which narrows it without removing it.

Two settlements, either of which is a measurement rather than a guess: scale
the constant by the row's live degree, or compensate the postsolve
accumulation with `ps_acc`. **Clamping the recovered `x_j` into the box is not
one of them** — that hides the row residue instead of removing it, which is
the shape D103's own repair was refused for.

### 1d. `greenbeb`, `scfxm3` and `forplan` cost more, and not in proportion

```
greenbeb   3.79e8 -> 5.74e8 work   1.5126x    3 rows and 10 columns removed
scfxm3     8.42e6 -> 1.14e7 work   1.3557x    3 rows and  3 columns removed
forplan    1.07e6 -> 1.25e6 work   1.1648x    5 rows and  5 columns removed
```

Three firings costing 51% is the same shape §2 already carries for the cost-0
singleton column on `grow22`, and it has the same missing piece: nothing
measured says which way a firing goes on a model that has not been run.

**None of the three crosses the gate's own 2.0x work bar** (`bench/run.c`,
`WORK_REGRESSION_FACTOR`), so the gate reports `0 regressed` and cannot see
any of this. It came out of the per-instance diff.

### 1e. `maros-r7`'s iteration got 15.7x cheaper and the model only shrank 31%

Work falls 64.0x while iterations fall 4.07x, so the cost of an iteration
falls 15.7x. The model loses 31% of its rows and 31% of its nonzeros. That
does not account for it.

The hypothesis with something behind it: `maros-r7`'s factors carry **4.801x**
its basis nonzeros, the worst ratio in the set (D46), and 980 of the columns
removed were singletons. If the fill collapsed with them, that is a fact about
the factorization and belongs in §5's factorization item rather than here. It
has no measurement. Take one before believing either half of this paragraph.

## 2. Presolve makes `grow22` and `grow7` far worse (opened by D103)

Not a regression from D103 — the records either side of it are bit-identical —
so this arrived with presolve and has never been asked about.

```
grow22   4.4e7 -> 4.9e8 work   11.16x    2179 -> 16381 iterations   7.51x
grow7    6.4e6 -> 5.5e7 work    8.56x     544 ->  4804 iterations   8.82x
```

They are the only two instances of 94 past 2x, and they set the standard set's
worst case while its geometric mean is 0.810x.

**Measured, in `bench/measurements/02-11/`.** One family fires on them and
nothing else: `JM_PS_SINGLETON_COL`, the cost-0 bounded singleton column
(D95), exactly 20 times on each of `grow7`, `grow15` and `grow22`. So it is
the whole of the difference, and `-DJAOS_NO_PRESOLVE` already measured the
other side.

What the 20 firings do is not visible in the `presolve=` field, which reports
20 columns and 20 nonzeros of 8252 and no rows at all. Every one of the 20
rows is an **equality `== 0`** and every one becomes a **range of up to
5e5**. Twenty exact pins become twenty things that constrain nothing in
practice, and a dual simplex steers by those pins. None of the 60 records
leaves a row unconstrained on both sides, so a check for "did this row become
free" would miss it: the damage is the width.

**What is not explained, and blocks a repair.** `grow15` gets the same 20
firings on the same rows with the same magnitudes — the three are one model
family at three sizes — and presolve **halves** its iteration count. So the
relaxation is not uniformly harmful and nothing measured says which way it
goes on a model that has not been run.

The candidate rule is to refuse the firing when the relaxation widens a row
beyond some multiple of its own scale; an `== 0` row becoming `[0, 5e5]` is an
unbounded relative widening. It needs a sweep on both sides and a campaign,
because the family pays 0.810x over the set as it stands and a rule that stops
it firing pays that back.

## 3. Doubleton equalities: a family nobody has counted (opened by D104)

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

## 4. A fourth instance set — the conclusions are population-dependent and say so

Every verdict in this repository is taken on 139 models: 94 netlib standard,
16 Kennington, 29 netlib infeasible. Netlib is a 1980s collection and it is
small. Three entries already in the record say the population is doing more
of the deciding than it should:

- **D46**: two instances are **74%** of the standard set's total work. A set
  total is a statement about `maros-r7` and `dfl001`.
- **D101** deferred three families because they remove 0.15% *on these 139
  models*, and its reopen condition is written as a model population, not as
  an opinion. A fourth set is the executable form of that condition.
- **§1's own counter** reads 3315 rows on netlib and **0 on Kennington**. The
  two sets already disagree about what is worth building.

**What blocks it, and how Kennington already solved it.** netlib has
published exact rational optima (Koch), which is what lets the gate say
`objective=ok`. A modern set has no such reference. Kennington answers that
by entering at a lower tier — `bench/README.md` calls it "the same, for
correctness only" — and a fourth set enters the same way: checker verdict,
solution digest, determinism and work units, no reference objective.

Candidates, none of them verified yet. Each needs its licence, its format
and its size read before anything is fetched:

- **MIPLIB 2017** LP relaxations. Large, modern, curated by ZIB.
- **Mittelmann's LP benchmark** (plato.asu.edu). The set HiGHS, SoPlex and
  Clp are actually compared on, which makes `bench/compare` directly
  readable against published figures.
- **The Mészáros collection.** Kennington is already part of that family, so
  the fetch path and `emps` handling are known to work.

Not before §1 closes. A new set rewrites nothing but it adds a fourth
baseline, and a baseline added mid-change cannot be read.

## 5. After presolve — the rest of M2, in order

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

## 6. After M2 — feature expansion (decided 2026-08-13)

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

## 7. Presolve is closed — what that means

**REQ-presolve is done.** Five families live, three deferred on a count with
an executable reopen condition (D101), the postsolve defect closed in both
halves (D99, D100), an infeasible model no longer published OPTIMAL (D102),
and the sense and window defects repaired (D103). `jaos-measurer` returned
**ACCEPT** on that last one from its own binaries, and the three baselines
were rewritten deliberately afterwards and confirmed by a following gate run:
all three now read `0 regressed, 0 improved, 0 new` and exit 0.

Nothing in this section is presolve being unfinished. All four are questions
presolve's own measurements raised, and each has its number already.

## Refusals and deferrals — what would reopen each

A refusal is a measurement, and a measurement is valid while its premises
hold. D24's reason expired when presolve landed, and it was caught by an
accident rather than a checklist (D94). This table is the checklist: when a
change satisfies a condition in the right column, re-ask that question. Until
then, do not — a refusal whose premise has not changed just fails again.

| decision | what was refused or deferred | reopens when |
|---|---|---|
| D101 | duplicate rows, duplicate columns, dominated columns — 0.15% left to remove on these 139 models | a model population where `bench/measurements/02-07/`'s counter reports a non-trivial share. The condition is executable, not a matter of opinion. Three pieces of the work have no published source and would have to be derived with their own tests |
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | the over-tightening on `pilot`/`pilot87`/`agg`/`maros` is derived, AND a dual postsolve for an imposed bound exists; then only under a campaign. **The condition is unchanged and the prize is not**: doubleton substitution needs the same machinery, and it is 8.55% of netlib's live rows and 29.36% of Kennington's, of which 19 rows in total can be built without it (§3). D97 weighed this feature alone; it now unlocks two |
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
- **`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` is RED, and was before this
  plan touched anything.** Two tests fail:
  `test_singleton_col_between_two_removals_solved_path` (expects 3 basic, gets
  2) and `test_a_fold_onto_the_box_at_scale_still_collapses` (expects 1, gets
  2). Both are white-box tests pinning presolve's own behaviour, and presolve
  compiles out of that build, so neither can hold there. Confirmed on a
  worktree at the commit before this plan started, same two, same messages.
  The reference build is the project's only oracle for output no predicate of
  the three sets reads (`jaos-testing`), so a red one is not a small thing:
  it means nobody runs it, and nobody has. The repair is a guard on each,
  the same `#if !defined(JAOS_NO_PRESOLVE)` the counter tests already carry.

- **Two positive tests had no fault-build guard.** `make test
  EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` did not compile at all until
  2026-08-15 (`make_frozen_row_infeasible_model`, fixed), and once it did,
  `test_a_maximised_singleton_row_is_owed_its_multiplier` and
  `test_a_maximised_empty_column_takes_its_upper_bound` failed because a
  fault build is meant to break exactly what they assert. Both now carry the
  guard every other positive test in the file has. **All seven off-by-one
  negative tests pass**, which is the first time that has been true since the
  compile broke.

- **A row's own width can be destroyed by the shift that removes a column,
  and no family is involved.** `cur_rl[i]` and `cur_ru[i]` are running
  differences, so a row the caller wrote as `[1, 2]` reads as a single number
  once a fixed column takes 1e17 off both: `ulp(1e17)` is 16, and `1 - 1e17`
  and `2 - 1e17` are the same double. The row handed to the simplex has lost
  its own width, and the answer is wrong by up to that width whatever
  presolve does next. `row_traffic[i]` is exactly the quantity that would say
  when this has happened and no site compares against it for this purpose.
  The implied free column singleton now declines such a row
  (`test_a_range_row_that_shifted_into_an_equality_is_declined`), which keeps
  that family inside its measured scope and does not repair this. Found by
  `numerics-reviewer`, 2026-08-15.

- **Two assignments in the replay are correct by an argument no assert
  states.** `JM_PS_EMPTY_ROW` writes `sol_row[i] = 0.0` and
  `JM_PS_SINGLETON_ROW` writes `sol_row[i] = rec->coef * xv`, where every
  other producer accumulates. Both hold today: an empty row had every column
  dead before it fired, and a singleton row had exactly one live column, so
  neither can be overwriting a share that already arrived. Both depend on
  arena order rather than on anything checked, and this class has now cost one
  campaign (D106). The cheap enforcement `numerics-reviewer` proposes is one
  debug-build pass at the end of `jm_postsolve_expand`: recompute every row's
  activity from `sol_col` and assert it matches `sol_row`. That is the
  predicate the checker already applies, extended from the instances that
  reach it to all of them.

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
