# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries say what changed and what it cost. The reasoning lives where it
belongs: `DECISIONS.md` for closed decisions, `TODO.md` for what is still
open, `bench/README.md` for the gate, and the commit each entry came from.

## [Unreleased]

### Added

- **The implied free column singleton.** A column with one matrix entry, in an
  equality row that already confines it strictly inside its own box, is
  substituted out exactly and the row goes with it. Nothing is narrowed and
  nothing is published, so this is not the bound tightening D97 refused.
  Removes 1041 rows, 2040 columns and 47043 nonzeros over 17 of the 94
  standard instances, and **980 rows from `maros-r7`**, which is the figure
  stated before the code existed. Kennington is bit-identical, as its counter
  said it would be. Work over the standard set: geometric mean **0.9527x**,
  `maros-r7` 0.0156x, `greenbeb` 1.5126x (D106). `PRESOLVE_IMPLIED_FREE_ULPS`
  is its margin and is swept in `docs/tolerances.md`.

- `bench/compare` gains the **P0** rung: presolve on both sides, the dual
  forced, no crash basis, one thread. It is T0 with one line changed per
  competitor, and it is the rung the M2 gate is judged on now that JAOS
  presolves. T0 through T3 keep their definitions and their records. JAOS
  reads 4.13x HiGHS, 1.18x SoPlex and 3.50x Clp per solve, on 2.29x, 1.73x and
  2.39x the cost of an iteration, and is faster than SoPlex on 10 of 21 (D104).

### Changed

- §1b closed: the implied-free window's `max(1, scale)` floor declines
  nothing — a floor-less build at the shipping 8 ulps reproduces all 94
  standard instance lines bit for bit, digests included, after proving
  itself by reproducing the committed margin-0 `maros-r7` reading (D109).
  The 1353 rows between margins 8 and 0 are declined by any nonzero window,
  not by the floor. `ULPS = 8` ships unchanged. Readings in
  `bench/measurements/02-16/`.

- §1b's blocker cleared: `d2q06c`'s 2.2163x at margin zero is four more rows
  of 2171 buying 1.7525x iterations, the D108 trajectory class, with the
  extra iterations on degraded pricing (`jm_dse_update` 0.45x per iteration
  against `jm_harris_pick` 1.42x — D63's restart mechanism). No relaxation
  defect hides in the number. The floor question in `TODO.md` §1b now stands
  alone. Readings in `bench/measurements/02-15/`.

- §1d closed: `greenbeb` pays D106's overcost in iterations (1.3779x,
  per-iteration flat) and `scfxm3` per iteration in the ratio-test path
  (`update_dual` 1.71x against an LU side at 1.05–1.11x), measured from the
  committed records and a callgrind pair calibrated to reproduce both
  records exactly. Two mechanisms, both downstream of an exact substitution,
  no site-local predictor — a trajectory refuse rule is refused (D108).
  Readings in `bench/measurements/02-14/`.

- The inequality half of the implied free column singleton is refused on its
  count (D107). A sign-classifying counter, calibrated on a hand model and on
  02-10's committed values, reads **341 sign-ok rows of the 3315 as loaded**
  — 10%, not the two thirds on record — 304 of them on the six `ship*`
  instances, none on `stocfor3`, zero on Kennington. The sign condition
  declines nothing a feasible bounded model can carry, and that is derived in
  the entry. Readings in `bench/measurements/02-13/`.

- `bench/compare`'s P0 rung re-taken after D106, at `a88e99b`: **3.15x HiGHS,
  0.95x SoPlex, 2.57x Clp** per solve, on 2.04x / 1.51x / 1.95x the cost of
  an iteration — faster than SoPlex on the geometric mean for the first time,
  11 of 20 per instance. `maros-r7` fell from 72.5x HiGHS to 1.33x; the worst
  instance is now `stocfor3` at 30.0x (`TODO.md` §5). The pre-D106 reading is
  kept at `bench/compare/results/P0-2026-08-14.txt`.

- Presolve carries its own objective, `cur_cost[]`. Every family read
  `m->col_cost` directly, so no reduction could change a cost even in
  principle; the implied free column singleton has to, because eliminating a
  column from a row pushes its cost onto every other column in that row
  (`TODO.md` §1). Nothing writes the array yet — it is a copy of the caller's
  costs for the whole run, and the reduced model, the records and the
  objective offset all read it instead. 110 solution digests and 29
  infeasibility verdicts unmoved, over 139 instances.

- Presolve is complete at five families. The three that were scoped and never
  built — duplicate rows, duplicate columns, dominated columns — are deferred
  with the count that defers them: 471 removable rows of about 297000 and 1526
  removable columns of about 1029000 across the three sets, 0.15% of each, and
  concentrated in `cre-a` and `d6cube`. The reopen condition is a model
  population where the committed counter reports more, so it is executable
  rather than an opinion (D101). Nothing is removed and no behaviour changes.

### Fixed

- A removed column's share of every row it touches. `JM_PS_SINGLETON_COL` and
  `JM_PS_FREE_COL_SINGLETON` wrote their own row and stopped, so every other
  row those columns touch — all of them dead, since both families need a live
  degree of one — was left short by that column's contribution, and
  `jaos_solution` published it. No digest covers a row activity and the
  checker recomputes its own, so nothing saw it until the implied free column
  singleton read one: `greenbeb`, `modszk1` and `tuff` reported row
  violations of 900, 1.67e5 and 27.7 with no column violation beside them
  (D106).

- `make test EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` compiles again.
  `make_frozen_row_infeasible_model` had no fault-build guard and
  `-Werror=unused-function` refused the whole file, so no negative test in
  `tests/test_presolve.c` could be run at all.

- `bench/compare/run-compare.sh` prints a competitor's summary when one of its
  instances reports zero iterations. Clp does, on `d6cube`, `maros-r7` and
  `woodw`, whenever it may choose its own algorithm; dividing by that count is
  a fatal error in awk rather than a NaN, so the whole block aborted before
  its first line and Clp's summary was silently absent from T1, T2 and T3 from
  2026-08-11. The records held the data throughout. Such an instance now stays
  in the time row, leaves the two iteration rows, and the count and names of
  what left are printed. Recomputed, Clp reads 3.79x, 5.88x and 5.92x at those
  three rungs (D104).

- Presolve reads the model's sense. Every cost-direction and dual-sign rule in
  the file was stated for MINIMIZE, which is the canonical form `src/check.c`
  and `src/simplex.c` convert into, and presolve was the stage that did not
  convert. `max x1` with `x1` an empty column of cost 1 in `[0, 5]` published
  objective 0 instead of 5, and in `[-inf, 5]` reported UNBOUNDED on a model
  whose optimum is 5. netlib is entirely MINIMIZE and `tests/test_presolve.c`
  had zero MAXIMIZE cases, so neither the gate nor the suite could see it.
  110 solution digests and 29 infeasibility verdicts unmoved (D103).

- `PRESOLVE_ROUND_ULPS = 8` replaces `PRESOLVE_TIGHTEN_EPS = 1e-9` at the
  three sites that ask whether a residue is rounding, and the old constant is
  deleted rather than left unread. The window is relative, so at model scale
  1e-9 stopped being small: a row missed by 1.5 came back OPTIMAL, a column
  published a fifth of a unit above its own declared upper bound, and D102's
  frozen-row test was walked through by a model infeasible by 0.5. Nothing on
  the three sets moves, because no feasible model among them carries a residue
  at any of the three sites — the twelve that do are all on `netlib-infeas`
  and none is below 3.69e8 ulps against a window of 8 (D103).

- A relaxed row is tested for feasibility once the boxes are final, so an
  infeasible model is no longer published OPTIMAL. Both the row pass and the
  activity pass skip a frozen row, correctly, and between them nothing asked
  whether it could still be satisfied: `min x0 s.t. x0 + x1 = 100, x0 in
  [4,4], x1 in [0,3]` came back OPTIMAL with `x1 = 96` against a box of
  `[0,3]` and a column violation of 93. No digest moved on any of the 110
  optimal instances, which is the whole claim — the change can turn OPTIMAL
  into INFEASIBLE and nothing else. Work rises where frozen rows are walked
  (+111057 netlib, +68894 Kennington, +3732 infeasible), and two infeasible
  instances now leave in presolve rather than the simplex: `pilot4i` from 408
  iterations and 7063304 units to 0 and 13185 (D102).
- `bench/run` prints per-instance seconds to six decimals instead of three.
  At millisecond resolution a solve under 500 us printed `0.000` and carried
  no time ratio at all, and the ones that did print landed on so few distinct
  values that a ratio between two runs read exactly 1.0000x for reasons that
  had nothing to do with the solver: `adlittle` and `stocfor1` both read
  `0.001`, and now read 0.000710 and 0.000708. The clock already supplied
  nanoseconds. Console only, so all three records come back byte-identical.
- A folded singleton row's multiplier now goes to the row whose bound the
  column rests on. Several rows can fold into one column, the replay reaches
  them in LIFO order, and the test that chose between them read the column and
  never the row, so the first record took the whole reduced cost and the row
  that owed it published zero. Standard set 89/94 to 94/94 checker ok, which
  closes the gate; 127 of the 139 records bit-identical and `netlib-infeas`
  entirely so; no work unit, iteration count or presolve dimension moved on any
  set. Seven further records move their digest, a dual moving between two
  dual-feasible assignments, and the published basis moves on three instances
  with `warm` unmoved on all three (D100).
- A cost-0 singleton column is recovered against the row bounds recorded when
  it left, not the row's original pair. The replay is LIFO, so the columns
  removed before it had not yet added their share to the activity it was
  judged against, and the published point missed the row by exactly their sum
  (D99). Standard set 79/94 to 89/94 checker ok, Kennington 12/16 to 16/16 and
  back to no field differing from its pre-defect baseline, the 29 infeasible
  lines unmoved; no work unit, iteration count or presolve dimension moved on
  any instance of any set. Five rejections remained after it, dual-side and
  independent, and D100 closed them.
- `jm_postsolve_solved` initialises the basis statuses it publishes. The
  arrays are allocated without zeroing and no replay writes a surviving frozen
  row's status, so what went out there — and into the next solve's warm start
  — was whatever the heap held. ASan and UBSan do not see an uninitialised
  read; valgrind does, and now reports none. The value is defined and still
  not the right one (`TODO.md`).

### Changed

- The planning layer is retired (D98). The record is `SPECS.md`, `TODO.md`,
  `DECISIONS.md`, this file, `docs/` and `bench/`; the per-change process is
  the loop in `CLAUDE.md`; raw measurement records live in
  `bench/measurements/<id>/`. `.planning/` (194 files) is deleted, its open
  items moved to `TODO.md` and its owed decision entries written (D94–D97).

### Added

- Presolve and postsolve, first five reduction families: empty rows and
  columns, singleton rows, cost-0 singleton columns (D95), fixed columns,
  forcing and redundant rows, behind a cascading round loop with measured
  caps (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_ROUND_ULPS = 8`, each swept with
  a canary). A model presolve solves outright publishes with no simplex run.
  `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` compiles it out and reproduces the
  pre-presolve baselines bit for bit (D96). Bound tightening was built six
  ways, measured and refused (D97). The checker's remaining refusals were
  dual-side and closed by D99 and D100.
  **What it cost, presolve on against presolve off**, geometric mean of
  per-instance ratios: work 0.810x on the standard set, 0.651x on Kennington,
  0.084x on the infeasible set where ten models never reach the simplex.
  Seconds on the six instances it removes the most from, `J=1`, minimum of
  three alternating rounds: **about 0.3x** against a negative control that
  reproduces at 1.0. An independent re-run of the same protocol reads
  0.3066x and 1.0012x, so the centre holds and four significant figures do
  not (D103). Two instances get much worse and are handed to `TODO.md`: `grow22`
  11.16x work and 2179 to 16381 iterations, `grow7` 8.56x and 544 to 4804.

- `jaos_solve_time`: seconds the last solve took. `SPECS.md` had carried this
  as missing since M1 while the premises required it — every run reported wall
  clock and the library did not, so anyone who was not JAOS's own bench runner
  could not get the number the whole competitive comparison is about. It
  covers the same span as the work counter, taken beside it at the end of
  publishing so neither is short by one BTRAN, and it shares one clock reader
  with the time limit so the figure a caller sees and the figure the budget
  was judged against cannot drift. Retired with the rest of the answer when
  the model is modified: seconds for a withdrawn result are a number about
  nothing. **The only non-reproducible number in the API**, and the header
  says so and says not to diff it against a stored one.

- `jaos_set_progress_callback`: a solve can be watched, and stopped. A watcher
  is told the iteration count, the work units and the total primal
  infeasibility — and no objective, because a dual simplex carries a point it
  cannot vouch for until it finishes and this library does not hand back
  numbers it will not stand behind. It may look and it may stop; it may not
  steer, and it may not touch the model. Asked every 64 iterations and never
  on a clock, so *when* it is asked is reproducible and the same answers give
  the same solve bit for bit — a callback that always continues returns the
  same bits as no callback, over all 139. A stop is the new
  `JAOS_SOLVE_INTERRUPTED`, appended to the enum rather than inserted, and it
  keeps the basis it stopped on, so solving again resumes instead of starting
  over (D79).

- `jaos_add_rows`, `jaos_add_cols`, `jaos_delete_rows` and
  `jaos_delete_cols`: the problem itself can grow and shrink, not only its
  numbers. Additions append, so no existing index moves and the prefix of
  every array copies straight over; a new row is a transpose rather than an
  append, and counting per column first makes it one rebuild instead of one
  insertion per entry. Deletion takes a **set** of indices, because deleting
  one at a time leaves the caller tracking a renumbering that shifts under
  them — given the set, JAOS renumbers once and refuses an index named twice.
  The stored basis survives exactly when what is left is still a basis, which
  is the one rule `jaos_set_basis` already enforces: rows arrive basic and
  columns nonbasic, so additions keep it and deletions usually do not. All 139
  digests unmoved — no path the gate walks calls any of this (D77).

- `jaos_check_report` gains `certified_suboptimality` and
  `unquantified_rays`: how much better the objective provably gets, and how
  many directions could not be quantified. The step comes from moving one
  column on its own with every other variable pinned — feasible by
  construction, so it is a guaranteed minimum and `|w| * t` is a certified
  lower bound on `P - P*` needing **no basis, no factorization and no
  reference value**, which is what D47 costed this route at. It recovers D47's
  constructed case exactly, 0.1, on a point where every other number reads
  zero. Largest certificate over all 110 reference answers: 4.98e-16, so it
  does not false-alarm; five instances carry a ray whose rate the checker
  calls zero, and those are counted rather than reported as infinite
  suboptimality. Decides nothing, and on the evidence it is not fit to: on
  `pilot` at the refactorization intervals where this solver is known to
  return an answer 1.04e-3 off with the checker green, it reads the same
  ~1e-25 it reads on the correct answers. A column moving alone is stopped by
  the first tight row and a vertex is what tight rows are, so it cannot fire
  where simplex answers live. Sound, and uninformative (D73).
- `jaos_check_report` gains `gap_certified` and `max_dropped_multiplier`: the
  checker now says when `gap_positive` is not the bound on suboptimality that
  `jaos.h` documents it as being. It stops being a bound whenever a
  multiplier's sign points at an infinite bound, because the term it owes the
  dual objective is minus infinity and gets dropped — two variables and one
  constraint build a point that is arbitrarily suboptimal with every number in
  the report reading zero. Neither field decides anything and no verdict
  moved: three gates PASS, 0 regressed. **98 of the 110 accepted answers carry
  a dropped term**, against the 15 D47 estimated, and not one of them reaches
  1e-6 — the whole exposure sits inside what the checker already calls zero
  (D71).

- **Resumable budgets.** A solve stopped by a work or time limit keeps the
  basis it stopped on, so raising the limit and calling `jaos_solve` again
  continues instead of starting over — until now the run's work was thrown
  away, which made a budget a way to abandon work and nothing else. An
  INFEASIBLE or UNBOUNDED verdict keeps one too, for the branch-and-bound node
  that differs by one bound. A numerical failure keeps none: it cannot corrupt
  an answer, but it is the one state the solver does not vouch for. There is
  still nothing to read in between, and `jaos_basis` still refuses — a
  stopping point is not a solution (D70).
- `make warm` and `bench/warm.c`: what warm re-solve buys, which the gate
  cannot say — the gate solves each instance once from a fresh load, and that
  is the one case warm starting does not touch. One branch-and-bound branching
  step per instance, warm against cold on the same perturbed model:
  **0.0055 of the iterations and 0.0166 of the work** over 92 of the standard
  94, and **0.0006 and 0.0041** over 11 of Kennington's 16 — geometric means,
  no disagreements and nothing the checker refused on either set. `grow15`
  takes 1 iteration against 20305, `cre-b` 1 against 17132, and the work ratio
  improves with model size because a warm solve's floor is two
  refactorizations however big the problem. Three checks before believing it —
  the cold number matches the gate's own iteration counts, the branch moved
  the optimum on 85 of 92 and 7 of 11, and every warm answer was verified
  independently. `make warm-kennington` runs the second set (D69).
- `jaos_col_cost`, `jaos_col_bounds` and `jaos_row_bounds`: the model can be
  read back, not only written. A caller who loaded the model knows what is in
  it; one who read it from a file does not, and being allowed to change a
  bound with no way to see it is not an API. The first program to need them
  was JAOS's own campaign, which cannot take a branching step without knowing
  whether `floor(x_j*)` is still above the column's lower bound (D69).
- **Warm re-solve.** A solve that reaches an optimum leaves its basis on the
  model, and the next solve starts from it: change a bound and solve again,
  and nothing has to be called. The basis is stored apart from the answer for
  exactly this reason — the answer is discarded when the problem moves, the
  basis is not. `jaos_set_basis` hands one in from elsewhere and
  `jaos_clear_basis` forgets it, without which one solve would make every
  later solve a re-solve for good. A basis that is wrong costs iterations and
  never the answer; one that would leave a nonbasic variable with no bounds was
  refused and the solve ran cold, because that pins a row's activity at zero
  and the method could not always price it back off — **that refusal is gone
  as of D90**, once D85 taught the clean-up which way a free column improves.
  All 139 answers unmoved (D68).

- `jaos_set_coefficient`: one matrix entry can be changed, created or
  removed. Three operations under one name, because the stored matrix keeps
  its columns ascending by row index with no duplicates and no explicit
  zeros — so zero deletes the entry and a new index inserts one in sorted
  position. Unlike a bound or a cost it invalidates the row-wise mirror and
  the scaling, both of which are computed from the matrix (D67).
- `jaos_set_col_cost`, `jaos_set_col_bounds` and `jaos_set_row_bounds`: a
  loaded model can be changed instead of rebuilt. Each one discards the
  answer the model was holding — an optimum computed for the problem as it
  stood describes a different problem once a bound moves, and reading it back
  would be a wrong number returned with confidence. Neither touches the
  matrix, so the scaling and the row-wise mirror stay correct and are left
  alone. `lower > upper` is accepted and comes back infeasible, which is the
  rule `jaos_load_lp` already applies (D66).
- `jaos_set_log_callback` and `jaos_set_log_level`: the solver stops being
  silent. Four levels, and no default destination — nothing is written until
  a callback is installed, because a library that writes to stdout cannot be
  embedded. Paced by iteration count and never by a clock, so two runs of one
  model produce the same lines. The closing line reports refactorizations,
  weight restarts and stalls: the three events four diagnoses this milestone
  had to patch counters into the solver to see. Solving at full verbosity
  returns the same bits as solving silently, checked bit for bit and over all
  139 instances (D65).
- `jaos_set_primal_tolerance` and `jaos_set_dual_tolerance`: the first two
  settings a caller has beyond the budgets. Both default to 1e-7 and 0
  restores that; a value that is not finite and non-negative is refused with
  a reason rather than clamped. All 139 answers are unmoved, because a model
  that sets nothing behaves exactly as before. The API configures the
  contract — precision, limits, where output goes — and never the method:
  which pricing rule, when a weight stops being worth carrying, when to
  refactorize are the solver's to decide (D64).
  it, and compiles again from the profile that produced. Worth **1.1122x**
  over the plain shipping build — three times what every optimisation flag in
  it is worth put together — with the gate green and all 94 digests unmoved.
  Kept out of `make` because it takes minutes and needs the instances
  downloaded first (D62).
- `-j N` on the acceptance runner and `J=N` on every netlib make target: the
  instances are solved N at a time, one process each, and the record is
  reassembled in manifest order. All 139 lines come out byte-identical to the
  sequential record. The standard set takes 84 s instead of eight minutes,
  the infeasible set 9 s instead of two and Kennington 8 min 21 s instead of
  thirty. The seconds printed alongside are
  inflated by the contention and the runner says so on every parallel run
  (D57).
- `jaos_basis` reports where every column and row activity rests in the basis
  behind the answer — basic, at a bound, or free. It is the part of a
  solution the values cannot carry: a basic variable sitting exactly on a
  bound reads no differently from a nonbasic one resting there, and only one
  of the two is a constraint the optimum is held by. PLAN 2.4 promised it and
  M1 had not delivered it (D33).
- A primal ratio test, used only to clean up after the dual solve. `greenbea`
  goes from REJECTED to checker ok in eight pivots, at fifteen significant
  digits of Koch's value, and `pilot`'s objective comes inside tolerance from
  390x outside it. Grows M1's declared scope by that ratio test and nothing
  else; the primal simplex stays out (PLAN 2.1). 0 regressed, 2 improved on
  the standard 94; 0/0/0 on the other two sets (D28).
- Bland's rule as a fallback a detected cycle switches on. `grow15` had been
  running to the iteration guard at 189201 iterations and now solves in 21653.
  Costs nothing where nothing cycles: `grow22`, `grow7` and `truss` are
  bit-identical (D26).
- The dual simplex re-enters from the settled point, moving a nonbasic to its
  other bound and running again. `nesm` goes from REJECTED to checker ok for
  seven iterations; `pilot` and `pilot87` improve two orders of magnitude on
  the dual violation. 0 regressed, 1 improved; 0/0/0 elsewhere (D25).
- Three fields in `jaos_check_report`: `gap_positive` and `gap_negative` split
  the gap into the two sums it is the difference of, and
  `max_row_violation_relative` reports the row residue against the row's
  traffic. No verdict reads any of them. The record carries all three per
  instance (D24).
- A fuzzer for both readers, `tests/test_fuzz.c`: truncation at every offset
  of every corpus file, seeded edits, random bytes, keyword salad, and named
  edge cases, each fed to both readers. 11543 cases in the suite, 1.6M under
  `JAOS_FUZZ_SCALE=200`, clean under ASan+UBSan. Closes the M1 gate's
  condition 4, which had been asserted and never tested (PLAN 2.9).
- Baselines for the Kennington and infeasible sets, with
  `make netlib-kennington-baseline` and `make netlib-infeas-baseline`. Both
  gates report PASS, which is the state in which a summary line cannot show a
  change (D21).
- `bench/run.c` takes `-e infeasible`: shape and determinism still hold, the
  reference objective and the checker drop out, and an instance returning an
  optimum fails as a false optimum. The only check in M1 that looks for a
  wrong answer rather than confirming a right one (PLAN 2.9).
- The two remaining M1 instance sets, pinned and running:
  `bench/netlib-kennington.manifest` (16) and `bench/netlib-infeas.manifest`
  (29), expanded with netlib's `emps` — fetched and checksummed by
  `bench/fetch.sh`, never stored here (PLAN Q6, D11). Kennington passes 16 of
  16 on the first run; the infeasible set refused 28 of 29 with no false
  optima. Each set fetches into its own directory, because `greenbea` names
  two different models across sets.
- `bench/netlib.baseline` records what every instance did, and `make netlib`
  diffs against it. The gate's own verdict read `NOT MET` for almost the whole
  of M1 and reads `PASS` now, and at neither end can it answer the question
  every change raises — did this make anything worse? Work may grow 2x before
  it counts as a regression.
  `make netlib-baseline` rewrites it and is never a side effect of running
  the gate (D21).

### Benchmark harness

- **Rungs T1 to T3, and Clp as a third competitor.** The ladder now says what
  the missing features are worth: free algorithm choice **nothing**, on
  iteration counts identical to being forced; presolve **1.42x** against HiGHS
  and **1.14x** against SoPlex; stock defaults nothing further (D81). Clp
  builds through its own pinned CoinUtils and Osi chain, and its reading is
  the point of having a third: the rivals disagree about JAOS's iteration
  count — 1.47x, 0.70x, 1.67x — and agree about the cost of an iteration —
  2.54x, 1.92x, 2.26x (D83).

- **The comparison was timing a warm re-solve.** It solves each model N times
  and keeps the fastest, and once a solve began leaving its basis behind every
  repeat after the first re-solved warm in one iteration and the minimum
  picked it — `25fv47` recorded 0.0015s and 0 iterations against a true 0.49s
  and 9459. The basis is cleared before each timed solve now, and the driver
  checks itself: cold repeats are bit-identical, so a disagreement in
  iterations or work aborts rather than being averaged in. No library code was
  involved and no committed figure was affected (D80).

- `EXTRA_CFLAGS` on the release and dev builds, empty in every shipping build,
  so a method constant can be swept over a range without editing the source
  between runs.

### Changed

- **The dual ratio test's dense scan walks the nonbasic set** rather than every
  variable, and bills what it visited rather than the dimension. The trajectory
  is identical on all 139 instances — 110 digests and 29 infeasibility verdicts
  unmoved — so the work counts fell because the charge was redefined underneath
  it, not because the solver took a different path. It costs 1.60% more
  instructions on `truss` and buys no time this host can resolve (D93).

- **The checker separates the suboptimality bound from the verdict**, which is
  what closes D47. A bound implied by the rows is sound but slack, so its term
  in the duality identity survives at an optimum and measures the bound rather
  than the point; feeding that to `dual_feasible` rejected correct answers, so
  the verdict now reads only terms from bounds the model declared and the
  bound keeps every term. With that the propagation is safe to iterate, and
  certification goes from 12 of 110 accepted answers to **64** — `etamacro`,
  the case D47 named as live at 2.25e-07, is now certified with nothing
  dropped. New `relative_suboptimality` reads 6.9e-05 where `pilot` is right
  and 5.02e-03 where it is wrong; the gate watches it in place of the dropped
  term, which propagation has reduced to noise (D91).

- **A warm start no longer refuses a basis with a free nonbasic.** D68 put
  that refusal in because the method could not price such a column back off
  zero and would publish a suboptimal point as OPTIMAL; D85 repaired that, so
  the refusal lost its premise. `cycle` goes from paying the full cold price —
  1537 iterations, its warm figures identical to its cold ones — to 16
  iterations and a ninth of the work. All 139 answers unmoved, since no cold
  solve reaches this code. The test that pinned the refusal now pins the
  repair, on D68's own example (D90).

- **The re-entry loop publishes its best round rather than its last.** On a
  trajectory that oscillates the two are different, and which one got
  published was decided by where `SETTLE_ROUNDS` happened to fall. "Best" is
  lexicographic — a round whose dual violation is inside tolerance beats one
  that is not, and only then does the lower objective win — because the first
  criterion tried, plain lowest objective, publishes a point the independent
  checker rejects. Both quantities are compared in the model's own space,
  since the scaling factors are per column and differ between the rounds being
  compared. `pilot87` at a refactorization interval of 24 now publishes a
  point whose dual violation the checker reads as zero, for 2.9e-9 relative of
  objective. No answer in the gate moves: at the shipped interval the loop
  converges and ends on its own best round (D89).

- **The acceptance gate watches the checker's dropped term**, which is the one
  correctness quantity no predicate covered. `checker` stays green while the
  guarantee behind it stops holding — that is the whole of D47 — and D82 is
  the receipt: a change that published an answer out of tolerance on `pilot`
  passed this gate. Judging a dropped term needs knowledge the checker does
  not have; noticing it *grow* does not. Baselines carry it, growth past 2x
  above a floor of 1e-9 is a regression, and both constants are measured: a
  solver change that moved no answers moved 0 of 94 dropped terms, while the
  case this must catch is a factor of 40. Calibrated by injecting a fault and
  confirming it is caught. Older nine-field baselines still read (D88).

- **The checker bounds unbounded variables by what the constraints imply**,
  so a dual term that used to be minus infinity — and was dropped, taking
  `gap_positive`'s guarantee with it — is finite wherever a row bounds the
  column with the others inside their boxes. An implied bound is satisfied by
  every feasible point, so adding it changes neither the feasible region nor
  the optimum, and a dual bound for the tightened problem holds for the
  original. Certification roughly doubles, 12 of 110 accepted answers to 27,
  and D47's constructed case goes from every number reading zero on a point
  0.1 from optimal to refusing it with `gap_positive` at exactly 0.1. All 139
  digests unmoved. **It does not close D47:** `pilot` at intervals 24, 32 and
  96 still reads green on an answer 1.04e-3 out of tolerance, because no
  single row bounds the column that costs it (D87).

### Fixed

- **A residue only a primal pivot can remove is no longer hidden by the
  scaling.** `wants_a_pivot` and the primal clean-up's re-check asked for a
  sign breach past `DUAL_TOL` in the solver's scaled space, while the answer is
  judged in the model's own: on `pilot87` a logical with a reduced cost of
  6.53e-09 scaled carries **1.67e-06 published**, because its row's scale
  factor is 256, and the point went out as OPTIMAL with the checker refusing
  it. They ask for a breach in **either** space now — the union and not the
  published reading, because substituting drops twenty-six candidates across
  that instance's three solves to gain two. `pilot87`'s gate answer and its
  warm re-solve are bit-identical and `etamacro`'s certificate tightens 14x;
  92 of 94 standard instances are untouched and all three gates read 0
  regressed (D92).

- **`bench/warm.c` ran the independent checker on the warm answer only**, so
  the perturbed model's cold solve was the only published answer in this
  repository that nothing judged — and it was the one that was wrong. It judges
  both now and names which side was refused. Its parallel path also parsed a
  failure note with `%79s`, so every note containing a space reached the
  summary as its first word (D92).

- **A pivot is refused when the factorization contradicts itself**, which is
  the stability trigger PLAN 2.5.5 asked for and never got. Each iteration
  already computes the pivot element twice — `alpha_q` by BTRAN, `col[r]` by
  FTRAN — so the check is free; the entering column is now transformed before
  anything is mutated, which is what leaves an iteration to abandon. Past
  `LU_AGREE_TOL` the pivot is declined, unbilled, and a rebuild asked for,
  unless the factorization was already fresh — where the basis itself is the
  problem and declining forever would be the only alternative to proceeding.
  `pilot87` at a refactorization interval of 128 went from tripping the
  iteration guard after 1,382,801 iterations to OPTIMAL in 214,631; all 139
  digests unmoved, because no pivot in the gate comes within four decades of
  the threshold (D86).

- **A nonbasic free variable with a negative reduced cost is no longer
  invisible**, which could publish a suboptimal point as OPTIMAL. Zero is the
  only dual feasible reduced cost for a variable with neither bound, so it is
  the one status whose cost can be wrong in either direction; `wants_a_pivot`
  and `primal_ratio_test` both read the status instead, which puts a free
  variable in the upper-bound branch and repairs one sign only. Both now read
  the sign of the reduced cost, which is bit-identical for a bounded status —
  `dual_breach` has already fixed the sign there — and all 139 answers, work
  units and iteration counts are unmoved. On the constructed model the answer
  goes from 0.0 to -6 (D85).

- **Loading a problem no longer discards the logging callback.** A model
  configured before it was loaded lost `jaos_set_log_callback` and
  `jaos_set_log_level` in silence — the natural order to write it in, and
  broken since logging landed. The list of settings that survived a load was
  the defect rather than its contents: the comment beside it had already
  warned that a setting added without being added to the list would be lost,
  and that is exactly what happened to the next setting added. Configuration
  is now one sub-struct that the load saves and restores whole, so there is
  nothing left to forget. All 139 answers unmoved (D78).

- A solve that failed reported none of what it did. The three counts —
  refactorizations, weight restarts, stalls — were logged on the success
  branch only, which is the branch nobody has to investigate. And the
  iteration guard said how many iterations it had taken and nothing else; it
  now also says how long the total infeasibility has stood still and whether
  Bland's rule was on, which is the difference between a cycle the
  anti-cycling rule never caught and one it caught and could not finish.
  Establishing which of those `pilot87` was took two runs and a throwaway
  build (D72).

- `docs/tolerances.md` listed `pilot` and `pilot87` as refused by the checker
  and counted "92 of 94 instances" green. Both closed two decisions ago —
  `pilot` by D29, `pilot87` by D30 — and the document's own next paragraph
  narrated the second repair while the table above it still called it
  outstanding. All 94 are green. In a repository whose first rule is that the
  design is written down and must not be reconstructed from the code, a
  document that contradicts both the code and itself is the expensive kind of
  defect (D71).

- A basis of nothing but structurally empty columns made `jaos_solve` fail
  instead of reporting the model infeasible. `refactorize` asked for capacity
  equal to the basis's nonzero count, and a request of zero leaves the arrays
  unallocated, so the factorization was handed a null index array and refused
  it as bad input. Unreachable from the slack basis, where every logical
  carries an entry; a warm start reaches it by keeping a column basic after
  the last coefficient in it is deleted (D68).

- A valid model could come back as `JAOS_ERR_INVALID_INPUT` — the code the
  library reserves for a caller's mistake — from inside `jaos_solve`.
  `pivot()` reports a failed basis update by asking for a rebuild and
  returning success, and `primal_cleanup` was the one loop that pivoted
  without reading that flag: both triangular solves return without writing
  once the factorization is wrecked, so the ratio test and pricing row that
  follow were computed from whatever the buffers last held, and the update's
  own guard was what stopped it. The loop now leaves and lets the caller
  refresh. `pilot` at a refactorization interval of 48 goes from that error
  to `optimal`; the other eleven cells of the same sweep and all 139
  reference instances are unchanged, digest for digest (D48).

- A model with a finite optimum could be reported `INFEASIBLE`. The verdict
  is reached when no pivot clears `PIVOT_MIN`, and those are exactly the
  magnitudes that drift in a factorization patched by many updates — so
  optimality was being re-checked against a fresh factorization (D20) while
  infeasibility was accepted the first time it was reached. It now gets the
  same second opinion. Costs 0.04% and moves no iteration count; the 29
  genuinely infeasible models are still refused, 29 of 29 (D39).

### Changed

- The shipping build is `-O3 -flto` instead of `-O2`, and there is one set of
  flags rather than the two the plan proposed. Measured over the whole
  standard set with every verdict, iteration count and digest unmoved: `-O3`
  1.0055x, `-flto` 1.0330x, `-march=native` 1.0072x on top of LTO — noise,
  and it makes the archive undistributable, so it stays as `NATIVE=1`.
  Removing the work counter and the clock check was measured too, at 0.987x
  and 1.004x: the public budget API is free (D62).
- The LU's elimination walks each column once, in place, instead of copying
  it into a dense buffer and back out. The multipliers belong to the pivot
  rather than to any one column, so they are scattered once per pivot and
  every column of the pivot row meets them where it stands. On `maros-r7`
  those two copies were 42% of the whole program against 6.5% for the
  subtraction they carried. `maros-r7` 26.965 s -> 22.635 s (1.191x), and
  **1.569x against where M2's speed work started this session**. All 139
  digests, iteration counts and work units unmoved; clean under ASan and
  UBSan (D59).
- The LU's elimination asks for a column's capacity once per column instead
  of once per entry it writes. On `maros-r7` that append ran 1,552,126,296
  times for a quarter of the whole program's instructions, against 2.9% for
  the one line in the same loop that bills a work unit. `maros-r7` 35.512 s
  -> 27.711 s (1.281x) and `pilot87` 32.233 -> 27.562 (1.169x) — the two
  instances that carry 74.1% of the standard set's work — with all 139
  digests, iteration counts and work units unmoved (D58).
- The elimination stops rebuilding a column when there is nothing to
  eliminate. It scattered every column of every pivot row into a dense
  buffer and pushed the survivors back regardless of what the pivot column
  held — and on a basis that is already triangular it holds nothing, so the
  pass was copying each column onto itself. On `fit2p` that was 97% of
  pivots and 344 million appends per eleven factorizations. **`fit2p` goes
  from 8.25 s to 2.46 s**, and with the entry below, from 12.87 s to 2.46 s.
  Surgical by construction — `maros-r7`, the highest-fill instance, moves
  1.02x. All 139 reference instances identical, digest for digest (D56).
- Appending to a sparse vector tests its own capacity instead of calling
  across a translation unit to be told there was room. `jm_svec_push` called
  `jm_grow` twice per element and `jm_grow` lives in `util.c`, so in the
  build JAOS ships — `-O2`, no LTO — that was 63% of every instruction
  executed on `fit2p` and charged no work unit at all. **`fit2p` goes from
  12.87 s to 8.42 s and `maros-r7` from 50.5 s to 36.7 s**; under `-flto` it
  changes nothing, because the compiler was already doing it. All 139
  reference instances identical, digest for digest (D55).
- The FTRAN reports where its answer is nonzero too, and the steepest-edge
  recurrence and both updates of `x_B` walk that instead of every row. This
  one needed no ordering, because all three readers are elementwise — which
  is why its threshold sits at half the other two. Work falls **1.557x on
  Kennington**, 1.010x on the standard set and 1.037x on the infeasible one:
  the largest single entry of M2 so far, with no digest and no iteration
  count moved (D44).
- The BTRAN reports where its answer is nonzero, taken from the pass that
  was already visiting every slot to permute it back, and pricing walks that
  instead of scanning the whole pricing row. On Kennington that scan was 27%
  of everything the solver billed. It needed a second loop of the same
  length to go with it — the reset of basic slots — because one charge had
  been standing for both. Work falls **1.255x on Kennington**, 1.007x on the
  standard set and 1.008x on the infeasible one; **all 139 instances get
  cheaper and none gets dearer**, with no digest and no iteration count
  moved (D43).
- The exact steepest-edge weight is summed over the pattern of the row it is
  the norm of, instead of over the dimension. That pattern costs nothing to
  have: pricing already walks the whole of `rho` in ascending order looking
  for rows to skip. Work falls **1.208x on Kennington**, 1.009x on the
  standard set and 1.025x on the infeasible one — all 139 instances cheaper,
  no digest and no iteration count moved. The other half of the same charge,
  the steepest-edge update itself, needs FTRAN to hand over a pattern and is
  still open (D42).
- The dual step walks the pricing row's pattern as well, which is the other
  and larger half of the same idea. It needed one thing the ratio test did
  not: the loop also repairs reduced costs that have drifted past their
  bound, so skipping a variable is only safe where that repair would have
  done nothing. `duals_dirty` names that condition and the two places that
  break it pay one full sweep to restore it. Work falls **1.451x on
  Kennington**, 1.015x on the standard set and 1.015x on the infeasible one,
  again with **no digest and no iteration count moving**. Kennington is now
  1.895x cheaper than when M2's pricing work began (D41).
- The pricing row is read through its pattern rather than in full wherever
  that pattern is small. `alpha = rho' M` is dense storage holding a sparse
  vector — 0.1% of the variables on `ken-18`, which is two thirds of the
  Kennington set's dense sweeping on its own, against 83.6% on `osa-60` —
  and both the ratio test's scan and the clear that starts the next
  iteration walked every slot to find the few that were there. Work falls
  **1.306x on Kennington**, 1.014x on the standard set and 1.012x on the
  infeasible one; 138 of 139 instances get cheaper, none gets dearer, and
  **no digest and no iteration count moves anywhere** (D40).
- BTRAN solves only the slots that can produce a nonzero, found by searching
  the factor's dependency graph instead of computing zeros to discover they
  are zero. 96.7% of them are. Work falls 1.040x on the standard set, 1.095x
  on the infeasible one and 1.057x on Kennington — `ken-18` included — with
  **no digest and no iteration count moving anywhere**, because the skipped
  slots are exactly zero rather than nearly zero (D38).
- A published zero is now always `+0.0`. `-0.0` is the same number and every
  check agreed it was, but it made two identical answers differ in bytes —
  which is what a solution digest compares. Normalising moves 90 of 94
  digests and not one work unit, and without it D38 could not have been told
  apart from a change that alters answers (D37).
- Pricing walks the matrix by row instead of by column, so a zero in the
  pricing row skips a whole row of the matrix rather than being spread
  across every column that touches it. Total work over the 110 solved
  reference instances falls 1.19x, 96 of them get cheaper, the best is 2.21x
  and the worst 14 pay up to 5% — and **not one solution digest or iteration
  count moves**, on any of the three sets (D35).
- The claim that a different C library cannot change a JAOS answer is
  measured rather than argued. `log2` in the scaling was the only libm result
  IEEE leaves unpinned anywhere in the solver; perturbing it moves no scale
  factor of the 139 instances until the offset is about 4x10^8 ulps, and the
  probe was shown to fire before its silence was believed (D34).
- The work-unit table loses its per-iteration row. The weight is zero and it
  is measured, not assumed: the basis update turns out to be 1.8% of an
  iteration rather than the whole of it, and the other 98% is already charged
  by dimension. No figure anywhere moves — this is an attribution of the
  units, not a reweighting of them (D32).
- The re-entry weighs a column on what its wrong sign costs the objective,
  not on the size of the breach. `etamacro` goes from REJECTED to checker ok
  for 9 extra iterations; every other instance of the standard 94 is
  bit-identical and `pds-20` costs one iteration more. Two earlier forms of
  this were measured and left out, one of them costing `pds-20` 3.2x — both
  in D27.
- The checker's bound-proximity test scales with what the value being tested
  is made of: the window is `tol * s`, with `s = max(1, sum_j |A_ij x_j|)`
  for a row and `max(1, |x_j|)` for a column. Row 3 of Netlib's `finnis`
  carries 4.0e10 of traffic and was being asked to rest within 1e-6 of a
  bound where one ulp is 7.6e-6. `finnis` goes from REJECTED to checker ok
  and nothing else moves: 0 regressed, 1 improved, 0 new on the standard 94,
  0/0/0 on the other two sets. D23 carries the identity that makes the
  loosening safe and the case it must still reject; `docs/tolerances.md`
  carries the formulas.
- The checker's primal feasibility test stays absolute. Extending D23 to it
  was measured and refused: primal feasibility is the hypothesis of the
  identity D23 rests on rather than a test beside it, and exactly one
  instance of the 94 exceeds 1e-6 on a row. D24 has the reasoning, and the
  `Q`/`N` instrumentation the argument turned up as pending work.
- The checker's relative gap is scaled by `1 + |primal| + |dual|` rather than
  `max(1, |primal|)`, which reported a larger error the further the two
  objectives were apart. This is the PDLP form HiGHS adopted. No verdict
  moved on the Netlib set, which is the only reason it is in.
- The checker no longer drops a small multiplier from the dual objective.
  Below tolerance it is still held to no sign condition, but it contributes
  `w * bound` like any other, because what gets dropped is small only if the
  bound is. That is what had been rejecting `pilot-ja`, whose dual violation
  is exactly zero; checker-green went from 86 to 87 with nothing else moving.
  D22 has the reasoning, including two repairs that were measured and
  rejected first — one of which passed the entire gate while being vacuous.
- The solver core is back to where it was on 2026-08-06, reverting ten
  commits that had not been run against the Netlib set. Measured per instance
  rather than by the summary line, they fixed `grow15` and `nesm` and cost
  `pilot-we` (a feasible problem reported infeasible), `pilotnov` and
  `grow22` (2179 iterations to 167865). The summary line was identical before
  and after, because the gains and the losses cancelled.
- A model whose optimum lies beyond the bound dual phase 1 lends is refused
  with a numerical error naming the column, rather than answered. Reaching
  such an optimum needs a phase 1 that lends nothing. No instance of the 139
  ever reached the lent bound, so the loan size is a performance parameter and
  not a correctness risk, and Q9 closed on that (D31).
- `jaos_solution` refuses to report anything for a solve that found no
  optimum, under the same rule as `jaos_objective`: a buffer of zeros cannot
  be told apart from an answer that is genuinely zero.
- Builds pin `-ffp-contract=off`. C23 lets the compiler fuse `a*b+c` where
  the hardware offers it, which would make the same model produce different
  bits on different machines — the opposite of what D8 promises.
- `docs/plan-m2.md` is now `docs/research/plan-m2.md`, alongside the notes it
  is built on. It is a proposal; `PLAN.md` is the plan. Its speedup figures
  come from the literature and none has been measured in JAOS (D17).

### Fixed

- The primal clean-up decides which columns want a pivot before it moves any
  of them, and calls in each candidate's own cost loan before judging it. It
  had been able to take exactly one pivot per call: `wants_a_pivot` reads the
  duals out of `rho`, which the first pivot overwrites with a pricing row, and
  underneath that `pivot()` lends away every other candidate's sign condition.
  `pilot87` goes from an objective `2.28e-3` out to `1.33e-7` relative, its
  dual violation from `1.87e-5` to `0`, and it gets cheaper. **The standard
  Netlib gate now reports PASS** — 94 of 94 on every condition. 0 regressed,
  3 improved; 92 of the 94 keep their exact iteration count (D30).
- The refresh that verifies a declaration of optimality refines its two
  solves — one step of iterative refinement on `x_B` and on `y`. `pilot` was
  rejected on a row `1.73e-6` outside its bound that no basic variable
  violates; it is the residual of the basis solve, and refining takes it to
  `6.73e-13`. Checker goes from 92 to 93 of 94. The solve loop is deliberately
  left unrefined: 93 of the 94 instances take exactly the iteration count they
  took before, and total work falls 0.029% (D29).
- A basis the factorization finds singular is repaired rather than ending the
  solve. `jaos_internal.h` had always said rank deficiency is a fact the
  caller acts on by replacing basis columns, and the caller did not: pairing
  the rows the factorization could not cover with the basis positions it
  could not use gives a completion that is triangular by construction.
  `gran` reaches INFEASIBLE where it used to give up after 1728 iterations,
  taking the infeasible gate to 29 of 29, and no instance of the standard set
  moves. `repair_singular_basis` carries the proof and why no unit test can produce one.
- Every reference optimum in `bench/netlib.manifest` is now Koch's.
  `maros-r7` and `pilot87` had fallen back to the netlib readme, so `pilot87`
  was judged against a value 1.26e-6 from the exact optimum where this gate's
  tolerance is 1e-6. The report's PostScript carries the table as literal
  text where its PDF did not; `bench/koch-refs.py` extracts it and
  `bench/koch-verify.py` reproduces 82 of the pinned references exactly with
  none in disagreement. No verdict moves.
- The acceptance runner reads the whole manifest before solving anything, and
  closes the file first. It used to parse a line, solve that instance, then
  parse the next, holding the file open across the run; a rewrite in that
  window shifted every later byte offset and `sctap2` was judged against a
  reference no line of the file ever carried. Instance paths are now
  bound-checked rather than silently truncated.
- PLAN 2.8 and Q10 recorded a cause for the dual violations that the
  measurement does not support, and both now carry the measurement instead.
  The six open instances had been grouped by the size of the number the
  checker prints — which is a multiplier's magnitude, not a distance —
  and measured one at a time they are four separate things. Two repairs for
  the largest group were implemented, measured and reverted; both reported
  `greenbea`, a feasible model, as INFEASIBLE.
- `jaos_check_solution`'s contract in the public header said "tol is an
  absolute tolerance on violations" after D23 had already scaled the
  bound-proximity test. It now says which half is which.
- A solve no longer stops on values it carried rather than computed. Basic
  values and the factorization both drift as pivots accumulate, so a solve
  stopped exactly when its numbers looked feasible without being it. The
  point is recomputed from a fresh factorization and priced again before the
  answer is accepted — on `afiro`, the difference between finishing 1.8e-5
  from a bound and finishing on it. Costs one refactorization per solve,
  which the work counter bills (D20).
- An unbounded model is identified by a direction along which the objective
  has nothing to stop it, checked against the bounds the model declares. It
  used to be identified by a variable coming to rest on a bound the solver
  had invented, which a model with a large but finite optimum does too. On
  3000 generated LPs, 8 called unbounded now solve to an optimum the checker
  accepts, with no other model changing its answer or its work (D19).
- The Netlib gate accounts for the objective constant an MPS file can declare
  through an `RHS` entry on its objective row. JAOS applies it, following the
  convention CPLEX documents; the published optima do not, so a correct
  answer differed from both reference sets by exactly that constant on the
  one instance where it is visible. The reader is unchanged, deliberately.
- Settling up can no longer park a basic variable on an artificial phase-1
  bound, which would manufacture the evidence of unboundedness after the
  verdict was already read.
- A failed solution-buffer allocation no longer leaves the model believing
  the buffers exist; a later solve would have written through the missing
  ones.
- The published work count includes the final kernel run of publishing
  itself. It used to be taken one BTRAN too early.
- The release build, which `main` could not complete: `build_crash_basis`
  declared `nsel` twice and left a variable unused, and `-Werror` refused the
  object. Nothing depended on either variable.
- A 648-byte leak across 13 allocations that AddressSanitizer reported in the
  LU tests, gone with the revert of the code that introduced it.

### Added — the initial implementation

- `bench/`: the Netlib acceptance gate, runnable. `make netlib` fetches the
  94 instances of the standard set — pinned by sha256 in a committed
  manifest, never stored in the repository — and judges each solve against
  the published optimum, the independent checker, and a second solve that
  must agree bit for bit. Reference values are Thorsten Koch's exact
  rationals where they exist; the netlib readme's differ from them beyond
  the gate's tolerance on eight instances. The last full run is recorded in
  `bench/results/netlib.txt` and the README summarises it. The gate is not
  met yet, and the record is what says so.
- `docs/tolerances.md`: every number a solve compares against, which space it
  acts in, and the formulas the independent checker judges with.
- `docs/work-units.md`: what a work unit is, where each weight is charged, and
  what sits outside the budget — so a work limit can be chosen rather than
  guessed.
- `README.md`: what JAOS is, what it solves today, how to build it, and the
  design commitments that constrain it.

- `DECISIONS.md`: the durable record of closed design decisions — language and
  toolchain, dependency policy, determinism, problem scope, and the rules under
  which correctness and speed may be claimed.
- `PLAN.md`: the staged build order for the whole declared scope, and a fully
  specified first milestone — LP correctness on the Netlib set — with acceptance
  criteria, draft tolerances and work-unit weights, and a verified bibliography.
- Build scaffold: `make all | test | sanitize | clean` on GCC 14 / C23; the
  public header `jaos.h` with status codes and version query; Unity v2.7.0
  vendored under `tests/` as the dev-time test harness.
- Model core: create/load/query/free for LPs in bounded form, with validating
  copy-on-load (sorted CSC, explicit zeros dropped, structural errors rejected)
  and an internal row-wise mirror.
- Independent solution checker: judges a claimed primal (and optionally dual)
  solution against the model in original space — bound and activity violations,
  dual sign conditions including complementary slackness, and the objective gap.
- MPS reader (`jaos_read_mps`): fixed and free layouts, RANGES, all continuous
  BOUNDS types, OBJSENSE, objective constants, Fortran D exponents,
  locale-independent number parsing; every rejection carries a line number via
  `jaos_model_error`. Integer constructs are recognized and rejected until MILP
  lands. Dialect decisions documented in `docs/format-support.md`.
- LP-format reader (`jaos_read_lp`): CPLEX-style core dialect with line-wrapped
  expressions, labels, glued coefficients, repeated-variable summing, all bound
  forms including `free` and infinities. Ranged constraints, constants inside
  constraints and integer sections are recognized and rejected with line
  numbers.
- Matrix scaling: Curtis-Reid by default, geometric-mean equilibration as an
  option. Factors are exact powers of two, so applying them adds no rounding
  error, and the stored matrix is left untouched. See `docs/scaling.md`.
- Sparse LU factorization with Markowitz threshold pivoting, and the forward
  and transposed triangular solves built on it. A singular matrix is reported
  through the factorization's rank rather than treated as a failure.
- Forrest-Tomlin basis updates: replacing one basis column costs work
  proportional to the change instead of a full refactorization. A replacement
  that would leave an untrustworthy pivot is refused rather than accepted
  quietly.
- Deterministic work counter wired into the factorization and solve kernels —
  the currency of the reproducible budget.
- Dual simplex: `jaos_solve` finds optimal solutions for linear programs with
  bounds, ranged rows and equalities, in either objective sense, and reports
  infeasible models as infeasible. Results come out through `jaos_solution`,
  `jaos_objective` and `jaos_status_of`.
- Budgets: `jaos_set_work_limit` (reproducible) and `jaos_set_time_limit`
  (wall-clock), reported back through `jaos_work_units` and
  `jaos_iterations`. Pricing and the ratio test are charged to the work
  counter, so a work limit bounds the whole solve rather than only its
  linear algebra.
- A model this build cannot solve yet reports `JAOS_SOLVE_UNSUPPORTED` with
  an explanation, rather than being rejected as invalid input.
- `jaos_objective` reports through a status and an out-parameter, so a
  genuine objective of zero is distinguishable from having no answer.
- Dual steepest-edge pricing: the solver picks which violated constraint to
  repair by how far the repair actually travels, not by the size of the
  violation as written. A model in mixed units no longer sends it after the
  row with the smallest units first.
- Harris' two-pass ratio test: the pivot is chosen by spending a bounded
  amount of dual feasibility — one tolerance's worth — on a better
  conditioned one. Where many candidates block at the same point, which is
  most of a degenerate solve, this is what decides between them.
- Bound flipping in the ratio test: a variable with two finite bounds no
  longer stops an iteration short. It is swapped to its other bound and the
  step carries on, so a model whose answer fills one bounded column after
  another is solved in one long step instead of one step per column.
- The dual feasibility the ratio test spends on better-conditioned pivots is
  now lent rather than given away: the cost of the affected column is moved
  just enough to keep it feasible, the loan is recorded, and it is called
  back before any answer is reported, so what comes out belongs to the
  problem that was asked about. Where calling it back leaves a column on the
  wrong side, it is swapped to its other bound whenever that keeps the
  solution feasible — which can only improve the objective.
- Steepest-edge pricing weights are checked against an exactly known value
  every iteration, repaired where the exact one is available, and restarted
  when they have drifted past being informative. Pricing that has quietly
  stopped meaning anything costs iterations rather than answers, so nothing
  else would have reported it.
- Solving now runs on a scaled copy of the model, computing a Curtis-Reid
  scaling first if none was chosen, so a model whose coefficients span many
  orders of magnitude is solved on numbers that do not. The model itself is
  untouched and every answer — values, activities, duals, reduced costs —
  comes back in the units it was written in.
