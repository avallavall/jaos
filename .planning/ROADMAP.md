# Roadmap: JAOS — Just Another Optimization Solver

## Overview

This milestone is the project's **M2 — LP competitiveness**. M1 shipped a
revised dual simplex that is correct on all 139 Netlib reference instances; the
gap to the field is now measured rather than guessed, and decomposed: 3.72x
HiGHS, 1.34x SoPlex, 3.77x Clp per solve at T0, on 1.47x / 0.70x / 1.67x their
iterations and 2.54x / 1.92x / 2.26x per iteration (D83). JAOS takes 30% fewer
iterations than SoPlex and is still slower, and three separately written dual
simplexes agree about the per-iteration cost while disagreeing about everything
else — so it is a property of JAOS.

The journey is therefore a speed ladder taken in the order the measurements
argue for, not the order PLAN.md's phase numbers were written in. The head is
the ratio test's candidate admission, and its first deliverable is the decision
that PLAN.md says must exist before any code. Presolve follows. Then the
per-iteration half of the tail — the factorization's fill and the triangular
solves' downstream traffic — then the iteration-count half, Devex pricing
alongside the re-entry loop's unexplained oscillation. The milestone closes on a
comparison that can actually close a gate, with both halves of its criterion
stated rather than one.

Every phase that touches the solver passes `bench/README.md`'s three campaigns
and its per-instance baseline diff. None of them may spend determinism, and none
of them may introduce a number without a measurement on both sides of it.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Candidate admission in the ratio test** - Decide how the ratio test admits candidates, with a measurement, then implement the decision
- [ ] **Phase 2: Presolve and postsolve** - Reduce a model before the simplex sees it, and put the solution back in the caller's model
- [ ] **Phase 3: The factorization and the solves that read it** - Less fill in the factors, and sparse results that stay sparse downstream
- [ ] **Phase 4: The search path** - Devex pricing, and the re-entry loop's oscillation explained or cured
- [ ] **Phase 5: Close M2** - State both halves of the close criterion and take the measurement that closes it

## Phase Details

### Phase 1: Candidate admission in the ratio test

**Goal**: How the ratio test admits candidates is settled by a measured comparison, and the settled rule is in the solver without weakening what Harris's two passes guarantee.
**Depends on**: Nothing (first phase)
**Requirements**: REQ-ratio-test-candidate-admission
**Success Criteria** (what must be TRUE):

  1. A closed decision states how candidates are admitted and carries the measurement on both sides that chose it — and it exists before any implementation commit does, because PLAN.md states this item "needs its own decision before any code". — **MET by 01-05**: `DECISIONS.md` **D93**, indexed, 428 lines added and none changed. The pre-code record is `01-CONTEXT.md` of 2026-08-12 — thirteen numbered items D-01 through D-13, committed before `f2ed4bc` existed — and D93 is that decision closed with the measurement rather than one taken after the fact.
  2. That decision says what the chosen rule does to Harris's two-pass guarantee, and the case it must refuse is built and confirmed refused rather than assumed absent. — **MET by 01-05**: D93 records that the guarantee is a function of the candidate set and the arrival order alone, both preserved by construction because `admit_candidate`'s body is untouched and the bitmap is walked ascending in `v`. The refused case was built rather than assumed: `test_nonbasic_notices_a_missed_hook` asserts the bitmap does *not* match with a hook omitted, and two injected faults are named with what caught each — a no-op `jm_nonbasic_remove` caught by two unit tests while the run-time cross-check is **correctly silent** (a superset changes no candidate), and a no-op `jm_nonbasic_insert` aborting a solve at `src/simplex.c:1698`.
  3. `admit_candidate`'s cost is re-read on `truss` against the 14.98% of instructions it stood at, and reported beside a `J=1` same-instance time ratio — instructions are not seconds, and callgrind cannot see locality. — **MET by 01-04**: callgrind on both binaries in one session puts `admit_candidate` at **14.79% → 14.20%** (the parent reproducing D84's 14.98% to within 0.19pp), against `ftran_prefix` at 6.59% → 6.64%. Beside it, the `J=1` time ratio over the standard set is **0.9709x** (geometric mean of per-instance ratios; ratio of totals 0.9847x, not the result), `truss` 0.9759x. **And the instruction total ROSE 1.60%** — `admit_candidate` sheds 199M while the caller it is inlined into gains 995M, which is a relocation rather than a saving and is exactly what reporting the two beside each other exists to catch.
  4. All three netlib campaigns report PASS, and the per-instance baseline diff shows no regression on any of the 139 instances, on any of the four predicates or the work count. — **MET by 01-03** (44c0ef6, e8c2f58): three PASS at J=12, 0 regressed / 0 improved / 0 new on all three, 110 solution digests and 29 infeasibility verdicts unmoved over the 139 instances, iterations 1.0000x per instance, work down on 118 and up on none.

**Plans:** 5/5 plans complete

Plans:

- [x] 01-01-PLAN.md — the nonbasic bitmap wired end to end: storage, maintenance at all eight membership sites, the list-driven dense scan, a debug-build cross-check against the scan it replaces, and the unit tests shown to reject a broken maintenance sequence
- [x] 01-02-PLAN.md — the work charge counts what the scan visited, behind the one-way gate the work-unit contract earns, with `docs/work-units.md` landing alongside it
- [x] 01-03-PLAN.md — all three campaigns, 110 solution digests and 29 infeasibility verdicts confirmed unmoved over the 139 instances, and only then the three baselines rewritten and confirmed by a following gate run
- [x] 01-04-PLAN.md — the `J=1` same-instance time ratio as a geometric mean over the standard set, callgrind on `truss` beside it, read against 4.2%
- [x] 01-05-PLAN.md — D93 with the measurement on both sides, the changelog entry, and the `SPECS.md` figures this phase moved

**Acceptance in source**: absent, and a decision is required before implementation. Deriving a numeric target is plan-phase work; the threshold the plans measure against is D-13's 4.2% — three times the harness's measured repeatability — and `01-05` is required to show that derivation rather than assert it.
**Acceptance derived in planning**: ACCEPT at a geometric mean of per-instance `J=1` time ratios of 4.2% or better over the standard set, with all 139 digests unmoved. Below 4.2% the phase reports INCONCLUSIVE and closes with a refusal entry rather than a yes (D-04, D-13).
**Acceptance measured**: **INCONCLUSIVE** (01-04, closed as D93 by 01-05). No answer moved — **110 solution digests and 29 infeasibility verdicts over 139 instances**, with iteration counts 1.0000x per instance — and the time ratio is **0.9709x, a 2.91% improvement, against a 4.2% bar**. Six readings pooling all six rounds run from 2.16% to 4.07% and none reaches it. **The verdict does nonetheless depend on having run six rounds**: on the protocol as literally written — pooled minimum over three alternating rounds — rounds 1–3 read 1.01% and rounds 4–6 read **5.12%, over the bar**, because running order alone is worth 2.4 percentage points on this host. Two findings belong with it. **The negative control closes it**: eight instances that provably cannot be sped up, because their work is bit-identical under both binaries, read 0.9699x paired and 0.9356x pooled — a 3.0–6.4% "improvement" where the truth is zero, against a 2.91% headline. And callgrind says the instruction total **rose 1.60%**. The right finding is therefore **"the bar cannot be tested on this host"** rather than "the candidate missed the bar"; the host's own repeatability, measured the way D81 measured the 1.4% the bar is three times, is **6.27%**. The code stays in the tree under the developer's pre-authorisation of 2026-08-12.
**Phase closed**: all four criteria met. D93 is `DECISIONS.md`'s entry for it, the changelog carries a six-line entry pointing at it, and `SPECS.md`'s one moved figure — the warm-against-cold work ratio, 0.0162 → 0.0164 under the redefined charge — was re-read from the records `01-03` produced. `01-04-SUMMARY.md` carries an appended correction section from an independent audit that accepts the verdict and refutes three of the grounds first given for it.
**Ordering note**: this phase precedes presolve by explicit decision (WARNING 1, resolved). D81 measured presolve at 1.417x/1.136x against a per-iteration gap of 2.53x that no rung moves, and says in as many words that this reorders the plan. PLAN.md's phase numbers were never changed; this roadmap changes them.
**Closed ground**: D82 and D84 refused both halves of phase 6 item 3 — partial pricing on the leaving-row sweep and multiple pricing — on correctness rather than on a trade. This is the half neither touched, and D84's own profile is what points here.

### Phase 2: Presolve and postsolve

**Goal**: A model can be reduced before the simplex sees it, and a solution to the reduced model comes back as a solution to the one the caller loaded.
**Depends on**: Phase 1
**Requirements**: REQ-presolve
**Success Criteria** (what must be TRUE):

  1. A caller loading any of the 139 reference instances gets the same verdict, and an objective inside the gate's tolerance of Koch's reference, whether the reductions fire or not.
  2. Postsolve returns values, activities, duals and reduced costs in the original model's rows and columns, and the independent checker — which reads the model as loaded and has no access to the reduced one — accepts all 139. The correctness risk lives here, not in the reductions.
  3. Each reduction reports what it removed, so what presolve is worth on the standard set is a measured number *for JAOS* rather than one carried over from D81's reading of two competitors.
  4. Determinism holds across two solves with the basis cleared between them: status, iteration count, work units and the bits of every published value agree.

**Plans**: TBD
**Acceptance in source**: absent — no numeric target is stated for JAOS's own presolve.
**Open question this phase inherits**: T0 is defined as "the simplex and only the simplex", presolve off on both sides. See Open Questions below — presolve's contribution to the *stated* M2 close criterion is zero at that rung, and nothing in the ingest set says how the ladder is recalibrated once JAOS has one.
**Code note**: there is no presolve module in the tree today. The checker's independence is structural — `src/check.c` has no include chain to `scale.c`, `lu.c` or `simplex.c` — and a presolve must not be the thing that creates one.

### Phase 3: The factorization and the solves that read it

**Goal**: An iteration costs less, because the factors carry less fill and a sparse result stays sparse downstream of the solve that produced it.
**Depends on**: Phase 2
**Requirements**: REQ-lu-fill-and-markowitz, REQ-hyper-sparse-downstream-results
**Success Criteria** (what must be TRUE):

  1. Markowitz chooses on live counts that are current rather than stale, and the fill the factors carry is reported per instance against the 2.673 nonzeros-per-basis-nonzero the set stands at, and the 4.801 `maros-r7` stands at.
  2. `stocfor3`'s per-iteration cost is re-read with `dfl001` as the control it was measured against, and the share spent in `memset` plus `memcpy` plus `malloc` is reported against the 18.8% before.
  3. Every figure is a geometric mean of per-instance ratios rather than a sum over the set, and the instances that move against the mean are named rather than averaged away.
  4. All three campaigns PASS with no per-instance regression; work units carry the verdict on determinism and a `J=1` time ratio says whether the units bought anything.

**Plans**: TBD
**Acceptance in source**: absent for both requirements.
**Bounded**: going further than the stale live counts and the fill means left-looking elimination, which PLAN.md calls a rewrite needing its own decision. That decision may be written in this phase; the rewrite is not started in it.
**Struck off by measurement — do not re-cost**: `compact_pivot_row`'s missing row-to-position lookup (under 0.5% on `maros-r7`) and per-column arrays against a single arena (allocation 0.73%, `_int_malloc` 0.30%). The arena's remaining argument is locality and needs a cache simulation before it is either costed or dropped.

### Phase 4: The search path

**Goal**: JAOS spends fewer iterations on the instances measured to spend extra ones, and the re-entry loop's oscillation stops being unexplained.
**Depends on**: Phase 3
**Requirements**: REQ-devex-pricing, REQ-reentry-oscillation
**Success Criteria** (what must be TRUE):

  1. Devex pricing runs in place of the exact steepest-edge recurrence, the full gate passes, and iteration count and per-iteration cost are reported separately — trading one for the other is exactly what Devex does.
  2. `pilot`, `pilot87`, `25fv47` and `greenbea` — the four discarding their steepest-edge weights on 80–93% of their iterations (D63) — are reported individually, not folded into a set mean.
  3. Why `pilot87`'s re-entry loop oscillates has a written mechanism with a measurement behind it. Either a cure survives the full gate, or the direction is closed the way D74 closed the last one — by measuring what it costs, not by declaring the cost small.
  4. All three campaigns PASS with no per-instance regression, and the published answer still does not depend on where `SETTLE_ROUNDS` falls (D89).

**Plans**: TBD
**Acceptance in source**: stated for Devex (full gate, with iteration count and per-iteration cost reported separately). Absent for the oscillation — no cure is named.
**On the oscillation, honestly**: its measured cost is 278 iterations of 116,071 on `pilot87` at interval 24 — 0.24% (D89). That is small, and "accepted limitation" was a defensible reading of the source. It is in this milestone as open work by explicit decision, and its first deliverable is investigative rather than implementation. D51 named the mechanism (every clean-up pivot borrows to repair, and repaying creates the next round's work); D74 closed the only cure proposed, at 2.372x `pilot87`'s iterations for the 0.980x it bought `pilot`; D89 removed the consequence and states plainly that it "does not make the loop converge".
**Closed ground**: restarting the weights to the exact one rather than 1.0 is refused (D63), and `DSE_DRIFT = 10.0` is bounded on both sides with an interior one value wide.

### Phase 5: Close M2

**Goal**: The competitive claim is made on numbers that can close a gate, with both halves of the close criterion stated rather than one.
**Depends on**: Phase 4
**Requirements**: REQ-m2-competitive-gate
**Success Criteria** (what must be TRUE):

  1. The per-instance guard factor is either a number with a measurement on both sides of it, or the criterion is recorded as closing on the geometric mean alone — and whichever it is, is written down with what decided it. **It is unset today and no number for it appears anywhere in this roadmap.**
  2. The T0 comparison is re-run against HiGHS 1.15.1, SoPlex 8.0.3 and Clp 1.17.11 at their pinned checksums, with tolerances equalised explicitly and every competitor run judged by the same verified-answer rule JAOS is — a time without a verified answer is discarded.
  3. The result is reported as a geometric mean of per-instance time ratios over the standard set, taken on a host that satisfies D17: a number taken under WSL is a development number and cannot close this gate.
  4. Any instance failing the guard, if a guard exists, is named rather than averaged away — and the rung each record was taken under travels with it.

**Plans**: TBD
**Acceptance in source**: partially stated. "Strictly faster than the best competitor at T0 on the geometric mean" is precise; the guard factor is written as "a stated factor" and no factor is stated anywhere in the ten ingested documents.
**Standing**: 3.72x vs HiGHS, 1.34x vs SoPlex, 3.77x vs Clp at T0 (D83). Measured repeatability of the harness is 1.4%; any claim below that is a claim about the machine.
**Known blocker**: this phase needs a controlled host and the ingest set names none. See Open Questions.

## Open Questions

Surfaced rather than answered. Each needs a decision or a measurement that does
not exist in the ten ingested documents.

1. **The M2 per-instance guard factor is unset.** `bench/compare/README.md`
   names "a stated factor" and no number is given anywhere. Phase 5 criterion 1
   is where it gets one — by measurement, or by an explicit record that the
   criterion closes on the geometric mean alone. It is not guessed here.
   (WARNING 3, resolved as: leave absent.)

2. **T0 is "the simplex and only the simplex", presolve off on both sides.** So
   presolve — Phase 2 — contributes nothing to the M2 close criterion *as
   stated*. `bench/compare/README.md` says the ladder "is recalibrated as JAOS
   grows" and does not say how. Whether M2 closes at T0 alone, or across rungs
   once JAOS has a presolve, is not decided in the source. This does not block
   Phase 2; it is a question Phase 5 must answer before it reports.

3. **No controlled host is named.** D17: WSL2 is adequate for development and
   regression catching, not for published figures, and the machine that builds
   and measures JAOS is a Windows host running WSL. Phase 5 cannot produce a
   gate-closing number without a host that satisfies D17. Phases 1–4 are
   unaffected — their verdicts are digests, work units and a same-instance time
   ratio, all of which WSL can carry.

4. **Five of seven requirements carry no acceptance criterion**, and a sixth
   carries half of one. That is `PLAN.md`'s own state — it says what to build
   without saying what closes it. Deriving each is `/gsd-plan-phase` work, and
   for the guard factor it is a measurement rather than a choice.

5. **The gate cannot see the defect class that produced three of last
   milestone's four closures.** Those were found by sweeping `REFACTOR_EVERY`
   over 16..256, a parameter the gate never varies, and no Makefile target
   automates the sweep. Any phase here that changes the factorization or the
   pivot path should consider whether its own trajectory needs sweeping, and
   `make clean` between settings or the sweep measures one binary N times.

## Coverage

✓ All 7 v1 requirements mapped to exactly one phase each
✓ No orphans, no duplicates
✓ 2 requirements recorded complete and not re-planned
✓ 16 requirements deferred to backlog with reasons, one of them (`pilot87`'s
  suboptimality bound) carrying a promotion trigger: it re-enters the milestone
  if it blocks a gate

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Candidate admission in the ratio test | 5/5 | Complete    | 2026-08-12 |
| 2. Presolve and postsolve | 0/TBD | Not started | - |
| 3. The factorization and the solves that read it | 0/TBD | Not started | - |
| 4. The search path | 0/TBD | Not started | - |
| 5. Close M2 | 0/TBD | Not started | - |

---
*Roadmap created: 2026-08-12 from `.planning/intel/` (ingest of 10 documents) and `.planning/codebase/`*
