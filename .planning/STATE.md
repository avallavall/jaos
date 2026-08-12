---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 01
current_phase_name: candidate-admission-in-the-ratio-test
status: executing
stopped_at: "Completed 01-03-PLAN.md. All 139 digests confirmed unmoved and the three baselines rewritten and confirmed. Next: 01-04 (the J=1 time ratio and callgrind on truss)"
last_updated: "2026-08-12T17:05:00.000Z"
last_activity: 2026-08-12
last_activity_desc: "Executed 01-03 — three gates PASS, 139 digests unmoved, iterations 1.0000x per instance, work down on 118 and up on none, the three baselines rewritten in their own commit after the confirmation"
progress:
  total_phases: 1
  completed_phases: 0
  total_plans: 5
  completed_plans: 3
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-12)

**Core value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Current focus:** Phase 01 — candidate-admission-in-the-ratio-test
**Milestone:** M2 — LP competitiveness

## Current Position

Phase: 01 (candidate-admission-in-the-ratio-test) — EXECUTING
Plan: 4 of 5
Status: Executing Phase 01 — 01-01 through 01-03 complete; the source edits are finished and the record and baselines are settled
Last activity: 2026-08-12 — 01-03 landed (44c0ef6, e8c2f58)

Progress: [██████░░░░] 60%

## Performance Metrics

**Velocity:**

- Total plans completed: 3
- Average duration: ~30 min
- Total execution time: ~90 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | ~90 min | ~30 min |

**Recent Trend:**

- Last 5 plans: 01-01 (~13 min, 2 tasks, 3 files, 2 commits), 01-02 (~22 min, 1 task, 3 files, 1 commit), 01-03 (~55 min, 2 tasks, 8 files, 2 commits)
- Trend: duration is set by machine time, not by task count or diff size.
  01-02 was 9 lines of code and cost 22 minutes in measure-revert cycles;
  01-03 wrote no code at all and cost 55, of which **34.4 min was campaign
  time under WSL** that no estimate models. Kennington alone is ~8 min and
  this plan ran it three times.

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

### Pending Todos

- **`01-05` must number its decision D93**, not one past whatever
  `DECISIONS.md` ends at when it runs. Four files already cite D93:
  `src/simplex.c`, `docs/work-units.md` and two places in
  `tests/test_simplex.c`. `jaos-record` warns that source comments cite
  decision headings, so a different number leaves four dangling references.

- **`01-05` must not write "iterations" for the dense-branch count.** The
  quotient `work_saved / rows` counts calls to `dual_ratio_test`, and `galenet`
  makes 2 of them in a solve that reports one iteration. The distinction holds
  on 1 instance in 139, which is exactly the kind that gets rounded away.

- **Nothing in the repository reads the `baseline: NOT COMPARED` line.** It
  exists so a record produced by a `-w` run can be told from a checked one, and
  it went unread long enough for such a record to sit committed as the standard
  set's record (`64efcc6:bench/results/netlib.txt`). Fixed there by 01-03's
  confirming run; the missing check is two lines and belongs in
  `preflight.sh`. Not added in 01-03, because a tooling edit mid-plan
  invalidates the campaign.

### Blockers/Concerns

- **[Phase 5] No controlled host is named.** D17 says a WSL number cannot close
  a gate, and the machine that builds and measures JAOS is a Windows host
  running WSL. Phase 5 cannot produce its closing number without one. Phases 1–4
  are unaffected.

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
Stopped at: Completed 01-03-PLAN.md — all three gates PASS at J=12, 139 digests unmoved, iterations 1.0000x on every instance individually, work down on 118 and up on none. The three baselines rewritten in their own commit (e8c2f58) after the confirmation was already committed (44c0ef6), and a confirming gate run diffs clean against them.
Resume file: .planning/phases/01-candidate-admission-in-the-ratio-test/01-04-PLAN.md

Next: execute 01-04 (the `J=1` same-instance time ratio, and callgrind on
`truss` against 14.98%). **Roadmap criterion 4 is met and the record and the
baselines now come from one build**, so the next per-instance diff shows only
what 01-04 does.

Two things 01-04 needs that 01-03 produced. **Every second in 01-03 is a `J=12`
number and useless to it** — D45's ratio needs `-j 1`, two binaries in one
session, alternated, minimum over three or more rounds. And **the two instances
that carry 74% of the standard set barely move**: `pilot87` 0.9956x,
`maros-r7` 0.9987x. `truss` — the instance criterion 3 names — does take the
dense branch, at 0.9857x. Choosing instances by D46's names alone would pick
the two where this change is nearly invisible.
