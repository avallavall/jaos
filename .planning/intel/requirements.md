# Requirements

Extracted from `PLAN.md` (type PRD, precedence 2). PLAN.md is "what is open, in
the order it will be done"; these entries are its phases, items and open
questions. Phases 1 and 2 are recorded as complete because PLAN.md's own phase
table says so — a downstream roadmap needs to know they are not open work.

Where PLAN.md states no acceptance criterion, `acceptance:` is marked absent
rather than inferred. Several entries carry a `- note:` line pointing at a
conflict recorded in `.planning/INGEST-CONFLICTS.md`; those are the entries a
roadmapper must not take at face value.

IDs are derived from descriptive slugs, not from PLAN.md's item numbers: phase 6
numbers two different items `5` and inserts a `3a`.

Source paths are relative to the repository root.

---

## REQ-phase1-know-where-we-stand
- source: PLAN.md
- description: Time every run, and compare against HiGHS, SoPlex and Clp. Complete per PLAN.md's phase table — every question it was opened to answer has a number (D81, D83).
- acceptance: satisfied — the gap has a number, a decomposition, and a price on each missing feature
- scope: benchmarking, comparison harness, tier ladder

## REQ-phase2-make-it-usable
- source: PLAN.md
- description: Options, model modification, warm re-solve, callbacks. Complete per PLAN.md's phase table — a caller can configure the contract, change the problem in any way including its dimensions, watch a solve, stop it and resume it.
- acceptance: satisfied — D64, D65, D66, D67, D68, D69, D70, D77, D78, D79
- scope: public API, options, logging, model modification, warm re-solve, callbacks

## REQ-presolve
- source: PLAN.md
- description: Phase 3. Build presolve and postsolve. Measured worth: 1.417x for HiGHS and 1.136x for SoPlex (D81), against a per-iteration gap of 2.53x that no rung moves. PLAN.md states it is "no longer the largest single algorithmic gap".
- acceptance: absent — no numeric target stated for JAOS's own presolve
- scope: empty and singleton rows and columns, forcing and redundant constraints, bound tightening, fixed variables, duplicate rows and columns, dominated columns, and the postsolve that puts a solution back — "which is where the correctness risk lives, not in the reductions"
- note: ordering against phase 6 is disputed — see WARNING 1 in INGEST-CONFLICTS.md

## REQ-write-mps
- source: PLAN.md
- description: Phase 4. Write MPS.
- acceptance: absent
- scope: writers

## REQ-write-lp
- source: PLAN.md
- description: Phase 4. Write LP.
- acceptance: absent
- scope: writers

## REQ-write-solution-file
- source: PLAN.md
- description: Phase 4. Write a solution file.
- acceptance: absent
- scope: writers

## REQ-sensitivity-and-ranging
- source: PLAN.md
- description: Phase 4. Sensitivity and ranging.
- acceptance: absent
- scope: post-solve analysis

## REQ-exportable-certificates
- source: PLAN.md
- description: Phase 4. Infeasibility and unboundedness certificates, exportable.
- acceptance: absent
- scope: certificates

## REQ-exact-rational-verification
- source: PLAN.md
- description: Phase 4. Exact rational verification of a final basis. Open question Q8: GMP is the obvious tool and D11 excludes it; the alternatives to weigh are iterative refinement, interval arithmetic in plain `double`, or hand-rolled rationals used only to verify a final basis.
- acceptance: absent — the method is undecided
- scope: exact verification, Q8

## REQ-python-bindings
- source: PLAN.md
- description: Phase 5. Python bindings first. "Nothing to design until the C API stops moving." Needs phases 2 and 4 to have stopped moving; 2 has.
- acceptance: absent
- scope: bindings

## REQ-lu-fill-and-markowitz
- source: PLAN.md
- description: Phase 6 item 1. The factorization and the scatter its factors cost every solve — 77.8% of the standard set. Opened and partly paid (D58, D59). What is left: the stale live counts that make Markowitz choose on a pessimistic estimate, and the fill itself — the factors carry 2.673x the nonzeros of the basis, two thirds of every factorization is free triangularization and 31.8% is where Markowitz actually chooses. Going further than those two means left-looking, "which is a rewrite and needs its own decision".
- acceptance: absent
- scope: LU factorization, fill reduction, Markowitz pivot search
- note: two sub-items are struck off by measurement and must not be re-costed — the missing row-to-position lookup (`compact_pivot_row`, under 0.5% on `maros-r7`) and per-column arrays against a single arena (allocation 0.73%, `_int_malloc` 0.30%). The arena's remaining argument is locality and needs a cache simulation before it is either costed or dropped.

## REQ-ratio-test-candidate-admission
- source: PLAN.md
- description: Phase 6 item 3a, "the new head of this list". `admit_candidate` is 14.98% of instructions on `truss` under callgrind against `ftran_prefix` at 6.68%; it runs once per nonbasic variable of the pricing row, so it is the O(`nvar`) half of D61's 36.5% — the half neither refused pricing scheme touched, and the larger one. PLAN.md states it "needs its own decision before any code" because restricting its candidate set decides which column enters, putting Harris's two-pass guarantees at risk.
- acceptance: absent — a decision is required before implementation
- scope: ratio test, candidate admission, Harris two-pass
- note: instructions are not seconds and callgrind cannot see locality; `perf` is not installed on the development machine

## REQ-hyper-sparse-downstream-results
- source: PLAN.md
- description: Phase 6 item 4. `stocfor3` is a memory-traffic instance and the fourth worst in the set: 6.79x per iteration on 0.97x the iterations, with the triangular solves 43.0% and `memset` plus `memcpy` plus `malloc` 18.8%, against 11.3% for `dfl001` as a control. PLAN.md records it as "measured and left; the repair is to keep the sparse results sparse downstream rather than to make the clears faster."
- acceptance: absent
- scope: hyper-sparsity, dense vector clearing, triangular solves

## REQ-devex-pricing
- source: PLAN.md
- description: Phase 6 item 6. Devex pricing as an alternative to the exact steepest-edge recurrence — the cure D63 points at for the iteration-count half of the tail. Its weights are approximate by construction and reinitialised by design, and it drops the second FTRAN per iteration.
- acceptance: full gate, with iteration count and per-iteration cost reported separately, "because trading one for the other is exactly what it does"
- scope: pricing rule, steepest-edge weights, search path

## REQ-primal-simplex
- source: PLAN.md
- description: Phase 6 item 7. A primal simplex, which phase 4's crossover needs anyway. D81 measured it as worth nothing as a speed argument on the standard set — both rivals chose the dual on every instance when free to, on identical iteration counts — and D85 removed the other reason it was being waited on.
- acceptance: absent
- scope: primal simplex, crossover prerequisite
- note: PLAN.md and D81 both state this must never again be justified as a speed argument

## REQ-barrier-and-crossover
- source: PLAN.md
- description: Phase 7. Barrier for LP with crossover. SPECS.md records it as "not optional at large scale".
- acceptance: absent
- scope: interior point, crossover

## REQ-deterministic-parallel-bnb
- source: PLAN.md
- description: Phase 7. Deterministic parallel branch and bound with `jaos_thread.h` (D13, D8).
- acceptance: absent — D8 requires bit-identical results, and D8 records that parallel determinism costs performance by an amount that is "a measurement we owe, not a number we assume"
- scope: parallelism, determinism

## REQ-milp
- source: PLAN.md
- description: Phase 7. MILP correctness on the MIPLIB 2017 easy subset, then cuts, MIP presolve and primal heuristics against the benchmark subset.
- acceptance: correctness on the MIPLIB 2017 easy subset first; the benchmark subset after
- scope: branch and bound, cuts, MIP presolve, primal heuristics

## REQ-qp-conic-nlp-minlp
- source: PLAN.md
- description: Phase 7. Convex QP and MIQP over the barrier machinery, then conic, then NLP, then MINLP. Network specializations slot in as detection plus dedicated algorithms — "they accelerate, they do not gate". SDP stays unscheduled.
- acceptance: absent
- scope: QP, MIQP, conic, NLP, MINLP, network specializations

## REQ-nlp-derivative-strategy
- source: PLAN.md
- description: Open question Q5. NLP derivative strategy — AD, finite differences, or user-supplied. PLAN.md states it is decided when phase 7 reaches NLP because it shapes that engine's public API.
- acceptance: absent — deferred by design
- scope: NLP, public API shape, Q5

## REQ-lp-mps-dialect-edges
- source: PLAN.md
- description: Open question Q2. LP and MPS dialect edge semantics, fixed as encountered and recorded in `docs/format-support.md`. One is closed: an `RHS` entry on the objective row sets a constant, JAOS negates it as CPLEX documents, and the published Netlib optima omit it, so the gate carries the constant in its manifest.
- acceptance: each edge case lands in `docs/format-support.md` in the same commit that settles it
- scope: MPS reader, LP reader, dialect edge cases, Q2
- note: PLAN.md states the general rule this question keeps producing — "an instance disagreeing with a reference is not evidence about which of them is wrong"

## REQ-reentry-oscillation
- source: PLAN.md
- description: The re-entry loop's oscillation. D51 named the mechanism — every clean-up pivot borrows in order to repair, and repaying is what creates the next round's work — and D74 refuted the obvious cure by measuring that removing the loan costs `pilot87` 2.372x its iterations. D89 removed the consequence (the published answer no longer depends on where `SETTLE_ROUNDS` falls) and states plainly that it "does not make the loop converge". PLAN.md: "What remains open is the oscillation itself, and no cure is named for it."
- acceptance: absent — no cure is named, and the measured cost is 278 iterations of 116,071 (0.24%) on `pilot87` at interval 24
- scope: re-entry loop, cost shifting, SETTLE_ROUNDS
- note: PLAN.md's enclosing section header says "all four are now closed" while this entry says the oscillation is open — see WARNING 2 in INGEST-CONFLICTS.md

## REQ-pilot87-suboptimality-bound
- source: PLAN.md
- description: `pilot87`'s suboptimality bound is not understood. Across the four variants D92 measured, `pilot87`'s `gap_positive` moves between 0.0068 and 26.7 while every one of those answers sits inside tolerance of Koch's reference and reads `dual_feasible`. Whether a `Q` of 26.7 is the answer getting worse or the bound going slack is not separable today. It matters because the gate watches the quantity (D88's mechanism, D91's quantity) and it is what refused two of D92's three candidate repairs — "a change detector on a quantity nobody can interpret is a gate that can only ever be obeyed."
- acceptance: a second, independent estimate of `P - P*` on the same point. `certified_suboptimality` is explicitly not it, and that is measured (D73).
- scope: independent checker, suboptimality bound, acceptance gate
- note: PLAN.md records this as "not a defect with a reproduction, which is why it is here rather than above"; D92 hands it to PLAN.md as its own left-open item

## REQ-m2-competitive-gate
- source: bench/compare/README.md
- description: What closes M2 — JAOS strictly faster than the best competitor at tier T0, on the geometric mean of per-instance time ratios over the standard set, with a guard that no single instance is more than a stated factor slower. The geometric mean is what ratios require; an arithmetic mean of ratios flatters whichever side wins the big instances.
- acceptance: partially stated — "strictly faster than the best competitor at T0 on the geometric mean" is precise; the per-instance guard factor is written as "a stated factor" and no number is given anywhere in the ingest set
- scope: milestone M2, competitive gap, comparison harness
- note: the missing guard factor is WARNING 3 in INGEST-CONFLICTS.md. Current standing per D83: 3.72x vs HiGHS 1.15.1, 1.34x vs SoPlex 8.0.3, 3.77x vs Clp 1.17.11.

## REQ-miplib-subsets
- source: SPECS.md
- description: MIPLIB 2017 easy subset and MIPLIB 2017 benchmark subset, both recorded in SPECS.md's bar table as "not started".
- acceptance: absent — no target stated beyond the sets themselves
- scope: MILP acceptance sets
- note: extracted from SPECS.md rather than PLAN.md because SPECS.md's §8 table is where these two bars are named; PLAN.md phase 7 names the same sets without a status
