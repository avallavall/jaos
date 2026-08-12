# JAOS — Just Another Optimization Solver

## What This Is

A mathematical-programming solver written from scratch in C23 — no external
dependencies, Apache 2.0, Linux and GCC only. It reads an LP from disk (fixed
or free MPS, and a CPLEX-style core LP dialect), solves it with a revised dual
simplex, and proves the answer right with an independent checker that never
sees how the answer was produced. It does that correctly on all 139 Netlib
reference instances. Version 0.1.0-dev; the consumer is a C caller embedding
`libjaos.a` behind one header.

## Core Value

A correct answer, bit-identical on every machine and every run, proved by a
checker that had no access to the solver that produced it.

Speed is what this milestone buys. Correctness and reproducibility are what
it may never spend.

## Current Milestone

**M2 — LP competitiveness.** JAOS is measured slower than the field at T0 and
the decomposition is known: 3.72x HiGHS 1.15.1, 1.34x SoPlex 8.0.3, 3.77x Clp
1.17.11 per solve, on 1.47x / 0.70x / 1.67x their iterations and 2.54x / 1.92x
/ 2.26x per iteration (D83). Three separately written dual simplexes agree on
the per-iteration cost while disagreeing about everything else, which makes it
a property of JAOS rather than an artefact of one rival.

M1 — readers, dual simplex, scaling, correct optima across Netlib (D7) — is
complete, as are the project's own phase 1 (know where we stand) and phase 2
(make it usable).

## Requirements

### Validated

Shipped and confirmed by measurement. Locked.

- ✓ **REQ-phase1-know-where-we-stand** — every run is timed and compared against
  HiGHS, SoPlex and Clp; the gap has a number, a decomposition, and a price on
  each missing feature (D81, D83).
- ✓ **REQ-phase2-make-it-usable** — options, model modification, warm re-solve,
  callbacks. A caller can configure the contract, change the problem including
  its dimensions, watch a solve, stop it and resume it (D64–D70, D77–D79).

### Active

The M2 scope. Detail and acceptance status in `.planning/REQUIREMENTS.md`.

- [ ] **REQ-ratio-test-candidate-admission** — how the ratio test admits
  candidates, decided before any code is written
- [ ] **REQ-presolve** — presolve and the postsolve that puts a solution back
- [ ] **REQ-lu-fill-and-markowitz** — the fill the factors carry, and the stale
  live counts Markowitz chooses on
- [ ] **REQ-hyper-sparse-downstream-results** — keep a sparse result sparse
  downstream of the solve that produced it
- [ ] **REQ-devex-pricing** — Devex as an alternative to the exact steepest-edge
  recurrence
- [ ] **REQ-reentry-oscillation** — the re-entry loop's oscillation, open, with
  no cure named
- [ ] **REQ-m2-competitive-gate** — what closes M2, including the guard factor
  that is not yet a number

### Out of Scope

Deferred to backlog for this milestone. None is refused; each is work this
milestone does not do.

- Writers — write MPS, write LP, write a solution file — not on the speed path
- Sensitivity and ranging — post-solve analysis, not on the speed path
- Exportable infeasibility and unboundedness certificates — same
- Exact rational verification of a final basis (Q8) — method undecided, GMP
  excluded by D11
- Python bindings — needs the C API to have stopped moving; phase 4 has not
- Primal simplex — D81 measured it worth nothing as a speed argument on the
  standard set, and PLAN.md and D81 both state it must never again be justified
  as one. Crossover will need it; crossover is not in M2.
- Barrier and crossover, deterministic parallel branch and bound, MILP,
  QP/MIQP/conic/NLP/MINLP, the NLP derivative strategy (Q5) — M3/M4 work
- LP and MPS dialect edges (Q2) — fixed as encountered, not scheduled
- MIPLIB 2017 easy and benchmark subsets — no MILP path to benchmark yet
- `pilot87`'s suboptimality bound — deferred **with a trigger**: it is promoted
  back into the milestone if it blocks a gate. It already refused two of D92's
  three candidate repairs, so this is a live possibility rather than a formality.

## Context

- **The design is written down; do not reconstruct it from the code.**
  `SPECS.md` is what JAOS is built to be, `PLAN.md` what is open, `DECISIONS.md`
  why with the measurement that closed each, `CHANGELOG.md` what landed and what
  it cost, `bench/README.md` the acceptance gate, `bench/compare/README.md` the
  comparison method, `docs/` tolerances, work units, scaling and format support.
- **A codebase map exists** at `.planning/codebase/` — ARCHITECTURE, STACK,
  STRUCTURE, CONVENTIONS, TESTING, INTEGRATIONS, CONCERNS. Read it to size work
  against the real tree; do not modify it.
- **Where the work lives.** `src/simplex.c` is the solve orchestrator (pricing,
  ratio tests, pivoting, the re-entry loop, publication); `src/lu.c` is the
  Markowitz factorization and Forrest-Tomlin updates; `src/check.c` is the
  independent checker and must never gain an include chain back to `scale.c`,
  `lu.c` or `simplex.c`; `src/scale.c` is Curtis-Reid. There is no presolve
  module yet.
- **Build and measure under WSL.** The Windows side has no compiler. GCC 14
  minimum. The campaigns all take `J=N` and cost minutes rather than tens of
  minutes when given one (D57). `$?` does not survive Git Bash → WSL: write the
  commands to a script file and run that.
- **Three of the four defects closed this milestone were found by sweeping
  `REFACTOR_EVERY` over 16..256** — a parameter the acceptance gate never
  varies. The standing suite structurally cannot catch that class; only a
  deliberate trajectory sweep can, and no target automates it.
- **This ingest resolved three warnings by user decision**, recorded in
  `.planning/INGEST-CONFLICTS.md` and carried into the roadmap: the candidate
  admission goes before presolve; the re-entry oscillation is roadmap work, not
  an accepted limitation; and the M2 guard factor stays absent until it is
  measured.

## Constraints

- **Determinism**: bit-identical results on every machine and every run — no
  clock decides anything, no iteration order depends on an address, no
  reassociated floating point (`-ffp-contract=off` is load-bearing), no unseeded
  randomness. (D8, D34)
- **Measurement**: every number needs a measurement on both sides — a tolerance,
  a threshold, an interval. Fitting a constant to one instance is how this
  project loses weeks. (D17)
- **Dependencies**: none, and no code read from another solver. Papers, theses
  and textbooks only. Two closed exceptions, neither extended: netlib's `emps`
  as a dev-time converter, and Unity for the test suite. (D2, D11, D12, D15)
- **Cost currency**: deterministic work units are the unit of cost. Wall-clock
  seconds are reported on every run and never enter `bench/results/*.txt` or a
  baseline — a baseline that changes every run cannot detect a regression.
  (D16, D17, D45, D57)
- **How a change is judged**: three things — solution digests for correctness,
  work units for determinism and cross-machine comparability, and a
  same-instance time ratio at `J=1` for what the other two cannot see. (D45)
- **The gate**: `bench/README.md`. Three sets — standard 94, Kennington 16,
  infeasible 29 — four per-instance predicates, all-or-nothing, plus the
  per-instance baseline diff which is now the only thing that can report a
  regression at all. Any phase that changes the solver passes it. Updating the
  baseline is a separate command and never a side effect of running the gate.
- **Tolerances are frozen** at the Netlib gate's close (D31); changing any of
  them is a changelog entry. All eight historical checker failures closed as
  defects with a mechanism and **not one closed by moving a number**.
- **Two tolerance spaces**: the solver runs on a scaled copy, so its tolerances
  are magnitudes in scaled space; the checker runs on the model as loaded.
  Neither converts into the other, and confusing them misreads every figure.
  (D27, D89, D92)
- **Reporting**: a geometric mean of per-instance ratios, never a sum over a
  set — two instances are 74% of the standard set's total. (D46)
- **Runtime**: C23, Linux, GCC 14 minimum, Apache 2.0, no external
  dependencies. Windows host, built and measured under WSL. (D1, D14)
- **The API configures the contract, never the method**: tolerances, budgets,
  logging and callbacks are the caller's; the pricing rule, refactorization
  interval and scaling mode are the solver's. (D64, D65, D79)
- **Process**: finish every source edit before launching a campaign — a run
  takes tens of minutes and is only valid for the tree that produced it.
  Commits are at Claude's discretion; pushes always need explicit approval.

<decisions>
## Locked Decisions

`DECISIONS.md` at the repository root is the authority — 92 closed entries,
D1 through D92, contiguous, each carrying the measurement that closed it.
This section does not replace it and does not restate it. It carries the IDs
that constrain M2's scope, so a planner knows which ground is already settled
before proposing to re-open it.

Every entry D1–D92 is **locked**. A requirement, plan or success criterion
that contradicts one is wrong by construction.

### What the milestone may not spend

- **D8, D34** — determinism, and the one place D8 rested on an argument now
  rests on a measurement.
- **D17** — no claim without a run; WSL2 is adequate for development and
  regression catching, **not for published figures**.
- **D45** — a change is judged on digests, work units, and a same-instance time
  ratio. The work counter is optimistic against the clock by a factor that is
  not constant.
- **D46** — geometric mean of per-instance ratios, never a sum.
- **D2, D11, D12, D15** — no dependencies, no code read from other solvers.
- **D31** — the tolerances freeze where they stand.
- **D88, D91** — the gate watches `relative_suboptimality`; growth past 2x above
  a floor of 1e-9 is a regression. No verdict reads the bound.

### Directions already closed — do not re-propose

- **D82** — partial pricing on the leaving-row sweep: refused, and on
  correctness, not on a trade. `pilot` publishes an out-of-tolerance OPTIMAL.
- **D84** — multiple pricing: refused. The leaving-row sweep is the wrong thing
  to make cheaper; the profile points at `admit_candidate` instead.
- **D74** — removing the loan the re-entry's clean-up takes: costs `pilot87`
  2.372x its iterations for the 0.980x it buys `pilot`. The direction is closed.
- **D63** — restarting the steepest-edge weights to the exact one rather than
  1.0: refused. `DSE_DRIFT = 10.0` is bounded on both sides and the interior is
  one value wide.
- **D35** — filtering basic columns out of the pricing sweep. **D36** — a
  scatter-form BTRAN. **D46** — caching `col_max_abs`; `PIVOT_SEARCH_LIMIT`
  stays at 4. **D61** — forcing the two dense sweeps' helpers inline, at
  0.9969x. **D76** — `restrict` on the LU kernel pointers. A crash basis,
  measured once and refused.
- **D73** — `certified_suboptimality` as a *verdict*: refuted. It is a sound
  bound that never overclaims and reads the same ~1e-25 on answers known to be
  1.04e-3 wrong as on correct ones.
- **D92** — judging the dual breach in the published space *instead of* the
  scaled one: refused at both scopes. The repair is the union of the two.

### Settled constants — do not re-derive

`REFACTOR_EVERY` 64 (D39) · `DSE_DRIFT` 10.0 (D63) · `PIVOT_SEARCH_LIMIT` 4
(D46) · `SPARSE_ALPHA_DEN` 4 and `SPARSE_RHO_DEN` 4 (D40, D41, D43, D61) ·
`SPARSE_COL_DEN` 8 (D45) · `LU_AGREE_TOL` 1e-5 (D86) · `IMPLIED_ROUNDS` 64
(D91) · the frozen tolerance set in `docs/tolerances.md` (D31).

### What each M2 requirement inherits

| Requirement | Locked decisions it must respect |
|---|---|
| REQ-ratio-test-candidate-admission | D82, D84 (both pricing halves refused — this is the remaining half), D45, D46 |
| REQ-presolve | D81 (its measured worth: 1.417x HiGHS, 1.136x SoPlex), D18 (the checker reads the model as loaded, never the reduced one), D8 |
| REQ-lu-fill-and-markowitz | D58, D59 (already paid), D46 (`PIVOT_SEARCH_LIMIT` confirmed at 4) |
| REQ-hyper-sparse-downstream-results | D38, D40–D44 (the pattern-reporting solves), D45 |
| REQ-devex-pricing | D63 (Devex is the named cure and why), D8 (weights are reinitialised by design — that must stay deterministic) |
| REQ-reentry-oscillation | D51 (the loan ledger), D74 (the only cure proposed, closed), D89 (the consequence removed, the oscillation left open), D92 |
| REQ-m2-competitive-gate | D17 (host), D46 (geometric mean), D52/D53/D60/D83 (the standing figures and how they were taken) |

</decisions>

## Key Decisions

Decisions taken at this milestone's opening. The project's own closed
decisions live in `DECISIONS.md`; this table is the roadmap-level record.

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Milestone 1 of this roadmap is the project's **M2 — LP competitiveness** | M1 and the project's phases 1 and 2 are complete; the gap has a number and a decomposition | — Pending |
| **Candidate admission goes before presolve** | D81 measured presolve at 1.417x/1.136x against a per-iteration gap of 2.53x that no rung moves, and states in as many words that this reorders the plan. PLAN.md's phase numbers were never changed; the user's resolution of WARNING 1 changes them. | — Pending |
| **The candidate-admission decision is the first deliverable, before any code** | PLAN.md: restricting the candidate set decides which column enters, putting Harris's two-pass guarantees at risk — "needs its own decision before any code" | — Pending |
| **The re-entry oscillation is roadmap work, not an accepted limitation** | User's resolution of WARNING 2, taken over the defensible "accepted limitation" reading. Its measured cost is small — 278 iterations of 116,071 on `pilot87` at interval 24, 0.24% (D89) — and no cure is named; D74 closed the only one proposed. First deliverable is investigative. | — Pending |
| **The M2 per-instance guard factor stays absent until measured** | `bench/compare/README.md` names "a stated factor" and no factor is stated anywhere in the ten ingested documents. This project's first rule is that every number carries a measurement on both sides; inventing one is named in its own documents as how it loses weeks. | — Pending |
| Writers, bindings, certificates, primal simplex, barrier, MILP and the rest go to backlog | None is on the path to a faster LP solve at T0 | — Pending |

---
*Last updated: 2026-08-12 after ingest of the JAOS planning record (10 documents, `.planning/ingest-manifest.yaml`) and milestone-1 scoping*
