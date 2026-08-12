---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 01
current_phase_name: candidate-admission-in-the-ratio-test
status: executing
stopped_at: "Completed 01-02-PLAN.md. Every source edit of phase 01 is landed and the tree is clean. Next: 01-03 (the three campaigns, digests first, then the baselines)"
last_updated: "2026-08-12T14:26:00.000Z"
last_activity: 2026-08-12
last_activity_desc: "Executed 01-02 — the dense ratio test bills what it visits, docs/work-units.md landed with it, WORK_PINNED 8545->8536, and the <62000 ceiling's pair re-measured after finding it had drifted 2800 units"
progress:
  total_phases: 1
  completed_phases: 0
  total_plans: 5
  completed_plans: 2
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-12)

**Core value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Current focus:** Phase 01 — candidate-admission-in-the-ratio-test
**Milestone:** M2 — LP competitiveness

## Current Position

Phase: 01 (candidate-admission-in-the-ratio-test) — EXECUTING
Plan: 3 of 5
Status: Executing Phase 01 — 01-01 and 01-02 complete; the phase's source edits are finished
Last activity: 2026-08-12 — 01-02 landed (b65d9f2)

Progress: [████░░░░░░] 40%

## Performance Metrics

**Velocity:**

- Total plans completed: 2
- Average duration: ~18 min
- Total execution time: ~35 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 2 | ~35 min | ~18 min |

**Recent Trend:**

- Last 5 plans: 01-01 (~13 min, 2 tasks, 3 files, 2 commits), 01-02 (~22 min, 1 task, 3 files, 1 commit)
- Trend: a one-task plan cost more than a two-task one. The diff was 9 lines
  of code; the time went on four measure-revert cycles under WSL to read
  figures Unity does not print. Task count does not predict duration here.

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

### Pending Todos

- **`01-05` must number its decision D93**, not one past whatever
  `DECISIONS.md` ends at when it runs. Four files already cite D93:
  `src/simplex.c`, `docs/work-units.md` and two places in
  `tests/test_simplex.c`. `jaos-record` warns that source comments cite
  decision headings, so a different number leaves four dangling references.

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

Last session: 2026-08-12T14:26:00.000Z
Stopped at: Completed 01-02-PLAN.md — the dense branch bills its visited count, `docs/work-units.md` landed in the same commit, `WORK_PINNED` 8545 -> 8536 with the derivation, and the `<62000` ceiling re-measured on both sides. `make test` (78+4+12), `make sanitize` and `make all` all green.
Resume file: .planning/phases/01-candidate-admission-in-the-ratio-test/01-03-PLAN.md

Next: execute 01-03 (the three campaigns). **The tree is now settled — every
source edit of phase 01 is committed and the working tree is clean, so a
campaign launched from here is valid for the tree that produced it.** All three
baselines are stale by a known cause: every dense-ratio-test work figure in
them comes from the previous definition. D-06's ordering still binds — confirm
the 139 digests unmoved first, then rewrite. The expected work diff is down or
unchanged on every instance, never up; an instance whose work rises is a defect
rather than a re-definition.
