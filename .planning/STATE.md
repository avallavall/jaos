---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 01
current_phase_name: candidate-admission-in-the-ratio-test
status: phase-complete
stopped_at: "Completed 01-05-PLAN.md. Phase 01 is closed: D93 is written and indexed, the changelog and SPECS.md are current, and all four roadmap criteria are met. Next: Phase 02 (presolve and postsolve) — no plans exist yet"
last_updated: "2026-08-12T17:05:00.000Z"
last_activity: 2026-08-12
last_activity_desc: "Executed 01-05 — D93 closed and indexed with the measurement on both sides, the 4.2% derivation traced to D81, the null result stated as 110 digests + 29 infeasibility verdicts over 139 instances, and an audit correction appended to 01-04-SUMMARY.md"
progress:
  total_phases: 1
  completed_phases: 1
  total_plans: 5
  completed_plans: 5
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-12)

**Core value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Current focus:** Phase 01 — candidate-admission-in-the-ratio-test
**Milestone:** M2 — LP competitiveness

## Current Position

Phase: 01 (candidate-admission-in-the-ratio-test) — **COMPLETE**
Plan: 5 of 5
Status: Phase 01 closed. All five plans executed and all four roadmap criteria met. D93 is written, indexed and cited from the three source files that were pointing at it forward
Last activity: 2026-08-12 — 01-05 landed (D93, the changelog, SPECS.md, and the audit correction on 01-04-SUMMARY.md)

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 5
- Average duration: ~40 min
- Total execution time: ~200 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 5 | ~200 min | ~40 min |

**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files | Commits |
|------|----------|-------|-------|---------|
| 01-01 | ~13 min | 2 | 3 | 2 |
| 01-02 | ~22 min | 1 | 3 | 1 |
| 01-03 | ~55 min | 2 | 8 | 2 |
| 01-04 | ~75 min | 3 | 1 | 1 |
| 01-05 | ~35 min | 2 | 4 | 3 |

**Recent Trend:**

- Trend: duration is set by machine time, not by task count or diff size.
  01-02 was 9 lines of code and cost 22 minutes in measure-revert cycles;
  01-03 wrote no code at all and cost 55, of which **34.4 min was campaign
  time under WSL** that no estimate models. Kennington alone is ~8 min and
  this plan ran it three times.
- **01-04 is the extreme of that trend.** It modified no repository file by
  design, so its whole diff is one summary — 7,400 tokens against an estimate
  of 70,000, an order of magnitude over. What it actually cost is **47 minutes
  of WSL machine time**: 12 sequential `J=1` passes over the standard set, two
  callgrind runs and three builds. No field of the estimate models that, and a
  plan whose deliverable is a number will always look free to a token estimate.
- **01-05 is the one plan the estimate could see**, and it is the shape that
  explains the other four: 12,300 realized against 45,000 estimated, under by
  3.7x rather than by 10x. A documentation plan's length is set by the number
  of things that have to be recorded, and that number was known in advance. No
  machine time at all — it builds nothing and runs no campaign.

*Updated after each plan completion*

## Accumulated Context

### Decisions

`DECISIONS.md` at the repository root is the authority — D1 through D92, all
locked, each carrying the measurement that closed it. PROJECT.md's
`<decisions>` block carries the IDs that constrain this milestone; it does not
replace the file.

Roadmap-level decisions taken at milestone opening:

- Milestone 1 of this roadmap is the project's **M2 — LP competitiveness**
- **Candidate admission before presolve** (WARNING 1) — D81 measured presolve at
  1.417x/1.136x against a per-iteration gap of 2.53x, and says that reorders
  the plan

- **The candidate-admission decision is the first deliverable, before any code**
  — PLAN.md item 3a says so outright

- **The re-entry oscillation is roadmap work** (WARNING 2), not an accepted
  limitation, despite a measured cost of 0.24% on `pilot87` and no cure named

- **The M2 per-instance guard factor stays absent until measured** (WARNING 3)
  — no number is invented anywhere in these artifacts

Taken during execution of 01-01:

- **The nonbasic set is a persistent bitmap**, `s->nbmark`, deviating from the
  `amark` bitmap-clears-itself convention on purpose and saying so in the
  header and the struct

- **The D-08 cross-check runs over both branches**, not only the one it
  replaced, so the pattern/dense equivalence claim at `simplex.c:1562-1568` now
  has a run-time proof on every solve in the suite

- **A missed `jm_nonbasic_remove` is a performance fault, not a correctness
  one** — the bitmap becomes a superset and `admit_candidate` rejects the
  extras, so the D-08 assertion is silent and correct to be. The
  correctness-dangerous fault is a missed `jm_nonbasic_insert`, and that is
  what the assertion was calibrated against

Taken during execution of 01-02:

- **The dense branch bills the visited count; `pivot`'s sweep still bills
  `s->nvar`** and is right to — it reads every variable's status. The plan's
  `grep -c 'nvar * JM_WORK_NONZERO' == 0` criterion was therefore refused:
  meeting it would have corrupted work units on every instance with nothing
  pinned to catch it

- **The bitmap words are billed nothing.** A word is not a variable, and a rate
  for them would be a number with no measurement on either side. The
  `nvar/64`-read floor it leaves is recorded in `docs/work-units.md` beside the
  unbilled `alpha` sweep

- **The `<62000` ceiling's failure side was re-measured rather than carried**,
  which the plan permitted skipping. Once the correct side came in at 60701 —
  2.1% under the bound — a stale pair could no longer answer whether 62000 still
  separates correct from broken. It does: 60701 < 62000 < 64633

Taken during execution of 01-03:

- **The digest confirmation got its own commit before the rewrite ran**
  (44c0ef6, then e8c2f58), so D-06's ordering is a fact `git log` can be asked
  rather than a claim a summary makes about itself

- **Work is the only thing that moved, on all five records**, proved by masking
  the work field and diffing end to end against 64efcc6 rather than by reading
  `record_diff.py`'s per-instance list — which has a reporting gap on
  sub-threshold `drop` changes. Iterations are 1.0000x per instance, not merely
  in the mean

- **The saving is an exact whole multiple of the row count on all 139
  instances**, which inverts to the number of dense-branch calls without
  instrumenting anything. It counts **calls, not iterations**: `galenet` makes 2
  in a solve reporting `iters=1`, so `dual_ratio_test` runs somewhere that is
  not a counted iteration

- **D46 was wrong in both directions on the same change** — the ratio of totals
  understates it on the standard set (0.9933x against a per-instance 0.9779x)
  and overstates it on the infeasible set (0.9669x against 0.9857x)

Taken during execution of 01-04:

- **VERDICT: INCONCLUSIVE.** The `J=1` same-instance time ratio over the
  standard set is **0.9709x** — a 2.91% improvement against a 4.2% bar. Six
  defensible readings of the same data run from 2.16% to 4.07% and **none of
  them reaches it**, which is the only reason a verdict can be stated at all on
  a host this noisy. The code stays, under the developer's pre-authorisation

- **Callgrind says the change costs 1.60% MORE instructions on `truss`.**
  `admit_candidate` sheds 199M and `simplex.c:run`, which the scan is inlined
  into, gains 995M — a relocation that costs five times what it saves. It pays
  6.82 instructions on each of 145.9M visits to save exactly 12.0 on each of
  16.6M skips. Whatever seconds it may buy are not bought by doing less

- **Running order alone is worth 2.4 percentage points on this host.** Rounds
  1–3 candidate-first read 0.9865x, rounds 4–6 parent-first read 0.9624x. The
  plan's three-round alternation puts the candidate first every time and cannot
  see this; three rounds would have reported 1.35%

- **The 4.2% threshold is three times a 1.4% repeatability that belongs to
  D81**, not D83 as `01-04-PLAN.md` says twice — D83's 1.4% is Clp against
  HiGHS on total time. And this reading's own repeatability, measured the way
  D81 measured its own, is **6.27%**: the bar is three times a figure four
  times smaller than the noise the reading actually exhibits

- **`01-03`'s warning was right.** `pilot87` and `maros-r7` read 0.9972x and
  0.9987x on the clock, against work ratios of 0.9956x and 0.9987x. A time
  ratio taken on D46's two names alone would have read 0.998x — nothing

Taken during execution of 01-05:

- **D93 is closed and its heading names the finding rather than the verdict** —
  the bar cannot be measured on this host, which is what the negative control
  establishes and is *not* the same claim as the candidate missing the bar.
  428 insertions to `DECISIONS.md` and 0 deletions; the seven forward citations
  in `src/simplex.c`, `docs/work-units.md` and `tests/test_simplex.c` resolve

- **"All 139 digests unmoved" is false and was not written**, though the plan's
  own `must_have` required the sentence. Re-read from the record files: 94
  netlib + 16 Kennington lines carry `digest=` and the 29 infeasible lines
  carry **none** — their invariant is `expected=infeasible verdict=ok det=ok`.
  **The correct claim is 110 digests and 29 infeasibility verdicts over 139
  instances**, and `DECISIONS.md` already said 110 in four earlier entries, so
  writing 139 would have contradicted the file it was being added to

- **An independent audit accepted the verdict and refuted three of its stated
  grounds**, and both facts are recorded. The estimator-independence claim is
  false on the plan's literal three-round protocol (rounds 4–6 read 5.12%, over
  the bar); the negative control was in the data and was never run; and the
  callgrind profile contains two solves, so the per-call figures are 6.0 and
  3.41 rather than 12.0 and 6.82. **A marked correction section was appended to
  `01-04-SUMMARY.md`** rather than merged into it

- **`SPECS.md`'s competitive-gap figures were checked and deliberately left.**
  `bench/compare` did not run in this phase, so none was re-measured; each
  keeps the citation that dates it. That they are pre-phase is stated as a
  caveat rather than repaired by an invented correction

### Pending Todos

- **Nothing in the repository reads the `baseline: NOT COMPARED` line.** It
  exists so a record produced by a `-w` run can be told from a checked one, and
  it went unread long enough for such a record to sit committed as the standard
  set's record (`64efcc6:bench/results/netlib.txt`). Fixed there by 01-03's
  confirming run; the missing check is two lines and belongs in
  `preflight.sh`. Not added in 01-03, because a tooling edit mid-plan
  invalidates the campaign. **Handed on by D93.**

- **`bench/run.c` prints seconds as `%8.3f`.** 8 instances of the standard set
  carry no time ratio and 42 more read exactly 1.0000x, so every future time
  ratio on this set is half made of instances that cannot show the difference.
  A two-line change, not made mid-measurement. **Handed on by D93.**

- **`galenet` makes two calls to `dual_ratio_test` in a solve reporting one
  iteration.** The quotient `work_saved / rows` counts calls, not iterations —
  a distinction that holds on 1 instance in 139 and is exactly the kind that
  gets rounded away. Recorded in D93; not chased.

- **Phase 02 has no plans.** `/gsd-plan-phase` is the next action, and its
  Open Question is already on the roadmap: T0 is "the simplex and only the
  simplex", so presolve contributes nothing to the M2 close criterion as
  stated, and nothing says how the ladder is recalibrated once JAOS has one.

### Blockers/Concerns

- **[Phase 5] No controlled host is named.** D17 says a WSL number cannot close
  a gate, and the machine that builds and measures JAOS is a Windows host
  running WSL. Phase 5 cannot produce its closing number without one. **01-04
  measured what that costs rather than leaving it a principle:** the same binary
  against itself reads up to 6.27% across rounds, 8.67% on the instances the
  clock can see, and running order alone is worth 2.4 percentage points. A
  same-session A/B ratio divides most of the host out and did not divide out
  enough to resolve a 4.2% bar. Phases 1–4 are not blocked, but every time ratio
  they take inherits this floor.

- **[All phases] The runner prints seconds as `%8.3f`, and half the standard set
  is under ten quanta.** 8 of 94 instances print `0.000s` and carry no time
  ratio at all; 42 more read exactly 1.0000x because both minima land on the
  same millisecond. Any geometric mean of per-instance *time* ratios over this
  set is therefore half made of instances that could not have shown the
  difference. A two-line change to `bench/run.c` fixes it; not made in 01-04,
  because a source edit mid-measurement invalidates the measurement.

- **[Phase 5] The M2 guard factor is unset** and must be measured or explicitly
  dropped from the criterion. It is not to be guessed.

- **[Phase 2 → Phase 5] T0 is "the simplex and only the simplex."** Presolve
  contributes nothing to the close criterion as stated; how the ladder is
  recalibrated once JAOS has a presolve is undecided in the source.

- **[All phases] Five of seven v1 requirements carry no acceptance criterion.**
  That is `PLAN.md`'s own state, not an extraction gap. Deriving each is
  plan-phase work.

- **[Phase 1 → all phases] A ceiling drifted 2800 units with nothing watching.**
  `test_a_clean_up_pass_dispatches_every_column_it_identified`'s correct-side
  figure was 58141 in its comment and measured 60941 at HEAD before 01-02
  touched anything, against a bound of 62000. A ceiling is read only when it
  trips, so it drifted to 1.7% of its headroom silently. Both sides are current
  now, but nothing in the repository re-measures a bound of this shape on the
  way up, and this is the only one that has been checked.

- **[All phases] The gate cannot see the defect class that produced three of
  last milestone's four closures** — those came from sweeping `REFACTOR_EVERY`
  over 16..256, which no target automates. `make clean` between settings or the
  sweep measures one binary N times.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Backlog | Writers (MPS, LP, solution file), sensitivity and ranging, exportable certificates, exact rational verification, Python bindings | Deferred | 2026-08-12 |
| Backlog | Primal simplex, barrier and crossover, deterministic parallel B&B, MILP, QP/conic/NLP/MINLP, NLP derivative strategy, MIPLIB subsets | Deferred | 2026-08-12 |
| Backlog | LP/MPS dialect edges (Q2) | Deferred | 2026-08-12 |
| Backlog — with trigger | `pilot87`'s suboptimality bound — re-enters the milestone if it blocks a gate | Deferred | 2026-08-12 |

## Session Continuity

Last session: 2026-08-12T17:05:00.000Z
Stopped at: Completed 01-05-PLAN.md. Phase 01 is closed — D93 written and indexed, changelog and SPECS.md current, all four roadmap criteria met
Resume file: None

Next: **plan Phase 02 (presolve and postsolve).** No plans exist for it yet.

**Phase 01 closed on a refusal-shaped entry that is not quite a refusal.** D93
records that the ratio test's dense scan walks the nonbasic set, that no answer
moved anywhere in the record — 110 digests, 29 infeasibility verdicts, 139
iteration counts, five records byte-identical once the work field is masked —
that the change **costs 1.60% more instructions on `truss`**, and that the 4.2%
bar it was to be judged against **cannot be measured on this host**. The code
stays under the developer's pre-authorisation of 2026-08-12.

**The one thing every later phase inherits is the negative control.** Eight
standard-set instances have bit-identical work under both binaries, so the
change provably cannot speed them up, and they read **0.9699x paired and
0.9356x pooled** — a 3.0–6.4% "improvement" where the truth is zero, the same
size as the 2.91% headline. Any future same-instance time ratio on this host
without a control of that shape is not measuring what it thinks it is. Two
companions to it: **running order alone is worth 2.4 percentage points**, so an
alternation in one direction only measures the order; and the plan's literal
three-round protocol would have read **5.12% and reported ACCEPT** in the
parent-first order.

The phase does not roll on to the next candidate path: restricting the
candidate set ahead of `bfrt_walk` and `jm_harris_pick` stays deferred (D-04),
available as its own decision and explicitly **not refused**.
