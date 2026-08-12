---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 01
current_phase_name: candidate-admission-in-the-ratio-test
status: executing
stopped_at: "Completed 01-01-PLAN.md. Next: 01-02 (the D-09 work charge, one line, behind its own gate)"
last_updated: "2026-08-12T13:52:12.000Z"
last_activity: 2026-08-12
last_activity_desc: "Executed 01-01 — the nonbasic bitmap, eight maintenance sites, the D-08 cross-check and five calibrated tests; make test/sanitize/all all green"
progress:
  total_phases: 1
  completed_phases: 0
  total_plans: 5
  completed_plans: 1
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-12)

**Core value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Current focus:** Phase 01 — candidate-admission-in-the-ratio-test
**Milestone:** M2 — LP competitiveness

## Current Position

Phase: 01 (candidate-admission-in-the-ratio-test) — EXECUTING
Plan: 2 of 5
Status: Executing Phase 01 — 01-01 complete
Last activity: 2026-08-12 — 01-01 landed (f2ed4bc, ebe052f)

Progress: [██░░░░░░░░] 20%

## Performance Metrics

**Velocity:**

- Total plans completed: 1
- Average duration: ~13 min
- Total execution time: ~13 min

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 1 | ~13 min | ~13 min |

**Recent Trend:**

- Last 5 plans: 01-01 (~13 min, 2 tasks, 3 files, 2 commits)
- Trend: —

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

### Pending Todos

None yet.

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

Last session: 2026-08-12T13:52:12.000Z
Stopped at: Completed 01-01-PLAN.md — the bitmap, eight sites, the D-08 cross-check and five calibrated tests. `make test` (78+4), `make sanitize` and `make all` all green.
Resume file: .planning/phases/01-candidate-admission-in-the-ratio-test/01-02-PLAN.md

Next: execute 01-02 (the D-09 work charge). Note that 01-03 and 01-04 are the
campaign plans and are blocking checkpoints; no campaign has been run, and none
is valid until 01-02's source edit has landed.
