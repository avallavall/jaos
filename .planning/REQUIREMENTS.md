# Requirements: JAOS — Just Another Optimization Solver

**Defined:** 2026-08-12
**Core Value:** A correct answer, bit-identical on every machine and every run, proved by a checker that had no access to the solver that produced it.
**Source:** `.planning/intel/requirements.md` (25 requirements extracted from `PLAN.md`, with two whose content sits in `bench/compare/README.md` and `SPECS.md`)

IDs are the descriptive slugs the ingest assigned, not PLAN.md's item numbers —
phase 6 numbers two different items `5` and inserts a `3a`. Keeping the slugs
keeps traceability with `.planning/intel/requirements.md`.

**On acceptance criteria.** Eighteen of the twenty-five requirements carry
`acceptance: absent`. That is the source's own state, not an extraction gap:
`PLAN.md` says what to build without saying what closes it. Where a requirement
below says *absent*, it is absent — deriving one is legitimate work for
`/gsd-plan-phase`, and **no number has been invented here to fill a gap.**

---

## v1 Requirements — M2, LP competitiveness

Seven requirements. Five carry no acceptance criterion, one carries a stated
one, one carries half of one.

### Ratio test

- [ ] **REQ-ratio-test-candidate-admission**: `admit_candidate` runs once per
  nonbasic variable of the pricing row and is 14.98% of instructions on `truss`
  under callgrind, against `ftran_prefix` at 6.68% — the O(`nvar`) half of D61's
  36.5%, the half neither refused pricing scheme touched, and the larger one.
  - *Acceptance:* **absent** — and a decision is required before implementation.
    PLAN.md: restricting its candidate set decides which column enters, putting
    Harris's two-pass guarantees at risk, so it "needs its own decision before
    any code".
  - *Caveat carried from the source:* instructions are not seconds and callgrind
    cannot see locality; `perf` is not installed on the development machine.

### Presolve

- [ ] **REQ-presolve**: Build presolve and postsolve — empty and singleton rows
  and columns, forcing and redundant constraints, bound tightening, fixed
  variables, duplicate rows and columns, dominated columns, and the postsolve
  that puts a solution back, "which is where the correctness risk lives, not in
  the reductions".
  - *Acceptance:* **absent** — no numeric target stated for JAOS's own presolve.
    D81's 1.417x (HiGHS) and 1.136x (SoPlex) are readings of two competitors,
    not targets for JAOS.

### Per-iteration cost

- [ ] **REQ-lu-fill-and-markowitz**: The factorization and the scatter its
  factors cost every solve — 77.8% of the standard set. Opened and partly paid
  (D58, D59). What is left: the stale live counts that make Markowitz choose on
  a pessimistic estimate, and the fill itself — the factors carry 2.673x the
  nonzeros of the basis (4.801 on `maros-r7`), two thirds of every factorization
  is free triangularization and 31.8% is where Markowitz actually chooses.
  - *Acceptance:* **absent**.
  - *Bounded:* going further than those two means left-looking elimination,
    "which is a rewrite and needs its own decision" — out of this milestone.
  - *Struck off by measurement, do not re-cost:* the missing row-to-position
    lookup (`compact_pivot_row`, under 0.5% on `maros-r7`) and per-column arrays
    against a single arena (allocation 0.73%, `_int_malloc` 0.30%). The arena's
    remaining argument is locality and needs a cache simulation before it is
    either costed or dropped.

- [ ] **REQ-hyper-sparse-downstream-results**: `stocfor3` is a memory-traffic
  instance and the fourth worst in the set — 6.79x per iteration on 0.97x the
  iterations, with the triangular solves at 43.0% and `memset` plus `memcpy`
  plus `malloc` at 18.8%, against 11.3% for `dfl001` as a control. The repair is
  to keep the sparse results sparse downstream, not to make the clears faster.
  - *Acceptance:* **absent**.

### Search path

- [ ] **REQ-devex-pricing**: Devex pricing as an alternative to the exact
  steepest-edge recurrence — the cure D63 points at for the iteration-count half
  of the tail. Its weights are approximate by construction and reinitialised by
  design, and it drops the second FTRAN per iteration.
  - *Acceptance:* **stated** — full gate, with iteration count and per-iteration
    cost reported separately, "because trading one for the other is exactly what
    it does".

- [ ] **REQ-reentry-oscillation**: The re-entry loop's oscillation. D51 named the
  mechanism — every clean-up pivot borrows in order to repair, and repaying is
  what creates the next round's work. D89 removed the consequence (the published
  answer no longer depends on where `SETTLE_ROUNDS` falls) and states plainly
  that it "does not make the loop converge".
  - *Acceptance:* **absent** — no cure is named. D74 closed the only one
    proposed, by measuring that removing the loan costs `pilot87` 2.372x its
    iterations for the 0.980x it buys `pilot`.
  - *Measured cost, stated honestly:* 278 iterations of 116,071 on `pilot87` at
    interval 24 — **0.24%** (D89). This is small. It enters the milestone as
    open work by explicit decision, not because the number argues for it; the
    first deliverable is investigative rather than implementation.
  - *Source conflict:* PLAN.md's enclosing section header says "all four are now
    closed" while this entry says the oscillation is open. WARNING 2 in
    `.planning/INGEST-CONFLICTS.md`; resolved in favour of open.

### Milestone gate

- [ ] **REQ-m2-competitive-gate**: What closes M2 — JAOS strictly faster than the
  best competitor at tier T0, on the geometric mean of per-instance time ratios
  over the standard set, with a guard that no single instance is more than a
  stated factor slower. The geometric mean is what ratios require; an arithmetic
  mean of ratios flatters whichever side wins the big instances.
  - *Acceptance:* **partially stated.** "Strictly faster than the best competitor
    at T0 on the geometric mean" is precise. **The per-instance guard factor is
    written as "a stated factor" and no number is given anywhere in the ten
    ingested documents. It is left unset. It is to be measured, not guessed.**
  - *Standing per D83:* 3.72x vs HiGHS 1.15.1, 1.34x vs SoPlex 8.0.3, 3.77x vs
    Clp 1.17.11.
  - *Host constraint:* D17 — a number taken under WSL is a development number
    and cannot close a gate.

---

## Complete

Recorded so they are not re-planned. Both are marked complete by PLAN.md's own
phase table.

- ✓ **REQ-phase1-know-where-we-stand** — time every run, compare against HiGHS,
  SoPlex and Clp. *Satisfied: the gap has a number, a decomposition, and a price
  on each missing feature* (D81, D83).
- ✓ **REQ-phase2-make-it-usable** — options, model modification, warm re-solve,
  callbacks. *Satisfied* (D64, D65, D66, D67, D68, D69, D70, D77, D78, D79).

---

## Backlog — deferred from this milestone

Sixteen requirements. None is refused; each is work M2 does not do. Promotion
back into a milestone goes through `/gsd-review-backlog`.

### Writers and post-solve analysis

- **REQ-write-mps** — write MPS. *Acceptance: absent.*
- **REQ-write-lp** — write LP. *Acceptance: absent.*
- **REQ-write-solution-file** — write a solution file. *Acceptance: absent.*
- **REQ-sensitivity-and-ranging** — sensitivity and ranging. *Acceptance: absent.*
- **REQ-exportable-certificates** — infeasibility and unboundedness certificates,
  exportable. *Acceptance: absent.*
- **REQ-exact-rational-verification** — exact rational verification of a final
  basis (Q8). GMP is the obvious tool and D11 excludes it; the alternatives to
  weigh are iterative refinement, interval arithmetic in plain `double`, or
  hand-rolled rationals used only to verify a final basis. *Acceptance: absent —
  the method is undecided.*

### Bindings

- **REQ-python-bindings** — Python first. "Nothing to design until the C API
  stops moving." Needs phases 2 and 4 to have stopped moving; 2 has.
  *Acceptance: absent.*

### Algorithms not on the T0 speed path

- **REQ-primal-simplex** — which phase 4's crossover needs anyway. D81 measured
  it worth nothing as a speed argument on the standard set — both rivals chose
  the dual on every instance when free to, on identical iteration counts — and
  D85 removed the other reason it was being waited on. **PLAN.md and D81 both
  state this must never again be justified as a speed argument.**
  *Acceptance: absent.*
- **REQ-barrier-and-crossover** — barrier for LP with crossover. SPECS.md
  records it as "not optional at large scale". *Acceptance: absent.*
- **REQ-deterministic-parallel-bnb** — with `jaos_thread.h` (D13, D8).
  *Acceptance: absent* — D8 requires bit-identical results, and D8 records that
  parallel determinism costs performance by an amount that is "a measurement we
  owe, not a number we assume".
- **REQ-milp** — MILP correctness on the MIPLIB 2017 easy subset, then cuts, MIP
  presolve and primal heuristics against the benchmark subset. *Acceptance:
  correctness on the easy subset first; the benchmark subset after.*
- **REQ-qp-conic-nlp-minlp** — convex QP and MIQP over the barrier machinery,
  then conic, then NLP, then MINLP. Network specializations slot in as detection
  plus dedicated algorithms — they accelerate, they do not gate. SDP stays
  unscheduled. *Acceptance: absent.*
- **REQ-nlp-derivative-strategy** — Q5: AD, finite differences, or
  user-supplied. Decided when phase 7 reaches NLP, because it shapes that
  engine's public API. *Acceptance: absent — deferred by design.*
- **REQ-miplib-subsets** — MIPLIB 2017 easy and benchmark subsets, both "not
  started" in SPECS.md §8. *Acceptance: absent.*

### Format and verification

- **REQ-lp-mps-dialect-edges** — Q2: LP and MPS dialect edge semantics, fixed as
  encountered. One is closed: an `RHS` entry on the objective row sets a
  constant, JAOS negates it as CPLEX documents, the published Netlib optima omit
  it, and the gate carries the constant per instance in its manifest.
  *Acceptance: each edge case lands in `docs/format-support.md` in the same
  commit that settles it.*
- **REQ-pilot87-suboptimality-bound** — `pilot87`'s suboptimality bound is not
  understood. Across the four variants D92 measured, its `gap_positive` moves
  between 0.0068 and 26.7 while every one of those answers sits inside tolerance
  of Koch's reference and reads `dual_feasible`. Whether a `Q` of 26.7 is the
  answer getting worse or the bound going slack is not separable today.
  *Acceptance: a second, independent estimate of `P − P*` on the same point.
  `certified_suboptimality` is explicitly not it, and that is measured (D73).*
  - **Deferred with a trigger.** It is promoted back into this milestone if it
    blocks a gate. It already refused two of D92's three candidate repairs, and
    PLAN.md's own words are "a change detector on a quantity nobody can
    interpret is a gate that can only ever be obeyed" — so the trigger is a live
    possibility, not a formality.

---

## Out of Scope

Excluded by closed decision, not deferred.

| Feature | Reason |
|---------|--------|
| External dependencies of any kind | D2, D11. Two closed exceptions, neither extended: netlib's `emps` as a dev-time converter, Unity for the test suite. |
| Code read from another solver's source | D12. Papers, theses and textbooks only. Running a competitor as a benchmark is not what D12 forbids; reading its source is. |
| GMP for exact arithmetic | D11 excludes it; Q8 weighs the alternatives. |
| Choosing the algorithm through the API; turning scaling off or picking its mode | D64 — the API configures the contract, never the method. |
| Constraint programming | D10. |
| Metaheuristics | D9 — primal heuristics inside, metaheuristics outside. |
| GPU or any non-CPU target | D3. |
| SDP | Unscheduled in SPECS.md. |
| GLPK as a comparison competitor | "It would be a floor rather than a target" — deliberately absent from the harness. |
| A crash basis | Measured once and refused: it destroys the exact steepest-edge weights the slack basis gives. |
| Partial pricing on the leaving-row sweep; multiple pricing | D82, D84 — both built, swept and refused on correctness, not on a trade. |

---

## Traceability

| Requirement | Phase | Acceptance in source | Status |
|-------------|-------|----------------------|--------|
| REQ-ratio-test-candidate-admission | Phase 1 | absent (decision required first) | Pending |
| REQ-presolve | Phase 2 | absent | Pending |
| REQ-lu-fill-and-markowitz | Phase 3 | absent | Pending |
| REQ-hyper-sparse-downstream-results | Phase 3 | absent | Pending |
| REQ-devex-pricing | Phase 4 | stated | Pending |
| REQ-reentry-oscillation | Phase 4 | absent (no cure named) | Pending |
| REQ-m2-competitive-gate | Phase 5 | partial (guard factor unset) | Pending |

**Coverage:**
- v1 requirements: 7 total
- Mapped to phases: 7
- Unmapped: 0 ✓
- Duplicated across phases: 0 ✓

**Requirements not in v1:** 2 complete, 16 backlog. 7 + 2 + 16 = 25, the full
extracted set.

**Acceptance coverage:** 1 of 7 v1 requirements carries a stated acceptance
criterion, 1 carries half of one, 5 carry none. Deriving the missing five and
the missing half is `/gsd-plan-phase` work — and for the M2 guard factor it is
a measurement, not a choice.

---
*Requirements defined: 2026-08-12*
*Last updated: 2026-08-12 after initial roadmap creation*
