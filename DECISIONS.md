# Design decisions

Closed decisions only, with the measurement that closed them. What is
still open lives in `TODO.md`; what a feature is lives in
`SPECS.md`. Entries below that name `PLAN.md` are describing the state at the
time they closed — it is archived at `docs/archive/PLAN.md` since 2026-08-12.

Every heading below is the decision itself, not a topic — read the list
and you have the argument. Jump to the entry for the numbers behind it.

- **[D1](#d1-language-c23-gcc-linux)** — Language: C23, GCC, Linux
- **[D2](#d2-no-external-dependencies)** — No external dependencies
- **[D3](#d3-cpu-only)** — CPU only
- **[D4](#d4-generic-solver)** — Generic solver
- **[D5](#d5-a-library-that-does-not-know-its-consumers)** — A library that does not know its consumers
- **[D6](#d6-scope-is-the-full-mathematical-programming-taxonomy)** — Scope is the full mathematical-programming taxonomy
- **[D7](#d7-first-milestone)** — First milestone
- **[D8](#d8-deterministic-by-default)** — Deterministic by default
- **[D9](#d9-primal-heuristics-inside-metaheuristics-outside)** — Primal heuristics inside, metaheuristics outside
- **[D10](#d10-constraint-programming-is-out-of-scope)** — Constraint programming is out of scope
- **[D11](#d11-third-party-licences-and-who-authorises-an-exception)** — Third-party licences, and who authorises an exception
- **[D12](#d12-implemented-from-the-literature-not-from-other-peoples-code)** — Implemented from the literature, not from other people's code
- **[D13](#d13-threading-a-thin-layer-of-our-own-over-pthreads)** — Threading: a thin layer of our own over pthreads
- **[D14](#d14-build-one-plain-makefile)** — Build: one plain Makefile
- **[D15](#d15-test-only-dependencies-are-exempt-from-d2)** — Test-only dependencies are exempt from D2
- **[D16](#d16-the-work-unit-is-a-public-contract)** — The work unit is a public contract
- **[D17](#d17-no-claim-without-a-run)** — No claim without a run
- **[D18](#d18-what-the-independent-checker-guarantees-and-what-it-does-not)** — What the independent checker guarantees, and what it does not
- **[D19](#d19-unboundedness-is-read-off-a-ray-never-off-an-invented-bound)** — Unboundedness is read off a ray, never off an invented bound
- **[D20](#d20-optimality-is-not-declared-on-carried-numbers)** — Optimality is not declared on carried numbers
- **[D21](#d21-a-gate-that-fails-means-nothing-until-it-can-fail-differently)** — A gate that fails means nothing until it can fail differently
- **[D22](#d22-a-tolerance-excuses-a-condition-never-a-contribution)** — A tolerance excuses a condition, never a contribution
- **[D23](#d23-a-bound-proximity-test-is-judged-against-what-the-value-is-made-of)** — A bound-proximity test is judged against what the value is made of
- **[D24](#d24-the-primal-feasibility-test-stays-absolute)** — The primal feasibility test stays absolute
- **[D25](#d25-a-settled-point-is-handed-back-to-the-method-and-the-methods-answer-is-not-trusted-over-the-one-it-started-from)** — A settled point is handed back to the method, and the method's answer is not trusted over the one it started from
- **[D26](#d26-blands-rule-is-a-fallback-a-detected-cycle-switches-on-never-the-default)** — Bland's rule is a fallback a detected cycle switches on, never the default
- **[D27](#d27-the-re-entry-moves-a-column-when-its-wrong-sign-costs-objective-and-when-the-reduced-cost-carrying-it-is-a-number)** — The re-entry moves a column when its wrong sign costs objective, and when the reduced cost carrying it is a number
- **[D28](#d28-a-primal-ratio-test-enters-m1-the-primal-simplex-does-not)** — A primal ratio test enters M1; the primal simplex does not
- **[D29](#d29-the-refresh-that-verifies-an-optimum-refines-its-two-solves-the-solve-loop-does-not)** — The refresh that verifies an optimum refines its two solves; the solve loop does not
- **[D30](#d30-the-primal-clean-up-judges-every-candidate-before-it-moves-any-of-them-and-against-the-costs-the-model-owns)** — The primal clean-up judges every candidate before it moves any of them, and against the costs the model owns
- **[D31](#d31-what-the-campaign-settled-and-the-tolerances-freeze-where-they-stand)** — What the campaign settled, and the tolerances freeze where they stand
- **[D32](#d32-the-fixed-cost-of-a-simplex-iteration-is-zero-and-the-row-leaves-the-table)** — The fixed cost of a simplex iteration is zero, and the row leaves the table
- **[D33](#d33-the-public-apis-shape-as-decided-rather-than-as-drafted)** — The public API's shape, as decided rather than as drafted
- **[D34](#d34-the-one-place-d8-rested-on-an-argument-now-rests-on-a-measurement)** — The one place D8 rested on an argument now rests on a measurement
- **[D35](#d35-pricing-walks-the-matrix-by-row-and-the-answer-does-not-move)** — Pricing walks the matrix by row, and the answer does not move
- **[D36](#d36-a-scatter-form-btran-is-rejected-the-saving-is-real-and-the-arithmetic-is-not-free)** — A scatter-form BTRAN is rejected: the saving is real and the arithmetic is not free
- **[D37](#d37-a-published-zero-is-a-zero-and-the-digest-was-lying-about-it)** — A published zero is a zero, and the digest was lying about it
- **[D38](#d38-btran-skips-the-slots-it-can-prove-are-zero-and-proves-it-by-not-touching-them)** — BTRAN skips the slots it can prove are zero, and proves it by not touching them
- **[D39](#d39-infeasibility-gets-the-second-opinion-optimality-already-got)** — Infeasibility gets the second opinion optimality already got
- **[D40](#d40-the-pricing-row-is-read-through-its-pattern-and-the-pattern-costs-a-comparison)** — The pricing row is read through its pattern, and the pattern costs a comparison
- **[D41](#d41-the-dual-step-walks-the-pattern-too-and-an-invariant-is-what-makes-that-safe)** — The dual step walks the pattern too, and an invariant is what makes that safe
- **[D42](#d42-the-exact-weight-is-summed-over-rhos-pattern-which-nobody-had-to-go-and-find)** — The exact weight is summed over rho's pattern, which nobody had to go and find
- **[D43](#d43-the-solve-says-where-its-answer-is-and-one-charge-had-been-standing-for-two-loops)** — The solve says where its answer is, and one charge had been standing for two loops
- **[D44](#d44-the-forward-solve-says-where-its-answer-is-and-this-one-needed-no-ordering)** — The forward solve says where its answer is, and this one needed no ordering
- **[D45](#d45-the-work-counter-is-calibrated-against-a-clock-and-it-is-optimistic-by-a-factor-that-is-not-constant)** — The work counter is calibrated against a clock, and it is optimistic by a factor that is not constant
- **[D46](#d46-the-factorizations-fill-is-measured-the-pivot-search-is-confirmed-at-4-and-a-set-total-is-two-instances)** — The factorization's fill is measured, the pivot search is confirmed at 4, and a set total is two instances
- **[D47](#d47-a-reduced-cost-is-a-rate-and-the-checker-certifies-a-bound-it-cannot-prove)** — A reduced cost is a rate, and the checker certifies a bound it cannot prove
- **[D48](#d48-one-loop-pivoted-without-asking-whether-the-factorization-still-existed)** — One loop pivoted without asking whether the factorization still existed
- **[D49](#d49-the-re-entry-loop-stops-making-progress-and-the-round-cap-is-what-ends-it)** — The re-entry loop stops making progress and the round cap is what ends it
- **[D50](#d50-two-repairs-undo-each-other-and-the-loop-publishes-whichever-one-it-stopped-on)** — Two repairs undo each other, and the loop publishes whichever one it stopped on
- **[D51](#d51-the-residue-is-the-loan-ledger)** — The residue is the loan ledger
- **[D52](#d52-the-first-competitive-measurement-407x-and-it-is-not-the-algorithm)** — The first competitive measurement: 4.07x, and it is not the algorithm
- **[D53](#d53-two-rivals-agree-on-what-a-jaos-iteration-costs-and-that-makes-it-a-number-worth-attacking)** — Two rivals agree on what a JAOS iteration costs, and that makes it a number worth attacking
- **[D54](#d54-the-seventeen-is-two-different-things-and-only-one-of-them-is-visible-to-the-counter)** — The seventeen is two different things, and only one of them is visible to the counter
- **[D55](#d55-the-shipping-build-paid-15x-for-a-capacity-check-it-could-not-inline)** — The shipping build paid 1.5x for a capacity check it could not inline
- **[D56](#d56-the-elimination-rebuilt-every-column-of-every-pivot-row-including-when-there-was-nothing-to-eliminate)** — The elimination rebuilt every column of every pivot row, including when there was nothing to eliminate
- **[D57](#d57-the-gate-runs-its-instances-at-once-because-nothing-it-records-is-a-second)** — The gate runs its instances at once, because nothing it records is a second
- **[D58](#d58-the-elimination-asked-for-capacity-once-per-entry-it-wrote-and-the-entries-are-billions)** — The elimination asked for capacity once per entry it wrote, and the entries are billions
- **[D59](#d59-the-multipliers-belong-to-the-pivot-so-the-column-stops-being-copied-twice-to-meet-them)** — The multipliers belong to the pivot, so the column stops being copied twice to meet them
- **[D60](#d60-the-comparison-rebuilt-its-solver-only-when-its-own-driver-changed-so-it-measured-last-nights)** — The comparison rebuilt its solver only when its own driver changed, so it measured last night's
- **[D61](#d61-the-pricing-row-has-no-sparsity-to-exploit-and-the-calls-around-it-are-not-the-cost-either)** — The pricing row has no sparsity to exploit, and the calls around it are not the cost either
- **[D62](#d62-one-set-of-shipping-flags-chosen-by-measurement-and-the-counter-costs-nothing)** — One set of shipping flags, chosen by measurement, and the counter costs nothing
- **[D63](#d63-the-gaps-iterations-are-weight-restarts-and-the-threshold-that-causes-them-is-what-keeps-the-answers-right)** — The gap's iterations are weight restarts, and the threshold that causes them is what keeps the answers right
- **[D64](#d64-the-options-api-configures-the-contract-and-never-the-method-and-it-is-setters)** — The options API configures the contract and never the method, and it is setters
- **[D65](#d65-the-solver-speaks-only-when-spoken-to-and-never-on-a-clock)** — The solver speaks only when spoken to, and never on a clock
- **[D66](#d66-changing-a-model-discards-its-answer-and-the-two-that-do-not-touch-the-matrix-leave-the-derived-data-alone)** — Changing a model discards its answer, and the two that do not touch the matrix leave the derived data alone
- **[D67](#d67-setting-a-coefficient-is-three-operations-because-the-stored-matrix-has-an-invariant)** — Setting a coefficient is three operations, because the stored matrix has an invariant
- **[D68](#d68-a-basis-outlives-the-answer-it-produced-and-that-one-line-is-warm-re-solve)** — A basis outlives the answer it produced, and that one line is warm re-solve
- **[D69](#d69-what-warm-re-solve-buys-182x-the-iterations-and-60x-the-work-on-a-branching-step)** — What warm re-solve buys: 182x the iterations and 60x the work, on a branching step
- **[D70](#d70-a-budget-that-cannot-be-resumed-is-only-half-a-budget)** — A budget that cannot be resumed is only half a budget
- **[D71](#d71-the-checker-says-when-its-bound-is-not-a-bound-and-the-count-is-98-of-110)** — The checker says when its bound is not a bound, and the count is 98 of 110
- **[D72](#d72-pilot87s-iteration-guard-not-a-cycle-and-the-anti-cycling-rule-is-the-reason)** — `pilot87`'s iteration guard: not a cycle, and the anti-cycling rule is the reason
- **[D73](#d73-the-certificate-d47-wanted-without-the-factorization-it-thought-it-needed)** — The certificate D47 wanted, without the factorization it thought it needed
- **[D74](#d74-does-the-re-entrys-clean-up-need-to-borrow-at-all-measured-yes-and-pilot87-is-the-whole-price)** — Does the re-entry's clean-up need to borrow at all? Measured: yes, and `pilot87` is the whole price
- **[D75](#d75-the-non-aliasing-claim-holds-and-restrict-belongs-inside-the-kernel-rather-than-on-the-api)** — The non-aliasing claim holds, and `restrict` belongs inside the kernel rather than on the API
- **[D76](#d76-restrict-measured-and-refused-what-makes-it-safe-here-is-what-makes-it-worthless)** — `restrict` measured and refused: what makes it safe here is what makes it worthless
- **[D77](#d77-a-dimension-change-keeps-the-basis-exactly-when-what-is-left-is-still-a-basis)** — A dimension change keeps the basis exactly when what is left is still a basis
- **[D78](#d78-a-load-was-discarding-the-logging-callback-and-the-list-that-preserved-settings-was-the-defect)** — A load was discarding the logging callback, and the list that preserved settings was the defect
- **[D79](#d79-a-callback-may-look-and-may-stop-a-solve-and-may-not-steer-one)** — A callback may look, and may stop a solve, and may not steer one
- **[D80](#d80-the-comparison-was-timing-a-warm-re-solve-and-the-feature-that-broke-it-shipped-two-weeks-earlier)** — The comparison was timing a warm re-solve, and the feature that broke it shipped two weeks earlier
- **[D81](#d81-the-ladder-is-climbed-presolve-is-worth-142x-and-a-primal-simplex-is-worth-nothing)** — The ladder is climbed: presolve is worth 1.42x, and a primal simplex is worth nothing
- **[D82](#d82-partial-pricing-on-the-leaving-row-sweep-refused-it-saves-the-cheap-units-and-buys-the-expensive-ones)** — Partial pricing on the leaving-row sweep, refused: it saves the cheap units and buys the expensive ones
- **[D83](#d83-clp-is-the-third-reading-and-the-two-were-not-agreeing-by-coincidence)** — Clp is the third reading, and the two were not agreeing by coincidence
- **[D84](#d84-multiple-pricing-refused-too-and-phase-6-item-3-closes-with-both-halves-measured)** — Multiple pricing, refused too, and phase 6 item 3 closes with both halves measured
- **[D85](#d85-a-free-nonbasic-improves-in-the-direction-its-reduced-cost-points-and-the-status-was-never-able-to-say-which-that-was)** — A free nonbasic improves in the direction its reduced cost points, and the status was never able to say which that was
- **[D86](#d86-pilot87s-iteration-guard-is-a-factorization-that-stopped-agreeing-with-itself-and-the-two-solves-each-iteration-already-pays-for-can-say-so)** — `pilot87`'s iteration guard is a factorization that stopped agreeing with itself, and the two solves each iteration already pays for can say so
- **[D87](#d87-the-checker-bounds-what-the-rows-imply-which-closes-d47s-constructed-case-and-not-its-real-one)** — The checker bounds what the rows imply, which closes D47's constructed case and not its real one
- **[D88](#d88-the-gate-watches-the-dropped-term-because-the-checker-cannot-judge-it-and-the-predicate-cannot-see-it)** — The gate watches the dropped term, because the checker cannot judge it and the predicate cannot see it
- **[D89](#d89-the-re-entry-loop-keeps-its-best-round-and-best-is-defensible-before-it-is-close)** — The re-entry loop keeps its best round, and "best" is defensible before it is close
- **[D90](#d90-the-warm-start-stops-refusing-a-free-nonbasic-because-the-defect-it-was-avoiding-is-fixed)** — The warm start stops refusing a free nonbasic, because the defect it was avoiding is fixed
- **[D91](#d91-the-bound-and-the-verdict-stop-being-one-number-and-d47-closes)** — The bound and the verdict stop being one number, and D47 closes
- **[D92](#d92-a-residue-only-a-pivot-can-remove-was-hidden-by-the-scaling-and-the-repair-is-the-union-of-the-two-readings-rather-than-either-one)** — A residue only a pivot can remove was hidden by the scaling, and the repair is the union of the two readings rather than either one
- **[D93](#d93-the-ratio-tests-dense-scan-walks-the-nonbasic-set-and-the-bar-it-was-to-be-judged-against-cannot-be-measured-on-this-host)** — The ratio test's dense scan walks the nonbasic set, and the bar it was to be judged against cannot be measured on this host
- **[D94](#d94-d24s-nothing-is-gained-reason-expired-with-presolve-and-finnis-is-the-recorded-exception-on-the-absolute-row-test)** — D24's "nothing is gained" reason expired with presolve, and finnis is the recorded exception on the absolute row test
- **[D95](#d95-the-singleton-column-families-fire-only-at-cost-0-and-the-free-column-singleton-only-on-a-mutual-singleton)** — The singleton-column families fire only at cost 0, and the free-column-singleton only on a mutual singleton
- **[D96](#d96-presolves-gate-the-off-build-must-reproduce-the-baselines-bit-for-bit-and-the-on-build-is-judged-on-verdicts-not-bit-identity)** — Presolve's gate: the off-build must reproduce the baselines bit for bit, and the on-build is judged on verdicts, not bit-identity
- **[D97](#d97-bound-tightening-is-refused-every-design-returns-infeasible-on-models-that-have-an-optimum)** — Bound tightening is refused: every design returns INFEASIBLE on models that have an optimum
- **[D98](#d98-the-planning-layer-is-retired-the-record-is-five-documents-and-the-process-is-the-loop-in-claudemd)** — The planning layer is retired: the record is five documents, and the process is the loop in CLAUDE.md
- **[D99](#d99-the-singleton-column-was-judged-against-bounds-that-had-stopped-describing-its-row-and-that-is-the-whole-of-the-row-residual-defect)** — The singleton column was judged against bounds that had stopped describing its row, and that is the whole of the row-residual defect
- **[D100](#d100-several-rows-can-fold-into-one-column-and-the-multiplier-is-owed-to-the-one-whose-bound-the-column-rests-on)** — Several rows can fold into one column, and the multiplier is owed to the one whose bound the column rests on
- **[D101](#d101-the-last-three-presolve-families-have-015-left-to-remove-on-this-instance-set-which-defers-them-rather-than-refusing-them)** — The last three presolve families have 0.15% left to remove on this instance set, which defers them rather than refusing them
- **[D102](#d102-a-relaxed-row-is-skipped-by-every-pass-that-could-refuse-it-so-an-infeasible-model-was-published-optimal)** — A relaxed row is skipped by every pass that could refuse it, so an infeasible model was published OPTIMAL
- **[D103](#d103-presolve-was-written-for-one-objective-sense-and-one-tolerance-answered-a-question-that-has-no-tolerance)** — Presolve was written for one objective sense, and one tolerance answered a question that has no tolerance
- **[D104](#d104-the-ladder-is-recalibrated-and-jaoss-presolve-is-worth-what-the-others-are-worth-while-highs-presolves-the-instances-that-decide-the-set)** — The ladder is recalibrated, and JAOS's presolve is worth what the others' are worth while HiGHS presolves the instances that decide the set
- **[D105](#d105--what-highs-finds-in-maros-r7-is-the-implied-free-column-singleton-and-it-needs-an-implied-bound-rather-than-a-tightened-one)** — What HiGHS finds in `maros-r7` is the implied free column singleton, and it needs an implied bound rather than a tightened one

---

## D1 — Language: C23, GCC, Linux

JAOS is written in C23 (`-std=c23`), built with GCC. MSVC is explicitly not a
target. Linux is the execution platform; development happens on Windows through
WSL2.

The standard was chosen for ergonomics and safety, not speed: `-std=` does not
change generated code. What it buys is `constexpr`, `typeof`, fixed-underlying-type
enums, `[[nodiscard]]`, `unreachable()` and checked integer arithmetic
(`<stdckdint.h>`), the last of which matters for index arithmetic over sparse
matrices.

Toolchain, verified on Ubuntu 24.04 rather than assumed:

- GCC 13 only accepts `-std=c2x` and lacks `<stdckdint.h>` and `_BitInt`. GCC 14
  accepts `-std=c23` and provides both, so **GCC 14 is the minimum supported
  compiler**.
- `memset_explicit` and `TIME_MONOTONIC` are missing from glibc, not from the
  compiler. Monotonic timing therefore goes through POSIX
  `clock_gettime(CLOCK_MONOTONIC)` — which is the dependable route regardless.
- `#embed` needs GCC 15 and is unavailable. Nothing depends on it.

One measurement settled the choice of standard by itself. Compiling a call to the
absent `memset_explicit`, GCC 13 under `-std=c2x` emitted a warning and produced an
object file anyway; GCC 14 under `-std=c23` refused outright. C23 removes implicit
function declarations, turning a failure that would have surfaced at link or run
time into one the compiler will not let past.

## D2 — No external dependencies

Nothing outside the C standard library and the system threading primitives. Where a
third party would normally be pulled in, JAOS implements it instead.

This costs less than it appears to: serious solvers already hand-write the parts
that matter (sparse LU with Markowitz pivoting, basis updates), because generic
dense libraries do not fit the data structures.

## D3 — CPU only

GPU execution is out of scope. Not deferred — out. The simplex method is inherently
sequential and does not vectorise well, while the methods that do benefit from GPUs
(first-order methods, interior point) want a different data layout. Mixing both in
one binary degrades each. If a GPU solver is ever wanted, it is a separate project.

## D4 — Generic solver

Not specialised to scheduling, routing or any other domain.

## D5 — A library that does not know its consumers

The primary consumer is JAOT, which today drives SCIP, HiGHS and Hexaly and should
be able to drive JAOS the same way. That is a statement about who calls JAOS, not a
licence to shape its design: no API or algorithmic decision may be justified by how
JAOT happens to be built. Consumers write adapters.

## D6 — Scope is the full mathematical-programming taxonomy

Declared destination, in rough build order: LP; MILP; QP and MIQP; QCQP and MIQCQP;
SOCP and MISOCP; NLP; MINLP, convex and global non-convex; specialised network
algorithms (min-cost flow, transport, assignment). SDP is a distant candidate.
Stochastic, robust and multi-objective formulations are extensions over these
engines, not engines of their own.

Starting with LP is a build order, not a scope reduction.

## D7 — First milestone

Readers for both the MPS and LP file formats, plus LP solved by revised dual
simplex, producing correct optima across the Netlib LP test set. The readers are not
features — they are the only way to feed JAOS a real instance, and therefore the
precondition for measuring anything.

"Correct" gets a numeric definition in the plan: which reference values, and under
what tolerance. Several Netlib instances are numerically hostile by design, which
puts scaling inside this milestone, not after it.

## D8 — Deterministic by default

Same input, same parameters, same result and same search path — every time, on every
machine. The opportunistic mode exists but must be asked for explicitly.

Three consequences, binding from the first line of code:

- No algorithmic decision may read the clock. The clock reports, it never decides.
- The public API exposes two separate budgets: `time_limit` in wall-clock seconds
  (convenient, but where it cuts is not reproducible) and a work limit counted in
  deterministic work units (reproducible across machines).
- No iteration order may depend on memory addresses.

Parallel determinism costs performance; how much is a measurement we owe, not a
number we assume. The reason to decide this now rather than later is that
determinism cannot be retrofitted: once decisions consult the clock, every module
has to be reworked.

## D9 — Primal heuristics inside, metaheuristics outside

Primal heuristics — feasibility pump, diving, RINS, RENS, local branching — belong
inside the branch & bound. They are not optional extras: they need the node's LP
relaxation, the factorised basis and the tree, and without them a MILP never finds a
decent incumbent and the gap does not close.

Metaheuristics — genetic algorithms, tabu search, ALNS — and any decomposition of a
large problem into smaller ones stay outside, in the consumer. That is the
matheuristic pattern: the caller decides which piece to solve and hands it to the
exact solver.

This frontier follows from D5. A solver that also decomposed problems would need to
know what the problem means, which is exactly the knowledge a general-purpose
library must not have.

## D10 — Constraint programming is out of scope

JAOS solves mathematical programs: relaxations, bounds, branch & bound. Constraint
programming is a different paradigm — propagation and search rather than relaxation
— and shares almost no machinery with the simplex and B&B core. It would be a third
engine, not a later phase. If it is ever wanted, it is a separate project.

## D11 — Third-party licences, and who authorises an exception

The rule stays D2: implement it here. If an exception is ever genuinely warranted,
the acceptable licences are the permissive ones compatible with Apache 2.0 — Apache
2.0, MIT, BSD-2/3, ISC, zlib. GPL, LGPL and EPL are excluded outright.

An exception is never taken unilaterally. Every proposed external dependency is put
to the maintainer first, with its justification, and the maintainer decides. No
library enters this repository without that approval.

## D12 — Implemented from the literature, not from other people's code

Algorithms and papers are free — nobody owns the simplex method, or an article
describing Forrest–Tomlin updates. A specific implementation is copyrighted.

JAOS is therefore written from published literature: papers, theses, textbooks and
algorithm documentation. Source code of other solvers is not consulted, permissive
or copyleft alike. This is stricter than the law requires and deliberately so: it is
the only position that never has to be defended.

## D13 — Threading: a thin layer of our own over pthreads

Neither OpenMP nor C11 `<threads.h>`.

OpenMP is built for fork-join over regular loops, and its runtime owns the
scheduling. Branch & bound is the opposite shape — an irregular node queue with work
stealing, a shared incumbent and cuts propagating between workers — and handing
scheduling to a runtime contradicts D8 directly. Licensing was not the objection:
libgomp carries the GCC Runtime Library Exception and would not have infected
Apache 2.0.

C11 `<threads.h>` compiles on Ubuntu 24.04 (verified) but offers no barriers, no
rwlocks, no CPU affinity and no stack control, so it would collapse back onto
pthreads at the first real need.

What JAOS actually needs is small — spawn/join, mutex, condition variable, barrier,
C11 atomics — and lives behind a `jaos_thread.h` of a couple of hundred lines.
pthreads ships inside glibc, so D2 holds. Isolating it also leaves room for a Win32
backend later without touching the core.

## D14 — Build: one plain Makefile

Zero dependencies extends to the build. GNU make and GCC, one Makefile, no CMake, no
meson, no generator generating generators. If the build ever genuinely outgrows
this, that is a decision to revisit then — not one to preempt now.

## D15 — Test-only dependencies are exempt from D2

D2 binds the shipped artifact: the library that reaches production depends on
nothing beyond glibc. The test suite runs at development time and ships nowhere, so
it may use an established, lightweight C test framework instead of a hand-rolled
one. The licence rules of D11 still apply to it.

Framework: Unity (MIT) — pure C, three files vendored under `tests/`, never linked
into the library, with first-class floating-point assertions under tolerance, which
is most of what a solver's test suite does all day.

## D16 — The work unit is a public contract

Follows from D8, which promised a reproducible work budget without defining its
currency. The base currency is nonzeros processed — the dominant cost in sparse
linear algebra — with fixed, documented integer weights per discrete event: simplex
iteration, basis refactorisation, B&B node, cut round, heuristic call. The counter
counts events, never time, so it is deterministic by construction.

The unit's definition is part of the public API: it changes only at a major version,
and every change is a changelog entry. Redefining the unit silently would break
reproducibility of every budget any user ever recorded. The concrete weight table is
plan detail; this entry fixes what the plan must honour.

## D17 — No claim without a run

No statement about correctness or speed is made without executing the relevant
reference set: Netlib for LP, MIPLIB 2017 for MILP, later CUTEst for NLP and
MINLPLib for MINLP. Benchmark numbers additionally require a controlled environment
— WSL2 on a Windows host is adequate for development and for catching regressions,
but not for published figures.

A dedicated measurement host will be needed once there is something worth measuring:
same distribution, no desktop, nothing else running, CPU governor pinned to
performance. It gets built then, not before. The value of a benchmark host is
comparability over time, and that starts with the first number worth recording.

## D18 — What the independent checker guarantees, and what it does not

The checker is the oracle every solve is judged against, so what its
independence actually rests on has to be stated rather than assumed. Three
things:

- **Independent inputs.** Only the model as loaded and the claimed solution
  enter. No basis, no factorization, no solver state — a solver cannot pass
  its reasoning in alongside its answer.
- **Redundant identities.** Primal feasibility, dual sign conditions,
  complementary slackness and the primal-dual gap are checked together and
  constrain each other. The dual objective is accumulated from bounds while
  activities come from a separate pass over the matrix, so a corrupted dot
  product surfaces as a nonzero gap even where it also corrupts the reduced
  costs. The system is overdetermined; one broken kernel cannot satisfy all
  four at once.
- **Better arithmetic.** Accumulation is in `long double`, so the oracle's own
  rounding sits an order below what it is judging. A checker no more accurate
  than the solver is partly measuring itself.

It does **not** rest on the checker and the solver avoiding similar-looking
code. That was the original justification and it was wrong: both were written
by the same author with the same mental model, so duplication guards against
typos, not against a misconception — and a misconception is exactly what a
checker exists to catch. The two implementations stay apart for a different
and better reason: sharing would make the checker link against solver
internals, the coupling it exists to forbid. Once scaling reaches the solve
path (Q7) they operate on different matrices regardless.

The limit worth naming: the checker cannot detect a model the loader built
wrongly. It reads the same stored matrix the solver does, so if that is wrong
both agree about the wrong problem. Only external ground truth closes that,
which is one of the things the Netlib gate is for.

None of this holds unless the checker rejects what it should, so the suite
feeds it wrong dual signs, broken complementarity, primal violations and wrong
dual magnitudes, and requires each to be caught.

## D19 — Unboundedness is read off a ray, never off an invented bound

Dual phase 1 lends a finite bound to a column whose cost points at a bound it
does not have. The first version of this read the verdict off where the
optimum came to rest: a variable sitting on a lent bound meant the objective
had wanted to run past something that was never there, so the model was
declared unbounded.

That confuses two different models. "The objective reached the bound we
invented" is also what a model with a large but perfectly finite optimum
does, and such a model was reported `UNBOUNDED` — a wrong answer, not a
missing one, and a silent one.

The verdict is therefore taken from a direction rather than from a position.
Letting the column leave its lent bound moves the whole point along a
straight line; that line is a ray of the original problem exactly when no
basic variable runs into a bound **the model itself declared**. Lent bounds
do not count, and undoing one needs no saved copy: a loan only ever replaced
an infinity, since having no bound there is what made the column need one.

Two consequences worth stating, because they are what the decision buys:

- **The size of the lent bound no longer participates in any verdict.** It
  decides how often the method has to give up, never whether an answer is
  true. Sizing it is a performance question, which is where it belongs — the
  attempt to derive it from the model, and why that failed, is recorded in
  the working document.
- **The remaining case is refused rather than answered.** A column the
  objective still wants to push, stopped by a real constraint rather than by
  infinity, means the true optimum lies past what this phase 1 can reach.
  Reaching it means lifting the loan and re-solving, and the degenerate case
  of that needs a primal pivot, which does not exist yet. A solver that
  cannot get to the optimum says so; it does not substitute a verdict it can
  reach for the one it cannot.

Measured on a sweep of 3000 generated LPs against the previous
implementation: of the 858 the old code called unbounded, 8 now reach an
optimum the independent checker accepts, 46 are refused, and the rest are
confirmed unbounded by a ray. Every model that already reached an optimum
kept its status, objective, iteration count and work units unchanged, to the
bit. Work units do move on the unbounded path, which now runs a solve it did
not before — the counter has to bill it (D16).

## D20 — Optimality is not declared on carried numbers

A dual simplex iteration updates the basic values in place and patches the
factorization rather than rebuilding it. Both therefore drift, and the drift
has a property that makes it worse than ordinary rounding: the test that
would detect it is the very test being run. Optimality is declared when no
basic value violates a bound, and it is declared *on those values*. A solve
stops exactly when its numbers are wrong in the direction of looking
feasible.

This was not a theoretical worry. The first five Netlib instances JAOS ever
read found it: `afiro` — 27 rows, the smallest problem in the set — finished
with a row activity 1.8e-5 away from a bound it should have been sitting on.
Measured in the solver's own scaled space, where its own primal tolerance is
1e-7, that is 177 times outside it. `blend` was 288 times outside. The
objective still looked plausible, which is how this survives casual
inspection: it was right to six digits and wrong at the seventh.

So a declaration of optimality is now a proposal, not a conclusion. The
point is recomputed from a fresh factorization and priced again; only a
second opinion, owing nothing to the arithmetic that produced the first,
ends the solve. If the fresh numbers violate something after all, the loop
carries on and repairs it — those are iterations the drift was concealing.

The cost is one refactorization per solve, and the work counter bills it
(D16): a three-row test model went from 4411 units to 8517, which is the
factorization plus the pricing pass that follows it. On anything the size of
a real instance the proportion vanishes, and it buys the only thing a solver
sells.

What it bought, measured: on 3000 generated LPs, the independent checker had
been rejecting 364 of the 1269 models that reached an optimum — 202 on both
primal and dual conditions. Afterwards it rejects none. 641 objectives moved,
all of them towards exactness: −41.269232089702896 became −41.269230769230774,
which is −536/13, and −2.0000000000000013 became −2. No model changed status.

This is the end-of-solve half of the stability trigger PLAN 2.5.5 asks for.
The other half — watching an FTRAN/BTRAN residual during the solve and
refactorizing early — is not built, because no instance has yet shown it is
needed, and the residual worth acting on is a number only instances can
supply.


## D21 — A gate that fails means nothing until it can fail differently

The Netlib gate passes only when all 94 instances meet all four conditions, and
that is the right rule for deciding whether M1 is finished. It is the wrong
instrument for every change made on the way there, and the difference is not a
matter of degree: for the whole of M1 the gate reports the same word regardless
of what happened underneath it. A change that fixed one instance and broke two
scores exactly like a change that did nothing.

The summary counts do not save it either. They are sums, and sums cancel. Ten
commits landed in August 2026 that fixed `grow15` and `nesm`, made `pilot-we`
report a feasible problem as infeasible, moved `pilotnov` to a checker
rejection, and took `grow22` from 2179 iterations to 167865. Before and after:

    94 instances: 93 solved, 94 shape ok, 91 objective ok, 86 checker ok,
    93 deterministic, 1 failed / gate: NOT MET

Identical, digit for digit. The gate was working as specified. Nothing it was
specified to do could have caught this.

So a run is judged twice, against two questions that are not the same question:

- **The gate** — is M1 finished? All-or-nothing, unchanged, and `NOT MET` until
  it is not.
- **The baseline** — did this change make anything worse? Per instance, against
  `bench/netlib.baseline`, and it fails on the first predicate that stops
  holding whatever the totals say.

Work counts are in the baseline too, at a 2× allowance. Correctness is a
predicate and regresses visibly; cost is a number and degrades quietly, which
is the more dangerous of the two. An instance still reaching the same optimum
after eighty times the iterations has not kept working — it has become a
work-limit failure for every caller with a budget, and no predicate would say
so.

Two consequences follow, and both are the point rather than side effects:

**Updating the baseline is a separate command.** `make netlib-baseline`, never
`make netlib`. A baseline that refreshes itself records whatever just happened
as correct, which is precisely the failure it exists to prevent — it would have
absorbed all five changes above without a word.

**Improvements are reported, not just regressions.** A record that only ever
tightens is one nobody remembers to loosen, and an unexplained improvement is
as much a reason to go and look as an unexplained regression.

This does not lower the bar M1 has to clear. Every instance still has to meet
every condition before the gate passes. It adds a second, weaker claim that can
be true today — *nothing got worse* — because a bar that cannot be cleared for
months is a bar that stops being read.

## D22 — A tolerance excuses a condition, never a contribution

The checker holds a multiplier `w` — a row dual, or a column's reduced cost —
to a sign condition: positive means the variable rests on its lower bound,
negative on its upper. Below a tolerance that condition is waived, because
requiring a variable onto a bound on the strength of a number
indistinguishable from zero would reject solutions for their rounding.

The same line used to waive the multiplier's contribution to the dual
objective. Those are not the same claim, and treating them as one made the
oracle reject provably optimal answers.

`D(y)` is the sum over variables of the least `w · t` attainable inside
`[lo, hi]`. It is a function of `y` alone; which terms are large is not part
of its definition. What a magnitude filter discards is `w · bound`, and that
is small only if the bound is. Concretely, and this is a test in
`tests/test_check.c`:

```
min  x1 + 1e-7 x3    s.t.  x1 >= 1,  0 <= x1 <= 10,  1e6 <= x3 <= 2e6
```

`x1 = 1`, `x3 = 1e6`, `y = 1`, `d1 = 0`, `d3 = 1e-7`. Complementary slackness
holds exactly, strong duality holds exactly: `D = 1 + 0 + 1e-7·1e6 = 1.1 = P`.
Every value is exact in binary floating point. Dropping `d3` leaves `D = 1`
and a relative gap of 9% — on a pair that is optimal on every count. That is
the shape that rejected `pilot-ja`, whose duals are exactly zero in violation
and which failed on gap alone.

Two repairs were implemented and measured before this one, and both are worse
than the defect:

**Contribute `w · v`.** The term cancels, so the multiplier can neither invent
a gap nor conceal one — which sounds right and is fatal. On a model whose
multipliers all fall under the tolerance, `sum_v w_v v_v = c'z − y'Mz = P`
identically, so the gap is structurally zero for *every* feasible point and
the checker certifies the whole polytope. It accepted a pair 100 units above
an optimum of 1, and certified 2000 of 2000 random feasible points of a model
whose true optimum none of them reached. It also passed 98 unit tests and all
94 Netlib instances with a regression-free diff, which is the part worth
remembering: a green gate is not a proof, and this one was measured green
while being vacuous.

**Contribute `w · (nearest bound to v)`**, which is what HiGHS computes for
its own diagnostic. Choosing by primal position rather than by the sign of `w`
produces negative terms, and a negative term offsets a real residue somewhere
else in the model — the error becomes fungible. It is also discontinuous, the
dual objective jumping by `|w| · range` as `v` crosses the midpoint, and on a
free variable it evaluates `(-inf + inf) / 2`. HiGHS can afford this because
there the quantity is informative; here it is the oracle.

So the exemption covers the condition and stops. Every multiplier contributes
`w · bound` with the bound picked by its sign, as the definition says, and the
infinite-bound test comes first so that `0 · inf` is never formed.

What that buys is the reason to prefer it over merely working: with every term
present, `P − D` is exactly `sum_v w_v (v_v − bound_v)`, each term
non-negative and each one a single variable's complementary-slackness residue.
An accepted solution therefore carries `P − P* <= gap` by weak duality. The
checker stops reporting a reassurance and reports a bound.

The cost is real and is the right cost: a solver that stops on a suboptimal
basis is refused a certificate even when its primal point is fine, because the
`y` it hands over does not prove what it claims. `tests/test_simplex.c` has one
such case, where the same primal point is accepted the moment it is paired with
the dual the optimal basis would have produced.

## D23 — A bound-proximity test is judged against what the value is made of

The checker asks whether a value rests on a bound, and used to ask it with an
absolute tolerance: `v <= lo + tol`. For a column value that is right. For a
row activity it is not, because a row activity is a sum, and how precisely a
sum can be placed is set by the terms that went into it and not by the answer
that came out.

Row 3 of Netlib's `finnis` is the case that forced this. Its terms total
4.0e10 in magnitude and cancel to 1.5e-6 above a bound of zero. **One ulp at
4.0e10 is 7.6e-6.** The residue is a fifth of a single rounding step at the
scale the row works at — which is to say the activity is zero as precisely as
double precision is able to say so. Judged absolutely at 1e-6 the row is "not
at its bound", its multiplier of 28 fails complementary slackness, and the
checker reports a violation of 28 on a solution whose duality gap is
3.96e-11 and which publishes no violated sign condition anywhere in the
solver's own scaled space.

That is not a tolerance that is too tight. It is a tolerance measured in the
wrong units: it demands seventeen correct decimal digits of a sum that
cancels ten orders of magnitude, so no double-precision answer can pass it
and no amount of solver work can produce one. A gate condition that no
correct implementation can meet is not measuring the implementation.

So the window is `tol · s`, with `s = max(1, Σ_j |A_ij · x_j|)` for a row —
the sum of the magnitudes of its terms — and `s = max(1, |x_j|)` for a
column, which is the ordinary mixed form and unchanged in spirit. The
formulas are published in `docs/tolerances.md`.

**Why this is not the gate being made easier.** It is a loosening, and this
repository has already had one of those go badly: a checker rule that passed
98 unit tests and all 94 instances while certifying the entire polytope
(D22). So the rule is admitted only with the case it must still reject.

The argument is that the sign condition is a diagnostic and the gap is the
proof, and the two are tied by an identity rather than by convention:

```
P − D = Σ_v w_v · (v − bound_v)
```

with every term non-negative on a primal-feasible point. A row waived at
distance `d` with multiplier `w` therefore still contributes exactly `w · d`
to the gap, at full size, with nothing available to cancel it. The waiver can
decline to report a discrepancy a second time; it cannot conceal one. And the
gap is not relaxed by any of this.

`tests/test_check.c` carries the three cases that pin it:

- a row waived by the scale whose `w · d` is 500, refused on the gap, with
  `0 − (−500)` checked against `1000 × 0.5` so the identity is measured
  rather than believed;
- a row a hundred thousand times further out than the window, still reported
  at the full magnitude of its multiplier — the scale is a scale, not an
  amnesty;
- a column far from its bound with a real reduced cost, still a violation,
  confirming the row argument was not quietly applied to columns too.

On the Netlib standard set this moves exactly one instance, `finnis`, from
rejected to accepted, and clears the row condition on `pilot` without
changing its verdict — `pilot` fails on the gap and on the objective, which
this does not touch. Judged per instance against `bench/netlib.baseline`
(D21), because a tolerance change touches every instance at once.

## D24 — The primal feasibility test stays absolute

D23 made the checker's bound-proximity test scale with what the value being
tested is made of, and left `interval_violation` — the primal feasibility
test — absolute. Measuring the same way on the primal side shows the
absolute rule is wrong in both directions at once, which is what makes this
worth writing down rather than leaving implicit:

    instance   row violation   traffic   relative   ulp(traffic)
    finnis        8.44e-7       4.0e10   2.1e-17      7.6e-6
    adlittle      4.55e-13      2589     1.8e-16
    nesm          1.00e-8       0.70     1.4e-8       1.1e-16
    pilot         1.96e-5       1129     1.7e-8       2.3e-13

`finnis` clears the 1e-6 bar with 16% of the margin to spare while its
residue is a *tenth of one ulp* of the row it sits in — it is being asked to
place a sum more precisely than the sum can be represented, and that it
passes is luck rather than a property the solver controls. `adlittle` and
`nesm` are the other extreme: 1e-6 absolute concedes them 2.2e6 and 1e8 ulps
of their rows, so on those the rule is not measuring rounding at all.

**The relative rule is nonetheless refused, and the first reason is
sufficient on its own.**

**It is D23's premise, not a test beside it.** D23's licence is the identity
`P - D = sum of w_v (v - bound_v)` with every term non-negative, so that a
waived sign condition still reaches the gap at full size. Non-negativity is
not general: for `w > 0` the term is `w(v - lo)`, non-negative *if and only
if* `v >= lo`. Primal feasibility is the hypothesis of the theorem. Relaxing
it does not extend D23; it removes what D23 stands on — and the three tests
that pin D23 in `tests/test_check.c` assert `primal_feasible` for exactly
this reason. Worse than not being caught, an infeasible entity makes its
term *negative*, so it offsets real residues elsewhere: the fungibility
defect D22 already rejected in writing when it refused HiGHS's
nearest-bound rule.

**Nothing is gained.** Across the standard 94, exactly one instance has a
row violation above 1e-6 — `pilot`, at 1.96e-5, already rejected on the gap
and on the objective. The change buys no verdict and loses the one place the
signal is true.

**The window would be defined by the answer being judged.** Traffic is
`sum_j |A_ij x_j|`, a function of `x`. Two different optima of one model
would be judged against different feasible sets, and a row's window can be
inflated at will by adding a zero-cost `+M`/`-M` column pair held at `T`:
the activity does not move and the traffic grows by `2MT`. Under D23 that
buys a waived diagnostic the gap still charges for; here it would buy a
waived claim with nothing behind it.

**The field keeps it absolute.** Gurobi documents its tolerances as absolute
and explicitly independent of the scale of the quantities involved, pushing
the burden onto the modeller's choice of units; HiGHS's
`primal_feasibility_tolerance` is likewise absolute. And no defensible
constant exists here anyway: keeping `pilot`'s signal needs below 1.74e-8,
keeping `nesm` out of breach needs at least 1.43e-8 — a 21% slot fitted to
this sample, which is what D17 exists to forbid.

**What is done instead.** The measurement is real and it is kept, in the
report rather than in the predicate: the row residue relative to its traffic
is published alongside the absolute one and decides nothing. `finnis` at
0.11 ulp is the recorded boundary case.

Separately, and this is the finding the argument turned up: the gap is
`|Q - N|`, where `Q` sums the positive terms and `N` the negative ones a
within-tolerance primal violation contributes. The two cancel, and the
checker cannot tell a small gap from two large halves. Constructed and run
against the checker as it stands — a row violated by 9e-7 with a multiplier
of 1e9, paired with a column whose reduced cost of 1e-7 is waived under D22
at a bound 9e9 wide — it reports `gap = 1.34e-14` on a point carrying 900 of
each. Accumulating `Q` and `N` separately costs two `long double` adds in a
loop that already has both quantities in hand, changes no verdict, and turns
`P - P* <= gap` from a consequence of an unchecked binary hypothesis into
`P - P* <= Q`, a bound the checker publishes.

**Both are now built** (`gap_positive`, `gap_negative` and
`max_row_violation_relative` in `jaos_check_report`), and no verdict moved.
The record carries all three per instance, and putting it on the standard 94
turned the constructed case into a measured one — and a common one rather
than a curiosity. On **35 of the 93 instances that reach an optimum, `Q`
exceeds `|Q - N|` by more than a factor of two**: `pilotnov` by 157,
`greenbeb` by 34, `finnis` by 3. The gap those instances report is not the
bound they are entitled to; `Q` is, and it is up to 157 times larger.

Read the size before the ratio, because the ratio on its own overstates the
case. Every one of those `Q` values is tiny in absolute terms —
`7.85e-10` on `pilotnov`, against an objective of order `4.5e3` — so the
certificates were sound the whole time and no verdict was ever wrong. What
changed is that "sound" is now something the record shows rather than
something the identity was assumed to deliver. The construction that
motivated this is still a demonstration of cancellation and not of a false
acceptance; what the campaign adds is that the cancellation is the normal
case, which is the part nobody would have guessed from one built example.

Honesty about the limit of that construction: it demonstrates the
cancellation, not a false acceptance. No materially suboptimal point has
been exhibited that the checker accepts, and there is a structural reason to
expect difficulty — hiding `N` costs an equal `Q`, and `Q` is exactly what
bounds the suboptimality.

**One argument here expired and the measurement that replaced it is
stronger (2026-08-08).** "Nothing is gained" rested on `pilot` being rejected
on the gap and the objective as well, so that relaxing the row test bought no
verdict. After D28 that is false: `pilot`'s objective is within tolerance and
its dual violation is exactly zero, and the row is the only thing refusing it.
A relative rule would now buy a verdict.

It would buy the wrong one. `max_row_violation_relative`, which this decision
put in the report, says so directly:

| instance | row residue | relative to the row's traffic |
|---|---|---|
| `finnis` | 8.44e-7 | **8.21e-17** |
| `adlittle` | 4.55e-13 | 1.76e-16 |
| `25fv47` | 1.3e-12 | 6.08e-14 |
| `nesm` | 1e-8 | 1e-8 |
| **`pilot`** | **1.73e-6** | **6.93e-9** |

`finnis` carries the larger absolute residue and is a *fraction of one ulp* of
its row. `pilot` is seven to nine orders of magnitude above that band — about
3e7 ulps of a row carrying 250. Its row is genuinely outside its bound, and a
window of `tol · s` would be 2.5e-4 wide there and would wave through a real
primal violation. So the decision stands on a measurement now rather than on
the absence of one, and the "both too strict and too lax" finding above is
unchanged: `finnis` still passes the absolute test on luck and `nesm` still
passes it while 1e8 ulps out.

**If this is ever revisited**, the only scale-aware form that is safe for a
certificate is `min(tol, tol*s)` — narrowing. It can only turn acceptances
into rejections, so it cannot invalidate a certificate the way widening
does. It would put `nesm` in breach today, which is why it waits on Q10:
turning a pending diagnosis into a gate failure destroys the information the
diagnosis was going to give.

---

## D25 — A settled point is handed back to the method, and the method's answer is not trusted over the one it started from

Settling the shifts is what turns a solve that was optimal for a convenient
problem into an answer about the one that was asked. It can leave reduced
costs pointing the wrong way, and by a wide margin: `greenbea` finishes with
ten columns whose sign conditions are violated by up to 5.28, out of shifts
that never exceed `7.09e-6` (PLAN 2.8.1). `repair_dual_infeasibility` swaps
nonbasic variables between bounds and cannot reach any of it.

**What is done.** After settling, every nonbasic whose reduced cost breaches
its sign condition past `DUAL_TOL` is put right, and there are exactly two
ways to do that:

- one with a *real* bound on the other side is sent to it. Its reduced cost
  is feasible for that bound instead, at no cost in accuracy, and the primal
  breaks — which is the point, because primal infeasibility is what the dual
  simplex exists to remove;
- one without is shifted, exactly as the ratio test does mid-solve. This
  moves nothing and hands the method no work; it is there so that the ones
  that *can* move are not run past a ratio test whose candidates include
  costs already on the wrong side of zero, which is the hazard cost shifting
  exists to prevent in the first place.

Then the dual simplex runs again from that point. A round that moves nothing
does not happen at all — it would re-solve a point the method is already at
and settle back to the residue it started from.

**What makes it admissible is the fallback, not the attempt.** A model that
has just been proved to have an optimum has not become infeasible. A
re-entry that reports it has is reporting on itself: the flips are a
starting point of its own choosing, and the dual simplex finding no feasible
point from there says the choice was bad. So the settled point is saved
first, and **anything other than a second optimum is discarded**. This is not
defensive habit. It is the precise failure both earlier repairs of this
residue produced — a feasible model returned INFEASIBLE, which is strictly
worse than the defect being repaired — and refusing to publish it is the
condition on which re-entering at all is safe.

**What the guard does not cover.** A round's result is accepted for being a
second optimum, not for being a better one — nothing compares the two. The
re-entry runs the same method on shifted costs that the first pass did, so
it settles to a residue of its own, and no argument from construction says
that residue is smaller. That the answers improve is a measurement over
139 instances across three sets, not a property of the loop.

**A criterion of "keep the smaller violation" was considered and is
refused, and the measurement is why.** The worst breach standing at the top
of each round on `pilot`:

| round | movable | stuck | worst breach |
|---|---|---|---|
| 0 | 5 | 20 | 4.65e-3 |
| 1 | 2 | 23 | 4.79e-3 |
| 2 | 9 | 15 | **7.85e-2** |
| 3 | 0 | 7 | **2.87e-6** |

The sequence is not monotone and it is not nearly monotone: round 2 begins
seventeen times worse than the solve ended, and it is the round that
produces the final drop of three orders of magnitude. A rule that kept the
better of two consecutive points would have stopped after round 0 and thrown
that away. So the loop is not permitted to judge its own progress — it runs
until nothing is left to move — and what actually catches a worsening is the
baseline (D21), which compares against a recorded run rather than against
the previous round.

Five arrays are saved (`status`, `basis`, `lo`, `up`, `fake`) because they
are the whole of what a re-entry writes: `where` is the inverse of `basis`,
the primal values are what `compute_primal` derives from the nonbasic ones,
the reduced costs are what `compute_duals` derives from the costs, and the
factorization is of `basis`. Restoring the five and rebuilding lands on the
saved point bit for bit, which is what determinism (D8) requires of it.

**Measured, on all three sets, per instance against their baselines (D21):**
`nesm` goes from REJECTED to checker green, with its dual violation at
exactly 0 rather than merely smaller and its gap falling from `2.71e-11` to
`1.93e-16`. `pilot` and `pilot87` improve by two orders of magnitude on the
dual and by seven and three on the gap without changing verdict. `greenbea`,
`etamacro` and `finnis` are **bit-identical** — same digest, same iteration
count. Standard set: 0 regressed, 1 improved, 0 new. Kennington and the
infeasible set: 0/0/0, both still PASS.

**What it cannot reach, and this is the part that decides scope.** The three
instances that do not move are not unlucky, they are outside the mechanism by
construction, and the measurement says which reason applies to each:

| instance | residual sign conditions after settling | with a real opposite bound |
|---|---|---|
| `etamacro` | **0** | — |
| `greenbea` | 10 | **0** |
| `nesm` | 1 | 1 |
| `pilot` | 25 | 5 |
| `pilot87` | 48 | 15 |

`etamacro` has nothing to repair: its breach is `4.89e-8` in scaled space,
inside what this solver calls zero, and it becomes visible only because
`publish` divides by a column scale of `1/32`. `greenbea`'s ten all rest at
a lower bound of 0 with no upper bound at all, so there is nowhere to send
them; reaching them means letting one enter the basis, which is a primal
ratio test and is what PLAN 2.9 puts to the scope question rather than
inside this decision.

**`SETTLE_ROUNDS = 32` is a backstop, not a limit meant to bind**, and it
took a measurement to say that rather than assume it. Only three of the
standard 94 re-enter at all: `nesm` converges in one round, `pilot` in
three, `pilot87` in six. The constant was first written as 4 — which is
exactly where `pilot87` was still finding work, so it was not bounding a
pathology, it was deciding an answer:

| `pilot87` | stopped at 4 rounds | run to convergence |
|---|---|---|
| dual violation | 2.28e-4 | **3.33e-5** |
| gap | 2.27e-7 | **4.03e-8** |
| objective error | 3.21e-3 | **2.35e-3** |
| iterations | 50434 | 50616 (+0.36%) |

Better on every measure for a third of one percent of the work. A cap tight
enough to bind is a cap choosing the answer, which is not what this is for —
so it is set well clear of anything the set needs, in the same sense
`ITER_SANITY_FACTOR` is. Termination does not depend on it: a round only
begins if some column can still be moved, every round that moves something
makes at least one pivot, and `iters` accumulates across rounds so the
iteration cap in `run()` covers all of them together.

---

## D26 — Bland's rule is a fallback a detected cycle switches on, never the default

`grow15` ran to the internal iteration guard at 189201 iterations and was
reported as the JAOS defect it is. Q10 diagnosed it as a stall — iterations
whose entering candidate already had a zero reduced cost, so the dual step
`d_q / alpha_q` makes no progress — and proposed cost perturbation, which
was measured, worked, broke three other instances and was reverted.

**The diagnosis was close and not right, and the instrument says so
plainly.** From iteration ~3000 the solve repeats bit for bit: every window
of 2000 iterations reports the same total primal infeasibility to ten
significant digits, the same 43 violated rows, the same objective, and
exactly 1000 degenerate steps out of 2000. Logging the pivots inside the
repeat gives a **cycle of period four** over two rows and four variables:

    r=141: 398 leaves, 378 enters, theta = -1.21042e-06
    r=196: 440 leaves, 420 enters, theta = +1.67711e-06
    r=141: 378 leaves, 398 enters, theta = +7.83895e-09
    r=196: 420 leaves, 440 enters, theta = -1.70297e-08

with `xb` and the steepest-edge weight of each row returning to identical
values every fourth iteration. So it is not that no iteration makes
progress: half of them take a real step of about 1.7e-6, and the four steps
cancel exactly. That is a cycle, and a cycle has a known cure that a stall
does not.

**PLAN records that Bland's rule was tried and did not fix `grow15`. What
was tried was not Bland's rule.** It was a smallest-index tie-break among
equally sized pivots *inside the Harris window*, which carries none of the
guarantee: the guarantee needs the exact minimum quotient, no widening, and
the smallest index on the *leaving* choice as well. Built properly — index
rule on both choices, exact minimum ratio, no window and no bound flipping —
`grow15` solves in 11464 iterations at an objective matching Koch's to
sixteen digits.

**It cannot be the default, and that is measured too:**

| instance | dual steepest edge + Harris | Bland's throughout |
|---|---|---|
| `afiro` | 30 | 19 |
| `adlittle` | 76 | 25 |
| `bnl2` | 1904 | 830 |
| `grow7` | 544 | 4696 |
| `25fv47` | 9459 | **236918** |
| `grow22` | 2179 | **iteration guard at 277401** |
| `grow15` | **iteration guard at 189201** | **11464** |

Twenty-five times the iterations on `25fv47`, and `grow22` stops solving
altogether — which is precisely the failure the earlier attempt recorded, so
that half of its finding stands.

**So: run the fast rules, detect the cycle, switch, and switch back.** The
detector is the total primal infeasibility, which `price_row` already has in
hand — it visits every row and computes both violations, so the total costs
one add in a loop that was already running. The trigger is failing to
improve on the *best* total reached, not the last one, because the quantity
is not monotone and a solve that gets worse and then better has made
progress. Bland's goes off again the moment the best improves.

**One thing changes shape under the fallback and it is worth stating.** The
Harris path can declare a model infeasible from `bfrt_walk` retiring every
candidate without one blocking; Bland's has no bound flipping, so that route
does not exist while it is in force. It is not lost — infeasibility still
reaches the same verdict through an empty candidate set, which is the
condition both paths share — it is only detected a pivot or more later. A
fallback that runs for a few thousand iterations on a cycling model is not
where an infeasibility proof needs to be fast.

**The threshold is the one constant, and it is safe in a way the
perturbation size is not.** It cannot change an answer: it only decides when
to switch to a rule that is itself exact and terminating, so too small costs
iterations and too large costs iterations. Q10's perturbation had no such
property, which is why it is still open and this is not.

It is `STALL_FACTOR = 10` times `nrow + ncol + 1` — the normalisation
`ITER_SANITY_FACTOR` already uses, because a plateau that is long for a
small model is nothing for a large one. Measured across the standard 94, the
longest plateau on an instance that terminates:

| instance | plateau | of its size |
|---|---|---|
| `truss` | 16347 | **1.67** |
| `dfl001` | 17224 | 0.94 |
| `25fv47` | 1824 | 0.76 |
| `maros-r7` | 8676 | 0.69 |
| `grow15` | 187509 | **198** |

Two orders of magnitude of daylight between the worst healthy plateau and
the cycle. Ten is six times clear of `truss` and twenty times inside
`grow15`, and the absolute figures are why the trigger is not absolute:
`truss` spends 94% of a perfectly good solve on one plateau.

**What it costs the instances that do not cycle is nothing, exactly.**
`grow22`, `grow7` and `truss` come back with identical digests, identical
iteration counts and identical work. That is the property the two reverted
repairs did not have — both changed every model in order to fix one.

`grow15` now solves in 21653 iterations: 9460 before the trigger fires, then
Bland's. Slower than the 11464 of Bland's throughout, and that is the trade
being made deliberately — the 9460 are what buys every other instance its
unchanged behaviour.

---

## D27 — The re-entry moves a column when its wrong sign costs objective, and when the reduced cost carrying it is a number

D25 flips a nonbasic whose sign condition is breached past `DUAL_TOL`. That
reads the breach in the space the solver works in, and the space is not a
detail: `etamacro`'s breach is `4.89e-8` scaled — inside what this solver
calls zero — and `1.56e-6` once `publish` divides by a column scale of
`1/32`, which is past the checker's tolerance. One reading says leave it,
the other says repair it, and both are the same number seen through a change
of variable chosen for the solver's convenience.

**Two attempts got this wrong before the third, and both are in PLAN 2.8.1
because each one is what pointed at the next.**

*Judge the breach in the published space.* Closes `etamacro`, moves nothing
across 51 instances, and takes `pilot87` from a solve to a tripped iteration
guard at 1382801 iterations against 50616. What it settles is not "use the
other space" but that **any rule reading the breach has to pick one**.

*Judge the contribution to the duality gap instead.* `P − D = sum_v w_v
(v − bound_v)`, so a nonbasic on a bound with a wrong-signed reduced cost
contributes `|d|` times the width of its box — and `publish` divides `d` by
the same `gamma` it multiplies the value by, so that product is the same
number in either space. No choice to make. It is also exactly what D24 made
the checker publish as `Q`, and the two agree numerically: `etamacro`'s
three movable breaches contribute `2.011e-6`, `6.26e-7` and `1.676e-7` in
scaled space, summing to the `2.805e-6` reported in the original.

That closed `etamacro` with the other 93 of the standard set bit-identical —
and cost `pds-20` **3.2x its work**, 47785 iterations becoming 136750.
Instrumented there, the re-entry runs all 32 rounds without converging and
**every column it flips has a reduced cost below `DUAL_TOL`**, the smallest
between `2.2e-11` and `1.7e-10`. Their contributions clear the threshold
only because the boxes are 900 to 4955 wide.

**So the contribution answers whether a move is worth making, and says
nothing about whether there is anything there to move.** A product is only
as good as its factors, and on a wide box the first factor was rounding.

**The second half is not a second test.** `d_j = c_j − y' M_j` is a sum, and
a sum is known no more finely than the terms that went into it — which is
D23's argument for a row activity, read down a column. So `|d|` counts only
where it stands above `eps` times the traffic through the column,
`|c_j| + sum_k |y_k a_kj|`.

**The margin has a measurement on both sides, over both feasible sets.**
Of 110 instances, five have any column the re-entry would consider:

| instance | columns | `\|d\| / (eps · traffic)`, smallest |
|---|---|---|
| `etamacro` | 3 | **5.055e8** |
| `pilot87` | 15 | 6.985e10 |
| `pilot` | 5 | 6.339e11 |
| `nesm` | 1 | 3.199e13 |
| **`pds-20`** | 14 | **2.133** |

Seven orders of daylight between the columns that should move and the ones
that should not, and `NOISE_MARGIN = 1e5` is the geometric middle of it. The
behaviour saturates: `1e5` and `1e7` give identical answers on all three
sets. `1e3` also works and costs `pds-20` 28% more work, which is what the
margin buys.

`pds-20` keeps exactly one column — the one whose traffic *equals* its `|d|`,
a single term with nothing to cancel, so its reduced cost is exact however
small. It flips once and converges: **47786 iterations against a baseline of
47785**.

**What none of this reaches, and why that is structural.** A column with no
other real bound contributes nothing, by the same identity that gives the
term — there is no `w · bound` for an infinite bound. `greenbea`'s ten
columns are all of that kind. They are a dual violation with no objective
behind them, and no threshold on either factor can see them; what they need
is a primal pivot, which §2.1 excludes.

**The evidence that matters most is not the campaign.**
`tests/test_simplex.c` carries a hand-built three-column model whose optimum
can be read off by eye: column A costs nothing and satisfies the only row on
its own, so the answer is `x = (1, 0, 0)` at an objective of zero. The solver
used to stop at `x = (0, 0.001, 0.0999)` and an objective of `4.997e-8`, on a
basis whose duals could not be certified — A's reduced cost of `-5e-8`
against an upper bound of `100` is `5e-6` of unproven complementary
slackness, which is `Q` exactly. The test asserted that wrong answer and the
failed certificate that came with it, and PLAN 2.8 recorded the defect.

It now reaches `x = (1, 0, 0)`, objective zero, gap zero, both halves zero,
in two iterations. A correct answer replacing a wrong one, on a model where
nobody has to trust a reference value to see it.

---

## D28 — A primal ratio test enters M1; the primal simplex does not

PLAN 2.9's scope question had two honest answers, and the evidence for
choosing between them was assembled by D25, D26 and D27: after those, five of
the seven instances the checker once rejected had closed, and the two that
remained were **not near-misses**. `greenbea` finished with ten columns at a
lower bound of 0 with no upper bound at all, reduced costs from −0.019 to
−5.28. Every mechanism built by then was structurally blind to them: the term
D27 judges on is `w · bound`, and there is no such term for an infinite
bound. There was nowhere to move them to. What they wanted was to travel
until something stopped them.

**The scope grows, and by exactly one thing.** A primal ratio test, feeding
the basis change `pivot()` already performs. Nothing else: no pricing to
choose an entering column, no phase 1 of its own, no second set of weights,
and no use of it to solve anything. The entering column is named by the
residue — it is the column whose sign condition is broken — so there is
nothing here to price with. That is the distinction PLAN 2.1 now draws in
writing, because a line that moves once will move again by drift if nobody
writes down where it ended up.

**The dual pivot and the primal one are the same basis change.** `pivot()`
computes `theta_primal = (x_B[r] − bound) / alpha[q]`, and `alpha[q]` is row
`r` of `B^-1 M` at column `q` — the very number the ratio test blocked on.
The two methods differ only in which of `r` and `q` is chosen first: the dual
picks a violating row and then asks what may enter, the primal picks a
column and then asks what must leave. So this reuses `pivot()` unchanged.

**Two guarantees come free, and the re-entry of D25 has neither.** The point
stays primal feasible — that is what the ratio test is for. And the objective
cannot rise: `d_q` points the way `q` travels, so every step is a descent and
a degenerate step of zero changes the basis without moving the point. D25's
result had to be accepted on being a second optimum rather than a better one;
this one is better by construction.

**Only bounds the model declared may block.** A basic brought to rest on a
bound dual phase 1 lent would be published at a value the model never
allowed, which is the case `repair_dual_infeasibility` already refuses. If
nothing real blocks, the column is left alone. The honest reading of that
situation is an unbounded ray, and declaring one off a basis that has been
rebuilt twice is exactly the verdict D19 demands proof for — leaving the
residue is the smaller error.

**Measured on all three sets:**

| | before | after |
|---|---|---|
| `greenbea` objective | −72555233.859378919 | **−72555248.129846007** |
| Koch's exact value | −72555248.129845992 | *fifteen significant digits* |
| `greenbea` dual violation | 2.66 | **0** |
| `greenbea` checker | REJECTED | **ok**, for 8 extra iterations |
| `pilot` objective | out of tolerance by 390x | **within it** — error 2.3e-5 against 5.6e-4 |
| `pilot` dual violation | 7.97e-5 | **0** |
| `pilot` gap | 8.6e-13 | 6.6e-14 |

Standard set: **0 regressed, 2 improved, 0 new** — 94 of 94 solved, 93 of 94
on objective, 92 of 94 on the checker, 94 of 94 deterministic. Kennington and
the infeasible set: 0/0/0, both still PASS, and fifteen of Kennington's
sixteen instances **bit-identical** (the sixteenth is `pds-20` at 1.003x,
which is D27's one iteration and not this).

`greenbea` was the largest open item of the set and it closed for eight
pivots. `pilot`, which `docs/research/pilot-analysis.md` §3.2 recorded as
*further from Koch than MINOS 5.3, OSL and CPLEX all are*, is no longer a
wrong answer at all.

**What is left of condition 1a is two instances and two different causes.**
`pilot` is rejected on one row lying 1.73e-6 outside its bound and on nothing
else — a *primal* residue, and the number D24 is about. `pilot87` still
misses its objective by 7.6x, and nothing built here moves it.

**This retires one of D24's four arguments and D24 should be read
accordingly.** D24 refused to make the primal feasibility test relative, and
one of its reasons was that the change *buys no verdict*: "exactly one
instance of the 94 exceeds 1e-6 on a row — `pilot`, already rejected on the
gap and on the objective." That premise is now false. `pilot` is rejected on
the row alone, so a relative rule would buy a verdict.

**It would buy the wrong one, and D24 now records the measurement.**
`pilot`'s row residue is `6.93e-9` of what the row carries, where a healthy
row sits at `1e-14` to `1e-17` — `finnis` carries a larger absolute residue at
`8.21e-17` relative, a fraction of one ulp. `pilot`'s is about 3e7 ulps: the
row is genuinely outside its bound. So the argument that expired has been
replaced by a stronger one of the same shape, and D24's other three stand
untouched — the first still sufficient alone, that primal feasibility is the
hypothesis D23's identity rests on rather than a test beside it. What is left
on `pilot` is a primal defect to find.

---

## D29 — The refresh that verifies an optimum refines its two solves; the solve loop does not

D28 left `pilot` rejected on one row lying `1.73e-6` outside its bound and on
nothing else, and called it "a primal defect to find". It was found where
PLAN 2.9 said it would be and turned out to be something PLAN 2.5.5 did not
predict.

**What the defect is.** No basic variable is outside its bound — the worst
violation in the solver's own arithmetic is exactly `0`. The `1.73e-6` is the
disagreement between two computations of one quantity: the checker's dot
product `sum_j a_ij x_j`, and the solve `x_B = -B^-1 (N x_N)` that produced
the row activity JAOS published. Measured at the point the answer is accepted,
the residual of that solve is `7.06e-6` in the space the checker reads, where
every healthy instance sits twelve orders lower. One step of iterative
refinement — form `b - B x_B` against the basis columns, solve for the
correction, add it — leaves `9.09e-13`.

**Why refactorizing earlier could not have reached it.** 2.5.5 asks for "an
FTRAN/BTRAN residual check that refactorizes early", and D20 already
refactorizes once before accepting optimality, so the factorization this
residual is measured against is *fresh*. The error is not in a patched LU
that has drifted; it is the backward error of the triangular solves against a
basis this badly conditioned. Nothing about when the factorization was built
changes it. That half of 2.5.5 is answered by measurement rather than built:
the trigger it describes is aimed at a cause that is not the one here.

**Why not everywhere, which is the part with a price on it.** Refining every
solve was measured, and it is the failure this repository has now produced six
times: `pilot-ja`, a model with a known finite optimum, comes back
**INFEASIBLE**, and `pilot87` pays **4.5x** the work (186147 iterations
against 50893) for a verdict it already had.

The line that survives is not a cost-saving, it is what the numbers are for.
Mid-solve, `x_B` and `y` are inputs to a choice of pivot, and a trajectory is
not more correct for being computed from more accurate numbers — it is merely
a different trajectory, and the measurement says a worse one. At the moment
optimality is declared, the same two vectors *are* the answer, and an answer
is more correct for being more accurate. So this is the second half of D20's
own argument rather than a new mechanism: D20 refuses to read a verdict off
carried numbers, and this refuses to read one off an inaccurate solve of the
fresh factorization those numbers were rebuilt from.

**Why both solves and not the one that was broken.** Refining only the primal
closes the row — `1.73e-6` to `2.11e-13` — and takes `pilot`'s dual violation
from `0` to **`0.0688`** and its objective out of tolerance. A point read off
an accurate `x_B` and an inaccurate `y` is not more consistent than one read
off neither; the two travel together or not at all.

**Why unconditional rather than triggered on a threshold.** A threshold would
be a constant to justify on both sides (D17), and there is nothing to buy with
it: the refinement is two solves and two residuals, once per solve, and the
measurement below says what that costs. A rule with no number in it cannot be
tuned to an instance, which is the property every repair in this file has been
judged on.

**Measured on all three sets.**

| | before | after |
|---|---|---|
| `pilot` row residue | 1.73e-6 | **6.73e-13** |
| `pilot` relative row residue | 6.93e-9 | 2.32e-16 |
| `pilot` objective, relative error | 4.2e-9 | 9.4e-12 |
| `pilot` checker | REJECTED | **ok** |
| `pilot` work | 5559467003 | 5533266449 — it got *cheaper* |

Standard set: **0 regressed, 1 improved, 0 new** — 94 of 94 solved, 93 of 94
on objective, **93 of 94 on the checker**, 94 of 94 deterministic. **93 of the
94 instances take exactly the iteration count they took before**; `pilot` is
the only one whose path changes at all, and total work over the set falls by
0.029%. The worst cost anywhere is `sc50a` at 1.012x — a 47-iteration model
where one extra pair of solves is a measurable fraction of a small total — and
every instance above a second of work is within 0.001%.

The infeasible set is 0/0/0 and still PASS, and its record file comes back
**byte for byte identical** — not merely equivalent. That is the change
confining itself where it was aimed, confirmed structurally rather than by
inspection: none of those 29 instances ever declares an optimum, so the one
refresh that refines is the one that never runs there.

Kennington is 0/0/0 and still PASS, 16 of 16 on every condition, with 15 of
the 16 taking exactly their baseline iteration count; the sixteenth is
`pds-20`'s one extra iteration, which is D27's and predates this. Its work
rises 0.043%, and that number is the honest price of the change on models
this size — two extra solves against a basis of 105127 rows, once per solve.
Kennington is where this had to be checked rather than the standard set: it is
what caught the fourth attempt at the shift residue, which looked perfect on
the other two (PLAN 2.8.1).

**What is left of condition 1a is one instance.** `pilot87`, missing its
objective by 7.6x with a dual violation of `1.87e-5`, unmoved by this and by
everything before it. Six of the seven instances the checker once rejected
have now closed — `pilot-ja` (D21), `finnis` (D23), `nesm` (D25), `etamacro`
(D27), `greenbea` (D28) and `pilot` here — and not one of them by moving a
tolerance.

---

## D30 — The primal clean-up judges every candidate before it moves any of them, and against the costs the model owns

D28 built `primal_cleanup` and closed `greenbea` with it in eight pivots. It
had two defects, both invisible from the outcome, and together they were the
whole of what stood between `pilot87` and the M1 gate.

**The first is an aliased vector.** `wants_a_pivot` calls `column_traffic`,
which reads `s->rho` expecting the duals `compute_duals` left there —
`column_traffic`'s own comment says so, and warns that "price_and_select
overwrites `rho` with a pricing row mid-solve, so this would be wrong if
called from anywhere else". `primal_cleanup` is anywhere else: its first
pivot builds row `r` of `B^-1` in `rho` and never puts the duals back.

Measured on `pilot87`, every round: **12 candidates on entry, one pivot, zero
candidates on exit.** Not fewer — none. The traffic computed from a pricing
row is large enough that the noise test refuses every remaining column, so
the loop could only ever take one pivot per call. `greenbea`'s eight pivots
were eight separate rounds of the re-entry, which is exactly why nothing
looked wrong.

**The second is why fixing the first changed nothing.** Collecting the
candidates up front, while `rho` is still the duals, leaves the run **bit for
bit identical** — and that is the measurement that found the real mechanism.
`pivot()` runs `shift_to_feasible` over every variable, and that routine sets
`d[v] = 0` and books a loan. So the pivot before did not *repair* the other
candidates' sign conditions; it **lent them away**. A routine whose entire
job is the residue that settling reveals was reading costs that had just been
papered over, and finding nothing to do.

**The repair is to call in each candidate's own loan before judging it.**
Shifting a nonbasic's cost moves only that variable's reduced cost — the
duals come from the basic costs alone, which is the locality argument
`shift_to_feasible` is already written on — so taking it back is exact and
touches nothing else. It is the same act `settle_shifts` performs at the end,
performed one column at a time at the moment that column is being decided.

**And one piece of D29 was incomplete, which only this made visible.** D29
refines the two solves at the refresh that verifies an optimum, on the
grounds that the numbers there are the answer rather than an input to a pivot
choice. That rule is right and it was applied to one caller of three. With
`primal_cleanup` now taking several pivots, `pilot` began ending on the
refresh that rebuilds *after* a clean-up — which was not refining — and its
row residue came back at `1.89e-6`, the very number D29 had removed. The fix
is not a new rule but the existing one applied where it belongs: every
refresh whose result can be published refines, and there are three.

**Measured on all three sets.**

| | before | after |
|---|---|---|
| `pilot87` objective | 301.71262909440185 | **301.71038732387791** |
| Koch's exact value | 301.71034733311052 | relative error `2.28e-3` → **`1.33e-7`** |
| `pilot87` dual violation | 1.87e-5 | **0** |
| `pilot87` gap | 2.75e-8 | 4.88e-13 |
| `pilot87` work | 23747832220 | 23547935117 — it got *cheaper* |
| `pilot87` checker | REJECTED | **ok** |

Standard set: **0 regressed, 3 improved, 0 new**, and **`gate: PASS`** —
94 of 94 solved, on objective, on the checker and deterministic. 92 of the 94
take exactly their baseline iteration count; only `pilot` (+1.9%) and
`pilot87` (−0.08%) move at all, and total work over the set falls 0.014%.
The infeasible set is 0/0/0 and still PASS.

**What the objective trajectory said, and it is the reason this was worth
chasing rather than declaring an exception.** `pilot87` reached 301.71501 on
its first settled point — `4.66e-3` above the optimum — and the re-entry
recovered half of that in six rounds and then ground for twenty-five more,
buying `7e-5`, not monotonically, before stopping at `SETTLE_ROUNDS`. D25
records that cap as "a backstop and not a limit meant to bind". On `pilot87`
it bound, and what it was capping was a loop that could take one pivot per
round when twelve were waiting.

---

## D31 — What the campaign settled, and the tolerances freeze where they stand

M1's gate is met on all three instance sets, and four questions that had been
deliberately left open "until the campaign says" now have their answer. They
close together because they close on one body of evidence.

**The §2.6 tolerances are frozen.** They were drafts throughout, on the
explicit terms that they would be frozen when the Netlib gate closed and that
any later change would be a changelog entry. That condition is satisfied and
the freeze takes effect at the values as they stand — not one of which was
moved to close an instance.

That last clause is the whole reason the freeze is worth anything. Eight
instances were refused at some point and **every one closed as a defect with
a mechanism**: a contribution the checker was dropping, a bound-proximity test
judged absolutely on a row that cancels ten orders of magnitude, a settled
basis the method had never been handed back, a cycle read as a stall, a repair
test reading the wrong quantity in the wrong space, a column with nowhere to
rest, a residual of the basis solve, and a clean-up loop taking one pivot
where twelve were waiting. A tolerance that survived eight opportunities to be
blamed and was never the culprit is a number with evidence behind it.

**Q1 — dual phase 1 by artificial bounds survives.** The question was whether
the lent-bound method would hold up against real instances or have to be
replaced by a subproblem or cost-shifting method. It held: every instance of
all three sets solves, and no failure anywhere in the campaign was traced to
phase 1. Replacing it is now an M2 performance question and not a correctness
one.

**Q3 — no instance forces a presolve into M1.** The question reserved the
right to add the smallest presolve that would close the gate. The gate closed
without one. Presolve enters M2 as a performance matter, where it belongs.

**Q9 — the refusal never fired.** A model whose optimum lies past the bound
phase 1 lends is refused rather than answered, and how often that happens was
known only for generated models (46 of 3000, from a generator written to
provoke it). Across all 139 real instances it happened **zero** times. So the
loan size is a performance parameter and not a correctness risk, and the
refusal costs nothing today.

**Q10's perturbation half closes, and on the absence of a demand rather than
on a design.** §2.5.9 called for deterministic bound perturbation as an
anti-stall device. The one instance that appeared to need it was cycling
rather than stalling, and the cure for a cycle is exact and terminating where
a perturbation is neither. No instance of the 139 asks for it, and "how much
to perturb" remains a number with nothing behind it. It stays unbuilt.

**The distinction that matters here, stated because it is easy to lose.** An
unused device is not a validated one. Q9's refusal and Q10's perturbation
close because nothing demanded them, which is a different and weaker fact than
Q1 and Q3, which close because something was tried against real models and
held. If a model ever lands on either, they reopen with that model in hand —
which is the only way to size a fix rather than guess it.

**What remains of M1 is not a solver question.** The per-iteration work weight
of §2.7, deliberately left without a number until the iteration existed and
its non-update overhead could be attributed on its own; and the public API
shape of §2.4, which needs the maintainer's confirmation rather than a
measurement.

**Measured again after the repair, because D30 left a claim standing that is
no longer true.** D30 recorded that `SETTLE_ROUNDS` bound on `pilot87`, which
contradicted D25's description of it as "a backstop and not a limit meant to
bind". Both were right about their own moment, and the resolution is that the
cap was capping a defect rather than a solve. Instrumented over the standard
94 after the fix:

| | rounds per solve | clean-up pivots |
|---|---|---|
| 90 instances | 1 — open the loop, find nothing, leave | none |
| `etamacro`, `nesm` | 2 | none |
| `greenbea` | 2 | **8 in a single call**, where it used to take 8 rounds |
| `pilot` | 12 | 15 across five calls |
| `pilot87` | **16**, against a cap of 32 | 17 across three calls |

Nothing binds anywhere, and the worst case sits at half the cap. D25's
description holds again; its per-instance figures (`nesm` one round, `pilot`
three, `pilot87` six) predate the primal clean-up and are superseded by the
table above.

The flat cost of the mechanism is now visible too: ninety of the 94 pay one
round of asking and nothing else, which is two scans over the variables per
solve.

---

## D32 — The fixed cost of a simplex iteration is zero, and the row leaves the table

PLAN 2.7 has carried a work-unit weight with no number against it since the
counter was written: *fixed overhead per simplex iteration*, marked "see
note". The note said a dual simplex iteration performs exactly one basis
update, so a constant charged per iteration alongside the per-update one
would bill a single event twice under two names, and that it would get a
number once the iteration's non-update overhead — pricing, ratio test,
bookkeeping — could be attributed on its own.

It can now, and the answer is that there is no number to give: **the weight
is zero and the row leaves the table.**

**How it was measured.** A `JAOS_DIAG` build attributes every charge to the
phase of the iteration that made it, by setting an active bucket at the call
sites in `run()` and `pivot()` and reading it in `jm_work_add`. Nothing else
changes — no arithmetic, no loop order — so the instrument is checkable, and
was checked: over the standard 94 and the 16 Kennington, **all 110 instances
report identical iterations, identical work and identical solution digests**
to the committed record. The buckets sum to the total exactly, with no
unattributed remainder.

**Where the units actually go**, over 289,680,470,328 units across those 110
solves:

| Phase of the iteration | Share |
|---|---|
| pricing row and ratio test (`price_and_select`) | **53.09%** |
| dual update and steepest-edge weights | **27.52%** |
| the two FTRANs | 6.80% |
| the row scan that picks the infeasibility | 5.62% |
| refactorization and the refreshes | 5.07% |
| **the basis update** | **1.79%** |
| everything outside the loop | 0.11% |

**The premise behind the missing number was wrong, and the conclusion it was
protecting is right anyway.** An iteration is not a basis update wearing
another name: the update is 1.8% of it. The non-update overhead runs from
4.4x the update's cost on the smallest model to **1450x** on the largest,
median 32.5x. So a per-iteration constant would not have double-billed one
event — the two really are different events, and one of them is marginal.

What makes the weight zero is the other half. That non-update overhead is
already charged, in full, and charged *dimensionally*: the bookkeeping comes
to exactly `iters * (nvar + 2*nrow)` in **110 of 110 solves**, and every
other charge is per nonzero touched. Divided by `nrow + nvar`, the
per-iteration non-update cost sits between 3.40 and 68.12 across a dimension
range of 86 to 364,953 — a span of 4245x — with no drift toward a floor at
the small end, which is what a missing constant would look like. There is no
O(1) residue for a constant to represent. Adding one would charge a second
time for work already counted by its size.

Both fixed weights that remain — `JM_WORK_UPDATE` at 64 and
`JM_WORK_FACTOR` at 4096 — stay exactly as they are. They exist because a
basis update and a factorization each have an O(dim) floor independent of how
much they change, and that argument was never available for the iteration:
an iteration's floor *is* its dimensional work, and that is billed.

**A note for M2, which this measurement produced without being asked for
it.** More than half of all work is the pricing row and the ratio test, and
another 27.5% is two dense sweeps over every variable per iteration. That is
what hyper-sparsity [9] addresses, and it is now measured rather than
assumed. It changes no weight here; it says where M2 should look.

---

## D33 — The public API's shape, as decided rather than as drafted

PLAN 2.4 has held the API as "shape, not final signatures", to become a
decision record once the maintainer confirmed it. Confirmed, with the
instruction that where the draft and good construction disagree, the more
correct one wins. Two places, below.

**What is decided.** One public header, `include/jaos.h`, prefix `jaos_` /
`JAOS_`, opaque `jaos_model`. `int64_t` for every public index and count,
`double` for every value; internal storage may pack tighter and the ABI never
does. Every fallible function returns a `jaos_status` and results leave
through parameters. No global state, no errno. Two models are fully
independent. The library owns its memory and `jaos_model_free` releases all
of it. Queries copy into caller-provided buffers. Two budgets, separate, per
D8. SemVer, with version macros and `jaos_version()`.

**Decided differently from the draft, and the draft was wrong.** 2.4 listed
one flat set of statuses — OPTIMAL, INFEASIBLE, UNBOUNDED, WORK_LIMIT,
TIME_LIMIT, NUMERICAL_ERROR, OUT_OF_MEMORY, INVALID_INPUT. The
implementation splits them in two, and that split is the decision:
`jaos_status` says whether the call did its job, `jaos_solve_status` says what
the solve found. Hitting a work budget is not a failed call, and a flat enum
forces every caller to sort those two questions out for itself. `JAOS_ERR_IO`
and `JAOS_SOLVE_NOT_RUN` exist for the same reason: a file that cannot be
read is not a file with bad content, and a model that has not been solved is
not a model whose solve found nothing.

**Correction 1 — the no-internal-pointers rule was stated too broadly, and is
narrowed to what is true.** 2.4 says the library never hands out pointers into
its internals, "so no hidden lifetimes exist". `jaos_model_error` returns a
`const char *` into the model. The rule as written was therefore false, and
the honest question is which of the two to change.

The pointer stays, and the rule is restated: **no solution data leaves by
pointer.** Every number a caller computes with is copied into a buffer the
caller owns. The error message is diagnostic, not data, and the storage
behind it is `char err[256]` embedded in the struct — not an allocation. So
the pointer is stable for the entire life of the model and is invalidated
only by `jaos_model_free`; what changes between calls is the text in it,
never the address. That is a weaker claim than "no lifetimes" and a much
stronger one than the usual C convention, where such a pointer dies at the
next library call.

The alternative — copying the message into a caller buffer — was rejected on
correctness, not on effort. It gives the error path a length parameter, a
truncation mode and a status of its own, so reading about a failure acquires
its own way to fail. The diagnostic path should be the one path that cannot.

**Correction 2 — the basis statuses 2.4 promised are built, because their
absence was a real gap.** 2.4 lists basis statuses among what is queryable
after a solve, and M1 did not expose them: `publish()` had the information and
dropped it. The tempting reading is that nothing consumes them until
warm-started branch and bound in M3, so they could be deferred.

That reading is wrong, and the reason is that the basis is not a solver
internal — it is the part of the answer the values cannot carry. A basic
variable's value comes out of the factorization and may sit anywhere between
its bounds; a nonbasic one is pinned to a bound, and that pinning is what
makes a basis determine a point. A basic variable that lands exactly on a
bound is indistinguishable, in the published values, from a nonbasic one
resting there — and only one of the two is a constraint the optimum is held
by. Withholding it means a caller cannot tell which constraints are active,
which is an ordinary question to ask of an LP and not a warm-start feature.

So `jaos_basis(m, col_status, row_status)` with its own `jaos_basis_status`
enum, rather than a fifth buffer on `jaos_solution`: the statuses are a
different kind of quantity from the doubles beside them, and one query per
kind of answer keeps `jaos_solution` from growing a parameter of a different
type. The internal and published enums are mapped through a `switch` rather
than cast, so that renumbering either is a compile error and not a wrong
basis published in silence. Rows are described by their activity, which the
scaling cannot reorient: scaling multiplies a row by a positive factor,
moving where a bound sits but never which bound an activity rests on.

Availability follows `jaos_solution` — no optimum, no basis — and the reason
is sharper: a zeroed buffer does not read as absent, it reads as a solution in
which every variable is basic, which is not a thing a simplex can report.

The tests were checked the way a predicate has to be here: with the mapping
inverted at its one `switch`, `test_the_basis_names_which_rows_hold_the_optimum`
fails on the swapped constant and
`test_the_basis_agrees_with_the_values_it_came_with` fails on a row published
`AT_UPPER` whose upper bound is infinite. A basis that describes a different
point than the values beside it does not pass.

**What is not decided here.** None of this freezes an ABI. The version is
0.1.0-dev and SemVer permits 0.x to break; what is settled is the set of
conventions every later signature has to satisfy, not the signatures
themselves. The weights of D16 and D32 are the public contract that freezes at
1.0; the header is not, yet.

---

## D34 — The one place D8 rested on an argument now rests on a measurement

D8 promises the same result on every machine. Every mechanism behind that
promise is structural — fixed iteration order, no clock, no address-dependent
traversal, `-ffp-contract=off` so no FMA is contracted — except one, and
PLAN 2.11 said so plainly: **the scaling's determinism leans on libm.** The
exponents come from `log2`, whose last-ulp rounding IEEE does not pin down
across C libraries. The note ended with the right instruction, which is that
"could differ" is a claim a harness has to test rather than assume.

It has been tested, and the answer is that no realistic libm can change a
JAOS result. The margin is about **nine orders of magnitude**.

**What was actually at risk, and why it is narrow.** `log2` appears twice in
`src/scale.c` and nowhere else in the solver — no other translation unit
calls a transcendental function at all. Both results feed exponents that
`pow2_of` rounds to integers and turns into exact powers of two, so the only
way a libm difference becomes a different answer is by tipping one of those
`round()` calls across a half-integer. Everything after the rounding is
exact: a power-of-two factor multiplies a mantissa by 1.

**The measurement, which does not need a second machine and is stronger than
having one.** Comparing two machines compares two libms. Perturbing `log2`
by a controlled amount covers *every* libm within that amount. So the probe
nudges `log2` and asks whether any of the 139 instances produces a different
set of scale factors — hashed, compared bit for bit.

| Perturbation of `log2` | Instances whose factors changed |
|---|---|
| ±1 ulp, ±4 ulps | **0 of 139** |
| +1e-12 … +1e-6 | **0 of 139** |
| +2e-6 | **0 of 139** |
| +5e-6 | 2 |
| +1e-5 | 4 |
| +1e-4 | 10 |

The tipping point is between **2e-6 and 5e-6** in log2 units. An ulp of
`log2` over the range these matrices produce (|log2| up to about 33) is at
most 7.1e-15, so a library would have to be wrong by roughly **4x10^8 ulps**
before a single factor moved. Real implementations are specified within one
or two, and several are correctly rounded.

Measured alongside it, and it explains the result rather than merely
agreeing with it: across 1,590,682 rounded exponents on those instances, the
closest any one of them came to a tie was **4.29e-7** (`woodw`). Nothing sits
near the edge.

**The instrument was shown able to fire before its silence was believed.**
At 1e-4 it reports ten changed instances by name — `25fv47`, `beaconfd`,
`pilot` among them. A probe that had returned "all identical" at every level
would have proved only that it was not looking.

**What this does not establish.** It removes the one identified risk; it is
not a second machine. JAOS has still only ever run on one architecture and
one C library. Other divergence classes are handled by construction rather
than by measurement — no FMA contraction, no `long double`, locale-independent
number parsing in the readers, no address-ordered iteration — and those
remain arguments, though of a kind that a compiler flag and a code rule can
actually carry. If JAOS is ever built on another architecture, the
determinism harness should be run there; what will not need re-testing is
`log2`.

Condition 2 of the M1 gate is unchanged in verdict and better founded in
substance. What it verifies directly is still same-machine reproducibility —
two solves in one process and one across runs. What it now also has is a
bound on the only cross-machine mechanism anyone had identified.

---

## D35 — Pricing walks the matrix by row, and the answer does not move

The first change of M2, and the one D32's attribution pointed at: 53% of all
work units JAOS spends are the pricing row and the ratio test. Underneath
that number was a loop that asked every column in turn for its dot product
with `rho`, which costs the entire matrix on every iteration however sparse
`rho` happens to be.

`rho` is a row of `B^-1`. Measured over the standard set before touching
anything, its density is **0.24 at the median and 0.004 at the sparsest**.
The column view cannot use that: a zero of `rho` is spread across every
column that touches the row, so no column can skip it. The row view can — one
zero skips a whole row of the matrix.

**What changed.** `price_all` accumulates `alpha = rho' M` by walking the CSR
mirror over the rows where `rho` is nonzero, scattering into a dense `alpha`.
The mirror is `jm_model_ensure_rowwise`, which existed, worked, and had never
been called by anything: PLAN 2.5.1 has said since the beginning that the
dual simplex prices rows and the column view feeds FTRAN, and until now only
the second half was true.

**The answer is bit-identical, by construction rather than by luck.** Each
column of the CSC copy is sorted by row index, and the rows are visited in
increasing order, so every column accumulates its terms in exactly the order
the column-wise loop used. A skipped row would have contributed `0.0 * a_ij`,
which cannot change a sum — only the sign of a zero, and no test in the
method reads one. Confirmed rather than argued: **0 of 110 solution digests
moved and 0 iteration counts moved**, across the standard and Kennington
sets. Same search path, same answer, less arithmetic.

**Measured on all three sets.**

| | |
|---|---|
| Total work over 110 solved instances | 289,680,470,328 -> 243,168,942,577 |
| Overall | **1.19x less** |
| Instances that got cheaper | **96 of 110** |
| Per-instance factor | min 0.95x, median 1.17x, max 2.21x (`osa-14`) |
| Gate | PASS on all three sets, 0 regressed |

**It costs something, and the cost is understood rather than absorbed.**
Fourteen instances got more expensive, none by more than 5%: `grow7`,
`stair`, `perold`, `pilot4`, `pilot87`, `pilot-we` among them. These are the
models where `rho` is dense, so there are no rows to skip, and what remains
is the one thing the row view does that the column view did not — it reads
the entries of *basic* columns, because a row does not know which of its
columns are in the basis. The column-wise loop skipped a basic variable
without touching it at all.

That is also why the whole-solve gain (1.19x) is smaller than the pricing
pass's own (1.83x measured in isolation): the pricing pass is a bit over half
of total work, and part of what the row form saves it hands back on basic
columns.

Filtering those out would cost a status test per entry, on the hottest loop
there is, to save reading entries that are already in cache. That is a trade
with a measurement behind neither side, so it is not made here.

**The pinned work test caught it and was re-pinned deliberately** — 8535 ->
8544 on a three-row model, which is the basic-column cost with none of the
saving, because `rho` on three rows has no zeros to skip. A change detector
that fires on a change this size is doing its job; the number it now carries
is the record of what the accounting does, and the instance sets are where
the question of whether it is worth it gets answered.

**What this is not.** It is not hyper-sparsity in the sense of [9]. The
triangular solves themselves are still dense in the working vector, and the
pattern of `rho` is still discovered by scanning all `nrow` entries rather
than predicted from the factor's dependency graph. Both remain open and both
are larger wins than this one. This is the change that the same insight buys
in the pricing product, where JAOS happened to be spending the most.

---

## D36 — A scatter-form BTRAN is rejected: the saving is real and the arithmetic is not free

The obvious next target after D35, refused on measurement. Recorded because
the reasoning was sound, the prize was large, and it still failed — and the
way it failed is the thing worth keeping.

**The case for it.** After D35, attribution over the standard set puts 45.8%
of all work in the triangular solves: 28.6% in `pivot`'s two FTRANs and
17.2% in the BTRAN of the pricing row. FTRAN already skips a zero — both its
L pass and its U pass `continue` when the value they would scatter is zero.
BTRAN cannot: it resolves `U'` by dot products over `ucol`, and a dot product
must read its whole column to discover it contributed nothing.

Measured, that is a large amount of nothing. Over the standard set, **96.7%
of the 818 million slots resolved in the `U'` pass come out exactly zero**,
and 76.2% of the pass's entries could be skipped by a scatter — 54.1% of the
whole BTRAN. U is already stored in both orientations for the update's sake,
so the row view needed to scatter costs no memory that was not being paid.
Per instance the U-pass saving ran from 63.4% to 99.7%, median 92.6%.

**Why it was rejected.** Unlike D35, this changes the order the terms
accumulate in, and a triangular solve is where that matters most. Built,
tested — the whole suite passed, including all 18 LU tests — and run:

| | before | after |
|---|---|---|
| `pilot-ja`, `pilot-we` | optimal | **INFEASIBLE**, and both are feasible |
| `pilot` | checker ok | objective and checker fail |
| total work, standard set | 65,216,017,395 | **98,552,422,298** |
| `pilot87` | 50,850 iterations | 125,777 |
| `grow7` | 544 iterations | 5,402 |
| digests moved | | 54 of 94 |

**A feasible model reported INFEASIBLE is the exact failure this project
already learned to watch for**, and it appeared twice. The work went *up* by
half, because the pricing row is what chooses a pivot: degrade it and the
method makes worse choices, takes more iterations, and pays far more than
the solve ever saved. Twelve instances got cheaper and the median improved;
reading only those would have shipped it.

**What the failure teaches, which is not "scatter is bad".** The saving was
never the problem. The problem is that skipping was bought by reordering,
and those are separable. A dot product accumulates one column's terms
together; a scatter delivers them spread across the whole solve, interleaved
with everything else. On vectors that cancel — which is what `B^-1` rows do —
those two orders do not produce comparable error.

**So the route to the same 54% runs through predicting the pattern instead of
reordering the sum.** The slots whose value comes out zero are exactly those
unreachable from the right-hand side's support in the dependency graph of the
factor; a depth-first search finds them in time proportional to the result's
size, which is the Gilbert-Peierls idea hyper-sparsity [9] is built on. Slots
outside that pattern can then be skipped *without touching the arithmetic of
the ones inside it* — they are exactly zero, not nearly zero — so the dot
product stays exactly as it is for every slot that is actually computed. That
version should be bit-identical and save the same work, which is the
combination this attempt could not have.

That is the next attempt, and it is a larger piece of work than this one was.

---

## D37 — A published zero is a zero, and the digest was lying about it

Found while measuring D38, and it had to be fixed before D38 could be judged
at all — which is the reason it gets its own entry rather than a line in
someone else's.

**The defect.** IEEE keeps two zeros, and which one a value lands on depends
on the sign of whatever produced it. JAOS published whichever came out. So
two solves could report the same optimum, agree on every digit of the
objective, satisfy the checker identically — and differ in bytes, because one
wrote `-0.0` where the other wrote `0.0`.

That is a defect in the instrument this project leans on hardest. D21 made a
per-instance record the way changes are judged, and a solution digest is its
strongest single line: a change that should alter nothing must leave every
digest identical. A digest that moves when the *number* has not moved makes
that test report differences that are not differences.

**How much it was lying by:** normalising the sign of published zeros, with
no other change at all, moves **90 of the 94** digests on the standard set,
while moving **zero work units and zero iterations**. Nine out of ten
instances were publishing at least one signed zero.

**What it cost to fix:** one comparison per published number, on `publish`,
which runs once per solve.

**Why this is not cosmetic.** D38 was measured first without it, came back
with 89 moved digests, and read exactly like the scatter form D36 had already
been rejected for. The difference is that D36 changed answers and D38 does
not — and with a digest that cannot tell those two apart, the second one gets
thrown away for the first one's crime. The fix is what let the two be told
apart at all.

---

## D38 — BTRAN skips the slots it can prove are zero, and proves it by not touching them

D36 rejected the cheap way to make BTRAN skip zeros: scattering over `urow`
saved 54% of the pass and changed the order the sum accumulates in, which
cost two feasible models a wrong INFEASIBLE and raised total work by half.
The entry ended by saying the saving and the reordering were separable, and
that predicting the pattern would get one without the other. It does.

**The technique** [9], from Gilbert and Peierls. The U' pass computes
`v[s] = (y[s] - sum over ucol[s]) / d[s]`, so `v[s]` can only be nonzero if
`y[s]` is or if some `v[i]` feeding it is. The nonzero pattern is therefore
the set reachable from `y`'s support along U's rows, and a depth-first search
finds it in time proportional to that set. On the reference set it is about
3% of the slots — 96.7% of them resolve to zero.

**Why this one is bit-identical and the other was not.** The slots left out
are exactly zero, not nearly zero: they start at zero and receive nothing. So
every slot that *is* computed runs the same dot product, over the same
column, in the same order, against the same values. What changes is how many
of them run. Post-order fills the pattern from the back, which comes out as
topological order, so each slot is still reached after everything it depends
on.

**Measured on all three sets, against a zero-normalised reference (D37).**

| | standard | infeasible | Kennington |
|---|---|---|---|
| digests moved | **0 of 94** | — | — |
| iteration counts moved | **0** | **0** | **0** |
| instances broken | 0 | 0 | 0 |
| work | **1.040x** less | **1.095x** less | **1.057x** less |
| instances cheaper | 92 of 94 | — | **16 of 16** |

Total across the three: 246,174,482,638 -> 233,820,190,281, **1.053x less**,
and `ken-18` — the largest model JAOS solves, at 105127x154699 — comes down
1.066x. Nothing anywhere gets more expensive than 1.000x.

**What it costs, stated because the pinned test caught it.** The search is
billed for the edges it walks, and on a three-row basis it walks nearly the
whole of U to discover that nearly the whole of U is reachable: 8544 -> 8548.
The technique costs most and saves least exactly where there is nothing to
skip, which is why the question was settled on 139 real models and not on
that test.

**What remains.** The L' pass is untouched: only 4.1% of its entries sit
under a zero, and L has no row-wise copy to search over. And the search
itself still scans all `nrow` entries of `y` to find its roots, which is
O(dim) the technique is supposed to avoid — the callers know the support and
do not pass it. Both are smaller than what this took, and both are still
there.

---

## D39 — Infeasibility gets the second opinion optimality already got

D20 settled that a declaration of optimality is not accepted on carried
numbers: the point is recomputed from a fresh factorization and priced
again, because `x_B` is updated in place by every pivot and the factors are
patched rather than rebuilt, so both drift — and the drift is invisible from
the inside, since the test that would notice it is the one being run.

Every word of that applies to the other verdict, and it was not being
applied. `INFEASIBLE` was accepted the first time it was reached.

**The mechanism.** The dual is declared unbounded when the ratio test finds
no candidate, which happens when no `|alpha_v|` clears `PIVOT_MIN`. `alpha`
is `rho' M` and `rho` is a BTRAN against the patched factorization, so drift
shrinks exactly the numbers the test thresholds. A factorization that has
accumulated enough updates can therefore make every column look unusable,
and the solve reports that a model with a published finite optimum has no
feasible point.

**It is not hypothetical, and finding it needed an experiment the gate does
not run.** All 139 instances pass, and they always have — but they always
run with the same refactorization interval, so they always walk the same
trajectory, and the eight defects M1 closed were closed against that one
trajectory. Sweeping `REFACTOR_EVERY` across 16..256 walks different ones.
Before this change, seven of the eight values tried produced false
infeasibility: `pilot-ja`, `pilot-we`, `pilot87`, `agg`, `greenbea` and
`perold`, all with Koch references in the manifest.

After it, false infeasibility survives at one value out of eight, and only
at the extreme (256, sixteen times the tested interval).

**Cost.** One refactorization on a solve that is ending anyway: **0.04%** on
the infeasible set, and not one iteration count moves anywhere. The 29
genuinely infeasible models are still refused, 29 of 29 — which is the risk
in the other direction and the one that mattered to check.

**What this does not fix, stated because the same sweep measured it.** Two
other failure modes appear when the trajectory changes and neither is this
one. `pilot` and `pilot87` — the worst-conditioned models in the set, the
ones D29 and D30 were about — fall outside objective tolerance or get
rejected by the checker at several intervals; `pilot87` closed at a relative
error of 1.33e-7 against a tolerance of 1e-6, so seven times of margin, and
another trajectory spends it. And at intervals of 128 and above `pilot87`
trips the iteration guard, which the solver's own message calls a JAOS
defect. Both are real, both are open, and neither is a tolerance to widen.

**The interval stays at 64.** It is one of only two values that come out
completely clean, and the ones that looked cheaper looked that way because
`pilot87` — 38% of the standard set's work on its own — had dropped out of
the total by failing.

**A method worth keeping.** Varying a parameter that must not change any
verdict, and requiring the gate to hold across the range, measures something
139 instances at one setting cannot: not whether the gate passes, but how
much margin it passes with. It costs minutes with the parallel runner. It
found this.

---

## D40 — The pricing row is read through its pattern, and the pattern costs a comparison

PLAN 3.3 named this as M2's next target and said why: D35 made `alpha`
cheap to *produce* by walking rows, and every consumer of it still walked
all `nvar` entries. A sparse result feeding dense loops throws away what it
bought, which is exactly what [9] warns about.

**Measured before anything was written**, with a JAOS_DIAG build that counts
the nonzeros of `alpha` against `nvar` on every iteration:

| | standard 94 | Kennington 16 |
|---|---|---|
| pattern of `alpha` / `nvar` | 37.4% | **5.8%** |
| ratio-test candidates / `nvar` | 15.8% | 1.5% |
| per instance, median | 25.2% | 5.7% |
| per instance, min .. max | 0.8 .. 99.1% | 0.1 .. 83.6% |

The weighted numbers hide the finding, which is that **there is no typical
density and both extremes carry real weight**. `ken-18` is 66% of all the
dense sweeping the Kennington set does, on its own, and its pattern is
0.1%. `osa-60` is 3% of it at 83.6%. On the standard set `stocfor3` is a
quarter of the sweeping at 0.9% and `pilot87` is 15% of it at 65.8%. A
change that assumes either shape is wrong on a third of the work.

**What changed.** Three things, and only the first is new machinery.

1. `price_all` records where it wrote while it writes. The scatter already
   loads `alpha[c]` to add to it, so "was this slot zero" is a comparison
   against a value in a register — no second array, which is the whole
   difference between this and the basic-column filter D35 measured and
   refused. A slot that cancels back to exactly zero and is written again
   is recorded twice, so what comes out is a list rather than a set.
2. `jm_pattern_order` makes it a set, ascending, through a bitmap: mark,
   then read back over the touched word range, clearing as it goes.
   **A bitmap rather than a sort.** A sort costs `k log k` on a pattern
   that is read once, and duplicate removal comes free from a bitmap.
3. The ratio test walks the pattern where there is one, and so does the
   clear at the top of the next `price_all` — which quietly removes a
   `memset` of `nvar` doubles per iteration that no work unit ever counted.

**Ascending order is not tidiness, it is the correctness condition.**
`bfrt_walk` takes the first strict minimum it meets, `jm_harris_pick` the
first strict maximum inside its window, and `apply_flips` adds up one matrix
column per flipped candidate in the order they stand. All three break ties —
and, in the third case, order a floating-point sum — by position in the
candidate array. The dense scan filled that array in ascending variable
order, so any pattern in another order silently moves the trajectory. That
is also why the pattern is not simply consumed as the scatter produced it.

**Bit-identical, and confirmed rather than argued.** A variable outside the
pattern has `alpha` exactly zero, which the `PIVOT_MIN` test rejects before
anything else about it is read, so the two scans admit the same candidates
into the same positions. Over all three sets: **every one of the 110
published solution digests is unchanged, and not one iteration count moves
on any of the 139 instances.**

**Cost, against the committed baselines.**

| set | work before | work after | |
|---|---|---|---|
| standard 94 | 62,701,726,771 | 61,853,786,287 | 1.014x |
| **Kennington 16** | 168,372,717,242 | **128,912,974,652** | **1.306x** |
| infeasible 29 | 2,746,818,868 | 2,713,834,320 | 1.012x |
| all 139 | 233,821,262,881 | 193,480,595,259 | **1.209x** |

138 of 139 instances get cheaper and none gets dearer. The best on
Kennington are `pds-06` 1.393x, `pds-02` 1.385x, `ken-18` 1.344x; the worst
are the four `osa-*`, which are the dense ones and stay on the dense path,
at 1.01 to 1.04x. On the standard set the ceiling is `ship04l` at 1.411x
and the floor is `pilot87`, 38% of that set's work and 66% dense, at
1.0001x.

**The threshold, swept.** The pattern is walked when it covers at most
`nvar / SPARSE_ALPHA_DEN` slots. That constant is a number and it has a
measurement on both sides. Sweeping it — every point solving the whole set,
every point checked for status, iteration count, objective, checker,
determinism and digest:

| SPARSE_ALPHA_DEN | standard 94 | infeasible 29 |
|---|---|---|
| dense always | 62,701,726,771 (1.000x) | 2,746,818,868 (1.000x) |
| 1 (sparse always) | 63,828,762,995 (**0.982x**) | 3,018,870,603 (**0.910x**) |
| 2 | 61,877,454,651 (1.013x) | 2,727,662,783 (1.007x) |
| 3 | 61,849,797,319 (1.014x) | 2,712,617,118 (1.013x) |
| 4 | 61,853,786,287 (1.014x) | 2,713,834,320 (1.012x) |
| 6 | 61,872,128,786 (1.013x) | 2,717,391,579 (1.011x) |
| 8 | 61,887,517,817 (1.013x) | 2,721,158,185 (1.009x) |
| 16 | 61,922,218,103 (1.013x) | 2,725,666,623 (1.008x) |

Two things fall out of that table and neither was assumed beforehand.
**Always taking the sparse path is measurably worse than never taking it** —
1.8% more work on the standard set and 10% more on the infeasible one —
because ordering a pattern that covers most of the vector costs more than
the scan it replaces. And **the optimum is flat**: everything from 2 to 16
is within 0.6% of everything else on both sets, with 3 nominally best. The
only sharp edge in the whole range is the one at 1.

Kennington was swept at three points, and it names which instances the
threshold is protecting:

| SPARSE_ALPHA_DEN | Kennington 16 |
|---|---|
| 1 | 132,365,009,905 (0.974x) |
| 3 | 128,834,240,095 (1.001x) |
| 4 | 128,912,974,652 (1.000x) |

Between 3 and 4 nothing moves by more than 0.3% on any instance. Between 1
and 4, **the four `osa-*` models pay 1.28x to 1.35x more work** — `osa-60`
goes from 6.79e9 units to 9.17e9 — and every hyper-sparse instance in the
set is unchanged to four figures. Those four are exactly the ones the
diagnostic measured at 60.7% to 83.6% pattern density. The threshold does
one job and the sweep shows it doing it.

**It is set to 4, one notch to the dense side of where the counter puts
it, and that is deliberate.** Work units are blind to exactly the thing
that decides the other half of this trade: the sparse path replaces a
sequential scan of three arrays with dependent loads into them, and no
counter here can see a cache miss (D17). Every error the counter cannot
measure pushes the true crossover towards *denser*, so the conservative
choice is the higher divisor. Locating it properly needs Q4's host.

**Verdicts do not move with the threshold**, which is the property that
makes it a tuning knob rather than a decision: over 123 instances at eight
settings, every status, iteration count, objective, checker result,
determinism flag and digest is identical. What changes is only how `alpha`
is read.

**What this does not do.** Half of PLAN 3.3 is still open, and it is the
larger half on the big models. `pivot` still scans every variable to apply
the dual step, which is the same `nvar` per iteration this removed from the
ratio test — but its loop also runs `shift_to_feasible` over variables the
step did not touch, and skipping those needs the invariant that they were
already dual feasible, which `compute_duals` and `primal_cleanup`'s loan
call-in both break. The steepest-edge update still sweeps every row to
touch 0.17% of them on Kennington, and that one needs the FTRAN to produce
a pattern rather than a dense vector — hyper-sparsity proper [9], not this.

**The instrument was checked before it was believed.** `jm_pattern_order`'s
tests assert that the bitmap comes back all zero, and the fault that
matters — dropping the clear, so the *next* iteration inherits positions
nobody recorded — was injected and confirmed to fail four of the five
cases. A test that only read the returned list would have passed with the
leak inside it.

**The parallel runner earned its place again.** All of the sweep above is
one measurement per instance per setting, built `-O3 -march=native` and run
concurrently, and it reproduces the sequential `-O2` gate's own record byte
for byte on all three sets — checked, not assumed, at the
`SPARSE_ALPHA_DEN = 4` point, down to Kennington's 128,912,974,652 units.
Minutes instead of hours, which is what made sweeping eight settings worth
doing at all (PLAN 3.7).

It also produced one wrong reading on the way, which is recorded because
the failure mode is generic: two sweep runs were launched over the same
scratch directory and each deleted the other's work, and the point that
came out of it was short two instances of ninety-four while looking like a
perfectly ordinary total. Nothing about the numbers said so. **A sweep
point is now discarded unless its instance count matches the manifest**,
because a total is not evidence until it is a total of the right things.

---

## D41 — The dual step walks the pattern too, and an invariant is what makes that safe

The other half of PLAN 3.3, and the larger one. D40 stopped the ratio test
scanning every variable; `pivot` was still doing it, for the same `nvar`
per iteration, to apply the dual step.

**Why it could not simply be given the same treatment.** The step itself is
already sparse by construction — a variable the pricing row does not touch
gets `d[v] -= theta_dual * 0.0`, which leaves the value alone. But the loop
does a second thing on every variable it visits, and that one is not driven
by `alpha` at all: `shift_to_feasible` puts a reduced cost that has drifted
onto the infeasible side of its bound back where it belongs. Skipping a
variable skips its repair, and a reduced cost left breached is not untidy —
D28 records what it is. The ratio test reads a cost already past zero as
blocking immediately, and a step computed from one runs backwards.

**So the condition is named rather than hoped for.** Walking the pattern is
correct exactly when every nonbasic cost the step does not touch is dual
feasible already, because then the repair it skips would have done nothing.
That holds after a pivot, by induction: the pivot repairs everything it
moved, and nothing else moved. It is broken in exactly two places, and both
were found by asking which code writes a reduced cost without going through
a pivot:

- `compute_duals`, which rebuilds every cost from the basis and owes
  nothing to the shifting the solve has been doing;
- `primal_cleanup`, which calls a column's own loan back in before judging
  it — deliberately, and D30 is why.

Each sets `duals_dirty`, and the next dual update pays for one full sweep
to clear it. That costs `nvar` once per refactorization rather than once
per iteration.

**Bit-identical, over the whole gate.** All 110 published digests unchanged,
all 139 iteration counts unchanged, all three sets PASS.

One argument had to be checked rather than trusted, and the gate is what
checked it. On the dense path `d[v] -= theta_dual * 0.0` can turn a `-0.0`
into `+0.0`, and the sparse path leaves the `-0.0` standing. Every reader of
a reduced cost compares it against zero or a tolerance, where the two are
the same number, and `published` normalises the sign on the way out — which
is D37, and D37 exists because a difference of exactly this kind once made
two identical answers differ in bytes. The digests say the reasoning held.

**Cost, against the baselines D40 left.**

| set | before | after | |
|---|---|---|---|
| standard 94 | 61,853,786,287 | 60,945,483,751 | 1.015x |
| **Kennington 16** | 128,912,974,652 | **88,864,066,925** | **1.451x** |
| infeasible 29 | 2,713,834,320 | 2,673,400,285 | 1.015x |

136 of 139 get cheaper and none gets dearer. Taken with D40, Kennington has
gone 168,372,717,242 -> 88,864,066,925, which is **1.895x**, and the 139
together 233,821,262,881 -> 152,482,950,961, or 1.533x.

**The threshold sweep is the test this change most needed**, and it is a
different test from the one it was for D40. At `SPARSE_ALPHA_DEN = 1`
almost every iteration takes the sparse path in both consumers, so the
invariant runs under far more pressure than the gate puts it under at 4,
where a dense instance never exercises it at all. No unit test can stand in
for that: the models in the suite are small enough that `nvar / 4` is one
or two slots, so the suite runs the dense path almost exclusively.

Swept over the standard and infeasible sets at dense-always, 1, 2 and 4 —
**every status, iteration count, objective, checker result, determinism flag
and digest identical at every setting.** The invariant holds where it is
leaned on hardest.

| SPARSE_ALPHA_DEN | standard 94 | infeasible 29 |
|---|---|---|
| dense always | 62,701,726,771 (1.000x) | 2,746,818,868 (1.000x) |
| 1 | 62,604,789,725 (1.002x) | 2,871,034,741 (0.957x) |
| 2 | 60,887,135,693 (1.030x) | 2,648,689,325 (1.037x) |
| 4 | 60,945,483,751 (1.029x) | 2,673,400,285 (1.027x) |

**The optimum moved, and the reason is worth having written down.** Under
D40 alone, sparse-always cost 1.8% more than dense-always on the standard
set; with two consumers reading the same pattern it costs 0.2% *less*. The
ordering is paid once per iteration and amortised over everything that
reads it, so each consumer that goes sparse shifts the crossover further
towards sparse. That predicts the same again when the steepest-edge update
follows.

**The constant stays at 4 all the same.** The gain from 2 is 0.1% on the
standard set and 0.9% on the infeasible one, Kennington — now 58% of all
work — has not been swept on this tree, and D40's reason for sitting to the
dense side of the counter's own answer has not changed: work units cannot
see the indirection, and every cost they cannot see pushes the true
crossover the other way. Moving it on a 0.1% reading would be fitting a
constant to a measurement that cannot see half the trade.

---

## D42 — The exact weight is summed over rho's pattern, which nobody had to go and find

`pivot` charges `2 * nrow` per iteration for two loops that look alike and
are not. Attributed on the tree D41 left, that pair is **34.8% of the
Kennington set and 42.9% of `ken-18`**, which made it the largest single
thing left — and the two halves turned out to need completely different
work.

| | standard 94 | Kennington 16 | `ken-18` |
|---|---|---|---|
| the `2 * nrow` charge | 2.6% of the set | 34.8% | 42.9% |
| `rho` nonzero, of `nrow` | 32.61% | 1.02% | **0.08%** |
| `col` nonzero, of `nrow` | 33.04% | 0.17% | **0.03%** |

- `exact = ||rho||^2` sums a **BTRAN** result. Its pattern is free: the
  first thing `price_all` does is walk the whole of `rho` in ascending order
  looking for rows it can skip, so recording where the nonzeros are is one
  store per nonzero on a loop that was running anyway.
- `jm_dse_update` sweeps a **FTRAN** result. Nothing walks that vector
  first, so this half needs the solve itself to hand over a pattern — the
  reachability search of D38 applied to the forward direction. Still open.

This entry is the first half only, and it is deliberately the cheap one.

**Bit-identical by the simplest argument any of these changes has had.** The
slots skipped contribute `0.0 * 0.0` to a running total that is a sum of
squares and therefore never negative zero, so `x + 0.0` is exactly `x`.
Ascending order is inherited rather than arranged, because the walk that
recorded the pattern already was one. No ordering pass, no bitmap, no
threshold: `nrow` positions can each be recorded once, so the list is a set
already.

**Cost.**

| set | before | after | |
|---|---|---|---|
| standard 94 | 60,945,483,751 | 60,403,238,834 | 1.009x |
| **Kennington 16** | 88,864,066,925 | **73,555,422,842** | **1.208x** |
| infeasible 29 | 2,673,400,285 | 2,608,553,522 | 1.025x |

**All 139 instances get cheaper, none stays level**, no iteration count
moves and all 110 digests are unchanged. `ken-18` comes down 1.273x.

**A caveat this entry first carried and then had to withdraw.** It said the
counter was overstating the saving, because the `O(nrow)` walk the pattern
comes from happens in `price_all` and goes unbilled. The attribution run
that followed says otherwise: `price_all` charges `touched + nrow`, and on
the row-wise pass that `nrow` term is not the logicals its comment named —
only `nnz(rho)` logicals are written — it is that walk. So the walk is
charged, in `price_all`, before and after this change alike, and what D42
removed is a separate `nrow` of multiply-and-add. The saving is what the
counter says it is.

What `price_all` genuinely does not bill is the clear of `alpha` and the
reset of its basic entries, both of which PLAN 2.11 has recorded since
before any of M2. Neither is what this leans on.

The withdrawn caveat is left here rather than deleted, because the way it
was wrong is the ordinary way: it was reasoned from a comment instead of
measured, and the comment had been true of the code it was written for.

**Where this leaves M2's arithmetic.** Since the commit where M1's gate
first passed on all three sets, total work over the 139 reference instances
has gone 293,987,935,333 -> 136,567,215,198, which is **2.153x** — and
**3.043x on Kennington**, where 73% of it was. The standard set is 1.090x
over the same span, and that gap is the whole content of PLAN 3.2: the
mid-sized models spend their time in the triangular solves and the
factorization, and none of M2's entries so far have touched either.

---

## D43 — The solve says where its answer is, and one charge had been standing for two loops

The re-attribution that followed D42 put `price_all`'s walk over `rho` at
**27.45% of the Kennington set and 11.65% of the standard one** — the single
largest line in either. It is a scan of every row of a vector that is 1.0%
nonzero on Kennington and 0.08% on `ken-18`, looking for the rows it can
skip.

**The trap, which nearly cost a third wrong target in two days.** That walk
is not the only thing `price_all` does at the length of the basis: the reset
that puts basic variables' slots back to zero is another, and the function
bills `nrow` **once** for the pair. Removing only the walk would have left
the remaining loop justifying the whole charge, and the counter would have
credited a 27% saving for work that was still happening. Both had to go.

**What changed.**

- `jm_lu_btran_sparse` reports where its answer is nonzero. The solve's last
  pass already visits every slot to permute it back into the caller's
  indexing, so saying which ones carry a value costs a comparison against a
  value it has already loaded. A caller left to find out for itself scans
  the whole vector a second time; that second scan was the 27%.
- The reset visits the slots the scatter wrote instead of the basis
  positions, because no other slot can be anything but zero. The `status`
  read this costs is per pattern entry, not per matrix entry — the whole
  difference between it and the basic-column filter D35 refused.
- `price_all` walks `rho`'s pattern, ascending, which `jm_pattern_order`
  produces from what the BTRAN handed back in permutation order.

**The first measurement was mixed, and it was read as a diagnosis rather
than a verdict.**

| | first form | with thresholds |
|---|---|---|
| standard 94 | 0.9930x, 60 of 94 dearer | **1.0065x, all 94 cheaper** |
| Kennington 16 | 1.2384x, 6 dearer | **1.2553x, all 16 cheaper** |
| infeasible 29 | 0.9632x, 11 dearer | **1.0080x, all 29 cheaper** |

The cause was in numbers already measured. `rho` is 32.6% nonzero over the
standard set, and ordering a pattern that size costs more than the scan it
replaces. And the reset went from `nrow` to `np`, which on a model with far
more columns than rows is the **larger** of the two.

So both decisions became comparisons. The reset compares two loop lengths,
`np` against `nrow`, and takes the shorter — no constant to fit, because
both numbers are in hand. The ordering is gated by `SPARSE_RHO_DEN`. Above
it the pattern is collected, found too large and thrown away, which costs
nothing that was not already being spent inside the solve.

**The threshold, swept**, every point checked for status, iteration count,
objective, checker, determinism and digest, and every point identical on all
six:

| SPARSE_RHO_DEN | standard 94 | infeasible 29 |
|---|---|---|
| never order | 60,403,238,834 (1.000x) | 2,608,553,522 (1.000x) |
| 1 (always) | 60,395,842,900 (1.000x) | 2,620,124,297 (**0.996x**) |
| 2 | 60,030,319,943 (1.006x) | 2,596,230,529 (1.005x) |
| 3 | 60,013,100,251 (1.007x) | 2,587,134,529 (1.008x) |
| 4 | 60,015,257,439 (1.006x) | 2,587,812,579 (1.008x) |
| 6 | 60,023,268,092 (1.006x) | 2,590,323,797 (1.007x) |
| 8 | 60,027,430,249 (1.006x) | 2,591,843,685 (1.006x) |

The never-order point reproduces D42's committed baseline to the digit,
which is the check that the machinery costs nothing where the threshold
refuses it.

**Kennington was swept at three points as a prediction that failed**, and
the failure is worth more than the confirmation would have been. `rho` is
1.0% nonzero over that set, so `nr * DEN <= nrow` should hold for any
divisor up to about a hundred and 2, 4 and 8 should have come out identical.
They did not: 58,596,925,400, 58,596,535,370 and 58,765,607,674, with twelve
and fourteen instances moving. **`rho` has no density per instance. It has
one per iteration**, and at 8 enough individual iterations fall over the
line to cost 0.3%. That is the same sentence this entry already used to
explain why the threshold *helps* Kennington, applied to a prediction that
had not been checked against it.

**The constant stays at 4**, and now for a reason stronger than D40's. The
plateau is bounded on both sides by measurement rather than on one: 1 is
worse on the infeasible set, 8 is worse on Kennington, and 4 sits inside
with margin in each direction. D40's argument still holds on top of that —
the counter cannot see the indirection, so every cost it misses pushes the
true crossover towards dense — and 3, which is nominally best on two sets by
0.004% and 0.026%, is not a number to move a constant for.

**Kennington came out better with the threshold than without**, which is the
result that says what the mechanism is. The threshold does not only protect
dense instances from a rule meant for sparse ones; it stops the ordering
being spent on the dense *iterations inside* sparse instances. `rho` has no
density per instance. It has one per iteration.

**Cost, against the baselines D42 left.**

| set | before | after | |
|---|---|---|---|
| standard 94 | 60,403,238,834 | 60,015,257,439 | 1.007x |
| **Kennington 16** | 73,555,422,842 | **58,596,535,370** | **1.255x** |
| infeasible 29 | 2,608,553,522 | 2,587,812,579 | 1.008x |

**All 139 instances cheaper, none dearer**, no iteration count moved and all
110 digests unchanged.

**One decision here was about the accounting and it was made twice.** The
first version billed the reset, which is more honest in the abstract and
wrong in this entry: it mixes an accounting correction into a measurement,
and a baseline diff carrying both cannot be read as either. So the reset
stays unbilled, beside the clear of `alpha` and the rest of PLAN 2.11's
sweeps, until something charges all of them at once. **The evidence that
this came out right is the pinned work test not moving**: 8545, the value
D42 left. It passed through 8566, 8557 and 8548 on the way. A three-row
basis takes the dense branch of every decision above, so its total has to
come out unchanged, and it does.

**Since M1's gate first passed**, the 139 have gone 293,987,935,333 ->
121,199,605,388, which is **2.426x**, and **3.820x on Kennington**.

---

## D44 — The forward solve says where its answer is, and this one needed no ordering

The re-attribution after D43 put two lines at the top of the Kennington
column, tied at **26.40%** each: `price_row`'s scan for the infeasibility,
and `jm_dse_update`'s sweep over every row. The second is an FTRAN result
being walked to find the 0.03% of it that is nonzero on `ken-18`, and
`apply_flips`' update of `x_B` is another 9.42% doing the same thing to
another one.

**Cheaper to build than D43, for a reason worth naming.** D43 had to order
the pattern it produced, because `price_all`'s sums must accumulate in the
order the column-wise pass used and any other order moves the trajectory.
Nothing here does: the steepest-edge recurrence gives each row a weight from
its own old weight and the pivot, and both updates of `x_B` subtract
elementwise. So the pattern is used exactly as the solve produced it — no
bitmap, no `jm_pattern_order`, no second pass.

That also moves the crossover — and the sweep says it moves it off the end
of the range, which makes this constant the weakest of the three and the
entry says so rather than hiding it.

| SPARSE_COL_DEN | standard 94 | infeasible 29 | Kennington 16 |
|---|---|---|---|
| never | 1.000x | 1.000x | — |
| 1 (always) | **1.010x** | **1.034x** | 37,642,967,672 |
| 2 | 1.009x | 1.034x | 37,642,967,672 |
| 3 | 1.008x | 1.034x | — |
| 4 | 1.008x | 1.025x | 37,642,967,672 |
| 8 | 1.007x | 1.016x | — |

**There is no crossover.** On the two smaller sets it is monotone: the more
sparsely the vector is read the cheaper it gets, all the way to always. On
Kennington 1, 2 and 4 are byte-identical with not one instance moving,
because an FTRAN result 0.03% dense on `ken-18` never comes near any of
those lines. That is the null result D43's sweep was predicted to give and
did not, and here the prediction holds for the same reason it failed there:
`rho` had individual iterations dense enough to cross `nrow/8`, and this
vector does not.

Which is what the mechanism said it would do. D40 and D43 both pay a fixed
cost to order a pattern and have to earn it back, so both have a size below
which the pattern is not worth having. This one pays nothing, so there is
nothing to earn back and no size at which the dense loop wins **on the
counter**.

**The constant is 2 anyway, and it rests entirely on an argument the counter
cannot check**: the sparse loop indexes three arrays through a pattern where
the dense one reads one of them in order, and no work unit has ever seen a
cache miss (D17). D43 could point at measurements on both sides of its
plateau; this cannot point at either. The difference between 2 and 1 is
0.13% on the standard set and 0.02% on the infeasible one, which is the size
of thing worth giving up for a guess that might be wrong in the direction
the counter is blind to. Q4's host is what would settle it.

**What changed.** `jm_lu_ftran_sparse` records where its answer is nonzero
during the pass that permutes it back, the same trade `jm_lu_btran_sparse`
makes. `jm_dse_update` takes the pattern as an optional pair — the two forms
visit the same rows and compute the same numbers, and what differs is how
many rows are looked at to find them. `pivot`'s update of `x_B` and
`apply_flips`' both walk it too.

**Cost, against the baselines D43 left.**

| set | before | after | |
|---|---|---|---|
| standard 94 | 60,015,257,439 | 59,416,297,353 | 1.010x |
| **Kennington 16** | 58,596,535,370 | **37,642,967,672** | **1.557x** |
| infeasible 29 | 2,587,812,579 | 2,496,192,696 | 1.037x |

**The largest single entry of M2 so far.** 138 of 139 instances cheaper, one
level, none dearer; no iteration count moved and all 110 digests unchanged.

**The accounting did not move**, and that was checked the way D43's was: the
pinned work test stayed at 8545 without being touched. The `x_B` update
inside `pivot` was unbilled before this and stays unbilled; the one inside
`apply_flips` was billed `nrow` and is now billed for what it walks.

**Since M1's gate first passed**, the 139 have gone 293,987,935,333 ->
99,555,457,721 — under a hundred billion for the first time, **2.953x** —
and **5.946x on Kennington**.

---

## D45 — The work counter is calibrated against a clock, and it is optimistic by a factor that is not constant

Nine entries of M2 were accepted on work units because there was nothing
else to accept them on: D17 excludes this machine for published figures and
Q4's host does not exist. That left every one of them resting on an
assumption nobody had tested — that a unit removed is time removed.

**It is not, and the error is large.** Two experiments, both ratios of the
same instance against itself on the same machine, so almost everything that
makes this host unfit for absolute figures divides out.

**How many units a second buys, over the standard 94.** From 5.86e7 on
`fit2p` to 1.36e9 on `wood1p` — a spread of **23x**, and still **13x** among
the fifteen instances long enough for the clock to be trusted. The direction
is legible: models whose working set fits in cache buy far more units per
second. `fit2d` is 25 rows by 10500 columns and flies; `stocfor3` has 32370
variables at 0.9% density and is indirection from end to end.

**What M2 has actually bought, M1's close against today.**

| | work | **time** |
|---|---|---|
| standard 94 (the 26 above 50 ms) | 1.104x | **1.022x** |
| **Kennington 16** | 5.946x | **2.220x** |
| both, wall clock | — | 842.7 s -> 451.7 s, **1.866x** |

`ken-18` alone goes 569.8 s -> 240.5 s. So the milestone is real and it is
smaller than the counter says: **1.87x, against 2.95x claimed.**

**Nine instances got slower while the counter said they got cheaper.**
`greenbea` loses 8% of its time on 24% less billed work; `pilot`, `pilot87`
and `pilot-we` lose 3-5%; `osa-30` and `osa-60` on Kennington gain nothing
from a 2x work reduction. Every one of them is a model where the pricing row
is dense, so the sparse machinery is collected and thrown away.

**Which entry bought what, timed separately.** Each threshold turned off and
on over a panel spanning both regimes:

| | dense | sparse | Kennington | all |
|---|---|---|---|---|
| `SPARSE_ALPHA_DEN` = 4 (D40, D41) | 1.009x | 1.280x | **1.494x** | **1.245x** |
| `SPARSE_RHO_DEN` = 4 (D43) | 0.999x | 1.006x | 1.019x | 1.008x |
| `SPARSE_COL_DEN` = 2 (D44) | **0.987x** | 1.023x | 1.016x | 1.008x |
| `SPARSE_COL_DEN` = 8 | 0.992x | 1.012x | **1.030x** | **1.012x** |

**D40 and D41 delivered nearly all of the real speed. D42, D43 and D44
delivered about 5% between them**, against work-unit claims of 1.208x,
1.255x and 1.557x on Kennington.

**The mechanism, and it is not the one this entry first reached for.** The
tempting reading is sequential against random access. The sharper one is
**how much real work sits behind a billed unit**. D40 removed sweeps over
`nvar` in the ratio test and the dual update: a `status[]` byte, a `double`,
a branch and a call, per unit. D42, D43 and D44 removed sweeps over `nrow`
that are a contiguous `a[i] -= b * c[i]` — about the cheapest thing the
counter ever charges one for. Both are billed at 1.

**So the attribution table ranks targets by a metric biased against the LU.**
PLAN 3.2 puts 77.8% of the standard set inside the factorization and the
triangular solves, and those are the units with the most real work behind
them — random access through a factor. The dense sweeps that M2 has spent
nine entries removing were the cheap ones. That the standard set has moved
1.104x in units and 1.022x in seconds while nothing has touched its LU is
the same fact stated twice.

**What changes.**

- **`SPARSE_COL_DEN` goes from 2 to 8**, which is the first constant here
  whose value contradicts its own work-unit sweep — that sweep was monotone
  to "always" and wanted 1.
- **`SPARSE_RHO_DEN` and `SPARSE_ALPHA_DEN` stay at 4**, now confirmed
  against a clock rather than argued.
- **A change is judged on three things from here**: digests for correctness,
  work units for determinism and cross-machine comparability, and a
  same-instance time ratio to catch the `greenbea` case. Two of the three
  were already the rule; the third is what this entry adds, and it is
  available today on a host D17 excludes for absolute figures, because a
  ratio is not an absolute figure.

**What this does not license.** No number here may be published or compared
against another machine. WSL2 is a virtual machine with a memory balloon and
the Windows scheduler underneath, and the ~13x unit-rate spread above is
partly the machine and partly the models — this cannot say how much of
each. Q4's host is still what separates them, and the competitive gap M2's
gate asks for still needs it.

## D46 — The factorization's fill is measured, the pivot search is confirmed at 4, and a set total is two instances

PLAN 3.3's third item has been the largest untouched entry in M2 since the
milestone opened: the factorization and the scatter its factors then cost
every solve, 77.8% of the standard set's billed work, and D45 had just said
those are the units with the most real work behind them. Nothing in that
item had a number. This measures it.

**The instrument.** A throwaway build counts, per instance: nonzeros in
each basis as loaded, nonzeros in the finished L and U, pivots classified as
column singleton / row singleton / Markowitz, fill-in events, entries the
pivot search reads, updates and etas. One process per instance, both sets in
parallel. It validates itself the way PLAN 3.5 demands — total work comes
out at 59,511,697,769 on the standard set and 2,540,766,911 on the
infeasible one, both equal to the committed baselines to the digit.

**What the factorization actually costs.**

| | standard 94 | Kennington 16 | infeasible 29 |
|---|---|---|---|
| fill ratio, (L+U)/B | **2.673** | **1.026** | 1.239 |
| column singletons | 60.3% | **95.3%** | — |
| row singletons | 8.0% | 2.0% | — |
| Markowitz pivots | 31.8% | 2.7% | — |

Two thirds of every factorization is triangularization that costs nothing,
and on Kennington it is nearly the whole of it — 95.3% of its 238 million
pivots are column singletons and its factors carry 2.6% more nonzeros than
the basis does. **That is why its LU is 4.97% of its work**, and it is a
structural reason rather than an attribution: there is no fill there to
remove because there is no fill there to begin with.

**The hypothesis this was aimed at, and its refutation.** `find_pivot`
settles for the best of `PIVOT_SEARCH_LIMIT = 4` candidate columns, a
constant carried since M1 on "four is the classic compromise" and never
measured. If the search were under-powered, widening it would cut fill and
fill would cut work. Swept over 1, 2, 4, 8, 16 and 32 on the standard set
and 2, 4, 8, 16 on the infeasible one:

| psl | std work | std fill | infeas work | infeas fill |
|---|---|---|---|---|
| 1 | 127.7e9 | 3.447 | — | — |
| 2 | 63.3e9 | 2.472 | 2.729e9 | 1.274 |
| **4** | **59.5e9** | **2.673** | **2.541e9** | **1.239** |
| 8 | 56.6e9 | 2.588 | 1.869e9 | 1.219 |
| 16 | 92.0e9 | 2.895 | 2.535e9 | 1.189 |
| 32 | 83.8e9 | 2.720 | — | — |

**One candidate is genuinely bad** — 3.447 fill and 2.1x the work — so the
search does need to look around. From 2 upwards the fill moves within 1.2%
of itself while the totals swing by 60%, and the causal chain breaks
outright on the infeasible set: **psl=16 produces the lowest fill of any
setting there and costs 5.8% more work per instance than psl=4 does**, on
0.35% less fill. What moves is the trajectory, not the factorization. Per
instance at psl=8 against 4: `25fv47` 4.55x cheaper,
`greenbea` 2.78x, `perold` 3.29x — and `grow22` 7.7x dearer on 7.3x the
iterations, `grow7` 3.0x, `nesm` 1.65x. Iteration counts move by factors of
seven in both directions.

So the geometric mean of per-instance work ratios is 1.019x at psl=8 and
1.021x at 16, against a fill improvement of 1.15%. **`PIVOT_SEARCH_LIMIT`
stays at 4**, now on a measurement rather than on a phrase, and the
factorization's fill is not an ordering problem this constant can reach.

**`col_max_abs` is refused for a second reason, and it is the load-bearing
one.** PLAN 2.11 has it as a scan that could be cached. The scan is
209,866,212 entries over the standard set, against about 6.3e9 elimination
operations — under 7%, and a scan step is cheaper than an axpy. But the
cost is not why it stays: **a column's largest live magnitude changes
without the column being written**, because `row_done` retires entries the
column still holds. A cache refreshed where the elimination rewrites a
column would be stale for every column that merely lost a row, and a wrong
pivot magnitude is a stability decision made on a lie. The entry closes on
that, not on the percentage.

**And the finding that outlives all of the above: a set total is two
instances.** Over the standard 94, `pilot87` is 38.8% of the work and
`maros-r7` 35.4% — **74.1% between them**, 83.3% with `pilot`, 95.0% for
eight of the ninety-four. The infeasible 29 is worse: **`gosh` alone is
91.9%**. Kennington is `ken-18`.

Every "total work over the set" figure in PLAN 3 is therefore a weighted
statement about three or four models, and this is the third time that has
bitten: D39 found intervals that looked cheaper only because `pilot87` had
dropped out of the total by failing, PLAN 3.2's ranking was three changes
stale when it was last used, and the sweep above reads as a 1.051x win on
the standard set's sum and a 1.019x wash on its geometric mean. **A change
that does not move every instance in the same direction is reported as a
geometric mean of per-instance ratios from here**, with the sum kept for
what it is good for — comparing a tree against its own baseline instance by
instance. `bench/compare/README.md` already committed to the geometric mean
for the competitive gate; this is the same rule turned inward.

**What this leaves open.** The fill is 2.673 and this says only that the
candidate limit will not reduce it. It does not say the ordering is good —
that needs a comparison against a published figure for the same bases, or
against the two structural changes PLAN 2.11 still carries on the
factorization path. What it does close is the cheap experiment, which is
the one that would otherwise have been run again in six months.

## D47 — A reduced cost is a rate, and the checker certifies a bound it cannot prove

Q12 has held two failure modes since D39: `pilot` and `pilot87` fall outside
objective tolerance or get checker-rejected at several refactorization
intervals. This closes the first of them with a mechanism, and it is not a
tolerance.

**The failure, reproduced.** Sweeping `REFACTOR_EVERY` over 16, 24, 32, 48,
64 and 96 with everything else untouched:

| interval | `pilot` | `pilot87` |
|---|---|---|
| 16 | ok | ok |
| 24 | **objective out of tolerance**, checker green | checker REJECTED, dual 2.21e-4 |
| 32 | **objective out of tolerance**, checker green | checker REJECTED, dual 1.69e-6 |
| 48 | **`JAOS_ERR_INVALID_INPUT` from `jaos_solve`** | ok |
| 64 | ok | ok |
| 96 | **objective out of tolerance**, checker green | ok |

At 24 and 32 `pilot` stops at the same point to thirteen digits —
-557.48869037164388 and -557.48869037164343 — from two different
trajectories. That is a vertex, not drift.

**What that vertex is.** Comparing it against the accepted answer at 64,
column by column: column 1534 rests at its lower bound of 0 with reduced
cost **-3.474054690510639e-07** and **no upper bound**, and at the true
optimum it is **2990.3712634284152**. Column 1407 does the mirror image.
The product is

```
3.474054690510639e-07 x 2990.3712634284152 = 1.03887e-3
observed objective error                   = 1.03891e-3
```

**So the defect is that a reduced cost is a rate and is judged against an
absolute tolerance.** What a wrong-signed reduced cost can buy is the rate
times the distance the variable travels, and where the variable has no bound
on that side the distance is set by the constraints rather than by the box —
2990 here. A multiplier three times under the checker's 1e-6 threshold buys
a thousand times it. This is D23's lesson in the dual space: D23 was a
bound-proximity test judged absolutely on a row that cancels ten orders of
magnitude, and this is a sign condition judged absolutely on a rate whose
lever arm is unbounded.

**What was ruled out, and how.** The reference is right — JAOS itself
reaches Koch's value to 4e-11 at intervals 16 and 64. It is not a
measurement artefact: the same saved answer judged at thresholds from 1e-4
down to 1e-14 reports the same 3.474e-07 dual violation, appearing at 1e-7
and never shrinking. It is not the primal residue either: `col` is 4.918e-08
at 16, which is correct, and **0** at 96, which is wrong.

**The checker cannot see it, for a second and independent reason.**
`sign_condition` handles a wrong-signed multiplier on an infinite bound by
returning early — before contributing anything to the dual objective. The
term it skips is not small, it is minus infinity: the dual objective of a
variable free in the improving direction is unbounded below. What the
checker then compares against the primal objective is the dual objective of
a *different* problem, one where that variable has a finite bound. So
`jaos.h`'s documented guarantee, `P - P* <= gap_positive`, is not merely
loose. It is false, and `check.c`'s own claim that `|pos - neg|` reaches the
gap "along an independent route" fails at the same line.

**Built small, because it can be.** Two variables and one constraint:
minimise `-1e-7 x2` subject to `x1 + x2 <= 1e6`, both variables non-negative
and unbounded above. The optimum is `-0.1`. Offer the checker the origin
with a zero row dual, whose objective is 0:

```
checker primal_feasible    yes
checker dual_feasible      yes
checker max_dual_violation 0
checker objective_gap      0
checker gap_positive       0        <- and the true suboptimality is 0.1
```

The suboptimality is 1e5 times the tolerance and every number the checker
reports is zero. Raising the row's bound raises the suboptimality without
limit and changes not one of them. **The independent checker accepts an
arbitrarily suboptimal point**, and the only reason the gate has never
caught fire on it is Koch's reference values — which PLAN 2.10 already
names as the one thing in the milestone that does not come from JAOS. It
was right for a reason nobody had measured.

**How live this is.** Over the 110 accepted answers of the standard and
Kennington sets at the committed interval, 15 carry at least one dropped
term above 1e-12, and **`etamacro` carries one at 2.25e-07** — the same
order as the one that cost `pilot` 1.04e-3, in an answer the gate passes
today. The others are roundoff: `scsd6` at 3.0e-08, `pilot87` at 1.43e-08,
`pilot` at 8.62e-09, and everything else below 1e-9.

**What this does not close.** Two of Q12's modes remain open and both are
now reproducible: `pilot87` checker-rejected at 24 and 32 on dual violations
the checker *does* catch, which is a different fault, and `pilot` returning
`JAOS_ERR_INVALID_INPUT` from `jaos_solve` at 48 — a library error on a
model that solves at every other interval, which is the worst-shaped of the
three. Neither is diagnosed here.

**The obvious repair was measured, and it does not work.** The scale a
reduced cost should be judged against looks like the traffic of the dot
product that formed it, `|c_j| + sum |a_ij y_i|` — the same move D23 made for
rows, and the file already argues for it there. Measured on the offending
columns:

| answer | column | reduced cost | traffic | ratio |
|---|---|---|---|---|
| `pilot` at 32 — **the wrong one** | 1534 | -3.474e-07 | 2.400e-03 | **1.448e-04** |
| `pilot` at 64 — accepted | 1407 | -8.619e-09 | 6.342e-03 | 1.359e-06 |
| `pilot87` at 64 — accepted | 3406 | -1.432e-08 | 2.573e-05 | **5.565e-04** |
| `etamacro` at 64 — accepted | 498, 511 | -2e-09, -1e-09 | 2e-09, 1e-09 | **1.0** |

**It separates nothing.** The accepted answer for `pilot87` scores four
times worse than the rejected one for `pilot`, and `etamacro` scores 1.0 —
its reduced cost is a single well-determined term with no cancellation at
all. Any threshold that catches the wrong answer rejects two the gate
accepts today.

**And the reason generalises past this one test.** A backward-error ratio
asks whether the reduced cost is distinguishable from zero as a computation,
and in every row above it is: these are genuine small nonzero reduced costs,
not rounding noise. What separates a harmful one from a harmless one is the
distance the variable travels, which is a property of the whole polytope and
not of the column. **No local test on a reduced cost can do it**, and that
is why the absolute threshold has survived: the alternatives that look
principled are not better, they are differently arbitrary.

That leaves two honest routes, and choosing between them is the open work.
**Report the bound as void** — when a wrong-signed multiplier sits on an
unbounded improving direction the dual objective is minus infinity and
`gap_positive` is `+inf`, which costs nothing to compute and would say
plainly that JAOS cannot certify optimality on 15 of its 110 accepted
answers rather than reporting a small number instead. Or **compute what the
column is worth**: `|d_j|` times the step a ratio test allows is a certified
*lower* bound on the suboptimality, it needs no reference value, and it is
1.04e-3 on `pilot` at 32 by the arithmetic at the top of this entry. That
one needs `B^-1 a_j`, so the independent checker would need a basis and a
factorization of its own — a real cost, against the only thing measured here
that would actually catch the defect.

What this entry settles is that the case exists, that it is live on
`etamacro` today, and that the cheap repair is refuted.

## D48 — One loop pivoted without asking whether the factorization still existed

Q12's third mode, and the only one of the four that was a plain defect
rather than a question about what a tolerance means: `pilot` at a
refactorization interval of 48 came back as `JAOS_ERR_INVALID_INPUT` out of
`jaos_solve`, on a model that solves at every other interval.

**Where it came from.** All ten of that status's return sites are in `lu.c`,
so a throwaway build tagged each one. The one that fired is
`jm_lu_update`'s `lu->rank != lu->dim` — the factorization had already been
marked unusable before the call, and the trace shows the previous update was
the first after a rebuild.

**What that means.** `jm_lu_update` marks a factorization it has half
rewritten as unusable and returns `JAOS_ERR_NUMERICAL`; `pivot()` turns that
into `s->needs_refactor = true` and `JAOS_OK`, because the dual method's
loop reads that flag before every iteration. **`primal_cleanup` is the one
loop in the solver that pivots without going through it.** So after a failed
update it carried on to the next candidate, and everything it does reads the
factorization: `jm_lu_ftran` and `jm_lu_btran` both return without writing a
value once `rank != dim`, so the ratio test ran on a stale buffer, the
pricing row was priced from a `rho` that still held the unit vector it was
seeded with, and `pivot()` rewrote `basis`, `status` and `where` from the
result. The update's own guard was the only thing that stopped it, and it
stopped it by reporting a caller error.

**So the error was the backstop, not the defect.** No wrong answer escapes
this path today — the guard fires before the corrupted basis can be read,
and the caller refreshes whenever the loop took a pivot, so the one exit
that skips the rebuild (`pivots == 0`) is unreachable after an update has
failed. What escapes is a library error on a valid model, which is the one
thing `JAOS_ERR_INVALID_INPUT` is documented not to mean.

**The repair** is to leave the loop when the flag is raised. The remaining
candidates are not lost: the caller rebuilds and the outer round re-derives
the candidate set from the new point, which is what D30 established the
candidate set is — a snapshot of one point, never re-asked mid-loop.

**Evidence, and it is the sharpest shape this kind of change can take.** The
path only runs where an update has failed inside the cleanup, and today
every such solve ends in the error above, so no instance that passes can be
taking it. That is a prediction rather than a hope, and it holds: **all 139
reference instances come back identical to the committed record — status,
objective, iteration count, work units, every checker figure and every
solution digest.** The suite is 130 tests green.

And on the sweep that found it, of twelve cells exactly one moved:

| | 16 | 24 | 32 | **48** | 64 | 96 |
|---|---|---|---|---|---|---|
| `pilot` | = | = | = | **error -> optimal** | = | = |
| `pilot87` | = | = | = | = | = | = |

**What it does not fix, deliberately.** `pilot` at 48 now completes, and
what it completes to is outside objective tolerance with the checker green —
which is D47's mode, reached by a fourth trajectory. Converting a library
error into a wrong answer the gate catches is the whole of what this entry
claims.

**The hazard underneath is still a sentence.** `jm_lu_ftran` and
`jm_lu_btran` returning quietly on a wrecked factorization is documented in
a comment and enforced by nothing, and this defect is what that costs: a
caller that forgets computes with whatever the buffer held and finds out
later, somewhere else. The rule this project keeps relearning is that a
contract that matters must be an assert, a test or a structural
impossibility. Making these two report rather than return is a wider change
than this one and is not made here.

**And the case that proves it is not in the suite.** It is a bench instance
at a non-default constant, which no unit test reaches — the trajectory sweep
of PLAN 3.6 is the only instrument that runs it, and it has now found two
defects (D39, this one) that 139 instances at one setting did not.

## D49 — The re-entry loop stops making progress and the round cap is what ends it

Q12's fourth mode: `pilot87` is checker-rejected at refactorization intervals
24 and 32, on dual violations of 2.21e-4 and 1.69e-6 — four orders above the
tolerance, and unlike D47's mode the checker sees these perfectly well. It
also costs 116071 iterations there against 50850 at the committed interval.

**The trajectory, which is what a snapshot would have hidden.** One line per
settle round: how many variables breach a sign condition, the worst of them,
how many the cleanup wants to pivot, and whether anything can be repaired by
moving.

At interval 64 the loop converges. Sixteen rounds, the worst breach falling
from 7.95e-4 to zero, and it exits because the cleanup finds nothing left to
pivot:

```
ROUND  0  worst=0.000795338  breaching=48  wants=33
ROUND  6  worst=8.3257e-06   breaching=12  wants=12   cleanup pivots=9
ROUND 14  worst=8.53876e-06  breaching=2   wants=2    cleanup pivots=2
ROUND 15  worst=0            breaching=0   wants=0    EXIT
```

At interval 24 it does not. From round 12 the figures repeat with period
four, five times over, and the cap stops it:

```
ROUND 12  worst=0.000110302  breaching=6  wants=3
ROUND 13  worst=1.87911e-06  breaching=4  wants=4   cleanup pivots=3
ROUND 14  worst=2.74024e-05  breaching=5  wants=3
ROUND 15  worst=7.84605e-07  breaching=3  wants=3   cleanup pivots=2
ROUND 16  worst=0.000110302  breaching=6  wants=3      <- round 12 again
...  rounds 20, 24 and 28 the same, 56 iterations per turn ...
EXIT: SETTLE_ROUNDS cap of 32 rounds bound
```

**And the obvious repair is refuted before being written.** D26 cured
`grow15` by detecting a cycle and switching to Bland's rule, and detecting a
cycle means comparing a state hash. Hashing the basis and every variable's
status here gives **a different value in every one of the thirty-two
rounds** — including the five that agree on every figure above. So the loop
is not revisiting a basis. It is revisiting the same breaches by a different
route each time, which is degeneracy, and a cycle detector keyed on the
basis would run past it exactly as the loop does.

**What is established.** The loop makes no measurable progress after round
11, spends the remaining twenty rounds not making it, and terminates on a
constant rather than on a condition. `SETTLE_ROUNDS = 32` is doing the job
a convergence test should be doing, which is the anti-pattern D26 named at
the level of a simplex iteration and this is at the level of a round.

**What is not, and it is the more interesting half.** The loop's own worst
breach at the last round is **7.85e-07** and the checker's verdict on the
point it publishes is **2.21e-4** — a factor of 280. Those are different
quantities in different spaces: `dual_breach` measures a shifted cost in the
scaled problem, and the checker measures a reduced cost in the original one.
Until that factor is attributed, no threshold in this loop can be read as
meaning what it appears to mean, and stopping the loop earlier or later
cannot be judged. That is D27's fault class — a test reading the right
quantity in the wrong space — and it is the next question rather than a
conclusion here.

Nothing is changed in this entry. Both remaining halves of Q12 now have a
mechanism named and a specific next measurement, which is what the sweep of
PLAN 3.6 exists to produce.

## D50 — Two repairs undo each other, and the loop publishes whichever one it stopped on

D49 left `pilot87`'s non-convergence at interval 24 as a repeating pattern
with no repeating basis. Logging what each round does, and why each cleanup
candidate is or is not acted on, says what the pattern is.

**The rounds alternate, strictly.** `anything_to_move` decides which of two
repairs a round performs: move a breaching nonbasic to its other bound and
let the dual simplex run again, or — when nothing can be moved — pivot with
`primal_cleanup`. Through the whole cycle those alternate one for one, and
the period of four is two such pairs.

**And the two levels of residue belong to the two repairs.**

| round | repair | worst breach |
|---|---|---|
| 12 | move | 1.10302e-04 |
| 13 | pivot, 3 taken | 1.87911e-06 |
| 14 | move | 2.74024e-05 |
| 15 | pivot, 2 taken | 7.84605e-07 |
| 16 | move | 1.10302e-04 |

**Pivoting reduces the breach by two orders of magnitude and moving puts it
back.** That is the whole of the cycle, repeated five times until
`SETTLE_ROUNDS` stops it.

**Nothing structural repeats, which is why no detector would find it.** The
columns differ every round — 178, 280, 4513, 5940 in one cleanup and 179,
181, 3082 in the next — and D49's basis hash differs in all thirty-two. Each
cleanup pass also does its own job correctly: the one candidate it declines
in each of those passes is declined because an earlier pivot of the same
pass genuinely cleared its breach, which is the re-check D30 installed
working as intended. **What repeats is the level of the residue and nothing
else.** At interval 64 the same alternation converges to zero in sixteen
rounds, so this is not the mechanism being wrong — it is the mechanism not
terminating on this trajectory.

**What this makes of an existing decision.** D25's loop accepts a round's
result "for being a second optimum, not for being a better one — nothing
here compares the two", and says plainly that the improvement is a
measurement across three instance sets rather than a property of the
construction. That measurement was taken on trajectories that converge. On
one that oscillates between a good level and a bad one, not comparing means
**the answer published is decided by where the round cap happens to fall**,
and a constant chosen for being generous is not a tie-break rule.

So the change worth measuring is keeping the best round rather than the
last. It moves published answers wherever a later round is worse than an
earlier one, so it needs the full gate and a decision about what "better"
means — the dual breach is in the solver's scaled space, and D27 has already
established that the quantity to compare is the one with no space, the term
the breach contributes to the duality gap.

**Still not attributed:** why a move re-creates a breach of the same size it
just cost two orders of magnitude to remove. That is the question underneath
this one, and nothing here answers it.

## D51 — The residue is the loan ledger

D50 left one question: why a move re-creates a breach the size the pivot just
removed. It is not drift and it is not rounding. **The worst breach a round
publishes is the largest cost that round borrowed.**

The dual simplex keeps dual feasibility by shifting a nonbasic's cost until
its reduced cost is zero, recording the loan; `settle_shifts` calls every
loan in at the end of a round and recomputes the duals from the model's own
costs. Instrumenting that moment on `pilot87` at interval 24:

| round | loans repaid | largest loan | worst breach that appears |
|---|---|---|---|
| 27 | 11 | 1.10301e-04 | **1.10302e-04** |
| 29 | 11 | 2.74015e-05 | **2.74023e-05** |
| 31 | 11 | 1.10301e-04 | **1.10302e-04** |

Same number, six digits, every time. And the loan is re-borrowed
immediately: round 28 opens by lending 1.10302e-04 back out — the figure it
was just handed.

So the cycle of D50 is a ledger that never clears. `pivot()` runs
`shift_to_feasible` over every variable, so **every cleanup pivot borrows in
order to repair**, and repaying is what creates the next round's work. The
loop converges only where the borrowing shrinks faster than the pivoting
repairs, which at interval 64 it does — the last round there repays a single
loan of 5.74e-08 and leaves nothing breaching — and at 24 it does not.

That reframes the repair. "Keep the best round" (D50) treats the symptom;
the question this raises is whether a cleanup pivot needs to borrow at all,
or whether the loan can be bounded by what the pivot is worth. Neither is
answered here, and both are cheaper to reason about now that the residue has
a name instead of being attributed to arithmetic.

## D52 — The first competitive measurement: 4.07x, and it is not the algorithm

M2's gate has asked for a measured gap against open solvers since the
milestone opened, and it had never been run. Q4 said the gate needs a
dedicated host; that is true of a *published* figure and it had become a
reason to keep optimising blind, so the host requirement is now a label on
every line rather than a blocker.

**The reading.** HiGHS 1.15.1, pinned by checksum and licence-verified at
fetch, at tier T0 — presolve off, dual simplex forced, one thread, no
crossover, tolerances equalised at 1e-7 on both sides. JAOS built with the
same flags HiGHS's own Release build gets, so the comparison is of solvers
and not of optimisation levels. Minimum of two runs. Standard set, 94 of 94
verified against Koch on both sides.

| | JAOS vs HiGHS |
|---|---|
| **time per solve** | **4.07x slower** |
| iterations | 1.50x |
| **time per iteration** | **2.72x** |

Geometric means of per-instance ratios over the 19 instances above a 0.05 s
floor — below it HiGHS reports its own time to two decimals and the ratio
divides by a rounded zero. JAOS is faster on 0 of those 19. Totals over the
verified set: 116.2 s against 14.9 s.

**So the gap is not the algorithm.** Over all 94 instances the iteration
ratio is 1.14x and **JAOS takes fewer iterations than HiGHS on 47 of them**.
The pricing rule, the ratio test and the pivot choice are competitive. What
costs four times as much is each iteration, and on the larger models JAOS
also needs half again as many.

**And the extremes say where.**

| | time | iterations | per iteration |
|---|---|---|---|
| `maros-r7` | 41.4x | 2.34x | **17.7x** |
| `fit2p` | 16.5x | 0.99x | **16.7x** |
| `stocfor3` | 6.2x | 0.97x | 6.4x |
| `pilot87` | 16.1x | 4.60x | 3.5x |
| `dfl001` | 2.0x | 1.14x | 1.8x |
| `truss` | 1.3x | 0.90x | 1.5x |

`fit2p` takes the same number of iterations as HiGHS and seventeen times as
long. `maros-r7` is the same story and is the highest-fill instance in the
set — 4.801 against a set average of 2.673 (D46). The per-iteration cost
spans 1.5x to 17.7x, and it tracks the factorization.

**What this changes.** PLAN's phase 6 ranked its targets by billed work
units, and D45 had already warned those are biased. This says the same thing
from outside and sharpens it: **the target is not fewer iterations, it is a
cheaper iteration**, and the LU is where the difference lives. The internal
attribution put 77.8% of the standard set inside the LU; an external clock
now agrees.

**What it does not license.** Not one number here may be published or
compared against another machine — it is WSL, and every line of the record
says so. What a ratio of the same instance against the same instance on the
same machine can say is what it says here, and Q4's host is still what would
turn it into a figure anyone else can check.

## D53 — Two rivals agree on what a JAOS iteration costs, and that makes it a number worth attacking

D52 measured against HiGHS alone. SoPlex 8.0.3 joins the same rung, and what
it adds is not a second opinion about the gap — it is the thing that turns
"per-iteration cost" from an arithmetic artefact of one comparison into a
property of JAOS.

| tier T0 | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 |
|---|---|---|
| instances above the 0.05 s floor | 19 | 23 |
| **time per solve** | **4.13x** | **1.42x** |
| iterations | 1.50x | **0.65x** |
| **time per iteration** | **2.76x** | **2.18x** |
| JAOS faster on | 0 of 19 | **10 of 23** |

**JAOS takes 35% fewer iterations than SoPlex** and is still 1.42x slower.
Against HiGHS it takes 50% more and is 4.13x slower. Both readings say the
same thing from opposite directions: the search is not the problem.

**And the per-iteration ratio is the same number whichever rival measures
it.** Per instance, HiGHS first and SoPlex second:

| | | |
|---|---|---|
| `maros-r7` | 17.3x | 17.0x |
| `fit2p` | 17.5x | 21.6x |
| `truss` | 1.5x | 1.5x |
| `degen3` | 2.1x | 2.0x |
| `25fv47` | 1.9x | 2.0x |
| `stocfor3` | 6.6x | 1.4x |

Two independently written solvers, different pricing rules, different ratio
tests, different factorizations — and they agree on how much more a JAOS
iteration costs on a given model. That is what makes this a quantity to
attack rather than a quotient that happened to come out of a division.

**Two regimes, not a spectrum.** `maros-r7` and `fit2p` sit at seventeen
times; everything else lies between 1.2x and 3.5x. `maros-r7` is the
highest-fill instance in the set at 4.801 against an average of 2.673 (D46).
Whatever the seventeen is, it is not the same thing as the two.

**What the iteration counts say on their own.** JAOS needs fewer iterations
than SoPlex on sixteen of the twenty-six timed instances, and on `fit1p` a
seventh of them. The dual steepest edge is doing its job. The exception is
`grow15`, where JAOS takes **21.7x** HiGHS's iterations and 9.9x SoPlex's —
that is the model D26 closed by adding Bland's rule to a detected cycle, and
this is the first measurement of what that costs.

**One thing that is not about JAOS.** SoPlex returns `pilot87` outside the
gate's objective tolerance at this rung, on the model PLAN calls the
worst-conditioned in the set. JAOS does not. Recorded because a comparison
that only ever finds its own solver wanting is not being read carefully.

**What it does not license.** Same as D52: WSL, development numbers, every
line of the record says so, and the caveat in `soplex-T0.args` stands —
SoPlex switches to the primal part-way through a solve and `-s0
--int:algorithm=1` does not stop it, so its rung is a lower bound on the gap
a fully dual-forced SoPlex would show.

## D54 — The seventeen is two different things, and only one of them is visible to the counter

D53 found two instances at seventeen times the per-iteration cost of a rival
and everything else between 1.2x and 3.5x, and PLAN said to find out what the
seventeen was before touching the two. Crossing the measured time per
iteration against what the work counter billed for that iteration answers it.

Over the 26 instances above the timing floor, in microseconds of real time
per thousand billed work units:

| | units per iteration | µs per 1000 units |
|---|---|---|
| `fit2p` | 146,449 | **11.686** |
| `stocfor3` | 44,987 | 7.154 |
| *median of the 26* | *40,406* | *1.895* |
| `maros-r7` | **2,007,710** | 1.790 |
| `truss` | 66,602 | 1.351 |
| `fit2d` | 555,432 | **0.795** |

**`maros-r7` is honest work.** It bills two million units an iteration, fifty
times the median, and its time per unit is *below* the median. Its iterations
really are that expensive and the counter says so. That is the factorization
— it is the highest-fill instance in the set at 4.801 (D46) — and fill
reduction is the lever.

**`fit2p` is not.** It bills an ordinary 146k an iteration and takes 11.7 µs
per thousand, six times the median. Something there costs real time and bills
almost nothing.

**And the general figure is the one that matters most.** The cost of a billed
unit spans **0.795 to 11.686 µs — 14.7x — across the timed set**. D45
measured 13x from the other direction and this ties it to the comparison: a
ranking of targets by work units can be wrong by an order of magnitude
depending on which instance carries the total, and two instances carry 74% of
this one (D46).

**What it is not.** Not the shape: `fit2d` is 420 columns per row and the
cheapest unit in the set, `truss` is 8.8 and near the bottom, while
`stocfor2` at 0.9 sits high. Not size alone either: `stocfor3` has five times
`fit2p`'s rows and a better figure. What the top of the column has in common
is sparse wide matrices whose entries scatter across many rows — units that
are cheap to bill and expensive to fetch — which is a cache story and exactly
the mechanism D45 named without being able to isolate it.

**So phase 6 has two targets, not one**, and they need different work: the
factorization for `maros-r7` and everything like it, and whatever `fit2p`
spends that nobody bills. The second is the one no internal instrument has
ever been able to see, which is why it took an external clock to find it.

## D55 — The shipping build paid 1.5x for a capacity check it could not inline

D54 left `fit2p` spending six times the median real time per billed work unit
with no internal instrument able to say why, and said the next question was
for a profiler rather than the counter. It was.

**The measurement.** callgrind on `fit2p` and on `truss` as a control, both
bounded to the same 100 million work units so the comparison is of equal
billed work:

| | instructions | per billed unit | D1 miss rate |
|---|---|---|---|
| `fit2p` | 48,753,100,116 | **487** | 1.5% |
| `truss` | 2,787,059,676 | 27.9 | 4.6% |

**17.5x the instructions for the same billed work, and better cache
behaviour.** So it was never cache — last-level misses are 0.0% on both. The
counter was simply not counting what `fit2p` does.

Where the instructions went, on `fit2p`: `jm_svec_push` 39.0%, `jm_lu_factor`
33.0%, **`jm_grow` 24.1%**. On `truss` the same two array functions are 1.06%
and 0.92%.

**What it is.** `jm_svec_push` calls `grow_pair`, which calls `jm_grow`
**twice on every append**, to be told there was capacity. `jm_grow` lives in
`util.c`, so across a translation unit boundary it cannot be inlined, and the
solver's hottest append pays two calls per element.

**What removing it bought, and where.**

| | `-O2`, the shipping build | `-O2 -flto` |
|---|---|---|
| `fit2p` | 12.869 s -> **8.419 s** (1.53x) | 8.655 -> 7.694 (1.13x) |
| `maros-r7` | 50.547 s -> **36.722 s** (1.38x) | 37.164 -> 34.518 (1.08x) |
| `25fv47` | 0.512 -> 0.489 (1.05x) | 0.491 -> 0.495 |
| `truss` | 1.617 -> 1.600 (1.01x) | 1.658 -> 1.627 |

All 139 reference instances are identical to the committed record — status,
objective, iterations, work units, every checker figure and every digest —
which is the expected outcome: nothing about the arithmetic changed.

**And the method mistake, because it is the transferable part.** The profile
was taken at `-O2` without LTO and the *first* attempt to confirm it was
timed at `-O3 -march=native -flto`, where the change is worth nothing at all
— the geometric mean over 26 instances came out 1.001x. Two different
binaries, and the conclusion was almost thrown away. The rule the debug
method already states and this ignored: **a diagnostic build's output is not
a result until a clean build of the same flags confirms it.**

**What it says about Q11.** `-flto` was on the candidate list for a `native`
build. Most of what it was worth here is now captured portably: after the
fix, LTO alone buys 1.087x on `fit2p`, 1.004x on `truss` and 1.003x on
`25fv47`. The case for adding it to the shipping build is weaker than it was
this morning, not stronger.

**What it does not change.** The comparison figures of D52 and D53 stand
exactly as measured: they build JAOS with the competitor's own flags, which
include `-flto`, so this change moves none of them. The gap against HiGHS is
still 4.13x. What moved is what a user of `libjaos.a` gets.

## D56 — The elimination rebuilt every column of every pivot row, including when there was nothing to eliminate

D55 removed 20 billion of `fit2p`'s 48.75 billion instructions and left the
rest where the profiler had put it: `jm_lu_factor` at 56% and `jm_svec_push`
at 37%. Line attribution says why. **`jm_svec_push` is called 344,189,600
times** for eleven factorizations of a 3000-row basis holding 37,504
nonzeros — about 830 appends per nonzero.

**What the elimination does.** For each pivot, for each column of the pivot
row, it scatters the column into a dense buffer, applies the pivot column's
multipliers, and pushes the survivors back. The cost is the length of the
column, twice, whatever the multipliers do.

**And on a triangular basis the multipliers do nothing.** `piv_row` holds the
live rows of the pivot column below the pivot; when the basis is already
triangular there are none. On `fit2p`, L holds 101 entries against 3000
pivots, so **97% of pivots have an empty `piv_row`** and the pass was
scattering and rebuilding each column to copy it onto itself.

**The repair** is to compact the column where it stands when `piv_n == 0`,
and it must be bit-identical: no value changes, so none can newly fall under
the drop tolerance — an entry only reaches that point having survived an
earlier compaction, which already applied that test — and what is dropped is
exactly what the general path drops, entries whose row is done, in the same
order.

**Measured.** All 139 reference instances identical to the committed record,
digest for digest, 130 tests green.

| | `-O2`, shipping | `-O2 -flto` |
|---|---|---|
| **`fit2p`** | 8.252 s -> **2.460 s** (3.35x) | 7.567 -> **2.402** (3.15x) |
| `stocfor3` | 6.064 -> 5.878 (1.03x) | 1.02x |
| `25fv47` | 0.495 -> 0.484 (1.02x) | 1.00x |
| `maros-r7` | 36.750 -> 36.151 (1.02x) | 1.01x |
| `truss` | 1.615 -> 1.596 (1.01x) | 1.00x |

Surgical, which is what the reasoning predicted: it can only help a basis
that is close to triangular, and `maros-r7` — the highest-fill instance in
the set — is exactly where it cannot.

**Unlike D55, LTO does not hide this one.** D55 was a call the compiler could
inline given the whole program; this is work that did not need doing at all,
and no optimiser can find that. The two together take `fit2p` from 12.869 s
to 2.460 s in the shipping build, **5.2x**, and its gap against HiGHS at tier
T0 from 16.5x to about 4.6x.

**What it says about the work counter, again.** None of this billed a unit.
`jm_work_add(w, JM_WORK_ELIMINATED)` is charged inside the multiplier loop,
which for these pivots ran zero times — so the counter recorded the cheap
factorization it should have been and the solver did something else entirely.
That is now three findings deep in the same direction (D45, D54, this): the
counter measures the algorithm, not the program.

## D57 — The gate runs its instances at once, because nothing it records is a second

Half an hour per measurement was breaking the work, and the fix had been
sitting in the plan since M2 opened (PLAN 1.2) without being taken.

**Why it is safe, and it is the whole argument.** Everything `bench/run.c`
writes to a record is an integer the solver computed: the verdict, the shape,
the iteration count, the work units, the solution digest. Not one of them can
be moved by what else the machine is doing, and the instances do not depend
on each other. So `-j N` forks one worker per instance, N of them alive at a
time, and the parent reads their results back **in manifest order** — the
console, the record file and the baseline all come out in the order they came
out in before this existed.

**Checked rather than asserted.** Every one of the 139 lines is byte-identical
to the committed record, which was produced sequentially. The only difference
in any of the three files is the last line, where these runs were given a
baseline to compare against and the committed records were written by the
`-baseline` targets, which are not.

| set | sequential | `-j` | |
|---|---|---|---|
| standard 94 | ~8 min | **84 s** | `-j 10` |
| infeasible 29 | ~2 min | **9 s** | `-j 10` |
| Kennington 16 | ~30 min | **8 min 21 s** | `-j 6` |

Kennington gains the least and cannot gain more: `ken-18` alone takes 261 s
of the 501, so the set is bounded by its slowest instance however many cores
are thrown at it. The other two are bounded by the core count, which is what
`-j 10` on six cores is already exploiting.

**What it does invalidate is the seconds**, and the runner says so on the
console every time N > 1 rather than leaving it to be remembered. Concurrent
solves compete for cache and memory bandwidth, so each instance's time is
inflated by an amount this program cannot know. The record is untouched — it
is integers — but the time ratio D45 made one of the three things a change is
judged on has to come from `J=1`. A campaign that measures work units in
parallel and then times one instance sequentially gets both, which is the
intended shape.

**A worker that dies is not an instance that passed.** If a process exits
non-zero, is killed by a signal, or leaves no usable result, its instance is
recorded `WORKER-FAILED` and the run fails. The alternative — a missing line
— reads as a set that was never run.

**And the gate was made to fail three ways before this was believed**, because
139 green instances are exactly what a gate that stopped checking also
produces:

| built to be rejected | under `-j` |
|---|---|
| an instance costing 4x its baseline work | `REGRESSED work: 4973 -> 19894 (4.0x)`, exit 1 — same line as `-j 1` |
| an instance whose file cannot be read | `READ-FAILED`, `gate: NOT MET`, exit 1 |
| a worker killed before it writes anything | `WORKER-FAILED`, `gate: NOT MET`, exit 1 |

The third needed the defect injected into a throwaway copy of the runner,
which is the only way to find out whether that path works at all.

**Not taken: the flags.** The runner could also be built `-O3 -march=native
-flto`, which is where the remaining minute of the standard set is. That is
two changes measured at once, and which flags a build carries is Q11's
question. The parallelism is worth having on its own and is portable, which
`-march=native` by construction is not.
## D58 — The elimination asked for capacity once per entry it wrote, and the entries are billions

PLAN's ranking put the factorization first and said `maros-r7` was the
instance to read it on: 50x the median billed units per iteration at a
*below*-median cost per unit, which reads as work the counter can see. The
profile says otherwise, and it is D55 and D56's finding a third time.

**The measurement.** callgrind at the flags the library ships with — `-O2
-g -DNDEBUG`, no LTO — on `maros-r7` bounded to 5e9 work units, which is
4,876 of its 10,479 iterations, with `truss` as the control:

| | `maros-r7` | `truss` |
|---|---|---|
| `jm_lu_factor` | **49.41%** | 1.55% |
| `jm_svec_push` | **25.24%** | below 0.8% |
| the two triangular solves | 15.19% | 16.42% |
| `simplex.c:run` | 2.38% | 30.13% |

**Three quarters of `maros-r7` is one function and the array append it
calls.** By source line, inside the elimination:

| line | share | |
|---|---|---|
| `jm_svec_push(cv, i, v)` in the column rebuild | **24.66%** | **1,552,126,296 calls** |
| the drop test, the load, the loop, the counter | ~15% | the rest of the rebuild |
| the scatter into `work` | ~11% | |
| **`work[i] -= delta`** | **2.87%** | **the only line here that bills a unit** |

28 instructions per append, and the append writes two words.

**What it is.** The rebuild knows `nt` — the number of touched rows — before
it starts, and every entry it writes comes from a distinct one, so `nt`
bounds it exactly. It was asking `jm_svec_push` to check capacity on every
single entry anyway, through a call the compiler does not inline because the
function is external. Asking once per column instead of once per entry is the
whole change.

**Bit-identical, and it has to be.** Same values, same order, same drop test,
same `row_cnt` decrements. What moves is where the capacity check happens.
The failure mode it removes is also real: a push failing halfway through left
a column half-rebuilt, and the reservation now fails before anything is
written.

**A hypothesis the profile refuted, which is why it was profiled.** The
obvious suspect was `compact_pivot_row`, which scans a whole column to find
one row's entry and is O(r·c) per pivot — the "missing row-to-position
lookup" PLAN named as a live structural item. It does not appear at all: it
is under 0.5% on the instance built to expose it. Measure before repairing.

**Measured.** All 139 reference instances identical to the committed records
— status, objective, iterations, work units, every checker figure and every
digest. 130 tests green. Sequential timing, shipping flags, minimum of three
runs each, against a build of the previous commit:

| | before | after | |
|---|---|---|---|
| **`maros-r7`** | 35.512 s | **27.711 s** | **1.281x** |
| **`pilot87`** | 32.233 s | **27.562 s** | **1.169x** |
| `pilot` | 7.376 | 6.621 | 1.114x |
| `25fv47` | 0.482 | 0.473 | 1.020x |
| `80bau3b` | 0.204 | 0.200 | 1.017x |
| `fit2p` | 2.374 | 2.350 | 1.010x |
| `dfl001`, `stocfor3`, `ken-13`, `greenbea`, `d2q06c`, `truss` | | | 1.009x down to 0.993x |

**Geometric mean over the twelve: 1.048x**, and that number is the honest one
to lead with — but it is not what the change is worth. The two instances it
moves are `pilot87` and `maros-r7`, which D46 measured as **74.1% of the
standard set's total work between them**. A ranking by geometric mean and a
ranking by where the work is disagree here, and both are reported rather than
whichever flatters the change.

Where it does nothing is where the reasoning said it would: the saving is one
capacity check per entry rebuilt, so it scales with fill, and the instances
with little of it — `truss` at 0.993x, `d2q06c` at 0.997x — sit inside the
measurement noise in both directions.

The profile predicted 25% of instructions; the clock returned 21.9% of
`maros-r7`'s time. Close enough to say the profile was reading the right
thing.

**What it does not say.** The 16.5x per-iteration gap against HiGHS that PLAN
carries for `maros-r7` was measured by the comparison harness, which builds
JAOS with the competitor's flags. This changes the shipping build; whether it
moves that figure is a question for `make compare`, not for arithmetic on
this table.
## D59 — The multipliers belong to the pivot, so the column stops being copied twice to meet them

D58 removed the append and re-attributed what was left. On `maros-r7`, with
the same 5e9 billed units and the same 4,876 iterations, the program had gone
from 176.7 to 129.9 billion instructions and the shape of what remained was
plain:

| inside the elimination | share of the program |
|---|---|
| scattering the column into `work` | 17.9% |
| gathering it back out | 24.0% |
| **the subtraction the two exist to carry** | **6.5%** |

**Two copies of every entry to perform an arithmetic that touches at most
`piv_n` of them.** The copies are there because the multipliers were being
met in a dense buffer — the column was spread out so that `piv_row` could be
walked against it.

**It is the wrong way round.** The multipliers belong to the pivot, not to
any one column: the same `piv_n` values are applied to every column of the
pivot row. Scattering *them* costs `piv_n` once per pivot; scattering the
column costs its whole length once per column. So the column is walked once,
in place, and the rows the pivot updates that it did not carry — its fill —
are appended after it.

**The order is the invariant, and it is preserved exactly.** The gather
emitted the column's surviving entries in their existing order and then the
fill in `piv_row` order, because that is the order it appended them to
`touched`. The single walk emits the same two runs in the same two orders.
Every value is the same subtraction of the same product: `v - mult*urow`,
with `mult` copied from `piv_mult` rather than read from it.

**And the same is true of what is charged.** The two-pass form billed one
`JM_WORK_ELIMINATED` per multiplier per column, inside the loop. The single
walk bills `piv_n` of them per column, in one call. A work counter that moved
here would mean the arrangement had changed what the elimination does, which
is the thing being denied.

**Measured.** All 139 reference instances identical to the committed records,
digest for digest, work unit for work unit. 130 tests green, and green under
ASan and UBSan — which this change earns rather than assumes, because it
writes the column it is reading and appends past the end of what was there.

| | before | after | |
|---|---|---|---|
| **`maros-r7`** | 26.965 s | **22.635 s** | **1.191x** |
| `pilot87` | 28.157 | 26.550 | 1.061x |
| `greenbea` | 2.209 | 2.106 | 1.049x |
| `d2q06c` | 0.998 | 0.964 | 1.034x |
| `pilot` | 6.687 | 6.503 | 1.028x |
| `fit2p` | 2.402 | 2.340 | 1.026x |
| `ken-13`, `dfl001`, `stocfor3`, `truss`, `25fv47`, `80bau3b` | | | 1.007x down to 0.992x |

**Geometric mean over the twelve: 1.030x.** `maros-r7` gains three times what
`pilot87` does, and that is the shape the reasoning predicts: what is saved is
one copy of each column per pivot of its row, so it scales with how long the
columns are against how many multipliers meet them. `maros-r7` carries the
highest fill in the set.

**With D58, the two together.** `maros-r7` 35.512 s -> 22.635 s, **1.569x**,
and `pilot87` 32.233 -> 26.550, **1.214x** — the two instances D46 measured
as 74.1% of the standard set's work. Neither moved a digest, an iteration
count or a work unit.

**What the counter said about all of it: nothing.** Both changes are
invisible to it by construction, and both were found by profiling the build
that ships. That is now four in a row (D45, D54, D55/D56, this), and the
conclusion has stopped being a surprise: **the work counter measures the
algorithm, and the algorithm was never what was wrong.**

## D60 — The comparison rebuilt its solver only when its own driver changed, so it measured last night's

The gap against HiGHS was re-measured after D58 and D59 — the two entries
that had just taken `maros-r7` from 35.5 s to 22.6 s — and it came back
**3.86x against a committed 3.81x**. A tree 1.5x faster on its heaviest
instance, and the comparison could not see it.

**What it was.** `run-compare.sh` builds its own JAOS with the competitors'
flags (`-O3 -march=native -flto`), because a comparison of solvers must not
become a comparison of optimisation levels. It rebuilt that binary on one
condition:

```
[ ! -x "$jaos" ] || [ "$here/jaos_time.c" -nt "$jaos" ]
```

**The driver, and nothing the driver is made of.** `jaos_time.c` had not
changed since 2026-08-09; the binary was stamped 00:16:56 and `src/lu.c`
08:10:44. Eight hours of solver went unmeasured, and the record that came out
was indistinguishable in every respect from a measurement of the tree that
produced it.

**Why the committed record survives.** The binary was last built at 00:16:56,
eleven seconds after D56 landed at 00:16:45 — so `bench/compare/results/T0.txt`
does describe the tree it claims to. That is luck, not design: it happened
because something had removed the binary that evening, not because anything
checked.

**The repair, in two parts.** The staleness test now covers every source the
binary is made of and names the one that triggered it, so a rebuild is
announced rather than silent. And the record carries the commit it was taken
from, plus a `WITH UNCOMMITTED CHANGES` marker — because a comparison is only
ever read next to another comparison, and two records that do not say what
they measured cannot be subtracted.

**What the failed run is worth, since it was paid for.** It is the same
binary as the committed record, run a day later: 3.81x against 3.86x, 2.60x
against 2.57x per iteration. **The harness repeats itself to about 1.3%**,
which is the noise floor any future claim about the gap has to clear. Nothing
else in this repository had measured that.

**And the general rule, which is D17 arriving somewhere new.** Every other
instrument here refuses to run rather than run on the wrong thing: the gate
errors if a baseline is missing, the runner writes `NOT COMPARED` into the
record when it had nothing to compare against. This one silently used what it
found. A measurement tool has to make being wrong noisy.

**The gap, re-measured on the tree that earned it.**

| tier T0 | committed record | after D58 and D59 |
|---|---|---|
| vs HiGHS, time per solve | 3.81x | **3.70x** |
| vs HiGHS, time per iteration | 2.60x | **2.52x** |
| vs SoPlex, time per solve | 1.36x | **1.31x** |
| vs SoPlex, time per iteration | 1.95x | **1.87x** |
| JAOS faster than SoPlex on | 10 of 22 | **11 of 22** |
| **`maros-r7` per iteration vs HiGHS** | **16.49x** | **10.98x** |
| `pilot87` per iteration | 3.33x | 2.86x |

`maros-r7` goes 35.052 s -> 23.072 s, **1.519x with `-O3 -march=native
-flto`** against 1.569x at the shipping flags — so LTO does not hide these
two, which is what D56 predicted for work that is removed rather than
inlined.

**The set figure moves 2.9% and the noise floor is 1.3%**, so it is real and
it is small. Two instances improving cannot move a geometric mean over
eighteen by much, and that is the mean behaving correctly rather than a
disappointment: the same two instances are 74.1% of the *work*, which is a
different question from the one a per-instance mean asks.

**This record carries `WITH UNCOMMITTED CHANGES`** and the marker is telling
the truth: `run-compare.sh` itself and `DECISIONS.md` were modified when it
ran. `src/` and `include/` were the committed `ccad702`, which is what the
seconds are about.

## D61 — The pricing row has no sparsity to exploit, and the calls around it are not the cost either

The instance that decides the gap is not `maros-r7`. Against HiGHS the set
figure is a geometric mean over eighteen instances, and D58 and D59 moved it
2.9% by making one of them 1.5x faster. What sets it is the ordinary
instance, and `truss` is one: **`jm_lu_factor` is 1.55% of it** against 64.8%
of `maros-r7`. The two need different work.

Where `truss` goes: `pivot` 36.8%, and inside it `update_dual` 14.9%,
`admit_candidate` 14.3%, `shift_to_feasible` 7.3% — **162,603,092,
162,456,002 and 146,993,159 calls** for 17,336 iterations. Two dense sweeps
over every variable, once per iteration, which is what PLAN has said about
Kennington all along and turns out to be true of the standard set too.

**First hypothesis: the sweeps are dense when they need not be.** There is
already a sparse path over the pricing row's pattern (D40, D41), taken when
the pattern fits `nvar / SPARSE_ALPHA_DEN`. Instrumented, the dense path runs
95.7% of the time on `truss`, 99.4% on `pilot87` — and almost never because
`duals_dirty` was set. It is the width test.

**Refuted by measuring the width.** The pattern is not sparse:

| | mean pattern / `nvar` | dense-sweep variables, threshold `nvar/4` -> `nvar/1` |
|---|---|---|
| `pilot87` | **0.852** | 349M -> 300M |
| `truss` | **0.833** | 164M -> 142M |
| `25fv47` | 0.786 | 21M -> 18M |
| `greenbea` | 0.716 | 106M -> 83M |
| `dfl001` | 0.664 | 377M -> 294M |
| `maros-r7` | 0.464 | 116M -> 61M |
| `stocfor3` | **0.015** | 9M (already sparse) |
| `fit2p` | **0.019** | 2M (already sparse) |

The pricing row touches **83% of the variables** on `truss`. Walking a
pattern that size costs an indirection and a random access per entry to skip
17% of a sequential scan. Loosening the threshold to its maximum buys 1.15x
in *variables visited* and would pay for it in locality — and the one
instance where it looks worthwhile, `maros-r7` at 0.464, is exactly the
"fitting a constant to one instance" this project forbids. **`SPARSE_ALPHA_DEN
= 4` is confirmed where it stands, and this question is closed rather than
re-costed.**

**Second hypothesis: the calls are the cost.** 24.6 instructions for a status
test, a comparison and four stores reads like call overhead, and the three
functions are `static` in one translation unit with GCC declining to inline
them. Forcing it with `[[gnu::always_inline]]` keeps the single definition
the comments insist on — the rule is still written once — and removes 470
million calls.

**Also refuted, by the clock. Geometric mean 0.9969x — it is slower.**

| `maros-r7` | `pilot87` | `truss` | `greenbea` | `dfl001` |
|---|---|---|---|---|
| 0.980x | 0.974x | 0.974x | 0.977x | 0.992x |

Every instance that matters lost, and the two that gained are the smallest
ones timed. The instructions were in the work, not in the call: three bodies
inlined into two sweeps that already run 162 million times cost more in
instruction cache than the calls cost in overhead. **Reverted.**

**What this leaves.** The dense sweeps are not a defect to repair, they are
the shape of the algorithm on these models: dual steepest edge prices every
variable, and on the standard set every variable is genuinely touched. Making
them cheaper means pricing fewer of them — partial and multiple pricing,
which is PLAN phase 6 item 3 and the first change here that cannot be judged
on digests, because it moves the search path.

## D62 — One set of shipping flags, chosen by measurement, and the counter costs nothing

Q11 had been open since M2 opened, carrying a list of candidates — `-O3`,
`-flto`, `-march=native`, `--gc-sections`, PGO — and a proposal for two build
targets, `release` and `native`. The list is now a table and the two targets
are one.

**Nothing here is a trade.** Every rung was run over the whole standard set
first, and every verdict, iteration count and solution digest came back
identical to the committed record. A flag that moved one would have been
disqualified whatever it bought; none did. Timed sequentially at the shipping
flags, minimum of three runs, geometric mean of per-instance ratios over
eight instances:

| | ratio | against |
|---|---|---|
| `-O3` | 1.0055x | `-O2` |
| `-O3 -flto` | **1.0330x** | `-O2` |
| `-march=native` | 1.0072x | `-O3 -flto` |
| **PGO** | **1.1122x** | `-O3 -flto` |

**LTO is the only flag that does anything, and PGO is worth three times all
of them together.** PGO gains on every instance that costs anything —
`truss` 1.246x, `maros-r7` 1.238x, `pilot87` 1.151x, `greenbea` 1.100x,
`dfl001` 1.097x — which is what makes it a result rather than a mean.

**`-march=native` is refused as a default, on two grounds and the weaker one
is the speed.** At 1.0072x it is inside the noise of this harness. The
stronger ground is that it produces a `libjaos.a` that dies with an illegal
instruction on any older CPU, and this is a library meant to be linked by
someone who did not build it. It survives as `NATIVE=1`, documented as
measured-and-it-did-not-pay rather than as a tuning knob.

**Two targets become one.** `release` and `native` were a plan to maintain
two sets of flags for one library; the measurement says the second set is not
different enough to exist. `make` builds `-O3 -flto -g -DNDEBUG` and `make
pgo` rebuilds it from a profile. PGO is deliberately not what `make` does: it
takes minutes rather than a second, and it cannot run before the instances
are fetched — a library that will not build without downloading 139 models is
one nobody can package.

**The archive needs `gcc-ar`.** An archive of LTO objects keeps its symbols
where only the linker plugin can see them, and plain `ar` writes an index
including them only where the distribution configured the plugin in.
`gcc-ar` loads it itself. Checked from the outside: a consumer compiled
**without** `-flto` links the archive and runs, which is the only test that
matters for a library.

**And the question that was actually asked: what does the instrumentation
cost?** The deterministic work counter is one inline function called from
every kernel, and the wall clock is read once every 64 iterations. Removing
each, and both:

| | ratio vs keeping it |
|---|---|
| no work counter | **0.9872x** |
| no clock check | 1.0038x |
| neither | 0.9970x |

**Nothing, and the counter's number is on the wrong side of one.** Removing
code cannot make a program slower except through alignment and layout
accidents, which is exactly what a 1.3% reading inside a ±5% per-instance
spread is. So `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit` — the public contract, and the only reproducible budget
the library has — are free, and there is no compile-time switch worth adding
to remove them.

**One instrument correction, because it nearly became a false finding.** The
first check reported the counter-less build as having moved all 94 digests.
It had not: the record line carries `work=`, which reads zero without a
counter, so every line differed while every digest matched. Comparing the
whole line was the wrong comparison, and a check that answers a question
nobody asked will eventually answer one somebody did.

## D63 — The gap's iterations are weight restarts, and the threshold that causes them is what keeps the answers right

**The set figure hides its own shape.** 3.70x against HiGHS is a geometric
mean over eighteen instances, and PLAN and SPECS have read the accompanying
1.47x iteration ratio as "the search is competitive, the iteration is what
costs". Instance by instance that is not what it says:

| | time | **iterations** | per iteration |
|---|---|---|---|
| `pilot` | 13.42x | **4.66x** | 2.88x |
| `pilot87` | 13.15x | **4.60x** | 2.86x |
| `25fv47` | 5.44x | **3.00x** | 1.81x |
| `greenbea` | 8.09x | **2.93x** | 2.76x |
| `maros-r7` | 25.64x | 2.34x | **10.98x** |
| `truss` | 1.33x | 0.90x | 1.47x |
| `dfl001` | 2.09x | 1.14x | 1.83x |

**Two regimes again, and only one of them is what the plan has been aiming
at.** `maros-r7` is a per-iteration problem. `pilot`, `pilot87`, `25fv47` and
`greenbea` are iteration-count problems, and between them they are most of
the tail.

**Where those iterations are not.** The re-entry loop D49–D51 documents as
non-converging was the obvious suspect: it contributes **2.8% of `pilot`,
1.7% of `pilot87`, 0.1% of `greenbea` and nothing at all elsewhere**. The
extra iterations are in the first dual pass.

**Where they are.** Steepest-edge weights are discarded wholesale — every
weight set to 1.0 — whenever the carried weight for the pivot row sits a
factor `DSE_DRIFT` from the exactly known one. Counted:

| | weight restarts | iterations vs HiGHS |
|---|---|---|
| `pilot87` | **93%** of iterations | 4.60x |
| `pilot` | **88%** | 4.66x |
| `25fv47` | **88%** | 3.00x |
| `greenbea` | **80%** | 2.93x |
| `truss` | 31% | 0.90x |
| `maros-r7` | 11% | 2.34x |
| `dfl001` | **0%** | 1.14x |

A solver that resets every weight to 1.0 nine times in ten is not pricing by
steepest edge; it is pricing by largest infeasibility with extra steps. The
ranking of restart frequency and the ranking of excess iterations are the
same ranking.

**And the mechanism is confirmed by removing it.** With the drift test
disabled entirely, the four fall to **`25fv47` 0.31x, `pilot87` 0.36x,
`greenbea` 0.40x, `pilot` 0.54x** of their iteration counts. The weights
were the cause.

**`DSE_DRIFT = 10.0` was a draft. It is now measured, and it stays.** Swept
over the whole standard set, with the gate as the evidence because this moves
the search path and no digest survives it:

| | gate | what breaks |
|---|---|---|
| 2.0 | **NOT MET** | **`greenbea` returns INFEASIBLE** — a false infeasible, the one failure this project treats as catastrophic; `grow22` 6.2x |
| **10.0** | **PASS** | nothing |
| 100.0 | PASS | `grow22` 7.2x the work, 2179 -> 15689 iterations |
| 1e4 | NOT MET | `pilot` outside objective tolerance |
| 1e8 | NOT MET | `pilot` 6x its iterations, `greenbea` INFEASIBLE |
| disabled | NOT MET | `pilot` outside tolerance, `grow22` 14.1x |

**Both sides bounded, and the interior is one value wide.** Tighter loses a
model to a false infeasible; looser costs `grow22` an order of magnitude and
then costs `pilot` its answer. The restarts are not waste to be reclaimed —
they are what stops a badly conditioned basis from pricing on numbers that
have stopped meaning anything, which is exactly what the constant's own
comment claimed without evidence. It now has evidence.

**So the tail is not a threshold to retune.** It is that `pilot`, `pilot87`,
`25fv47` and `greenbea` drift their weights on almost every iteration, and
the cure has to be weights that survive — a more stable recurrence, better
conditioning, or an approximation that does not pretend to be exact.
**Devex** [7] is the candidate the literature offers: its weights are
approximate by construction and reinitialised by design, so a basis that
destroys an exact recurrence does not degrade it the same way. That is a
pricing rule to build and measure, not a constant to move.

**One instrument correction.** The same diagnostic build also counted Bland's
rule and reported it switching on once per iteration on every instance. It
does not: the counter was inserted after the body of an `if` that has no
braces, so it ran unconditionally. The restart counts above sit inside a
braced block and are not affected. A diagnostic that reports something
impossible is reporting on itself.

**The restart's scale was tried, and it moves which instances pay.** The
mechanism above says the cascade is self-inflicted: 1.0 is four decades below
where these models' weights live, so the next drift test cannot help firing.
The exact weight of the pivot row is known at that line and is the right
scale, so restarting to it is the obvious repair. Measured over the standard
set, gate PASS on all 94:

| | iterations |
|---|---|
| `grow15` | **0.09x** |
| `pilot` | 0.76x |
| `greenbea` | 0.79x |
| `25fv47` | 0.96x |
| `pilot87` | 1.08x |
| `truss` | 1.23x |
| **`grow22`** | **13.88x** — 2179 iterations to 30251 |

**Geometric mean 0.9968x. Refused.** It buys `pilot` and `greenbea` roughly
what the mechanism predicted and takes `grow22` to fourteen times its work —
and `grow22` is the same instance that broke at `DSE_DRIFT = 100` and again
with the test disabled. Three separate attempts to keep more weight
information have now cost that one model an order of magnitude, which makes
it evidence about the method rather than about a constant: **on `grow22` the
carried weights are actively harmful, and restarting to a scale that
preserves them is what harms it.**

So the restart to 1.0 is load-bearing in a way its own comment did not claim
and nobody had measured: it is not only a guard against meaningless numbers,
it is what stops a model whose weights mislead from following them. Left
exactly where it is.

**And the conclusion for the plan is unchanged and now better supported.**
The cure for the iteration-count half of the tail is not a better restart —
every version of that tested so far trades one instance for another. It is a
pricing rule that does not depend on an exact recurrence surviving:
**Devex** [7], phase 6 item 6.

## D64 — The options API configures the contract and never the method, and it is setters

Phase 2 opened with the shape undecided — "an opaque options object passed to
`jaos_solve`, or setters on the model" — and with a larger question behind it
that the plan had not asked: *what* is a caller allowed to configure.

**The rule, decided first because it shapes everything else.** A caller sets
what depends on their problem and which the solver cannot know: how much
precision their data deserves, how long they will wait, where log lines go.
**How** the problem is solved is not theirs: the pricing rule, when a carried
weight stops being worth keeping, when to refactorize, whether a sparse or a
dense path is cheaper. Nobody linking this library can be expected to know
whether their model wants Devex or steepest edge, and an option that asks
them is a problem handed back to the caller. The solver has to be good enough
to decide, which is also why the adaptive work D63 points at matters.

That is already the practice — every constant in `docs/tolerances.md` is
measured and fixed rather than exposed — and it is now the stated rule.
`SPECS.md` moves "choose the algorithm" and "turn scaling off" from
**missing** to **out of scope**.

**So the API owes a caller two tolerances**, `PRIMAL_TOL` and `DUAL_TOL` —
the two every competing solver exposes, and the two the comparison harness
already equalises explicitly across solvers because a timing taken at
different tolerances is not a comparison. Everything else in that table stays
where it is.

**Setters, not an options object.** `jaos_set_work_limit` and
`jaos_set_time_limit` already are setters; adding an object would give two
mechanisms for one job, and with the list this short it buys nothing. Each
setter validates at the point of the mistake and leaves its reason in
`jaos_model_error`, where an options object would validate at `jaos_solve`,
one call away from the line that was wrong.

**Refused rather than clamped.** A tolerance that is not finite and
non-negative is an error, not a request to be helpful about. A solver that
substitutes its own number reports success for a run its caller cannot
reason about.

**Zero means the default**, which is the only way to say "whatever you would
have done" once a value has been set — and it is what makes the claim
testable: a model that sets nothing must behave exactly as it did before
these existed. **All 139 answers are identical to the committed records**,
which is that claim as a measurement rather than an assurance.

**Two claims this section used to make, and both were false.** It said a
caller cannot vary the checker's tolerance: `jaos_check_solution` has taken
`double tol` as a public parameter all along. And it attributed D47's
diagnosis to that absence — D47 needed `REFACTOR_EVERY` varied over 16..256,
a *method* constant that by the rule above is not going into the API at all.
What those diagnoses need is a build-time switch or a private entry point,
which is a different thing from an option.

**And the test found a defect before the feature reached anyone.** The one
that matters is not the validation test — a setting that is stored and never
consulted passes that — but the one that shows the tolerance being *used*:
`min x subject to x >= 5` starts five units outside the row, and a primal
tolerance wider than five makes that starting point feasible, so the solve
stops there and reports 0 instead of 5. It failed. `jaos_load_lp` resets the
model and preserves the budgets by listing them one by one; the new
tolerances were not on that list, so anyone configuring before loading — the
natural order to write it in — lost them silently. Fixed, and the reload test
now asserts all four settings rather than one.

## D65 — The solver speaks only when spoken to, and never on a clock

The solver was silent, which for a library is the right default and an
unusable one: a caller with a model that takes four minutes has no way to
tell a slow solve from a stuck one.

**No default destination.** A library that writes to stdout because nobody
told it not to cannot be embedded in a server, a GUI, or another library. So
`jaos_set_log_callback` installs one and there is no fallback: setting a
level without a callback changes nothing, and passing NULL turns output off
again. The line is valid for the duration of the call, like
`jaos_model_error` and for the same reason — it is diagnostic, not data, and
D33's rule is that no *solution* data leaves by pointer.

**Four levels, and each earns its place.** `SUMMARY` opens and closes a
solve. `PROGRESS` adds a line every thousand iterations. `DETAIL` adds the
events that change how the solve behaves.

**Paced by iterations, never by time.** A line every so many milliseconds
would make the output depend on the machine, and output that differs between
two runs of one model is what D8 exists to forbid. `LOG_EVERY` is a count.

**Free when nobody is listening.** Every site tests an inline predicate
before it formats anything, so a solve nobody is watching pays one comparison
per site rather than one formatted string.

**And the claim that had to be tested is not that lines come out.** It is
that they change nothing. A solver that priced differently when someone was
watching would be undebuggable. So the same model is solved silently and at
`DETAIL`, and the objective, the primal values, the duals, the iteration
count and the work units are compared **bit for bit** — not within a
tolerance, because "close" is not the claim. All 139 reference instances are
identical to the committed records with the logging compiled in.

**What the closing summary reports, and why those three.** Refactorizations,
weight restarts and stalls. That is not a decoration: **four separate
diagnoses this milestone had to patch counters into the solver to learn how
often the steepest-edge weights were being discarded** (D63), and a caller
has no such option. `jm_dse_update` now returns whether it restarted, which
makes an event that was invisible from outside into one the library reports.

Cross-checked rather than trusted: the new counter reports 8299 restarts on
`25fv47` and 682 on `grow15`, which are exactly the numbers the hand-patched
diagnostic produced before it existed.

**One defect the demo caught that the unit test could not.** The first
progress line read `infeasibility inf`, because it was emitted before the
pricing that computes the number. A three-column unit-test model never
reaches a thousand iterations, so nothing in the suite could have shown it —
it took running a real instance and reading the output. The line moved after
the pricing and says "best infeasibility", which is the quantity that is
actually kept.

## D66 — Changing a model discards its answer, and the two that do not touch the matrix leave the derived data alone

Three modifications land together — `jaos_set_col_cost`,
`jaos_set_col_bounds`, `jaos_set_row_bounds` — and the decisions worth
recording are not about their signatures.

**A modification discards the answer.** The optimum on the model was computed
for the problem as it stood, and the moment a bound moves it describes a
different problem. Leaving it queryable would let a caller change one number
and read back the previous answer with nothing to say it was stale: a wrong
number returned with full confidence, which is the failure mode this project
exists to refuse. The solution arrays are freed rather than flagged, so
`jaos_solution` reports `JAOS_ERR_INVALID_INPUT` and `jaos_status_of` reads
`JAOS_SOLVE_NOT_RUN` — the mistake surfaces at the call, not as a number.

**What is not invalidated, and it is not an oversight.** The scaling and the
row-wise mirror are derived from the matrix alone: `scale.c` reads no bound
and no cost, checked rather than assumed. So a bound or a cost change leaves
both exactly correct, and throwing them away would cost a Curtis-Reid pass
per modification for nothing. **A modification that touches the matrix must
invalidate them**, which is why changing a coefficient is separate work and
not a fourth setter written the same afternoon.

**`lower > upper` is accepted.** That is a model with no feasible point,
which the solve reports as infeasible; it is not a call to refuse.
`jaos_load_lp` applies exactly this rule — bounds may be infinite, never NaN,
and their order is not judged — and a modification that accepted less than a
load would make the same model buildable one way and not the other.
Configuration survives: budgets, tolerances and logging are not problem data.

**Both tests were shown able to fail before being believed**, which is the
discipline this repository has a receipt for. Two faults injected into a
throwaway copy, each the mistake most likely to be made here:

| injected fault | caught by |
|---|---|
| the modification stores the value and forgets to discard the stale answer | `test_a_modification_discards_the_answer` |
| the setter validates its argument and never stores it | `test_a_changed_bound_reaches_the_solve` |

The second is the one that matters for a setter, and it is the same shape as
the defect D64 found for real: a setting that is validated, stored and never
consulted passes every test that only checks what it refuses.

## D67 — Setting a coefficient is three operations, because the stored matrix has an invariant

`jaos_set_coefficient` looks like a fourth setter beside the bounds and the
cost. It is not, and the reason is worth recording so that the next person to
touch the matrix does not treat it as one.

**The stored copy is the authority the checker judges against**, and it holds
an invariant every reader, the scaling and the factorization all assume:
within a column, entries ascend by row index, with no duplicates and no
explicit zeros. So one call is three operations — replace where the entry
exists, **delete when the new value is zero**, insert in sorted position
where it does not. Writing a zero into the array would leave a model that no
longer matches the one loading the same data produces, which is a difference
nothing downstream is prepared for.

**And unlike a bound or a cost, it invalidates the derived copies.** The
row-wise mirror carries the values and the scaling was computed from the
matrix that just moved, so both are dropped and rebuilt on the next solve.
That is the whole reason this is separate work from D66 rather than written
the same afternoon: the two that leave the derived data alone and the one
that cannot are different decisions, and collapsing them would have made the
cheap case pay for the expensive one.

Both arrays are grown before either is written, so an allocation failure
leaves the model exactly as it was rather than half-enlarged.

**Shown able to fail first.** The two mistakes this code is most likely to
contain, injected into a throwaway copy:

| injected fault | caught by |
|---|---|
| insert appends to the column instead of keeping it sorted | `test_a_coefficient_replaces_inserts_and_deletes` |
| a zero is stored in place rather than deleting the entry | both coefficient tests |

The sorted-order test is the one that would otherwise have been missing: a
test that only solved the model would pass on an unsorted column, because a
two-entry matrix still gives the right answer. It asserts the invariant
directly, on every column, which is what this repository means by an
invariant being an assert rather than a comment.

## D68 — A basis outlives the answer it produced, and that one line is warm re-solve

The answer a solve leaves on the model is discarded the moment anything about
the problem moves (D66): it described the problem as it stood, and reading it
back afterwards would be a wrong number returned with confidence. The basis is
the opposite. A bound moving does not stop a basis being a basis, and for a
small change it is very probably near the new problem's. So the two are stored
apart — `sol_*_status` is an answer, `start_*_status` is a starting point — and
`model_answer_is_stale` frees the first and leaves the second. That separation
is the entire feature; everything below is consequence.

**Set two ways and they mean the same thing.** Every solve that reaches an
optimum leaves its basis there, so nothing has to be called for a re-solve to
be warm: change a bound, solve again, and the second solve resumes.
`jaos_set_basis` is for what that route cannot reach — a basis from another
model, one read back from a file, the one a branch-and-bound node hands its
children. Only a load drops it, because after a load the indices name a
different model.

**`jaos_clear_basis` is not a convenience.** Without it warm starting is a
one-way door: a model solved once could never be solved from scratch again,
and "what does this model do cold" would stop being a question the library can
answer about its own model. It is also what kept the acceptance gate honest —
see below.

**What the caller may hand in, and what is repaired instead of refused.**
Refused: a value that is not one of the four statuses, and any count of basic
variables other than `num_row`. Both are structural, and no later event can
make a wrong count right. Repaired: a status naming a bound the variable no
longer has, and a set of columns that no longer factors. Both are what a
*stored* basis looks like after the model moved under it — `jaos_set_col_bounds`
can retire the very bound a variable was resting on — so the solve has to cope
with them regardless, and refusing them at the setter would make a basis handed
in behave differently from the identical one a previous solve left behind.

**Dual feasibility comes from cost shifting, not from artificial bounds.** The
slack basis is dual feasible by construction: every nonbasic sits at the bound
its cost asks for, and a column whose cost wants a bound it does not have is
lent one. That reasoning is only available because `B = -I` makes the duals
zero, so a reduced cost *is* a cost. From a warm basis it is not, and the
reduced costs are not known until the basis is factored. So the repair happens
where they are: the first refresh shifts every breached cost to the feasible
side and writes down the loan, exactly as it already does after a singular
basis repair, and `settle_shifts` calls all of it back before any verdict is
read. Sizing loans off the duals instead would have stood a second dual phase 1
beside the one already running, for a mechanism this solver already has.

**The weights start at one, and here that is a prior rather than a fact.**
`B = -I` makes every weight exactly one, which is the reason the cold start is
the slack basis at all (and the reason a crash basis was refused). An arbitrary
basis has weights that cost one solve per row to know. One is the same neutral
value `repair_singular_basis` restarts to after it changes several columns of
`B` at once, and for the same reason: the exact weight injected each iteration
rebuilds the estimates from there. It costs pricing quality on the early
iterations, and no verdict depends on a weight.

### The wrong answer this was built to give, and the refusal that stops it

A nonbasic variable with no finite bound on either side rests at zero. For the
logical of a row that pins the row's activity at zero — a constraint the model
does not have. Constructed:

```
min 2x + 3y   s.t.  x + y >= 2,  x + y <= 100,  x <= 1.5,  0 <= x,y <= 5
```

optimum 4.5. Relax the third row to `x <= inf` and re-solve warm: the stored
status for that row was AT_UPPER, the upper bound is gone, there is no lower
one, and installing it as free pins x at zero. The solve reports **OPTIMAL
with objective 6**, where the optimum is 4.

It cannot price its way out, and the reason is exact rather than probabilistic.
`can_move` has nowhere to send a free variable and returns false for one.
`wants_a_pivot` computes its wrong-way direction as `status == AT_LOWER ? -d :
d`, so a free nonbasic is read as sitting at an upper bound: a positive reduced
cost is repaired and a negative one is invisible. `primal_ratio_test` takes the
same branch and would move it in the wrong direction if it got there.

So a warm start refuses to create one — a nonbasic with no finite bound at all
drops the whole warm basis and the solve runs cold, which is always correct.
Two things follow and both are deliberate. A stored status that was *already*
free is refused on the same rule, even though its reduced cost was zero at the
optimum it came from, because nothing at install time can know whether a cost
or a coefficient has moved since. And **the defect itself is untouched**:
`build_initial_basis` produces free nonbasics for zero-cost unbounded columns
and `repair_singular_basis` produces them for evicted ones, so it is reachable
without any of this. It is carried in PLAN.md as its own repair, with its own
measurement, and this decision only declines to widen it.

### What the gate said

All 139 answers unmoved, three sets, `gate: PASS` on each with 0 regressed.
That is the claim that matters: a model solved once from a fresh load takes the
same path it always did, and the reference sets are exactly that.

**They said the opposite first, and were right to.** 93 of the standard 94 and
all 16 Kennington instances came back `det: yes -> no`, every one of them still
optimal. The runner solves each instance twice and requires the two runs to
agree bit for bit — and the second solve had become a warm re-solve, reaching
the same optimum in no iterations for different work. The check was measuring a
sequence of calls and calling it a property of the solver. It now clears the
basis first, which is what makes the two runs the same input; `jaos_clear_basis`
exists partly because this needed it.

### A latent defect, found because a warm basis can go where a cold one cannot

`refactorize` asks for capacity equal to the basis's nonzero count, and
`JM_GROW` leaves a request of zero unallocated — so a basis of nothing but
structurally empty columns handed `jm_lu_factor` a null index array, which it
correctly refuses as bad input. The solve then failed instead of reporting the
model infeasible. The slack basis cannot reach that state, because every
logical carries an entry; a warm one reaches it by keeping a column basic after
the last coefficient in it is deleted. One slot is always asked for now, and a
test pins it.

## D69 — What warm re-solve buys: 182x the iterations and 60x the work, on a branching step

D68 built warm re-solve and proved it changes nothing — 139 answers unmoved.
That is the safety claim and it is the smaller half. The gate cannot make the
other one: it solves each instance once from a fresh load, which is exactly the
case warm starting does not touch. So `bench/warm.c` and `make warm`.

### The perturbation is one branch-and-bound branching step

Not a nudge with a size somebody picked. A branch is the workload that made
the dual simplex the method of choice for re-solving, it is what phase 7 will
do millions of times, and **its size comes from the model's own numbers rather
than from a constant fitted here** — which is the difference between a
measurement and a demonstration.

The rule is deterministic (D8): the lowest-indexed structural column whose
optimal value is not within 1e-6 of an integer, branched **down** to
`x_j <= floor(x_j*)` when that leaves the column a box and **up** to
`x_j >= ceil(x_j*)` when it does not. The previous optimum is cut off by
construction, since `x_j*` lies strictly between floor and ceil.

Each instance is then solved three times: an anchor solve from a fresh load,
which is where both the basis and the branch come from; **warm**, resuming from
that basis; and **cold**, the same perturbed model after `jaos_clear_basis`,
which is the answer JAOS gave before D68 existed. The anchor is not one of the
two numbers.

### The result

| | standard 94 |
|---|---|
| measured | 92 |
| skipped | 2 — `recipe` and `shell`, every structural column integral at the optimum, so there is no branch to take |
| disagreed, rejected, errored | **0, 0, 0** |
| iterations, `(warm+1)/(cold+1)`, geometric mean | **0.0055** |
| work units, warm/cold, geometric mean | **0.0166** |
| best | `maros-r7`, 0.0001 of the work |
| worst | `cycle`, 1.0000 |
| took exactly one iteration | 53 of 92 |
| took none at all | 1 of 92 |
| took **more** iterations warm than cold | **0 of 92** |

`grow15` is the shape of it: **1 iteration against 20305**. `25fv47` 3 against
9714, `80bau3b` 1 against 3783.

Iterations are reported as `(warm+1)/(cold+1)` because reaching the optimum in
no iterations is the outcome the feature exists to produce and a geometric mean
cannot hold a zero. The count of those is printed beside the mean rather than
folded into it.

**The two ratios are two orders of magnitude apart and that is the finding, not
a discrepancy.** A warm solve that takes one pivot still pays two full
refactorizations — the first one and D20's verification refresh — so its work
floor is fixed while its iterations go to almost nothing. Warm starting removes
iterations; it cannot remove the cost of proving the answer.

### Three things checked before believing any of it

**The cold number is honest.** If the branch made the model easier from
scratch, the ratio would be measuring that instead. Cold-on-perturbed against
the gate's own cold solve: `grow15` 20305 against 21653, `25fv47` 9714 against
9459, `80bau3b` 3783 against 3836, `maros-r7` 9817 against 10479, `cycle` 1537
against 1829. All within about 10%, in both directions.

**The perturbation is real.** A branch that cut nothing off would leave the
previous point optimal, warm would have nothing to do, and every ratio would be
measuring a perturbation that never happened. The anchor objective is recorded
for exactly this: **the optimum moved on 85 of 92**. The other seven kept the
same objective at a different point — the branch cut off the vertex and an
equivalent one existed — which is a genuine re-solve and not a no-op;
`grow15` is one of them, at 1 iteration against 20305.

**The answers agree.** Same verdict on all 92, objectives within 1e-6 relative,
and every warm answer put through the independent checker. Warm starting is a
starting point and never a claim, so a disagreement here would have been a
defect rather than a trade-off. There were none.

### `cycle` is the free-nonbasic refusal, costing exactly what it should

`cycle` reports warm and cold **bit-identical**: 1537 iterations and 19993693
work units both ways. That is the warm start declining to run at all and
falling back to the slack basis, and it is the only path in `build_warm_basis`
that can produce it — the other two fallbacks need a missing basis or a wrong
count of basic variables, and neither can happen after an optimal solve. The
instance carries seven `FR` columns, and one of them is nonbasic at the
optimum.

So D68's refusal has a price, and it is now measured: **one instance in 92 pays
the whole warm start for it.** Eleven instances of the set carry free or
minus-infinity columns and ten of them warm-start normally, which is the
reasoning D68 gave — a free variable usually ends up basic — confirmed rather
than assumed. Fixing the underlying defect (PLAN.md, carried defect 4) would
recover `cycle` and nothing else on this set.

### Kennington, and the ratio gets better as the models get bigger

|  | standard 94 | Kennington 16 |
|---|---|---|
| measured | 92 | 11 |
| skipped | 2 | 5 — all four `ken-*` and `pds-02` |
| disagreed, rejected, errored | 0, 0, 0 | 0, 0, 0 |
| iterations, geometric mean | 0.0055 | **0.0006** |
| work units, geometric mean | 0.0166 | **0.0041** |
| best | `maros-r7` 0.0001 | `cre-b` 0.0003 |
| worst | `cycle` 1.0000 | `pds-06` 0.0326 |
| took more iterations warm | 0 of 92 | 0 of 11 |

`cre-b` takes **1 iteration against 17132**, `pds-10` 1 against 17253,
`pds-20` 87 against 47963. Nothing fell back to the slack basis here: the
free-nonbasic refusal costs this set nothing.

**The work ratio is four times better than on the standard set, and that is
the direction it should go.** A warm solve's floor is two refactorizations
whatever the model; a cold solve's cost grows with the model. So the bigger
the instance, the smaller the share of it that the floor is — which says warm
starting scales with size rather than against it, and Kennington is 55% of the
two sets' total work.

Five of sixteen skipped is a large fraction and worth naming rather than
averaging over: the four `ken-*` are network models whose optimal values land
on integers, so there is no fractional column to branch on at all. They are not
evidence about warm starting in either direction.

The same two checks were run again. **The cold number is honest**: against the
gate's own iteration counts, `osa-07` is 601 against 601 and `osa-60` 6169
against 6169 — exact — with the rest within a few percent and `cre-b` the
largest gap at 17132 against 14614. **The perturbation is real** on 7 of 11;
the four that kept their objective (`osa-60`, `pds-06`, `pds-10`, `pds-20`)
moved to an equally good vertex, which is a genuine re-solve — `pds-10` did it
in 1 iteration where cold took 17253.

## D70 — A budget that cannot be resumed is only half a budget

D8 gave a caller two ways to stop a solve, and until now stopping was all they
did. `jaos_solve` came back WORK_LIMIT, the run's work was thrown away, and
raising the limit meant starting from the slack basis again. A budget whose
only use is to abandon work is a strange thing to have built a deterministic
work counter for.

So `publish` keeps the basis for the outcomes that are not an answer, and it is
three separate statements rather than one:

**Written and kept** for WORK_LIMIT and TIME_LIMIT, because that is what makes
a budget resumable — solve, raise the limit, solve again, and the second call
continues. And for INFEASIBLE and UNBOUNDED, which is less obvious and is the
branch-and-bound case: the model is answered, but the next node differs from it
by one bound and there is no closer place for it to begin.

**Cleared immediately after** from `sol_*_status`, because `jaos_basis`
publishes the basis *behind an answer* and there is none. A stopping point is
not a solution and the two must not come out of the same call. This is why the
starting basis lives in its own arrays (D68) rather than being read back out of
the published ones: here they hold different things and only one of them is a
statement about the model.

**Left out for JAOS_SOLVE_NUMERICAL_ERROR**, alone among the six. It could not
corrupt anything — a warm start is never a claim, and the solve that follows
proves optimality from scratch — but it is the one state this solver does not
vouch for, and handing it over as a starting point would be recommending it.

### The test that had to be able to fail

"Solve with a budget, raise it, solve again, get the right answer" passes
whether or not anything was resumed: a solve that stopped before its first
pivot leaves the slack basis, and starting from the slack basis is what the
solver did before any of this existed. Two assertions close that:

- the interrupted run must have got **past its first iteration**, or the basis
  it left is the one a cold start would have built anyway;
- the resumed run must cost **fewer iterations than a whole cold solve**, which
  is the only observation that says the first run's work was kept rather than
  redone.

### A defensive line that came due one commit later

D68 added `jaos_clear_basis` to the acceptance runner's *infeasible* path as
well as its optimal one, and said so in a comment: it made no difference then,
because an INFEASIBLE solve published no basis, and it was written "so that the
day a stopping point does get published for a non-optimal outcome this check
does not quietly stop measuring what it says it measures." That day was the
next commit. Without it, all sixteen infeasible instances would have started
reporting DIVERGED, and the reading would have been a determinism regression
rather than a feature landing.

## D71 — The checker says when its bound is not a bound, and the count is 98 of 110

D47 established that `gap_positive` — documented in `jaos.h` as satisfying
`P - P* <= gap_positive` — is not a bound whenever a multiplier's sign points
at an infinite bound, because the term it owes the dual objective is minus
infinity and gets dropped. It left two honest routes and said choosing between
them was the open work. **Both need the same thing first, and nobody had it:
the fact reported, and its size measured.** This does that and nothing else.

`jaos_check_report` gains `gap_certified` — the identity took every term, so
the bound holds — and `max_dropped_multiplier`, the largest one it could not
take. **Neither decides anything.** No verdict reads them, the gate is
untouched, and all three sets pass with 0 regressed. That restraint is not
timidity: deciding needs a threshold, and the measurement below is the second
independent refutation of every threshold there is.

### The measurement, over the 110 accepted answers of both feasible sets

**98 of 110 carry at least one dropped term.** D47 estimated 15, thresholding
at 1e-12; the real count at that threshold is 22, and with no threshold at all
it is 98. The distribution:

| multipliers above | answers |
|---|---|
| 1e-6 — *the checker's own tolerance* | **0** |
| 1e-7 | 1 — `etamacro` at 2.25e-07 |
| 1e-8 | 5 |
| 1e-9 | 6 |
| 1e-10 | 9 |
| 1e-11 | 18 |
| 1e-12 | 22 |
| 1e-13 | 43 |
| 1e-14 | 72 |
| 1e-15 | 84 |
| anything at all | 98 |

The largest is `etamacro` at **2.25e-07**, which reproduces D47's figure for
the same instance exactly and is the same order as the 3.47e-07 that cost
`pilot` 1.04e-3 of objective at a refactorization interval of 32.

### Two things this changes, and one it confirms

**Route A is not what D47 costed it at.** "Report the bound as void" would say
JAOS cannot certify **98** of its 110 accepted answers, not 15. That is not an
annotation on a gate, it is the gate. Anyone taking that route now knows the
price before paying it, which is the whole reason to measure before repairing.

**There is nowhere to put a threshold, and now that is a measurement rather
than an argument.** D47 refuted the obvious one on four columns, by showing
that a backward-error ratio scores an accepted answer four times worse than a
rejected one. This refutes every threshold from a different direction: the
distribution decays smoothly from 2.25e-07 to 1.35e-17 with no gap anywhere in
it. There is no valley to cut at. Two independent refutations of the same
class of repair is what closes a question rather than deferring it.

**And the sharpest fact is the first row of that table.** Not one dropped
multiplier in either feasible set reaches 1e-6 — the tolerance the checker
uses to decide whether a multiplier is nonzero at all. So every one of them is
a number this checker calls zero, and `pilot`'s 3.47e-07 was too. **The entire
exposure lives inside what the checker already calls zero**, which is why no
amount of tightening its sign test could ever have reached it.

### What is left, and it is now the only route with evidence behind it

D47's second route: `|d_j|` times the step a ratio test allows is a certified
*lower* bound on the suboptimality, it needs no reference value, and it was
1.04e-3 on `pilot` at 32. It needs `B^-1 a_j`, so the independent checker would
need a basis and a factorization of its own — a real cost, and a real question
about what "independent" then means, since the thing that verifies the answer
would start sharing machinery with the thing that produced it.

Until then the honest statement is the one D47 made and this measures: the
Netlib gate means what it means because of Koch's reference values, which
PLAN 2.10 already names as the one thing in the milestone that does not come
from JAOS. **It was right for a reason nobody had measured, and the size of
that reason is 98 of 110.**

### Found in the same audit, and repaired

`docs/tolerances.md` still listed `pilot` and `pilot87` as refused by the
checker and counted "92 of 94 instances" green. Both closed two decisions ago
— `pilot` by D29's iterative refinement, which took its row residue from
1.73e-6 to 6.73e-13 on a solve that came out *cheaper*, and `pilot87` by D30 —
and the document's own next paragraph narrated the `pilot87` repair while the
table above it still called it outstanding. All 94 are green and have been for
some time. In a repository whose first rule is that the design is written down
and must not be reconstructed from the code, a document that contradicts both
the code and itself is the most expensive kind of defect there is.

## D72 — `pilot87`'s iteration guard: not a cycle, and the anti-cycling rule is the reason

PLAN carried this as the one defect nobody had diagnosed: at a refactorization
interval of 128 and above, `pilot87` trips the internal iteration guard and the
solver's own message calls it a JAOS defect. This diagnoses it. It does not
repair it, and the last section says why that is the honest stopping point.

### Reproduced, against a control

Both runs are `pilot87` with nothing changed but `REFACTOR_EVERY`, driven
through the public API with logging at `JAOS_LOG_DETAIL` — no instrumentation
was needed for any of this, because D65's logging is the instrument.

| | interval 64 | interval 128 |
|---|---|---|
| outcome | optimal | **guard tripped** |
| iterations | 50,850 | **1,382,801** |
| work units | 23.1e9 | 242.1e9 |
| refactorizations | 806 | 14,908 |
| weight restarts | 47,538 (93.5%) | 849,164 (61.4%) |
| stalls | **0** | **2** |

The cap is `ITER_SANITY_FACTOR * (nrow + ncol + 1)` = 200 × 6914 = 1,382,800,
so it fires on the first iteration past it. 27 times the healthy iteration
count.

### The trajectory, which is where the shape is

Bland's rule engaged exactly twice, at iterations **96,766** and **794,076**,
each after 69,141 iterations without the total primal infeasibility improving
— `STALL_FACTOR` doing precisely what it is set to do (D17).

| from | to | total infeasibility | |
|---|---|---|---|
| 28,000 | 96,766 | flat at 1195.99 | Bland off, then engaged |
| 96,766 | ~644,000 | **still flat at 1195.99** | Bland on |
| 645,000 | ~724,000 | falls to 3.18, jumps, churns | three re-entries |
| 725,000 | 794,076 | flat at 25.4152 | Bland off, then engaged |
| 794,076 | 1,382,801 | **still flat at 25.4152** | Bland on, guard fires |

**The final stretch never improves once.** That is not read off the log's six
digits: `bland` is turned off by exactly one condition, `total < infeas_best`,
and the counter says Bland engaged twice in the whole solve. So across
588,725 iterations the total infeasibility did not once dip below 25.4152.

### The decisive test, and it refuted the obvious hypothesis

A solve that repeats bit for bit is cycling; one that does not is merely slow,
and the two have different cures. So a throwaway build hashed the basis and
every variable's status once per iteration.

| | iterations | distinct states | repeats |
|---|---|---|---|
| whole solve | 1,382,801 | 1,245,381 | 137,420 |
| **under Bland's rule** | 1,136,538 | 1,136,521 | **17** |
| with Bland off | 246,263 | — | 137,403 |

**With Bland off the solver cycles, hard**: single states are revisited
**11,379 times**, and 56% of the non-Bland iterations are revisits of a state
already seen. **Under Bland it does not cycle at all** — 17 repeats in 1.14
million iterations, which the five re-entry boundaries account for.

So the hypothesis this went in with is dead. It was that the cost shifting the
method does every iteration to hold dual feasibility changes the problem
underneath Bland's rule, whose termination proof assumes a fixed objective —
plausible, mechanical, and **wrong**: a rule that was not working would repeat
a basis, and this one never does.

Ruled out along the way, and cheaply, because it is the classic version of the
same error: that only the *entering* choice follows the index rule while the
row is still chosen by steepest edge, which is an implementation that is not
Bland's rule at all. `price_row` takes the lowest-indexed violating row under
`s->bland` and `dual_ratio_test` calls `jm_bland_pick`. Both choices follow the
index.

### What the defect is

**The solver's anti-cycling rule and its progress measure are about different
quantities, and on a degenerate enough instance they come apart completely.**

Bland's rule guarantees that no basis repeats — and the hashes show it
delivering exactly that, a million distinct vertices in a row. What it does not
guarantee is that *primal infeasibility* falls, and that is the only quantity
the solver watches. So the solve is behaving exactly as the rule prescribes
while every instrument the solver owns reports a hang, `bland` can never switch
off because switching off requires the improvement that is not coming, and the
guard eventually fires and reports a defect that the evidence says is not
there.

The guard is not wrong to stop. It is wrong about why, and it was the only
thing anyone had to go on.

### Validating the instrument, which is the part that makes the rest usable

The instrumented build and a clean one agree on **every** figure: 1,382,801
iterations, 242,063,185,486 work units, 14,908 refactorizations, 849,164 weight
restarts, 2 stalls. So the hashes describe the trajectory the solver actually
walks and not one the hashing perturbed. Nothing here was believed off the
diagnostic build alone.

### What is repaired here, and what is not

Repaired, because both are about the solver being able to say what happened:

- **The three counts are reported when a solve fails**, not only when it
  succeeds. They were on the success branch alone, which is the branch nobody
  needs them on. This diagnosis wanted exactly those three numbers and was
  handed a sentence.
- **The guard says how long the infeasibility has stood still and whether
  Bland's rule was on.** Those two numbers separate a cycle the rule never
  caught from a cycle it caught and could not finish, which are different
  defects, and establishing which one this was took two five-minute runs.

Not repaired, and deliberately. The cure is a progress measure that can see
what Bland's rule is actually making progress in — the dual objective, which
is non-decreasing across a dual simplex step whether or not the primal
infeasibility moves. That would let the solver distinguish "Bland is working
and this instance is hard" from "nothing is happening", which is the
distinction it currently cannot make and the reason this defect looked like
non-termination. It is a change to how every solve measures progress, so it
needs its own decision and its own measurement over all three sets, and
inventing it at the end of a diagnosis is how this project loses weeks.

Note also what this is *not*: 128 is not the shipped interval, and at 64 the
instance is clean. This is a latent defect a sweep exposed, which is the third
time that sweep has paid for itself.

## D73 — The certificate D47 wanted, without the factorization it thought it needed

D47 left two routes and D71 measured the first one to death: 98 of 110 answers
carry a dropped term, the distribution has no gap to threshold at, and voiding
the bound would void the gate. This takes the second route, and finds that the
expensive part of it was never the obstacle.

### The step does not need `B^-1 a_j`

D47's route B is `|d_j|` times the step a ratio test allows, and it costed that
at a basis and a factorization inside the checker — with a real question
attached about what "independent" means once the thing verifying the answer
shares machinery with the thing that produced it.

**Move the column on its own instead, with every other variable pinned where
it is.** Every row's activity moves by `a_ij t`; the step is the smallest
distance any row's own bound permits. The resulting point is feasible by
construction — no other variable moved, so no other bound can be broken — so
the distance is a *guaranteed minimum* rather than an estimate, and
`|w| * t` is a certified lower bound on `P - P*`.

It travels less far than the simplex direction, which lets the basics absorb
the move. A smaller lower bound is still a lower bound. And it is computed
from the row activities `check.c` already accumulates and nothing else: no
basis, no factorization, no reference value, nothing borrowed from the solver.

On D47's own constructed case it recovers **0.1 exactly** — the entire
suboptimality of a point on which every other number in the report reads zero.

### The first version was wrong, and the measurement is what said so

Five instances came back `inf`: `sctap3`, `sctap2`, `scorpion`, `pilot87`,
`finnis` — every one of them matching a published finite optimum.

The reason is exact and it is worth stating as a rule. **Where the step is
finite the product is self-limiting**: a multiplier that is really roundoff
certifies a roundoff-sized suboptimality, so no threshold is needed anywhere
and nothing can false-alarm. **Where the step is infinite the product is
infinite for any nonzero multiplier at all**, 1e-17 included — at which point
it has stopped being a certificate and has become D47's unanswerable question
wearing one: is this multiplier real?

So the two cases are split, on the checker's own `|w| <= tol` — the definition
of "nonzero multiplier" it already uses everywhere else, not a number invented
to make an awkward case go away. Below it the ray is counted; above it the
model genuinely is unbounded and infinity is the right answer. Both halves are
built in `tests/test_check.c`, because a rule that only ever counted would hide
a real unbounded model.

### What the reference sets say

| | standard 94 + Kennington 16 |
|---|---|
| largest certified suboptimality, anywhere | **4.98e-16** (`d2q06c`) |
| next | 3e-21 (`finnis`), then 1e-26 and below |
| instances with an unquantified ray | **5 of 110** |
| rays in total | 30 — `pilot87` 10, `sctap3` 8, `sctap2` 6, `scorpion` 4, `finnis` 2 |
| infinite certificates | **0** |

Certifying nothing on 110 answers that are all genuinely optimal is the
correct outcome, and it is the one that says the instrument does not
false-alarm. Its ability to fire is established by construction instead, which
is the only place it can be: `tests/test_check.c` builds the point it must
catch and it catches it to the last digit.

### The part that reframes D47

**105 of 110 answers are now fully quantified with no factorization at all,
and the 5 that are not are unquantifiable for a reason a factorization would
not fix.**

That last clause is the finding. A column whose move-alone step is unbounded
has a feasible ray of the *model* along it — no other variable moves, so no
other bound can object. If its rate were truly nonzero the model would be
unbounded, full stop. Adding `B^-1 a_j` computes a different, longer step; it
does not make a rate of 1e-8 distinguishable from zero. So the machinery D47
proposed would buy a better number on the 105 that already have one, and
nothing at all on the 5 that do not.

What is left is therefore not a missing factorization. It is the irreducible
core D47 named from the start: **no local test can tell a small reduced cost
that matters from one that does not**, and on these five that question is all
there is. `jaos.h` now says so in the report rather than in a comment nobody
reads.

### Still deciding nothing

No verdict reads either field, and the three gates pass with 0 regressed and
139 answers identical. Whether `certified_suboptimality > tol` should make
`dual_feasible` false is a real question and a separate one — it would be the
first predicate in this checker that can fail an answer no tolerance rejects —
and it needs an instance that fails it before it is worth deciding. None of
the 110 does.

### **Refuted, the same day, by the one instance that could test it**

Everything above about *soundness* stands. The claim that the factorization
"was never the obstacle" does not, and this is what refuted it.

`pilot` at a refactorization interval of 32 is D47's own reproduction: it
stops on a point 1.04e-3 away from the optimum with every checker number
green. Swept over the intervals D47 used, in the tree as it stands today:

| `REFACTOR_EVERY` | objective | `drop` | **`sub`** |
|---|---|---|---|
| 16 | ok | 8.62e-09 | 3.89e-32 |
| 24 | **OUT-OF-TOLERANCE** | 3.47e-07 | 1.58e-31 |
| 32 | **OUT-OF-TOLERANCE** | 3.47e-07 | **4.4e-20** |
| 48 | **OUT-OF-TOLERANCE** | 3.47e-07 | 3.11e-24 |
| 64 | ok | 8.62e-09 | 1.01e-31 |
| 96 | **OUT-OF-TOLERANCE** | 3.47e-07 | 2.88e-31 |

Four of six answers are wrong by 1.04e-3 and every one of them passes the
checker. **The certificate reads the same nothing on the wrong answers as on
the right ones.** It does not separate them; it carries no information at all.

**And the reason is structural rather than bad luck.** A column moving on its
own is stopped by the first row that is tight, and a vertex is *defined* by
rows being tight — so at any vertex the move-alone step is essentially zero,
whatever the reduced cost. The simplex direction travels because it lets the
basic variables move to keep those rows satisfied, which is exactly the part
that needs `B^-1 a_j`. Column 1534 wants to travel 2990; alone, it cannot
travel at all.

**So the 110-answer measurement above proves much less than it appeared to.**
"Certifies nothing on 110 correct answers" read as "does not false-alarm"; it
is better read as "cannot fire at a vertex", and the two are indistinguishable
from that data. The constructed test in `tests/test_check.c` fires only
because the point it judges is the origin with a row carrying 1e6 of slack —
not a vertex of anything tight. **A green result is not a proof**, and this is
what that rule costs when the only adversarial case is one you built yourself.

What survives, precisely:

- The number is a sound lower bound. It never overclaims, and 110 answers plus
  a constructed case say so.
- The split at `|w| <= tol` for unbounded rays stands, and so does the point
  that a factorization would not help those five: a feasible ray is a feasible
  ray whatever direction found it.
- **Route B needs the simplex direction after all**, and therefore a basis and
  a factorization inside the checker, and therefore an answer to what
  "independent" means. D47 costed it correctly. What this entry adds is the
  price of the cheap alternative, measured: on the one instance in this
  project known to be wrong, it is worth nothing.

## D74 — Does the re-entry's clean-up need to borrow at all? Measured: yes, and `pilot87` is the whole price

PLAN carries the re-entry loop as a defect: its two repairs undo each other,
`SETTLE_ROUNDS = 32` is what ends it rather than any condition, and the
question it leaves open is **whether a clean-up pivot needs to borrow at all**
(D49, D50, D51). This answers that question and closes the direction.

**Why it looked answerable.** `arm_reentry` shifts the cost of any column
whose reduced cost points the wrong way and which it cannot flip, and the
comment gives the reason: "the ratio test must not meet a reduced cost already
past zero". That reason has since expired in place — `admit_candidate` clamps
`rnum` at zero and its own comment says why, "an already-infeasible cost blocks
at once, and the step that follows repairs it exactly". Two mechanisms for one
hazard, one of them documented as the reason for the other.

**Measured, on a copy of the sources with that one call removed**, over the
whole standard set:

| | committed | loan removed |
|---|---|---|
| optimal | 94 | 94 |
| objective within tolerance | 94 | 94 |
| checker green | 94 | 94 |
| deterministic | 94 | 94 |
| **instances whose iteration count moved** | — | **2 of 94** |
| `pilot` | 25,342 | 24,825 — **0.980x** |
| `pilot87` | 50,850 | 120,640 — **2.372x** |

**So the loan is not load-bearing for correctness.** The gate passes without
it, every objective and every checker verdict is unchanged, and ninety-two of
the ninety-four instances do not notice: their trajectories are identical, which
also says the loan is only ever taken on the handful of instances that open the
re-entry loop more than once at all.

**It is load-bearing for cost, and the cost is one instance.** Removing it buys
`pilot` 2% and costs `pilot87` **2.372x its iterations** — and `pilot87` is
precisely the instance whose re-entry loop D51 says does not converge. Taking
the borrow away makes the non-convergence worse, which is the opposite of what
the question was hoping for.

**The direction is closed.** Not because the loan is elegant — two mechanisms
guarding one hazard is still one too many — but because removing it is
measured, changes no answer, and costs 2.37x on the one instance the whole
question is about. Whatever repairs the re-entry loop, it is not this.

What stays open is the loop itself, unchanged: `SETTLE_ROUNDS = 32` still ends
it rather than a condition. What is now known is that the ratio test's clamp
makes the loan redundant *as a guard*, so if a future repair wants it gone it
can have it for free on 92 instances and owes `pilot87` an explanation.

## D75 — The non-aliasing claim holds, and `restrict` belongs inside the kernel rather than on the API

Q11 left `restrict` on the kernel pointers open, with the right instruction
attached: it is "safe only if the non-aliasing claim is true — which is the
part to establish first". Established here. The measurement is not run, and
the last section says what it should be.

**The claim, and why it is checkable at all.** `jm_lu_ftran`, `jm_lu_btran`,
their sparse forms and `jm_lu_update` are declared in `jaos_internal.h`, so
the set of callers is closed: `src/simplex.c` and the tests, nothing else, and
no promise is being extracted from anybody outside this repository.

**Every vector those solves are handed**, across all thirteen call sites in
the solver: `s->col`, `s->y`, `s->rho`, `s->tau`, and two locals that are
assignments of `s->tau` — `compute_duals`'s refinement residual borrows it,
and says so. Each is its own `jm_alloc_array` or `jm_calloc_array` in
`sx_init`, none is ever assigned from another, and the factorization's own
storage — `tmp`, `spike`, `l_value`, `urow`, `ucol` — is allocated inside
`jm_lu_factor`. **So no caller-supplied vector can alias the factorization's
workspace.** The pattern arrays (`s->cpat`, `s->rpat`) are distinct
allocations again, and of a different type. The tests pass distinct buffers
too.

**But the audit moves where the qualifier goes.** Writing `double *restrict x`
in the *signature* makes it a promise every caller must keep, and it buys less
than it looks: `lu` is a struct pointer, and the arrays the inner loops
actually read — `lu->l_value`, `lu->tmp` — are pointers loaded out of it,
which no qualifier on `lu` makes restrict-qualified. The form that would help
is local:

```c
    double *restrict xr = x;
    const double *restrict lv = lu->l_value;
```

inside the kernels, where the promise is over one call rather than over an
API, and where it can be read and checked in the same screen as the loop it
constrains. That is strictly the safer of the two and it is the one this audit
supports: a contract the callers must honour is a contract that outlives the
audit that justified it.

**What the measurement has to be.** `restrict` must not move a single number —
it changes what the compiler may keep in a register, not what is computed — so
the acceptance test is that all 139 answers and every work-unit count are
identical, and the only thing left to read is seconds. Seconds need `-j 1`
(D57) and a same-machine ratio (D45), which is two sequential runs of the
standard set. The instances to weigh it on are the ones the LU dominates:
`maros-r7`, `pilot87` and `dfl001` are 77.8% of that set's work between the
first two alone (D46).

Left open deliberately rather than half-measured. Q11's own numbers set the
expectation: every optimisation flag in the shipping build is worth 3%
together, against 1.1122x for PGO, so this is a percentage and not a factor,
and a percentage measured on a contended machine is not measured at all.

## D76 — `restrict` measured and refused: what makes it safe here is what makes it worthless

D75 established the non-aliasing claim and said where the qualifier belonged.
This is the measurement it deferred, and the answer is no.

**What was built.** Local `restrict` copies inside the kernels, exactly as
D75 specified and never on a signature: `ftran_prefix`, both FTRAN and BTRAN
sparse forms, `btran_u_pattern`, and `jm_lu_update`'s dense row copy and
elimination. Every vector handed in, every array read out of `lu`, and the
DFS workspace — including hoisting `lu->stamp`, which without the promise
must be reloaded after each write to `mark`.

**The correctness half passed exactly as it had to.** All three gates PASS
with 0 regressed, 0 improved and 0 new, and `git diff` over the committed
records is empty: 139 answers and every work-unit count byte-identical. That
is the whole of what the work counter can say about this change, which is why
seconds were the only evidence left.

**The time ratio, `-j 1`, minimum of three alternating rounds, on the three
instances the LU dominates:**

| | shipping build | `-flto` removed |
|---|---|---|
| `dfl001` | 1.010x | 1.009x |
| `maros-r7` | 0.982x | 0.995x |
| `pilot87` | 0.992x | 1.012x |
| **geometric mean** | **0.995x** | **1.0053x** |

**The two builds disagree about the sign**, and every entry sits inside the
run-to-run spread of the binary that produced it — 0.66% to 1.43% across the
six sets of three, which is the same order as the 1.3% the comparison harness
repeats itself to (D60). So the finding is not "a small win". It is that the
effect is not resolvable from this machine's noise in either build, with the
point estimate landing on either side depending on which build is asked.

**The hypothesis this was run to test, and its refutation.** The obvious
explanation for nothing happening in the shipping build was that `-flto`
already proves what D75's audit proved by hand: the kernels sit behind an
internal header, so whole-program analysis sees the closed set of callers.
Removing LTO makes `lu.c` a single translation unit that genuinely cannot
know a caller's vector is not `lu->tmp` — the case where the qualifier has
the most to say. It said 1.0053x. **LTO is not what was absorbing it.**

**What actually explains it.** The loops `restrict` constrains are indexed
scatter and gather — `y[lidx[p]] -= lval[p] * ys`, `sum -= cval[p] *
y[cidx[p]]`. What costs there is the dependent load, and `restrict` does not
remove indirection; it removes redundant reloads of arrays that are not what
the loop is waiting on. And no loop here can be vectorised whatever the
compiler is told, because every one of them is a reduction or a scatter that
would have to reassociate, and `-ffp-contract=off` with no
`-fassociative-math` forbids exactly that. **The property that made the
change safe — that it cannot move a number — is the property that makes it
worthless: the transformations it unlocks are the ones this project has
already forbidden.**

**Refused, and the change reverted.** Not because it did nothing, but because
doing nothing is not free. A `restrict` local is an unenforceable promise: no
compiler checks it, no sanitizer catches it, and breaking it does not crash —
it produces a value read from a register that no longer matches memory, on
one instance, under optimisation, and not under `-O0`. Every future caller of
the LU solves would inherit a constraint it cannot see, in return for
something below the resolution of the instrument. That is the same trade D61
refused when inlining removed 470 million calls and came back 0.997x.

**What this does not establish.** Not that the effect is exactly zero — three
rounds on three instances bounds it to roughly ±1% and no finer. A pinned,
quiet measurement host could resolve it, and if one ever exists this is worth
half an hour. It would still have to beat the maintenance cost, and at ±1% it
does not.

**One observation, deliberately not a finding.** The LTO=0 binaries came out
*faster* than the LTO=1 ones on `maros-r7` and `pilot87` — 23.079 against
23.972 and 26.797 against 27.416 — which contradicts D62's 1.0330x for
`-flto`. The two campaigns ran minutes apart in separate sessions, so this is
precisely the cross-run comparison D45 and D60 say is not evidence, and it is
recorded here only so it is not discovered later and mistaken for one. D62
measured LTO over the whole standard set; these are three instances. Both can
be true. If it is worth settling, it needs its own same-session A/B.

Q11 is now closed in full. `--gc-sections` and `-fno-math-errno` were never
measured and this is the third reason in a row to expect nothing from them.

## D77 — A dimension change keeps the basis exactly when what is left is still a basis

The last structural gap in phase 2. `jaos_set_col_bounds` and
`jaos_set_coefficient` change what the numbers are; this changes how many
there are, which touches every array the model owns.

**Additions append, and that is the whole of why they are cheap.** New columns
occupy the indices above `num_col` and new rows above `num_row`, so no
existing index moves, no stored index has to be rewritten, and the entire
prefix of every array copies straight over. A caller holding column indices
across the call still holds the right ones.

**A new row is a transpose of the addition, not an append.** The matrix is
stored down and a row arrives across, so every column the new rows touch
gains entries. Doing that with `jaos_set_coefficient` would be one insertion
per entry, each one a `memmove` of the tail and a walk of every later column
start. Counting per column first makes it one rebuild: the new row indices
are all at least `num_row` and every old one is below it, so appending inside
each column keeps it ascending **with no sort at all**, and the loader's
invariant holds for free.

**Deletion takes a set, and that is a correctness decision rather than an
ergonomic one.** Deleting renumbers everything after what was deleted, so a
caller removing rows 2, 5 and 7 one at a time is deleting 2, then what was 6,
then what was 9. Given the whole set, JAOS renumbers once. An index named
twice is refused rather than absorbed: a caller who has named one twice has
lost track of what they are deleting, and the second deletion they believe
they are asking for is of something else.

### The basis, which is the only part that needed deciding

A stored basis is `num_col + num_row` statuses and a promise: exactly
`num_row` of them are basic. `jaos_set_basis` enforces that on anything handed
in, and `build_warm_basis` falls back to a cold start when it does not hold.
Four operations times "keep it or drop it" is eight cases to argue, and the
argument is the same one every time — so there is **one rule**: the basis
survives exactly when what is left still counts.

That is not a compromise, it is the definition. A basis whose count is wrong
is not a weaker starting point, it is not a basis.

What the rule then gives, without any case being written down:

- **Adding rows keeps it.** A new row's activity arrives basic, which is where
  the slack basis puts it, so basic and `num_row` grow together. This is the
  case worth having — a basis made primal infeasible by a new constraint is
  exactly the state the dual simplex is best at resuming from, and it is what
  a cutting-plane loop does every round.
- **Adding columns keeps it.** They arrive nonbasic at a bound; neither count
  moves.
- **Deleting normally does not**, and should not. Removing a row whose
  activity was nonbasic, or a column that was basic, leaves a count that no
  longer describes a basis.

**One exception, and it is D68's.** A new column with no finite bound has
nowhere to rest nonbasic, and a nonbasic free variable is the defect this
solver already carries: `can_move` has nowhere to send one and `wants_a_pivot`
reads it as sitting at an upper bound, so a negative reduced cost is never
repaired. Warm re-solve already refuses to create one. So does this — the
whole basis is dropped rather than one being manufactured. Dropping it on the
spot rather than leaving it for `build_warm_basis` matters: the model should
never hold a basis it already knows is unusable.

**A failed allocation loses the basis and not the operation.** A starting
basis is an optimisation and dropping one is always correct, so refusing an
otherwise complete modification because the *hint* could not be extended
would trade a working call for a faster one that never happens.

### What was measured

All 139 answers, iteration counts and work units unmoved across the three
gates, which is the expected result and worth stating for what it rules out:
no path the gate walks calls any of this, so a moved digest would have meant
the new code had reached the solver through the model struct.

Eight tests, and the two that matter are the ones built to be rejected: a new
column with infinite bounds must drop the basis, and deleting the basic column
must too. The second names which column is basic before deleting it — `x`
rests at its upper bound so it is nonbasic, `y` sits strictly inside its
bounds so it is basic, and one row means exactly one basic variable — because
a test that deletes "whichever one happened to be basic" is a test that stops
meaning anything the day the answer changes. The full suite runs clean under
ASan and UBSan.

The end-to-end case walks the whole path rather than the arrays: a model whose
optimum is 4.5, cut to 5 by an added row, dropped to 2 by an added column,
returned to 5 by deleting that column and to 4.5 by deleting the row. Each of
those four numbers is hand-computed in the test's own comment, because a
structural assertion about `a_start` cannot tell whether the scaling and the
factorization saw the change.

## D78 — A load was discarding the logging callback, and the list that preserved settings was the defect

Found by reading, while adding a field next to the ones it affected. Not
observed by any test, because the test that would have caught it is the one
nobody writes: configure, *then* load.

**The defect.** `model_release_arrays` ends in `memset(m, 0, sizeof *m)`, and
`jaos_load_lp` put four values back across it — the two budgets and the two
tolerances. `log_cb`, `log_user` and `log_level` were not among them. So

```c
jaos_set_log_callback(m, my_logger, ctx);
jaos_set_log_level(m, JAOS_LOG_DETAIL);
jaos_load_lp(m, ...);          /* the callback is gone here */
jaos_solve(m);                 /* silence, and no way to tell why */
```

Configure-then-load is the natural order to write it in — it is the order the
tolerance setters' own documentation implies — and it silently lost logging
from D65 until now.

**The comment beside that list had already predicted this**, in those words:
"a setting that is added without being added to this list is lost by anyone
who configures before loading — which is the natural order to write it in and
is how the primal tolerance was found to be dropped." It happened once, was
written down as a warning, and then happened again to the next setting added.
A comment stating an invariant has a failure rate of 100% given enough time,
and this is the receipt.

**So the repair is not "add three more lines to the list."** That fixes the
instance and leaves the mechanism, and the mechanism has now failed twice out
of the three times it was exercised. Configuration moved into a `jm_config`
sub-struct on the model, and `model_release_arrays` saves *that* across the
wipe and puts it back. There is nothing to remember, because a setting added
to the configuration is preserved by being inside the thing that is preserved.
`jaos_load_lp` lost its save-and-restore block entirely.

**Why this was safe to do mechanically.** 34 accesses across `src/` and the
tests, and every one that moved is a compile error if it is missed — `m->work_limit`
simply stops existing. The one thing to be careful of was the opposite
direction: `sx` carries its own resolved `primal_tol` and `dual_tol` (the
model's value or the built-in default), and a first pass moved those too.
The compiler caught all nine.

**Measured:** all 139 answers, iteration counts and work units unmoved across
the three gates — a struct that changes where a field lives must change no
number, and this is the check that says it did not. The regression test is
`test_configuration_survives_a_load`, which sets a log callback, a progress
callback, a work limit and a tolerance, *then* loads, and requires all four to
still be in force.

## D79 — A callback may look, and may stop a solve, and may not steer one

The last gap in phase 2, and it was left until last because it is the only one
that needed an answer rather than an implementation: what may a callback do to
a solve in progress, and what happens to determinism if it can stop one.

**What it may do: look, and ask to stop.** Not modify the model — the solver
is holding a scaling, a factorization and a basis derived from the model as it
stood, and changing the model underneath them leaves the two disagreeing with
nothing able to notice. And not steer: which column prices, when to
refactorize, whether a weight is worth carrying are the method, and D64's line
puts the method on the solver's side. A caller cannot know those answers and
being asked for them is a problem handed back.

**Determinism, stated exactly, because "it stays deterministic" would be a
dodge.** Two things hold and the second is the one that matters:

1. *When* the question is put is reproducible. The callback is invoked on a
   fixed iteration count and never on a clock — D65's rule for logging, for
   the same reason. The same model asks at the same iterations on every
   machine and every run.
2. Given the same sequence of answers, the solve is bit-identical, because
   nothing else about it depends on the callback existing.

What the caller decides is the caller's. If they decide on a clock, their
stopping point moves between runs — and **that is already true of
`jaos_set_time_limit`**, which has shipped since M1. This generalises a
precedent rather than breaking a rule: a callback that stops a solve *is* a
budget, one whose rule lives in the caller instead of in a number, which is
why the hook sits beside the budget checks and not somewhere of its own.

**Stopping is not answering.** `JAOS_SOLVE_INTERRUPTED`, appended to the enum
rather than inserted, so nothing below it renumbers for anyone who did not
recompile. `jaos_solution` and `jaos_objective` refuse. It keeps the basis it
stopped on and joins the list D70 wrote, so calling `jaos_solve` again
continues instead of starting over — an interrupt a caller cannot resume from
would be the half-budget D70 already rejected.

**What a watcher is told, and the one thing it is not.** Iterations, work
units, and the total primal infeasibility. **There is no objective, and its
absence is the design:** a dual simplex carries a point that is not feasible
until it finishes, so any objective reportable mid-solve is a number about a
point this library does not vouch for, and D20 is the rule against handing
those back. The infeasibility is not a substitute chosen for convenience — it
is the measure the solver's own progress and stall detection are written in,
so a watcher gets the real quantity rather than a plausible one. The first
call, before anything has been priced, reports the infinity that measure
starts at; that is documented rather than smoothed, because it is the true
answer to "how close is it" before the question has been asked once.

`PROGRESS_EVERY = 64` rather than `LOG_EVERY`'s 1000, because this one decides
something and a caller who wants to interrupt wants it to take effect. 64 is
already the granularity at which `TIME_CHECK_EVERY` settled that a stop is
responsive enough. A model with no callback pays one predictable branch.

**Measured:** all 139 answers, iteration counts and work units unmoved, which
is the claim that installing the hook changed no arithmetic. Three tests: a
watcher that always continues returns the same bits as no watcher — compared
with `EQUAL_MEMORY` and not a tolerance, because the claim is that the
arithmetic was untouched — and it asserts the watcher was actually called
first, since one that is never asked passes everything without proving
anything. A watcher that stops gets `INTERRUPTED`, is refused a solution,
keeps its basis, and the next solve finishes to the same objective a cold
solve reaches.

## D80 — The comparison was timing a warm re-solve, and the feature that broke it shipped two weeks earlier

Found by running the rungs T1 to T3 and refusing to believe the summary line.

**What the numbers said.** Every rung came back with JAOS ahead: 0.36x HiGHS's
time at T1, 0.28x at T3, and "iterations 0.00x" with awk dividing by zero.
Only one or two instances of ninety-four cleared the harness's 0.05s floor,
where T0 had cleared eighteen. Each rung finished in three minutes instead of
thirty.

**What was true.** `bench/compare/jaos_time.c` solves one model `repeats`
times and keeps the fastest, and since D68 a solve that reaches an optimum
leaves its basis on the model for the next one to resume from. So the first
repeat solved the problem and every later one re-solved it warm in a single
iteration. The minimum of N picked the warm run, every time. On `25fv47`:
**0.0015s and 0 iterations recorded, against a true 0.49s and 9459.**

**Nothing failed, and no single number looked wrong.** A 0.0015s solve is not
absurd on its own; an iteration count of 0 in a column of six-digit ones is
only visibly wrong if someone reads that column. This is the third time this
project has been handed a plausible table by an instrument that had quietly
changed what it measures — D60 was a comparison rebuilding a stale binary,
and D55 was work nobody billed.

**Where the lesson already existed.** The gate hit this exact problem and
solved it: its determinism check clears the basis between its two solves,
"or it would be a warm re-solve and would measure a sequence of calls rather
than the solver" (SPECS §8, D68). The comparison driver is a second program
that solves the same model twice, nobody thought about it on the day, and it
silently changed meaning. **A feature can redefine an instrument in a file
that the feature never touches.**

**The repair, and the guard that matters more.** `jaos_clear_basis` before
each timed solve, outside the timed region. And then the instrument checks
itself: two cold solves of one model are bit-identical by D8, so the repeats
must agree on iterations and work to the digit, and a disagreement now emits
`REPEATS-DISAGREE` and exits non-zero rather than being averaged into a
minimum. That check costs nothing and would have caught this on the first run
of the first rung.

**What is and is not affected.** The committed `T0.txt` is sound: it was taken
at `ccad702`, which predates warm re-solve, and its JAOS iteration counts
match the gate's record instance for instance — `25fv47` 9459 in both. So
D52, D53 and D60 stand. Everything measured by this harness *after* D68 landed
and before this entry is void, which is the three rung files produced an hour
ago and nothing else. All four rungs are being re-run together, T0 included:
the committed T0 is valid but it was taken in a different session with a
different driver, and a rung difference is only a measurement when both sides
come from one session, one machine and one binary (D45).

## D81 — The ladder is climbed: presolve is worth 1.42x, and a primal simplex is worth nothing

Phase 1's last open item. T1 to T3 exist now, all four rungs taken in one
session with one binary, and the two numbers the ladder was built to produce
are in. One of them reorders the plan.

**Read against the competitor itself, not through JAOS.** The harness reports
JAOS-versus-competitor at each rung, and a difference of two such ratios is a
weaker statement than it looks. Since JAOS is identical at every rung, the
direct measurement is competitor-at-T2 against competitor-at-T1, per instance,
geometric mean, over the instances where both rungs verified an answer and
both are above the 0.05s floor.

| step | what it removes | HiGHS | SoPlex |
|---|---|---|---|
| T0 → T1 | forcing the dual | 1.007x, **iterations 1.000x** | 0.976x, **iterations 1.000x** |
| T1 → T2 | presolve off | **1.417x**, iterations 1.287x | **1.136x**, iterations 1.106x |
| T2 → T3 | the remaining defaults | 0.997x | 0.999x |

### A primal simplex is worth nothing here, and the iteration counts prove it

T0 → T1 is not "small". **The iteration counts are identical — 1.000x, and the
records differ only in their header line.** Given the freedom to choose,
both competitors chose the dual simplex on every instance they were forced
into it at T0. The rung does not measure a feature JAOS lacks being worth
little; it measures the feature not being exercised at all, because on this
set the dual *is* the right method and a mature solver's own strategy picker
agrees.

So **JAOS having no primal simplex costs it nothing on the standard set.**
That does not remove it from the plan — phase 4's crossover needs one, and
carried defect 4 needs a primal step to repair a nonbasic free variable — but
it stops being a speed argument, and phase 6 item 7 should never again be
justified as one.

### Presolve is worth 1.42x, and that is smaller than the hole it was meant to fill

T1 → T2 is the number phase 3 has been waiting for since Q3 closed presolve
out of M1 for correctness reasons and never weighed it for speed. Presolve
buys HiGHS **1.417x** and SoPlex **1.136x**, and about 1.29x and 1.11x of that
is iterations it no longer has to run.

Put beside the gap it is supposed to close: JAOS is **3.71x** slower than
HiGHS at T0, where neither has presolve. A presolve as good as HiGHS's would
take JAOS to roughly **2.6x**, and the per-iteration cost — 2.53x, and
unmoved at 2.52x, 2.61x and 2.60x across all four rungs — would still be the
whole of what is left.

**That reorders the plan.** `PLAN.md` has presolve as phase 3 and the
per-iteration work as phase 6, on the stated grounds that presolve is "the
largest single algorithmic gap" and that phase 1 would say what it is worth.
Phase 1 has now said: it is worth 1.42x against a per-iteration factor of
2.53x that no rung moves. The cheaper iteration is the larger lever and it is
also the one already in progress.

T2 → T3 adds nothing (0.997x, 0.999x), which is worth recording so nobody
costs it again: at this size the defaults a user gets are the presolve and
nothing else. Parallelism does not appear because these models are too small
for it to pay.

### What validates all of it

**JAOS is the control, and it is a good one.** JAOS is byte-identical at every
rung, so its own cross-rung ratio is a direct reading of what this machine
does to a repeated measurement: **1.007x, 1.014x and 1.012x over T1, T2 and
T3 against T0, with iterations exactly 1.000x every time.** So the harness
repeats itself to about **1.4%** across four separate sessions — measured,
not assumed, and consistent with the 1.3% D60 estimated a different way. Every
step above except the two presolve columns is inside that floor, which is the
correct way to read "1.007x" as "nothing".

And the answers are the right answers: JAOS's iteration count in the
comparison record matches the gate's committed record instance for instance at
every rung — `25fv47` 9459, `truss` 17336, `stocfor2` 2263 — which is the
check D80 added after the harness spent an hour timing a warm re-solve.

T0 re-measured at 3.71x against HiGHS and 1.35x against SoPlex, against the
committed 3.70x and 1.31x. The first reproduces to 0.3%; the second is 3%
out, which is above the floor and is the honest caveat on this entry: SoPlex's
readings are noisier than HiGHS's, and its "faster on 10 of 22" against the
recorded 11 of 22 moves for the same reason.

## D82 — Partial pricing on the leaving-row sweep, refused: it saves the cheap units and buys the expensive ones

Phase 6 item 3, and the first change in this solver that could not be judged
on digests. It moves the search path, so it needed the full gate and a
different standard of evidence, and the evidence refuses it twice over.

**What was built.** `price_row` scans a rotating slice of the basic variables
instead of all of them, taking the best steepest-edge candidate inside it and
extending to the next slice only when a slice yields nothing. Two things were
not negotiable and are worth keeping written down, because any future attempt
has to honour both:

- **Bland's rule cannot be given a slice.** It promises the globally
  lowest-indexed violating variable, and that promise is the only thing
  between a degenerate solve and a cycle (D26). A slice can return an index
  higher than one it never examined, which is not Bland's rule on a budget —
  it is a different rule with none of the guarantee. So Bland forces a full
  sweep.
- **The progress measure cannot be fed a partial total.** `total` is where
  `infeas_best`, the stall counter and the Bland switch are written. A
  slice's total is smaller for a reason that is not progress, so letting one
  through would reset the stall counter every time the slice missed the
  violations, turning the one detector that catches a cycle into a thing that
  never fires. Full sweeps therefore happen on a fixed cadence too.

The sweep was billed for what it read rather than for the dimension, because
a sweep that charged a full pass for a slice would have hidden the whole
effect in the unit this project measures in (D16).

### The measurement

Reference is the committed record, which is sound here and would not be for
seconds: iterations and work units are deterministic integers, so they need
no same-session pairing. Geometric mean of per-instance ratios.

| | standard iters | standard work | Kennington iters | Kennington work |
|---|---|---|---|---|
| partitions = 2 | 1.031x | 1.005x | 1.099x | **0.891x** |
| partitions = 8 | 1.037x | 1.006x | 1.243x | **0.897x** |
| partitions = 32 | 1.126x | 1.111x | 1.343x | 1.070x |

**The 11% Kennington saving is the interesting number, and it is not real.**
It is exactly the failure D45 describes: the work counter bills one unit per
row of the pricing sweep, and D45 measured those `nrow` sweeps as *nearly
free* per unit while the `nvar` work they are traded against is expensive.
This change removes seven-eighths of the cheapest units in the solver and
pays for them with 10% to 24% more iterations, each of which drags in two
triangular solves and a pricing row. A ratio of 0.891 in units is a loss in
seconds, and the counter cannot see it — which is the whole reason D45 exists
and why a same-instance time ratio is the third leg of a verdict here.

### What actually closes it

Correctness, at every setting that was cheap enough to be worth having.

- **`pilot` publishes OPTIMAL on a wrong answer.** Objective −557.48693
  against Koch's −557.48973: `objective=OUT-OF-TOLERANCE`, and **the checker
  passes it**. That is D47's hole firing on a real instance rather than a
  constructed one, and it is now the second known route to a wrong `pilot`
  answer beside D73's refactorization intervals.
- **`wood1p` is rejected by the checker**, dual infeasibility 1.73e+05, at
  6061 iterations against 335 and 24.8x the work.
- `d2q06c` 3.3x the work, `greenbeb` 2.3x, and `woodw` **131.66x** the
  iterations at 32 partitions.

The standard gate reads NOT MET at 2 and at 8 partitions, and PASS at 32.
**That order is not a mistake and it is not an argument for 32**: which
instances break is scattered rather than ordered in the parameter, exactly as
D39 found sweeping `REFACTOR_EVERY` over 16..256, so a value that happens to
pass is not a value that is safe. Choosing 32 on this evidence would be
fitting a constant to whichever instances survived it, which is how this
project loses weeks.

### What is refused, and what is not

**Partial pricing on the leaving-row sweep is refused and the code is
reverted.** The idea is not that the sweep is free — it is that this sweep is
the wrong one to make cheaper, because the units it saves are the ones that
cost the least real time and the iterations it adds cost the most.

**Multiple pricing is untouched and still open.** It is a different technique
— select several candidates in one major iteration and run minor iterations
over that subset — and it does not trade candidate quality for scan length in
the same way. Nothing here measures it.

Two things from this attempt are kept because they are useful to the next one.
`EXTRA_CFLAGS` in the Makefile, so a method constant can be swept over a range
without editing the source between runs. And the discipline of proving the
restructuring is a bit-exact no-op *before* enabling anything: with
`PRICE_MIN_ROWS` above every model in the sets, all 139 answers and work
counts were identical, which is what made every number above attributable to
partial pricing rather than to the rewrite.

**And a note on how nearly this was not measured at all.** The first run of
this sweep reported exactly 1.0000x at every setting on both sets, because
`make` does not know that `CFLAGS` changed: after the first build the objects
were newer than the sources and every later setting rebuilt nothing, so one
binary was measured six times. The result was too clean to be a measurement.
Every sweep here now opens with a canary — a setting that *must* move the
numbers — and stops if it does not.

## D83 — Clp is the third reading, and the two were not agreeing by coincidence

The last open item in phase 1. `bench/compare/README.md` says why Clp is here:
"the one that shows whether the other two agree by coincidence." It does not.

All four rungs, three competitors, one session, one binary, on a still tree.

| tier T0 | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 3.72x | 1.34x | 3.77x |
| iterations | 1.47x | **0.70x** | 1.67x |
| **time per iteration** | **2.54x** | **1.92x** | **2.26x** |
| JAOS faster on | 0 of 18 | 10 of 22 | 0 of 17 |
| worst instance | `maros-r7` 25.7x | `maros-r7` 9.4x | `stocfor3` 14.6x |

**The per-iteration ratio is the finding.** Three separately written dual
simplexes — one from Huangfu and Hall's work, one from Wunderling's, one from
Forrest's — put JAOS's cost per iteration at 2.54x, 1.92x and 2.26x. They
disagree about everything else. Clp takes 1.67x JAOS's iteration count where
SoPlex takes 0.70x, and Clp lands within 1.4% of HiGHS on total time while
arriving there by a different route. **A quantity that survives three
implementations disagreeing around it is a property of JAOS**, which is what
this rung was built to establish and what the whole of phase 6 is aimed at.

There is a pleasing detail in the lineage. Clp's principal author is John
Forrest, and two entries in this project's own bibliography are his — [5],
the Forrest-Tomlin update JAOS implements, and [8], the Forrest-Goldfarb
steepest edge JAOS prices with. JAOS is built from this man's papers and has
never read his code, which is the exact line D12 draws.

**The rungs reproduce.** Taken in a fresh session with a third competitor
added, presolve reads 1.421x for HiGHS against the 1.417x of D81 and 1.111x
for SoPlex against 1.136x, and free algorithm choice reads 1.003x and 0.997x
on iteration counts that are again exactly 1.000x. The control is clean:
JAOS, byte-identical at every rung, reads 1.006x, 1.011x and 1.018x with
iterations exactly 1.000x. D81 stands as measured.

**Phase 1 is complete.** Every question it was opened to answer has a number:
what the gap is, what it decomposes into, what presolve is worth, what a
primal simplex is worth, and whether any of it is an artefact of one rival.

## D84 — Multiple pricing, refused too, and phase 6 item 3 closes with both halves measured

D82 refused partial pricing and said multiple pricing was untouched and still
open, because it is a different technique. It was built and measured. It is
also refused, and for a different reason, which is why the entry is worth
having rather than folding into D82.

**What was built.** A major iteration sweeps every basic variable and sets
aside the K best leaving rows; the minor iterations that follow re-score that
shortlist and take the best row still violating a bound, without sweeping
again. Nothing stored is trusted: between two pivots x_B moves and every
steepest-edge weight is updated, so a candidate's violation is recomputed and
re-scored at the moment it is used, and a row that has become feasible is
dropped. Bland's rule never reads the shortlist, for the reason D82 gives.

**K = 1 is the technique switched off by construction** — one candidate set
aside, used immediately, a fresh sweep next iteration — so the no-op check is
a setting rather than a separate build. It confirmed: three gates ran, 139
digests and work counts identical. Every number below is therefore the
technique and not the restructuring.

### The measurement

| K | standard iters | standard work | worst standard | Kennington iters | Kennington work |
|---|---|---|---|---|---|
| 2 | 1.176x | 1.139x | `wood1p` 88.7x | 1.034x | **0.819x** |
| 4 | 1.269x | 1.263x | `fffff800` 32.5x | 1.079x | **0.747x** |
| 8 | 1.486x | 1.517x | `brandy` 140.7x | 1.078x | **0.667x** |
| 16 | 1.560x | 1.673x | `woodw` 128.0x | 1.077x | 0.666x |

**The standard set closes it.** The gate reads NOT MET at every setting — 6
instances regressed at K=2, 21 at K=4, 32 at K=8 — and the cost is monotone in
K in both iterations and work. Individual instances come apart: `brandy` at
140x its iterations, `wood1p` at 89x, `woodw` at 128x. There is no value of K
that is merely a worse trade; the mildest setting already fails.

**And the Kennington column is the honest tension in this entry.** A third of
the billed work removed for 8% more iterations is a much better trade than
partial pricing ever showed — D82 managed 0.897x — and on the largest models
the sweep is genuinely large: `ken-18` has 105,127 rows, so an O(`nrow`) pass
per pivot is a real share of what gets billed.

It is not trusted, for D45's reason and not from suspicion of the number
itself. The units removed are the pricing sweep's, one per row, and D45
measured `nrow` sweeps as nearly free per unit while the work they trade
against is expensive. So a 0.667x in units is an unknown in seconds, and the
one measurement that would settle it — a same-instance time ratio at `-j 1` on
Kennington — was not taken, **because it cannot change the verdict**: the
technique fails the standard gate at every setting, and a technique that
returns wrong answers is not accepted on the strength of being fast on the
other set.

That is worth writing down precisely, because it is the one direction in this
item that pointed somewhere. If a future attempt confines a pricing shortlist
to models where the sweep dominates — and `PRICE_MIN_ROWS` in D82's build was
exactly such a gate, never combined with this — the Kennington column is where
to look first, and seconds rather than units are what it has to produce.

### What phase 6 item 3 now knows

Both halves are measured and both are refused. The item is closed as a *speed*
lever, and what closed it is the same fact twice: **the leaving-row sweep is
the wrong thing to make cheaper.** Its units are the cheapest in the solver,
and every scheme for scanning it less often pays in trajectory — worse
candidates, more iterations, and on this evidence wrong answers.

The profile taken while checking this item's premise says where to look
instead. On `truss` under callgrind, `admit_candidate` is **14.98%** of
instructions against `ftran_prefix` at 6.68%, and it is the ratio test's
candidate admission — called once per nonbasic variable of the pricing row,
the O(`nvar`) half of D61's 36.5%, never examined. Restricting *that*
candidate set decides which column enters rather than which row leaves, which
is a different and more dangerous change than either of the two refused here.
Callgrind counts instructions and not seconds and cannot see locality, the
limit D58 and D61 also stated.

---

## D85 — A free nonbasic improves in the direction its reduced cost points, and the status was never able to say which that was

### The question

`PLAN.md` carried this as the fourth of the known defects, found by
construction in D68 and never observed on any of the 139 instances. A
nonbasic *free* variable — one with neither bound, resting at zero — is dual
feasible only at a reduced cost of exactly zero, so it is the one status whose
reduced cost can be wrong in either direction. Three places read it and two
read it wrongly:

- `can_move` has nowhere to send a free variable and returns false. That is
  correct and stays: moving to the other bound is what it does, and there is
  no other bound.
- `wants_a_pivot` computed the size of the breach as
  `status == JM_AT_LOWER ? -d : d`, which puts a free variable in the
  upper-bound branch. A positive reduced cost was therefore repaired and a
  **negative one silently dropped**.
- `primal_ratio_test` took the same branch for its direction,
  `status == JM_AT_LOWER ? 1.0 : -1.0`, so had a free column ever reached it
  with `d < 0` it would have travelled the wrong way.

The expectation going in was that the state would be hard to reach, because
D68 had found it by construction and 139 instances had not. That turned out to
be right, and *why* it is right is the part worth having.

### Why 139 instances never found it

`admit_candidate` gives a free variable a ratio-test distance of exactly zero,
so its Harris ratio is zero and it is inside the window on every iteration
where the pricing row touches it at all. A free nonbasic is therefore the
strongest candidate the dual ratio test can see, and the method takes it back
into the basis at the first opportunity. Its column has to miss every pricing
row of the rest of the solve to survive.

So the state survives to the end only when **no dual iteration runs at all**
after it is created — that is, when the point that creates it is already
primal feasible. Two constructions were built before that was understood and
both were repaired by the solver, correctly, on the way past:

- A free *logical*, from a row with no bounds, evicted by
  `repair_singular_basis`. It never got evicted: the eviction picks the
  position the LU did not pivot on, and on a rank-1 basis of two singleton
  columns the LU pivots on the higher position, so the *structural* column was
  the one thrown out. Confirmed on an instrumented build —
  `[repair] rank=1 p=0 leaving=1` — not deduced.
- The same shape with the free column at the low position but the resulting
  point primal infeasible. The dual method ran, `admit_candidate` admitted the
  free column at distance zero, and it was priced back in within the iteration.

### The construction that does reach it

```
min -f          r0:  f + 2g  in [2, 6]          r1:  h in [0, 1]
f free,         g in [0, 2],                    h in [0, 1]
```

The optimum is **-6** at `f = 6, g = 0`, and a cold solve finds it.

Handed the basis `{f, g}` through `jaos_set_basis`, both columns live in `r0`
alone, so `B` has rank 1. `repair_singular_basis` pivots on the higher
position and evicts `f`, which has neither bound, to `JM_FREE`. That pins `f`
at zero, and the point that results — `g = 1`, everything else on a bound — is
**primal feasible**, so the dual method stops without an iteration and the
free column is never priced. Everything then rests on the primal clean-up.

Measured on an instrumented copy of the sources, before the repair:

```
[repair] rank=1 p=0 leaving=0 entering=4 lo=-inf up=inf
[cleanup] v=0 status=3 d=-1 breach=1 wants=0
cold=-6   hostile=0   verdict=optimal
```

`dual_breach` sees the violation and reports 1. `wants_a_pivot` returns false
on it. The solver publishes **0.0 with a verdict of OPTIMAL** where the
optimum is -6 — and that breaks a promise `jaos_set_basis` makes in the
header in as many words: a basis that is wrong, stale or hostile costs
iterations and cannot produce a wrong verdict.

### The repair, and why it cannot move anything else

Both sites now read the sign of the reduced cost instead of the status:

- `wants_a_pivot` measures the breach as `fabs(s->d[v])`.
- `primal_ratio_test` takes its direction from `s->d[q] < 0.0 ? 1.0 : -1.0`.

**Both are bit-identical to what they replaced for every bounded status**, and
that is by construction rather than by measurement. A column reaches either
site only past `dual_breach`, which has already established that `d < -DUAL_TOL`
at a lower bound and `d > DUAL_TOL` at an upper one. So `fabs(d)` *is* the old
signed expression there, and `d < 0` picks out exactly the two cases the status
test named. The only input whose behaviour can change is the one the old form
had no correct answer for.

The measurement agrees, which is what makes the argument checkable rather than
merely plausible: **all 139 answers, iteration counts and work units
unmoved** — `bench/results/` regenerates byte-identical to the committed
records across all three sets, 0 regressed and 0 improved on each. 73 unit
tests and ASan+UBSan clean. On the constructed model the answer goes from 0.0
to -6, matching the cold solve exactly.

### What was refuted

Nothing was refuted here, which is unusual enough to say plainly: the first
plausible repair was the correct one. What was refuted is two *constructions*,
above, and they cost more than the fix did. The lesson they carry is the one
worth keeping — the free nonbasic is not hard to create, it is hard to make
*survive*, because `admit_candidate` treats it as the best candidate in the
model. Any future attempt to reproduce this class of defect has to arrange for
zero dual iterations after the state appears, and the cheapest way to arrange
that is a repaired singular basis whose resulting point is already feasible.

### What is left open

`build_warm_basis` refuses any handed-in basis containing a nonbasic variable
with neither bound, and D68 put that refusal there *because of this defect* —
the comment beside it names the defect as its own repair. The premise of the
refusal is now gone, and lifting it is worth a measured amount: D69 says
`cycle` loses its entire warm start to it, one instance in 92.

That is deliberately not taken here. It is a second change, it moves warm
trajectories rather than nothing at all, and it needs the warm campaigns
(`make warm`, `make warm-kennington`) and not just the gate. Handed to
`PLAN.md`.

---

## D86 — `pilot87`'s iteration guard is a factorization that stopped agreeing with itself, and the two solves each iteration already pays for can say so

### The question

`PLAN.md` carried this as the third of the known defects. D72 established what
it is *not*: `pilot87` at a refactorization interval of 128 runs 588,725
iterations under Bland's rule without total primal infeasibility improving
once, over 1,136,521 **distinct** basis states, so the anti-cycling rule is
doing exactly what it promises and there is no cycle. D72's conclusion was
that the anti-cycling rule and the progress measure are about different
quantities, and that the cure is a progress measure the dual method actually
moves — the dual objective.

That cure is refuted here, and so is the objection raised against it. What the
measurements found instead is a different defect.

### Two refutations, so neither is attempted again

**The dual objective is not monotone on this solve.** Instrumented at interval
128, **757,263 of 1,382,801 steps decrease it** — 55%. Signed movement is
+1.32e217 against −2.18e214. As a progress measure it would oscillate more
than the primal infeasibility it was proposed to replace, not less.

**And it is not because the steps are degenerate**, which was the obvious
reason to expect it to be blind. Under Bland only **14.3%** of steps are dual
degenerate, 162,962 against 973,576. The dual objective moves on the large
majority of steps. It moves in both directions.

That second measurement is what turned the investigation, because a dual step
that *lowers* the dual objective is not a dual simplex step at all.

### What it actually is

`max |theta_dual|` at interval 128 is **1.21e213**, on a model whose optimum
is 301.7. Reconstructed from the model's own costs, the basis objective jumps
194 → −111379 → 105 → 11 → 30.9 → 6928 → 0, and sits at exactly 0 for the last
500,000 iterations while `infeas_best` is pinned at 25.4152.

The control settles it. The same instrument at three intervals:

| interval | result | steps | max abs theta_dual | steps > 1e6 | max 1/abs alpha_q |
|---|---|---|---|---|---|
| 64 | optimal | 50,833 | **0.625** | **0** | 4.0e5 |
| 96 | optimal | 45,629 | **0.623** | **0** | 1.5e5 |
| 128 | guard | 1,382,801 | **1.21e213** | **930,897** | 1.0e9 |

A healthy dual step here is about 0.6 and **not one of them exceeds 1e6** at
either working interval. At 128 two thirds of all steps do. `theta_dual` is
`d_q / alpha_q`, and `1/|alpha_q|` reaching 1e9 says where it comes from: the
ratio test is being handed pivot elements by a factorization that no longer
describes the basis. **Bland is not failing. It is grinding on numbers that
stopped meaning anything**, and it explains D39's finding that 64 is one of
only two completely clean values in a sweep of 16..256.

This is the stability trigger PLAN 2.5.5 asked for and that was never built.
`REFACTOR_EVERY`'s own comment said as much: "only the interval and the
reactive fallback on a failed update exist so far". An interval alone cannot
notice that it has become too long for a particular model.

### The detector, which costs nothing to compute

Each iteration already computes the pivot element twice, by two independent
routes against the same factorization:

- `alpha_q` — row *r* of `B^-1` dotted with column *q*, which arrives by BTRAN
  as part of the pricing row.
- `col[r]` — column *q* transformed by FTRAN, which the basis update needs
  anyway.

They are the same number in exact arithmetic. Their relative difference is
therefore not a heuristic about conditioning: it is **the patched
factorization contradicting itself**, read off work the iteration was already
paying for. This is the standard safeguard in [1] and [2], and it is step 4 of
this project's own debugging discipline — do two computations of one quantity
agree.

### The threshold, and the plateau that makes it robust

Measured over **all 139 gate instances at the shipped interval**:

| set | worst disagreement | pivots above 1e-7 |
|---|---|---|
| standard 94 | 2.55e-08 | **0** |
| Kennington 16 | 3.49e-10 | **0** |
| infeasible 29 | 7.83e-08 | **0** |
| **all 139** | **7.83e-08** | **0** |

Against **1.99** on `pilot87` at 128 — two computations of one number
differing by more than the number itself.

And the value inside that gap barely matters, which is the part worth having.
The first pivot to cross 1e-7, 1e-6, 1e-5, 1e-4 **and** 1e-3 is the same one,
**iteration 120880 of 1382801**. The decay does not creep in; it arrives. So
the constant sits on a four-decade plateau, and `LU_AGREE_TOL = 1e-5` is its
middle: 128x above the worst healthy pivot anywhere in the gate, 1e5 below the
broken one.

That crossing point is also why the detector is worth having rather than
merely correct: it fires at **8.7% of the solve**, before the remaining 91% of
the iterations are spent.

### What the repair had to get right

**The ordering, which is the design and not tidiness.** `pivot()` used to step
every reduced cost before it FTRANed the entering column, so by the time both
quantities existed the state was already half-changed and there was no
iteration left to abandon. The FTRAN is hoisted above the dual update. That
moves no arithmetic: the two share no buffers — `raw`, `col`, `cpat` against
`d`, `shift`, `cost` — and the work units they bill are integers whose order
of addition cannot change a total.

**Termination, which is where the easy mistake is.** A pivot is declined only
while `lu.n_updates > 0`. On a factorization just built from scratch the same
disagreement means the *basis* is that badly conditioned, and rebuilding would
produce the same numbers and the same refusal forever. There the pivot is
taken: the worse of two options and the only one that ends.

**Not billing a declined iteration.** Nothing moved, and counting it would let
a run of declines exhaust the iteration guard while reporting progress that
was never made.

### What it cost

**All 139 answers, work units and iteration counts unmoved**, across all three
sets, `bench/results/` regenerating byte-identical to the committed records.
That is by construction rather than by luck: no pivot anywhere in the gate
reaches 1e-5, so the detector cannot fire there.

And on the trajectories the gate does not walk, `pilot87`:

| interval | before | after |
|---|---|---|
| 96 | optimal, 45,653 iters | optimal, 45,653 iters — unchanged |
| **128** | **guard tripped, 1,382,801 iters** | **optimal, 214,631 iters** |
| 160 | broken (D39) | optimal, 51,691 iters |
| 256 | broken (D39) | **still broken** — abandoned after 25 minutes against 88 s at 128 |

**256 is the honest limit of this and is stated rather than omitted.** The
trigger makes a too-long interval survivable, not unbounded: past some point
the updates accumulate faster than a disagreement-driven rebuild can clear
them, and every iteration turns into a rebuild plus a wasted pivot. What that
buys at 256 is a different failure — slow instead of wrong — which is an
improvement in kind and not a cure.

### What is left open

The guard's message was separately wrong and is now corrected: it called a
solve "a JAOS defect" while Bland was doing exactly what it promises. It now
also reports how many pivots were declined, which is the number that
distinguishes a model this trigger is working hard on from one that is simply
large.

Whether `REFACTOR_EVERY` should move is *not* decided here and should not be
inferred from the table above. D39 chose 64 by sweeping correctness across
16..256, and one instance solving at 160 is not that sweep. The trigger makes
the interval safer; it does not re-open the question of its value.

---

## D87 — The checker bounds what the rows imply, which closes D47's constructed case and not its real one

### The question

D47 left the checker certifying a bound it cannot prove: where a wrong-signed
multiplier sits on an unbounded improving direction the dual objective's term
is minus infinity, `sign_condition` drops it, and `gap_positive` — documented
in `jaos.h` as satisfying `P - P* <= gap_positive` — can read zero on a point
that is arbitrarily suboptimal. Two routes were named. **Route A**, report the
bound as void, was refuted by D71: 98 of 110 accepted answers carry a dropped
term, so voiding would void the gate. **Route B**, compute what the column is
worth, was costed at a basis and a factorization inside the checker, which D18
forbids taking from the solver — and D73's cheap version of it was refuted
because a column moving alone cannot move at a vertex.

The open question was what "independent" would mean once the checker held a
factorization. **It turns out not to need one**, and the answer is worth
having even though it does not finish the job.

### The observation

The term is minus infinity because the variable has no bound on the improving
side. But the rows frequently *imply* one. From `rl <= sum_k a_ik x_k <= ru`,
holding every other variable inside its own box gives a finite range for `x_j`
whenever the rest of that row is bounded the right way. That is the standard
activity-based tightening a presolve does, and reading it uses **nothing but
the model**.

**It is sound for a reason worth stating precisely.** An implied bound is one
every feasible point already satisfies, so adding it changes neither the
feasible region nor `P*`. A dual bound valid for the tightened problem is
therefore valid for the original — which is exactly the claim `gap_positive`
was making and could not support. All three legs of D18 stand: independent
inputs (model and claimed solution only), redundant identities (one more now),
better arithmetic (`long double` throughout).

### What it closes

D47's constructed case, exactly. `min -1e-7 x2` subject to `x1 + x2 <= 1e6`
with both variables non-negative and neither bounded above, offered the origin
with a zero row dual:

| row bound | true suboptimality | `gap_positive` before | after |
|---|---|---|---|
| 1e6 | 0.1 | **0** | **0.1** |
| 1e8 | 10 | **0** | **10** |
| 1e3 | 1e-4 | **0** | **1e-4** |

`dropped_terms` goes to 0, `gap_certified` to true, and `dual_feasible` to
**false**: the checker refuses the point it used to certify. The bound is now
the true suboptimality to the digit, reached with no basis, no factorization
and no reference value.

Across the gate, certification roughly doubles: **12 of 110 accepted answers
certified before, 27 after** — 7 to 17 on the standard set, 5 to 10 on
Kennington. All 139 answers identical, all three gates PASS with 0 regressed,
167 unit tests green. The feared failure — 98 previously dropped terms all
becoming live and inflating the gap into false rejections — did not happen.

### What it does not close, which is the case that mattered

**`pilot` at refactorization intervals 24, 32 and 96 still publishes an answer
1.04e-3 out of tolerance with every checker number green.**

```
interval 24  objective=OUT-OF-TOLERANCE  checker=ok  drop=3.47e-07  cert=no
interval 32  objective=OUT-OF-TOLERANCE  checker=ok  drop=3.47e-07  cert=no
interval 96  objective=OUT-OF-TOLERANCE  checker=ok  drop=3.47e-07  cert=no
```

The same term is dropped as before. The rows of `pilot` do not imply any
finite bound on column 1534, whose reduced cost of -3.474e-07 and true value
of 2990.37 are the whole of the error.

**And the coverage says this is a mechanism limit rather than a near miss:**

| | columns with no explicit bound | rows imply one |
|---|---|---|
| `pilot`, upper | 2409 | **588 (24%)** |
| standard set, upper | 172611 | **66297 (38.4%)** |
| standard set, lower | 380 | **175 (46.1%)** |

Row-at-a-time tightening reaches about two fifths of the unbounded columns. It
misses the one that costs `pilot` its answer, and the reason is the one D47
already gave: what makes a dropped term expensive is how far the variable
travels, and that is **a property of the polytope, not of any single row**.
Column 1534's lever arm only appears by combining rows; no row bounds it
alone.

### Iterating the propagation is refuted, and it is refuted the interesting way

The obvious extension is to iterate: a bound derived this round makes its
row's range finite, which should let the next round reach a column the first
could not — ordinary constraint propagation, and it would plainly bound more
of `pilot`'s 2409 unbounded columns than 588.

It was built and measured at eight rounds. **It rejects the intervals where
`pilot` is right**:

| interval | objective | checker | gap |
|---|---|---|---|
| 16 | ok | **REJECTED** | 3.45e-05 |
| 24 | out of tolerance | REJECTED | 0.0025 |
| **64 — the shipped one** | **ok** | **REJECTED** | 3.45e-05 |
| 96 | out of tolerance | REJECTED | 0.0025 |

It does separate — 0.0025 against 3.45e-05 is a factor of 72 — but both are
past `CHECK_TOL`, so the gate breaks.

**And the reason is the part worth keeping.** An implied bound is *sound* but
*slack*. The gap identity is `P - D = sum_v w_v (v - bound_v)`, and every term
is zero at an optimum because complementary slackness puts each variable on
the bound its multiplier points at. A variable does not rest on its *implied*
bound — nothing put it there — so `w_v (v - bound_v)` is a live term measuring
the slack of the bound rather than the badness of the point. The published
duals are optimal for the original problem, not for the tightened one.

So the gap stops being a statement about the answer alone and becomes one
about the answer *and* how well the rows happen to bound the model. Tightening
harder makes it worse, not better, which is the opposite of the intuition that
motivates iterating.

### The margin, which is smaller than it was

That effect is present at one round too, and the honest figure is:

| | worst gap over the gate | margin against `CHECK_TOL` = 1e-6 |
|---|---|---|
| before | 5.09e-11 | 19600x |
| **after, one round** | **2.43e-08** | **41x** |
| eight rounds | 3.45e-05 | **rejects** |

One round costs three of the four and a half orders of margin the checker had.
41x is a real margin and the gate passes on it, but it is no longer roomy, and
a future change that adds terms to the dual objective has much less room than
this one did. That is stated here rather than discovered later.

### What this settles, and what it hands on

Settled: the implied bound is sound, cheap, independent, and worth having on
its own terms — it doubles certification, closes the constructed case, and
costs nothing measurable. It is kept at **one** round.

Also settled, and it is a constraint on every future attempt: **the gap cannot
absorb slack bounds.** Anything that makes `gap_positive` a true bound by
adding terms will spend the checker's margin, and there is now 41x of it. What
would break that trade is separating the two questions the gap currently
answers at once — how far from optimal the point is, and how well the bound
can be certified — which is a redesign of the report and needs its own entry.

**Not settled: defect 1 remains open**, with its scope reduced rather than
removed, and this entry is deliberately not written as closing it. The gap the
repair leaves is exactly the cases where no single row bounds the column, and
`pilot` sits in it. What would reach those is a bound derived from more than
one row at a time — which is a small LP in its own right, and reopens D47's
cost question in a different currency than the factorization it first named.

The one thing this does remove is the argument that route B needs the solver's
basis. It does not. Whatever closes the rest will not need one either.

---

## D88 — The gate watches the dropped term, because the checker cannot judge it and the predicate cannot see it

### The question

D87 left defect 1 open with its scope reduced: the checker bounds unbounded
variables by what the rows imply, which closes D47's constructed case and
lifts certification from 12 of 110 to 27, but `pilot` at refactorization
intervals 24, 32 and 96 still reports `checker=ok` on an answer 1.04e-3 out of
tolerance. Row-at-a-time tightening reaches 38.4% of unbounded columns and not
the one that costs `pilot` its answer.

Two routes remained. Derive the polytope direction inside the checker — the
original route B, which needs a factorization of its own and therefore a
second LU, since using `jm_lu_*` would link the checker against solver
internals and is the coupling D18 exists to forbid. Or accept that the checker
cannot judge a dropped term and make the *gate* watch it change.

**The second was chosen with the maintainer, 2026-08-11.** It does not repair
the checker and is not written up as doing so. It closes what the hole
actually cost.

### What the hole actually cost

D82. Partial pricing published an answer out of tolerance on `pilot` with
every checker number green, and this gate passed it. That is not a
hypothetical about a guarantee; it is a bad change reaching a decision because
the check meant to stop it could not see the quantity that had moved.

Watching that quantity *change* needs none of the judgement the checker cannot
make. D47 established that no local test on a reduced cost separates a harmful
dropped term from a harmless one, because what makes one expensive is the
distance the variable travels. But a **regression** in it is a different
question, and an easy one.

### Both constants are measured, on both sides

`DROP_REGRESSION_FACTOR = 2.0`, `DROP_FLOOR = 1e-9`.

**The quantity is as deterministic as a digest.** Across the solver change
that closed defect 3 — which moved no answers — **0 of 94** dropped terms
moved at all. Across the checker change of D87, 40 of 94 moved and every one
of them **downwards**, worst growth 0.9176x. Nothing legitimate has been seen
to grow one.

**The case it has to catch is a factor of 40.** `pilot` carries 8.62e-09 where
it is right and 3.47e-07 where it is wrong. A factor of 2 clears that with
twenty times to spare, and there is no measured growth on the other side for
it to collide with.

**The floor keeps arithmetic noise out.** 82 of the standard set's 94
instances carry a dropped term below 1e-9, decaying smoothly to 1e-17 (D71);
ratios between those numbers mean nothing. `pilot` at its correct value sits
at 8.62e-09, above the floor, so the case that matters is still compared.

### The instrument was calibrated before being believed

An instrument that finds nothing is worth nothing until it has been shown able
to find something, so a real fault was injected: `pilot`'s baseline drop
divided by 100.

```
pilot        REGRESSED    dropped term: 8.62e-11 -> 8.62e-09 (100.0x),
                          the checker's bound is that much less of one
baseline: 1 regressed, 0 improved, 0 new        (runner exits nonzero)
```

Caught, and the runner fails on it. Restored, and the gate returns to PASS
with 0 regressed. A nine-field baseline written before this existed is still
read, with its drop taken as "nothing to compare against" rather than as zero
— an older baseline costs the reader that one check and not the whole run.

### What this is not

It is not a repair of the checker, and defect 1 stays open in `PLAN.md`. The
checker still cannot certify `gap_positive` where no row bounds the column,
`pilot` at 24, 32 and 96 still reads `checker=ok`, and a *first* run that is
already wrong still passes — this catches the change from right to wrong, not
wrongness itself.

What closes the rest is unchanged from D87: not a better bound, but separating
the two questions `objective_gap` currently answers at once — how far from
optimal the point is, and how much of that can be certified. There is 41x of
checker margin left to spend on it.

---

## D89 — The re-entry loop keeps its best round, and "best" is defensible before it is close

### The question

D49, D50 and D51 left the re-entry loop oscillating: on `pilot87` at a
refactorization interval of 24 the rounds alternate move/pivot with period
four from round 12 to round 31, and `SETTLE_ROUNDS = 32` is what ends it
rather than any condition. D50's own proposal was to keep the best round
rather than the last, and it was never tried, because the quantity to compare
was unsettled — D49 had measured a factor of 280 between the loop's own
`dual_breach` and the checker's verdict and said plainly that until that was
attributed, no threshold in the loop means what it appears to.

### What the measurement said first

Instrumented per round on `pilot87` at 24:

| round | objective | worst breach | gap contribution | breaching | unbounded |
|---|---|---|---|---|---|
| 9 | **301.710389698** | 1.28e-06 | 2.36e-04 | 3 | 1 |
| 12, 16, 20, 24, 28 | 301.710400171 | 1.10e-04 | 3.16e-05 | 6 | 3 |
| 15, 19, 23, 27, 31 | 301.710401037 | 7.85e-07 | 0 | 3 | 3 |
| published | 301.710400171 | — | — | — | — |

Three things came out of it, and two of them shrink the defect.

**The cost of not converging is negligible.** The twenty wasted rounds consume
278 iterations out of 116,071 — **0.24%**. The entry made this sound
expensive; it is not.

**The damage is arbitrariness, and it is small.** The best objective seen is
1.05e-5 below what the loop published — 3.5e-8 relative, under the solver's
own tolerances. What is actually wrong is that *which* of those rounds gets
published is decided by where the cap falls.

**And D49's factor of 280 is attributed.** Publishing divides a structural's
reduced cost by `gamma_j` and multiplies its distance by the same `gamma_j`,
so `breach * distance` is scale-free while `breach` alone is not — and the
per-column factors differ between the rounds being compared, since D50
recorded that the breaching columns change every round. Comparing raw
`dual_breach` across rounds is a lottery weighted by the scaling. That answers
the half D49 left open.

### The first criterion was wrong, and the measurement said so

Every round leaves a *primal feasible* point, so its objective is an upper
bound on the optimum and the lowest one looks like the obvious winner. Built
that way, `pilot87` at 24 publishes round 9 — objective 301.71038969768654,
better by 1.05e-5 — and **the independent checker rejects it**, on a dual
violation of 5.12e-06 against a tolerance of 1e-6.

That is not an improvement. This solver's verdict is OPTIMAL, that verdict
rests on dual feasibility, and publishing it over a point an independent check
refuses is the thing the gate exists to prevent. A closer answer bought with a
defensible one is a bad trade at any exchange rate.

### The criterion that landed

Lexicographic: **defensible first, close second.** A round whose dual
violation is inside tolerance beats one that is not, whatever the objectives;
between two inside, the lower objective; between two outside, the smaller
violation. Both quantities are computed in the model's own space — the
violation by undoing `gamma_j` and `rho_i` per column and per row — so neither
comparison depends on the scaling.

`pilot87` at 24 then publishes round 15:

| | objective | worst breach (scaled) | checker |
|---|---|---|---|
| before | 301.710400171 | 1.10e-04 | — |
| **after** | **301.710401037** | **7.85e-07** | **ok, dual = 0** |

The objective is **8.7e-7 worse in absolute terms, 2.9e-9 relative**, and the
dual violation is **140x smaller**. The published point carries a dual
violation of exactly zero by the independent checker's reckoning.

### What it costs and what it buys

All three gates PASS, 0 regressed, and **no published answer in the gate moves
at all** — at the shipped interval the loop converges, so it ends on its own
best round and there is nothing to take. 167 unit tests green. The memory is a
second copy of the five arrays that make a point, on a path most solves never
enter.

What it buys is not accuracy. It is that **the published answer stops
depending on where `SETTLE_ROUNDS` falls.** The same model with a different
cap now returns the same point, as long as the best round is inside the range.
That is robustness rather than precision, and it is worth saying so plainly:
this entry does not make the loop converge, and D74 already closed the
direction that would have.

### What is left open

The oscillation itself. D51 named the mechanism — every cleanup pivot borrows
in order to repair, and repaying is what creates the next round's work — and
D74 refuted the obvious cure by measuring that removing the loan costs
`pilot87` 2.372x its iterations. Nothing here changes that. What this removes
is the consequence, which is the answer being chosen by a counter.

---

## D90 — The warm start stops refusing a free nonbasic, because the defect it was avoiding is fixed

### The question

D68 made `build_warm_basis` abandon any handed-in basis containing a nonbasic
variable with neither bound. The reason was not tidiness and was written down
at the time: such a variable rests at zero, which for a row's logical pins
that row's activity at zero — a constraint the model does not have — and the
method could not always price it back off. `wants_a_pivot` read a free
nonbasic as sitting at an upper bound, so it repaired a positive reduced cost
and dropped a negative one, and the point was published as OPTIMAL when it was
not. That was carried defect 4.

D85 repaired it. The premise of the refusal was therefore gone, and the price
of keeping it had been measured two milestones earlier: D69 found `cycle`
losing its **entire** warm start to it, one instance in 92.

### What it buys

| `cycle` | warm | cold |
|---|---|---|
| before | 1537 iterations, 19,993,693 units | 1537, 19,993,693 |
| **after** | **16 iterations, 2,221,915 units** | 1537, 19,993,693 |

Before the lift the warm figures *are* the cold ones, which is what a refused
warm start looks like: the solve fell back and paid for the whole thing again.
After it, 16 iterations against 1537 — **96x fewer**, and 9x less work.

Over the standard set the geometric means improve from 0.0055 to **0.0052**
in iterations and 0.0166 to **0.0162** in work. Small, because one instance in
92 moved; the point is that the instance that moved went from paying full
price to paying nothing.

All three gates PASS with 0 regressed and every digest byte-identical: no cold
solve reaches this code, so the acceptance record cannot move.

### The test that proves the two repairs meet

`test_a_status_whose_bound_was_retired` asserted that retiring both of a row's
bounds sent the next solve to the slack basis — it pinned the refusal. It is
re-pinned to assert the warm start now holds, and it is worth saying which
model it uses: `min 2x + 3y` subject to `x + y >= 2` with a second row relaxed
to free. **That is D68's own example**, the one whose comment said it would
publish 6 where the optimum is 4. It publishes 4, from the warm basis.

So the case that documented the defect is now the case that demonstrates the
repair, which is the tightest form this kind of evidence takes.

### What the campaign also turned up, and it belongs to D87

`make warm` exits nonzero on one checker rejection: `pilot87`. That is **not
this change**. Built from each of the last three commits in turn:

| tree | rejections |
|---|---|
| D86, the stability trigger | **0** |
| D87, the checker's implied bounds | **1** |
| D89, the re-entry criterion | 1 |
| this | 1 |

and the trajectory figures are identical in all four — `warm=838/759755913`
against `cold=120075/61322758828` — so the solver did not move. The verdict
did.

**And the verdict is right.** `pilot87`'s warm answer is 301.77550257870354
against the cold 301.77545866851051: the warm point is **4.4e-5 worse**, which
is a genuine suboptimality the checker used to accept in silence. D87 is what
made it visible, on an instance nobody constructed. That is the first evidence
that the implied bound catches something beyond D47's built case, and it
arrived from a campaign aimed at a different question entirely.

It is left failing rather than papered over. A checker rejection is what the
warm campaign should report, and `make warm` is not a gate (it reports a
ratio, not a verdict), so nothing downstream is blocked by it.

---

## D91 — The bound and the verdict stop being one number, and D47 closes

### The question

D87 left defect 1 open with a diagnosis rather than a cure. The checker bounds
unbounded variables by what the rows imply, which is sound — every feasible
point satisfies an implied bound, so it moves neither the feasible region nor
`P*` — but the bound is **slack**, and nothing puts the variable on it. Its
term in `P - D = sum_v w_v (v - bound_v)` therefore survives at an optimum,
measuring the bound's looseness rather than the point's error.

`dual_feasible` read that gap. So tightening the bounds made the verdict
worse: iterated propagation rejected `pilot` at the intervals where it is
*right*, including the shipped one. D87 recorded the constraint that follows —
as long as the gap answers two questions at once, every improvement to the
second is paid out of the first — and left the redesign as the next attempt.

### The separation

Two sums instead of one. `pos`/`neg` carry **every** term and bound the
suboptimality, which is what D47 asked for. `pos_model`/`neg_model` carry only
the terms from bounds **the model declared**, which must vanish at an optimum
of the problem as written. The verdict reads the second; the bound is the
first. A slack implied bound can no longer cost a correct answer its verdict.

With that, the propagation is free to be as good as it can be, and is iterated
to a fixed point (at most `IMPLIED_ROUNDS = 64`, exiting early when a round
bounds nothing new).

**That constant was swept, because the first version of this entry claimed a
measurement it did not have.** 8 was chosen by nothing at all. Certified
answers over the standard set, by round count:

| rounds | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 |
|---|---|---|---|---|---|---|---|---|
| certified | 17 | 23 | 32 | 38 | 46 | 47 | **48** | 48 |

The propagation reaches its fixed point at 64, so 8 was cutting off ten
answers that could have been certified. The cost is flat across the whole
sweep — 119 s to 128 s against a gate that takes about 120 s — so the passes
are free and there is nothing to trade against. The cap is therefore a safety
stop rather than a choice: the loop exits when a round bounds nothing new, and
64 is where that happens.

The canary moved at every step, so the sweep measured eight binaries and not
one binary eight times, which is the trap D82 fell into.

### What it closed

**`etamacro`, which D47 named as the live case at 2.25e-07:**

```
before:  drop=2.25e-07  cert=no
after:   drop=0         cert=yes   rsub=3.28e-09
```

**Certification across the gate**, which is the defect's own words — the
checker certifying a bound it cannot prove:

| | certified |
|---|---|
| before this session (D71) | 12 of 110 |
| one-pass implied bounds (D87) | 27 |
| **propagated, verdict separated** | **64 of 110** — 48 of 94 standard, **16 of 16** Kennington |

The largest surviving dropped term over the standard set falls from 2.25e-07
to **3e-08**, and everything below it is 1e-14 or smaller. What the checker
still cannot account for is arithmetic noise rather than structure.

### The bound separates the case D47 was built from

`relative_suboptimality` — `gap_positive` over `1 + |P|`, because an absolute
bound says nothing without the magnitude beside it:

| `pilot` at interval | objective | relative bound |
|---|---|---|
| 16, 64 | ok | **6.9e-05** |
| 24, 32, 96 | **out of tolerance by 1.04e-3** | **5.02e-03** |

A factor of **73**, and 6.9e-05 is also the worst value anywhere in the gate,
so nothing legitimate sits near the bad case. Kennington's worst is 4.72e-14.

**The verdict does not read it, deliberately.** Two data points of "wrong" —
and they are one defect seen three times — is not a measurement a threshold
can be set from, and a constant fitted to one instance is how this project
loses weeks. What reads it is the gate, by the D88 mechanism: carried in the
baseline, growth past 2x above a floor of 1e-9 is a regression. That needs no
absolute threshold at all, and the 73x separation clears the factor of 2 with
thirty-six times to spare. It replaces the dropped term as the watched
quantity, which after propagation is noise and no longer informative.

Calibrated by injection, as D88 was: `pilot`'s baseline value divided by 100
is reported as `REGRESSED ... 100.0x` and the runner exits nonzero.

### One verdict changed meaning, and it is worth stating

On D47's constructed case `dual_feasible` is now **true**. That is correct and
was always correct: x2's reduced cost of -1e-7 is below the tolerance with
which this checker decides a multiplier is nonzero at all, so there is no sign
violation to find. **That is why D47 was never a tolerance bug**, and why no
verdict on sign conditions could ever have caught it. What catches it is the
bound — 0.1 relative — and `jaos.h` now says which field answers which
question.

### What it cost

167 unit tests green. Three gates PASS, 0 regressed, after regenerating the
baselines. The predicates were verified unchanged first: the old baselines
report four differences and every one of them is on the numeric column, whose
meaning changed from dropped term to relative bound — no predicate moved.

### What is left

Not the defect. `pilot` at 24, 32 and 96 still reports `checker=ok`, because
the bound at 5.02e-03 is not read as a verdict and the sign conditions
genuinely hold. Making it a verdict needs more than one instance's worth of
evidence about where the line sits, and that evidence does not exist yet —
one more model known to be wrong would be worth more here than any amount of
further argument.

---

## D92 — A residue only a pivot can remove was hidden by the scaling, and the repair is the union of the two readings rather than either one

### The question, and both halves of how it was filed were wrong

`PLAN.md` carried one open defect: *`pilot87`'s warm re-solve is measurably
worse than its cold one*, with `make warm` exiting nonzero, the checker
refusing the **warm** answer and accepting the cold one. Re-measured on the
tree that carried the entry:

- **`make warm` exits 0.** Zero rejections over 92 instances. D91 changed the
  checker underneath the entry between it being written and being read.
- The objective difference is real and unmoved: warm `301.77550257870354`,
  cold `301.77545866851051`, warm 4.39e-5 higher on a minimisation.
- **The checker refuses the cold answer**, not the warm one. Warm reads
  `dual_feasible` with a violation of 0; cold carries **1.67213e-06** against
  a tolerance of 1e-6.

The entry said the opposite because the campaign could not have known.

### The instrument judged one of the two answers it compares

`bench/warm.c` ran the independent checker on the warm answer alone. The
reasoning was that cold is the reference and the gate already checks cold
answers — and the gate checks them on the models as **loaded**, which is the
one case a branch has moved away from. So the perturbed model's cold solve was
the only published answer in this repository that nothing judged, and it has
been wrong for as long as warm re-solve has existed.

It judges both now and names which side was refused, because "the pair was
refused" does not say where to look and here it is the unexpected side. With
that the defect has a size: **1 of 91 on the standard set, 0 of 11 on
Kennington.**

Fixed with it, because it made every parallel failure report misleading:
`read_result` parsed the note with `%79s`, which stops at the first token, so
`"path too long"` and `"first solve failed"` reached the summary as `path` and
`first` under `-j`.

### The defect

Instrumented on a copy of the tree, reporting the worst breach in both spaces
where the verdict is decided:

```
DIAG publish dual_tol=1e-07 solver_worst=0 (v=-1)
     model_worst=1.67213e-06 (v=5940 d=6.53175e-09 gamma=256 status=AT_UPPER)
```

`v = 5940` is the logical of row 1057 (`ncol = 4883`), resting at its upper
bound with a wrong-signed reduced cost. That cost is **6.53e-09 in the scaled
space** — a hundredth of `DUAL_TOL` — and **1.67e-06 once published**, because
the row's scale factor is 256; `6.53175e-09 * 256 = 1.6721e-06`, which is the
checker's number to every digit it prints.

Its other bound is infinite, so it cannot be flipped and its term in `P - D`
is zero. What it needs is a primal pivot, which `primal_cleanup` exists to
give it, and `wants_a_pivot` never offered one because it asked `dual_breach`,
which applies `DUAL_TOL` in the scaled space.

**This is D27's fault class and D27 resolved half of it.** D27 established that
any rule reading the breach must pick a space, then arranged for `can_move` not
to read the breach at all: it tests `|d|` times the width of the box, a product
`publish` leaves invariant. That works wherever there is a box. The predicates
serving the columns that have none were left on the scaled reading.

**And D27's refusal of the other route had expired.** It refused judging the
breach in the published space because that took `pilot87` from a solve to a
tripped iteration guard at **1,382,801** iterations — the same instance and the
same count D86 diagnosed four commits ago as the factorization having stopped
describing the basis. The premise was gone, so the route was re-measured rather
than re-argued, which is D90's shape.

### The population, and it inverts the obvious diagnosis

Substituting the published reading in the two predicates that select a
clean-up pivot repairs the defect **and costs `pilot87` its suboptimality
bound**: `Q` from 0.00682 to 26.8, the bound from 2.25e-05 to 0.0885, for 2.9x
the work. The obvious reading is that the solver started chasing residue it
should not. Counted at the candidate loop over `pilot87`'s three solves, that
is not what happens:

| phase | breached | **added** by the published reading | **dropped** by it |
|---|---|---|---|
| anchor, unperturbed | 20 | **0** | 3 |
| warm | 23 | **0** | 11 |
| cold, perturbed | 41 | **2** | 12 |

The two added are the defect — `v=5940` (row, published 1.67e-06, gamma 256)
and `v=68` (column, published 1.90e-07, gamma 4), both with no other bound.
Every one of the twenty-six dropped is a column whose scale factor is **above
one**, so a breach that is real in the arithmetic falls under `DUAL_TOL` once
published; `v=2245` at gamma 0.0625 carries a scaled breach of 1.5e-06.

**So the regression was the solver ceasing to repair residue it repairs
today** — three clean-up pivots on the unperturbed solve, eleven on the warm
one. Those pivots are what was holding the certificate up.

### The repair: either, not instead

`breached` asks whether there is a sign-condition breach in **either** space,
and the two predicates that select a clean-up pivot ask it. Nothing today's
code repairs is dropped, and the two columns that close the defect are added.

Neither reading dominates, which is the argument rather than a compromise: a
scale factor above one hides a breach from the caller's view, one below it
hides a breach from the solver's, and the scaling is a change of variable
chosen for the factorization's convenience. Repairing a residue the caller
cannot see costs iterations; ignoring one the caller can see is a wrong
answer. The union is conservative in the only direction that matters.

The three questions about a breach are now answered in three places, which is
the shape D27 was reaching for:

| the question | what answers it | space |
|---|---|---|
| is this worth *moving* to its other bound | `can_move`, `|d|` times the box width | none — invariant |
| is the answer *defensible* | `settled_dual_violation` | published only |
| is there anything *there to repair* | `breached` | either |

### What it cost

**`pilot87` is bit-identical on the gate**, and so is its warm re-solve — 838
iterations and 759,755,913 work units. The only solve of it that moves is the
perturbed cold one: 120075 → **120318** iterations, 61.32e9 → 61.77e9 work
units (**1.0073x**), objective 301.77545866851051 → 301.77545899031168, and the
checker accepts it.

**92 of the 94 standard instances are bit-identical.** The two that move are
`etamacro` and `pilot`, two of the five D27 named as having any column the
re-entry would consider, and neither regresses:

| | `Q` | `rsub` | iterations | work |
|---|---|---|---|---|
| `etamacro` | 2.48e-06 → **1.73e-07** | 3.28e-09 → **2.29e-10** | 708 → 709 | 1.005x |
| `pilot` | 0.0385 → 0.0387 | 6.9e-05 → 6.94e-05 | 25342 → 25886 | 1.036x |

`etamacro`'s certificate tightens by a factor of **14** for one iteration —
the same instance D27 opened on, and the direction that says the extra pivots
are repairing something rather than disturbing it. `pilot` pays 3.6% of its
work for a bound that does not move; its objective changes in the last digit,
-557.48970616317422 to -557.48970616317433.

All three gates read 0 regressed. `make warm` returns to 92 measured with 0
rejections and `make warm-kennington` to 11 with 0. ASan+UBSan clean.

### What was refuted on the way

**Substituting the published reading for the scaled one**, at either scope.
Both repair the defect and both are refused by the standard gate:

| what reads the published space | perturbed `pilot87` | unperturbed `pilot87` |
|---|---|---|
| `dual_breach`, so every reader | fixed, cold 120075 → 51309 | bound 2.25e-05 → **0.0883**, `Q` **26.7**, work 0.9921x |
| `wants_a_pivot` + the clean-up's re-check | fixed, cold 120075 → 50898 | bound → **0.0885**, `Q` **26.8**, work **2.917x** |

Under both, `etamacro` and `pilot` land on **bit-identical digests**, which is
what localised the trajectory change to the clean-up's selection and not to
`arm_reentry`'s shifting — the first hypothesis, and wrong.

**`settled_dual_violation` alone**, which is not a repair and is kept anyway.
D89 built it to rank the re-entry's rounds "in the model's own space" and its
comment said so, but it unscaled `dual_breach`'s *output*: the tolerance was
applied scaled and only the survivors converted, so a breach inside `DUAL_TOL`
scaled and outside it published arrived as an exact zero. It now applies the
tolerance after the conversion. It repairs nothing here, for a structural
reason — on this instance the re-entry finds nothing to move and takes no
clean-up pivot, so there is a single candidate point and no ranking to get
wrong — and it measured **94 of 94 bit-identical** with all three gates
unmoved.

### What is left open

`pilot87`'s `gap_positive` moves between 0.0068 and 26.7 across variants whose
objectives all sit within tolerance of Koch's reference and which all read
`dual_feasible`. D91 already records why that number can be live at a correct
optimum: an implied bound is sound but slack, and nothing puts the variable on
it. Whether a `Q` of 26.7 is an answer getting worse or a bound going slack is
not settled here, and it is what refused two of the three candidate repairs.
Handed to `PLAN.md`.

---

## D93 — The ratio test's dense scan walks the nonbasic set, and the bar it was to be judged against cannot be measured on this host

### The question, and what was expected

D84 refused multiple pricing and, in refusing it, pointed here. The profile it
took while checking its own premise put `admit_candidate` at **14.98% of
instructions on `truss`** against `ftran_prefix` at 6.68% — called once per
variable of the pricing row, the O(`nvar`) half of D61's 36.5%, and the half
neither D82 nor D84 touched. `PLAN.md` phase 6 item 3a carried it with an
instruction attached: it "needs its own decision before any code".

On `truss` the model has 9806 variables and 1000 rows, so 10.2% of what the
dense scan visits is basic and can only be rejected. **The expectation was that
not visiting them would show as a time saving** — the rejections are the large
majority of the calls, `admit_candidate`'s basic test is its first line, and a
tenth of the calls removed against a function that is a seventh of the program
looked like it should be visible on a clock.

It is not visible on this clock, and the reason is not that the effect is
smaller than expected. **It is that this host cannot resolve an effect of this
size at all**, which took a control nobody had asked for to establish.

### The decision came before the code, and the pre-code record is on disk

The source item required this one to have its own decision before any
implementation. That decision was taken and written down in
`.planning/phases/01-candidate-admission-in-the-ratio-test/01-CONTEXT.md` on
**2026-08-12**, as thirteen numbered items D-01 through D-13 — what would be
changed, what would not, how it would be verified, and what threshold the
result would be read against — and it was committed before the first
implementation commit `f2ed4bc` existed. D93 is that decision closed with the
measurement, not a decision reconstructed after the fact from what the code
turned out to do. D-04 in particular pre-authorised the refusal path: if the
measurement did not pay, the phase would close with an entry shaped like D82
and D84 rather than roll on to the next candidate path.

### What was built

A persistent bitmap `s->nbmark`, one `uint64_t` per 64 variables, holding
exactly `{v : status[v] != JM_BASIC}`, walked by the dense branch of
`dual_ratio_test` in place of `for (v = 0; v < nvar; v++)`. Maintained at eight
membership sites, rebuilt nowhere else, and cross-checked at run time in every
non-`NDEBUG` build against the scan it replaced — once per iteration, over both
branches, charging no work. `admit_candidate`'s body is untouched (D-02).

Commits: `f2ed4bc` (the walk), `ebe052f` (the tests and their calibration),
`b65d9f2` (the charge), `44c0ef6` and `e8c2f58` (the campaigns and the
baselines).

### The null result, and it is the strongest sentence in this entry

The change was designed to be an observable no-op, so what it did *not* move is
the claim worth checking, and it is checkable:

**110 solution digests unmoved and 29 infeasibility verdicts unmoved, over 139
instances, zero answers changed.** The counts are not interchangeable and the
distinction is worth keeping: 94 standard plus 16 Kennington instances publish
a `digest=` field and all 110 are bit-identical to the committed record; the 29
infeasible instances publish no digest at all, and their invariant is
`expected=infeasible verdict=ok det=ok`, every field of which is unmoved. 139 is
the instance count, not the digest count.

It is stronger than that. **Iterations are 1.0000x on every instance
individually**, not merely in the mean — including `pilot87`'s 50,850 and
`ken-18`'s 113,652. A digest is evidence about the endpoint; an identical
iteration count on a 113,652-iteration solve is evidence about the whole path.
And with the work field masked, **every per-instance line of all five records is
byte-identical** to the pre-phase record, with a canary confirming the mask does
not hide a real change. The only column that moved anywhere is the one the
accounting deliberately changed.

The warm campaigns agree: 92 of 94 and 11 of 16 measured, 0 disagreed, 0
rejected by the independent checker on either side of any pair. `build_warm_basis`
is one of the eight membership sites and the gate never loads a basis, so those
two runs are the only cover that site gets.

### The work counter, and the definition change underneath it

D16 makes the work unit a public contract, so what the dense branch charges is
part of the record and changing it is one-way. It charged `s->nvar`; it now
charges what the scan actually visited. **Dense-scan work figures recorded
before `b65d9f2` are not comparable with those after — they are not smaller,
they are differently defined.**

Both sides, on named instances, from the committed baselines at `64efcc6` and
at `e8c2f58`:

| instance | rows | before | after | ratio |
|---|---|---|---|---|
| `truss` | 1000 | 1,154,610,114 | 1,138,043,114 | 0.9857x |
| `gfrd-pnc` | 616 | 3,485,629 | 3,287,277 | **0.9431x**, the largest fall on the set |
| `stocfor3` | 32370 vars | 815,652,468 | 815,652,468 | **1.0000x — it never takes the dense branch** |

The pinned six-variable model in the unit suite moved 8545 → 8536, and the nine
units close exactly: 3 iterations × (6 variables − 3 nonbasic).

Per set, as a geometric mean of per-instance ratios (D46): standard
**0.9779x**, infeasible **0.9857x**, Kennington **0.9860x**. Down on 118
instances and **up on none**, which is what `visited <= nvar` requires. 21
instances never take the dense branch at all and their work is unchanged.

**The saving is an exact whole multiple of the row count on all 139 instances**,
which inverts to the number of dense-branch calls without instrumenting
anything — `truss` 16,567,000 / 1000 = **16,567 calls**, `gfrd-pnc` 198,352 /
616 = 322. A saving that was not this quantity would land on a multiple by
chance about one time in `nrow`, so 139 hits is a confirmation of the edit
stronger than anything in the unit suite. The quotient counts **calls to
`dual_ratio_test`, not iterations**: `galenet` makes 2 of them in a solve that
reports `iters=1`, so at least one call happens somewhere that is not a counted
iteration.

### The threshold, derived rather than asserted, and the citation that has to be right

D-13 fixed the bar at **4.2%**, and it is not a preference:

> 4.2% = 3 × 1.4%, where 1.4% is the repeatability **D81** measured on this
> harness — JAOS byte-identical at every rung of `bench/compare`, its own
> cross-rung ratios reading 1.007x, 1.014x and 1.012x over four separate
> sessions with iterations exactly 1.000x.

**The 1.4% is D81's and not D83's**, and the distinction matters because D83
also carries a 1.4% — "Clp lands within 1.4% of HiGHS on total time" — which is
a different quantity in a different table and reads as confirmation to anyone
who greps for the figure. The phase's own plan documents made that slip twice.

The second figure in circulation is **D60's 1.3%**, estimated a different way on
the same harness; D81 records its own 1.4% as consistent with it. Three times
1.3% would be 3.9%. **Nothing in this entry turns on the reconciliation**: no
reading of the data lands between 3.9% and 4.2%.

### The time ratio, which is the verdict D45 asks for

Same-instance, same-session, `J=1`, both binaries built by identical procedure
from `git archive` — the parent from `2b07de1`, the pre-phase tree, and the
candidate from `HEAD`. Six rounds, three in each running order. Geometric mean
of per-instance ratios over the standard set:

**0.9709x — a 2.91% improvement, against a 4.2% bar.** The ratio of totals
beside it reads 0.9847x and is *not* the answer (D46): `pilot87` and `maros-r7`
are 74% of this set's work and are exactly where this change is invisible.
`truss` on its own line, 1.496s → 1.460s, **0.9759x**. It is where the cost was
found and it does not decide alone.

Where it bites and where it does not, named rather than averaged: `dfl001`
0.9136x, `greenbea` 0.9505x, `greenbeb` 0.9513x, `d2q06c` 0.9701x, `25fv47`
0.9887x — against `pilot87` **0.9972x** and `maros-r7` **0.9987x**, whose work
ratios of 0.9956x and 0.9987x predicted precisely that. A time ratio taken on
D46's two names alone would have read 0.998x and called the change dead. Moving
the other way: `modszk1` 1.0588x, `perold` 1.0583x, `d6cube` 1.0571x, `fit2p`
1.0322x, and `pilot` at 1.0490x which is a minimum-estimator artifact — it is
faster in 5 of 6 paired rounds.

Two limits on the instrument, both stated rather than smoothed. The runner
prints seconds as `%8.3f`, so **8 of the 94 carry no ratio at all** —
`adlittle afiro blend kb2 recipe sc105 sc50a sc50b`, excluded and named rather
than floored at half a quantum — and **42 more read exactly 1.0000x** because
both minima land on the same millisecond. Half the geometric mean is made of
instances that could not have shown a difference of this size whatever the
truth was.

### The verdict does not depend on the estimator — but it does depend on having run six rounds

Six defensible readings of the pooled data run from 2.16% to 4.07% and none
reaches 4.2%, so among estimators that pool all six rounds the verdict is
stable. **That is the whole of what stability was established for, and it is
narrower than it sounds.**

The protocol as originally written asks for three alternating rounds. Run
literally, pooled minimum over three:

| three rounds | reading |
|---|---|
| rounds 1–3, candidate first | **1.01%** |
| rounds 4–6, parent first | **5.12% — over the bar** |

**A conforming three-round run in the parent-first order would have reported
ACCEPT.** The reason is running order: within a round the two binaries run
adjacent in time and the second slot is the faster one, so an alternation that
puts the same binary first every round hands the other a systematic advantage.
Measured, that advantage is **2.4 percentage points** — the same size as the
effect being measured (rounds 1–3 read 0.9865x paired, rounds 4–6 0.9624x).

This is the most transferable thing the phase learned. **Any same-instance time
ratio on this host that alternates in one direction only is measuring the
order.**

### The negative control, which is what actually closes this

It was in the data and nobody ran it. **Nine standard-set instances have
bit-identical work under both binaries** — `01-03` identifies them as never
taking the dense branch, and unchanged work *means* zero dense calls, since a
single one would strictly reduce the charge. The change therefore **provably
cannot speed them up**. Eight of the nine carry a time ratio:

| reading | the eight | the true answer |
|---|---|---|
| paired by round | **0.9699x** — a 3.0% "improvement" | **0.0%** |
| pooled minimum | **0.9356x** — a 6.4% "improvement" | **0.0%** |

**A 3.0% to 6.4% improvement on instances where the improvement is zero, against
a 2.91% headline.** Two of them appear in the phase's own tables of where the
change bites: `stocfor3` at 0.9533x, read as a place the change works, and
`fit2p` at 1.0322x, read as "genuinely mixed". Neither reading can be true.

That is why the finding here is **"the bar cannot be tested on this host"**
rather than "the candidate missed the bar". Three further readings agree:

- the same binary against itself across rounds, measured the way D81 measured
  its 1.4%, reads **6.27%** over the 86 rateable instances, **8.67%** over the
  25 the clock can see, and **9.68%** over the 10 largest — restricting to
  substantial instances makes it worse, so it is the host and not quantization;
- a bootstrap confidence interval on the protocol figure is
  **[−4.21%, −1.72%]**, and on the largest defensible reading the probability
  of clearing 4.2% is **0.41**;
- single-round readings span **5.13 percentage points**.

**The bar is three times a repeatability figure four times smaller than the one
this reading actually exhibits.** D17 already refused this host for published
figures; what this entry adds is the price, in the currency of a specific gate.

### Callgrind: the change costs instructions

Both binaries under callgrind on `truss` at `-j 1`, in one session. Running the
parent too was not in the plan and is what makes the reading mean anything —
D84's 14.98% records no build, so a fresh number compared against it alone
compares two unknowns. The parent reproduces D84 to within 0.19 of a
percentage point, which is what licences the rest.

| | parent | candidate | delta |
|---|---|---|---|
| `admit_candidate` | 8,060,038,036 (14.79%) | 7,861,234,036 (14.20%) | −198,804,000 |
| `simplex.c:run`, what the scan inlines into | 18,714,996,570 (34.33%) | 19,709,931,702 (35.59%) | **+994,935,132** |
| `ftran_prefix`, the nominated control | 3,594,648,962 (6.59%) | 3,679,427,488 (6.64%) | +84,778,526 |
| **PROGRAM TOTALS** | **54,507,175,480** | **55,377,182,992** | **+870,007,512** |

**The candidate executes 1.60% MORE instructions.** `admit_candidate`'s share
did fall, and the caller took back five times what the callee shed. **That is a
relocation, not a saving**, and it is only visible because both numbers were
taken. Any future reading of this change that reports the 14.79% → 14.20% alone
is reporting the half that flatters it.

**The profile contains two solves, not one.** The bench runner re-solves each
instance to establish `det=ok`, so the annotation shows `simplex.c:run (2x)` at
99.82% of instructions. Both builds do it, so the +1.60% and the 5:1 ratio are
unaffected — they are ratios of two numerators that scale together. **Per-call
figures are not**, and the per-solve arithmetic is:

- 16,567 dense calls per solve on `truss`, each skipping 1000 basics and
  visiting 8806 nonbasics, so 16,567,000 skips and 145,889,002 visits per solve
  — and 291,778,004, twice the latter, is what callgrind's call count records on
  the bitmap loop;
- the skipped calls are `admit_candidate`'s cheapest path, one status load and a
  return: 198,804,000 / 33,134,000 = **6.0 instructions saved per skipped call**;
- the bitmap machinery costs 994,935,132 / 291,778,004 = **3.41 instructions
  more per variable still visited**;
- so it pays 3.41 on 145.9M visits per solve to save 6.0 on 16.6M skips.
  **995M paid against 199M saved.** The bet was that skipping a tenth of the
  variables would pay for the indirection, and on instruction count it does not,
  by a factor of five.

The 16,567 is corroborated twice over: it is the work saving divided by the row
count, computed from two committed baselines that never saw a profiler, and
16,567 × 9806 = **162,456,002**, which is the `admit_candidate` call count D61
recorded on `truss` in an entirely different session.

Callgrind's own controls are better than the one that was nominated.
`ftran_prefix` moved 2.36% without being touched, because LTO privatised it
differently between the two builds (`ftran_prefix.lto_priv.0` against plain
`ftran_prefix`) — under `-flto` no function is truly untouched. But **24
functions above 1e6 instructions are identical to the digit in both binaries**,
`shift_to_feasible` at 4,417,992,736 among them. A control that cannot change is
worth more than any headline in the same table.

### One reading in the change's favour, and its limit

Over the 25 instances the clock can actually see, log work ratio against log
time ratio gives **r = +0.684 with a permutation p of 0.0003**, rising to
**+0.848 on the 10 largest**. Work saved does predict time saved, on the
instances where either can be measured, and that is a real dose-response rather
than a story fitted to a mean.

Over all 86 rateable instances the slope collapses to **+0.15 against a fitted
intercept of −2.24%**. So most of the 2.91% headline is intercept and not
response — an offset applying equally to instances the change cannot touch,
which is the negative control seen from the other side. Both halves belong in
the record; either alone misleads.

### What was refuted

The part that pays, in enough detail that nobody re-derives it.

**The membership invariant is that a variable is not basic, and never that it
has a finite bound.** A structure keyed on bound status silently drops every
free variable, and `JM_FREE` is assigned at three sites (`src/simplex.c:800`,
`:910`, `:1217`). The corollary held in the other direction too: the three
bound-flip sites — `apply_flips`, `repair_dual_infeasibility`, `arm_reentry` —
were deliberately left unhooked, because they toggle `JM_AT_LOWER` against
`JM_AT_UPPER` and never touch `JM_BASIC`. Hooking them would be maintenance
keyed on bound status, which is the same mistake wearing the opposite sign.

**A site table built by grepping for assignments to the status array is
incomplete.** It finds six membership-changing sites; there are **eight**.
`take_best_if_better` (`src/simplex.c:2631`) and `restore_settled` (`:2656`)
each restore the whole status array by `memcpy` and carry no assignment form at
all. A membership structure left stale across either of them desynchronises
silently — the solve continues and the answer is merely different. Both rebuild
`s->where` from `s->basis` immediately after the restore, and the membership
rebuild belongs in exactly that place for exactly the same reason.

**A missed `remove` is a performance fault and not a correctness one, and the
run-time cross-check is right to be silent on it.** It leaves the bitmap a
superset of the nonbasic set; the extras are basic; `admit_candidate`'s first
test rejects a basic variable; the candidate set is identical. The
correctness-dangerous fault is a missed `insert` — a nonbasic variable dropped
from the bitmap is never offered to the ratio test at all. Anyone calibrating
this instrument on a missed `remove` and reading its silence as a pass will
leave it uncalibrated.

**The work charge in `pivot` is not the same charge and must not be made to
match.** `src/simplex.c:2174` still bills `s->nvar` and is correct to: that loop
walks `[0, nvar)` and reads every variable's status through `update_dual`, so
the dimension *is* what it looked at. A plan criterion requiring no remaining
`nvar * JM_WORK_NONZERO` in the file was refused on those grounds; meeting it
would have altered work units on every instance in every campaign with nothing
pinned to catch it.

**And the estimator claim this phase first made about itself does not hold.**
"The verdict does not depend on the choice of estimator" is true only among
estimators that pool all six rounds; on the protocol as literally written it is
false, and 5.12% is on the wrong side of the bar. The stability that licences a
verdict here comes from having run six rounds in two orders, not from the
arithmetic being robust.

### Harris's two passes, and the case that had to be refused

The two-pass ratio test's guarantee is a function of the candidate set and the
order the candidates arrive in, and nothing else. **Both are preserved by
construction, which is why this phase is not the refused half of D82 and D84.**
`admit_candidate`'s body — the `JM_BASIC` test, `PIVOT_MIN`, the per-status sign
test, the clamped numerator — is untouched, verified against the diff. The
bitmap is walked ascending in `v`, exactly as `for (v = 0; v < nvar; v++)`
produced, and the contract at `src/simplex.c:1622-1635` says why that is not a
detail: `bfrt_walk`, `jm_harris_pick` and `apply_flips` each break an exact tie
by whichever candidate they meet first, so any other order is a different
trajectory.

**Preserved by construction is a claim, so the case the instrument must refuse
was built and confirmed refused before its passing was treated as evidence.**
Two faults, both injected and both reverted:

| injected fault | what caught it |
|---|---|
| `jm_nonbasic_remove` made a no-op | two unit tests — `test_nonbasic_survives_interleaved_eviction` and `test_nonbasic_notices_a_missed_hook`. The run-time cross-check was **silent, and correctly so** |
| `jm_nonbasic_insert` made a no-op | the run-time cross-check aborted a solve: `src/simplex.c:1698: dual_ratio_test: Assertion 'dn == n' failed` |

`test_nonbasic_notices_a_missed_hook` is the deliberately broken maintenance
sequence: it runs the eviction sequence with one hook omitted and asserts the
bitmap does **not** match the status array, then asserts it does once the hook
runs. A predicate rather than an assertion helper, precisely so the negative
case can use it.

The cross-check runs over the pattern branch as well as the dense one, which
makes it a run-time proof of the pre-existing pattern/dense equivalence claim
that had never had one. It costs nothing shipped: absence of the assertion text
was confirmed on the release object, not assumed from the `#ifndef NDEBUG`.

And the strongest evidence that the order held is not an assertion at all —
it is 139 iteration counts identical to the digit on real instances.

### The verdict, and why the code stayed

**INCONCLUSIVE.** The measurement did not reach its bar, and the honest reason
is that the bar is not reachable on this host rather than that the candidate
fell short of it.

**The code stays in the tree**, under the developer's pre-authorisation of
2026-08-12 taken before execution began and over the stated alternative of
reverting. What justifies keeping a structure on evidence that did not clear a
threshold, said plainly:

- it is a **proven observable no-op** on every published answer — 110 digests,
  29 infeasibility verdicts, 139 iteration counts, five records byte-identical
  once the work field is masked;
- work fell on 118 instances and **rose on none**;
- it is determinism-safe by construction, and the cross-check it brought is
  permanent cover for an equivalence claim the solver had been asserting in a
  comment since before this phase.

**What is unproven is that it buys wall-clock time**, and one instrument says it
costs instructions. This entry is not a claim that it pays. Nothing here should
be cited as evidence that walking a nonbasic set is faster than walking the
model; what it is evidence for is that doing so changes no answer, and that
deciding whether it is faster needs a host this project does not have.

### An amendment: the cross-check's silence stopped being correct partway through this phase

Recorded after the phase's gates had run, from an independent numerics review of
the landed diff. It is in this entry rather than a new one because it corrects a
claim this entry makes.

Above, and in `01-01-SUMMARY.md`, the D-08 cross-check's silence on a no-op
`jm_nonbasic_remove` is called correct on the grounds that a superset bitmap
changes no candidate — `admit_candidate` rejects `JM_BASIC` on its first line —
so a missed removal is a performance fault and not a correctness one. That
reasoning was sound when it was written, against the tree `f2ed4bc` left.

**`b65d9f2` falsified it, and nobody noticed because the two plans were read
separately.** Once the dense branch bills `visited` rather than `s->nvar`, a
superset bitmap inflates `s->work.units` — the currency the gate reads and
`bench/results/*.txt` pins — and `work_units` is tested against
`cfg.work_limit`. A caller with a limit set therefore stops at a different point
and **publishes a different answer on the same model**. That is not a
performance fault. The same applies to a `progress_cb` budget keyed on work.

The instrument could not see it because it compares candidate *sets* rather than
the invariant. The fix is the invariant, and it is O(1) — exactly `nrow`
variables carry `JM_BASIC` wherever the dense branch runs, so the bitmap's
popcount is invariantly `nvar - nrow`:

```c
assert(visited == s->nvar - s->nrow);
```

Calibrated the way `jaos-testing` requires rather than assumed: with
`jm_nonbasic_remove` made a no-op — the fault this entry's own table records the
cross-check as *silent* on — the new assertion aborts at
`src/simplex.c:1686`. Reverted, `make test` (78+4+12), `make sanitize` and
`make all` are green again.

**No measurement in this entry moved, and this was checked on the binary rather
than argued from `NDEBUG`.** The `.text` section of `build/bench/run` — the
linked campaign binary, after LTO — is 97,059 bytes with an identical SHA-256
built from either source. Only the whole-file hash differs, which is the `-g`
line table shifted by the nineteen added lines. Comparing `build/release/*.o`
would have proved nothing: `SHIP` carries `-flto`, so those objects hold GIMPLE
bytecode and have no `.text` at all.

What this does not catch is a compensating pair — one missed insert and one
missed remove inside the same window. The direct bitmap-against-status
comparison would, at the same `O(nvar)` the debug block already spends. It is
not added: no such pair has been constructed, and a check nobody has seen fail
is not yet evidence of anything.

### What is left open

`PLAN.md` is archived since 2026-08-12; open work lives in
`TODO.md`, and these went there.

- **Restricting the candidate set ahead of `bfrt_walk` and `jm_harris_pick`** —
  the higher-ceiling, higher-risk path, and the only one that puts Harris's
  guarantees at stake. Not attempted here and **not refused**. It remains
  available as its own decision, and it would need one.
- **Widening the hyper-sparse path** so the pricing row has a pattern more
  often, which attacks the same cost at its root. Phase 3,
  `REQ-hyper-sparse-downstream-results`.
- **A trajectory sweep over `REFACTOR_EVERY`.** Raised as the roadmap's Open
  Question 5 and not scheduled here, though this phase changed the pivot path.
  D39 and D82 both found that which instances break is scattered rather than
  ordered in a method constant.
- **A host that can resolve 4.2%.** Phase 5 already carries this as a blocker
  under D17; the negative control above is the first measurement of what its
  absence costs a specific gate rather than a statement of principle.
- **The runner's `%8.3f`.** 8 instances of the standard set carry no time ratio
  and 42 more read exactly 1.0000x. Every future time ratio on this set inherits
  that. It is a two-line change to `bench/run.c`, not made here because a source
  edit mid-measurement invalidates the measurement.
- **Nothing in the repository reads the `baseline: NOT COMPARED` line**, which
  exists so a record produced by a `-w` run can be told from a checked one. It
  went unread long enough for such a record to sit committed as the standard
  set's own record. `preflight.sh` is where the check belongs.
- **`galenet` makes two calls to `dual_ratio_test` in a solve reporting one
  iteration.** A small unrecorded fact about where the ratio test runs, not a
  defect, and not chased here.

## D94 — D24's "nothing is gained" reason expired with presolve, and finnis is the recorded exception on the absolute row test

**The question.** D24 kept the checker's primal row-proximity test absolute
rather than relative, on the grounds that relativising it gained nothing —
every instance passed either way — while naming `finnis`'s pass "luck rather
than a property the solver controls". 02-01 landed the first presolve
reduction (columns fixed as loaded) and had to ask whether that luck survives
a reduced model.

**The measurement.** It does not. With presolve on, `finnis` flips the
absolute test: `row=8.44e-07` off, `row=3.76e-06` on. Both readings are a
fraction of one ulp of a row whose terms total 4.0e10 — one ulp there is
7.6e-6 — and the objective, the dual conditions and `rowrel` (D24's own
relative reading) are clean on both sides. The control build,
`EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`, reproduces the documented D24 value on the
nose, so the flip is presolve's different pivot path on a genuinely reduced
model and not a defect in the reduction.

**What was refuted on the way.** A long-double accumulator for the
row-bound/objective-offset shift, tried mid-investigation on the guess that
the shift was losing precision. It moved `finnis`'s residual not at all and
cost `pilot87` — this file's own established amplifier (D74, D89, D92) —
2.31x its work and 2.53x its iterations:

| build | iters | work |
|---|---|---|
| baseline, presolve off | 50850 | 22,977,661,512 |
| committed code, presolve on | 53621 | 24,983,178,548 |
| with the long-double accumulator | 117653 | 58,042,043,010 |

**What closed.** The absolute test stays absolute and the tolerance is not
widened; `finnis` is its one named exception under presolve, and `rowrel` is
the reading that decides. D24's structure stands; its "nothing is gained"
sentence does not, and this entry is where that is recorded.

**Distinct from the open defect.** At HEAD `finnis` is also among the 15
standard-set answers the checker refuses — its terms there are `row=3.64e+03`
and `dual=66.2`, orders of magnitude past anything this entry discusses. That
is the open postsolve defect (`TODO.md` #1), a different failure than the
absolute-row flip this entry closes.

## D95 — The singleton-column families fire only at cost 0, and the free-column-singleton only on a mutual singleton

**The question.** REQ-presolve lists singleton columns without qualification.
Can a nonzero-cost singleton column be eliminated by pushing it to its
favourable bound and letting its row absorb the difference?

**Refuted by counterexample, before any code.**

```
minimize x_j   s.t.   x_j + y = 5,   x_j in [0, 10],   y in [-2, 0]
```

The favourable (cost-minimizing) bound gives `x_j = 0`, forcing `y = 5` —
outside `y`'s own box. The true optimum is `x_j = 5`, forced by the row.
Which bound is optimal is decided by the row's dual, information a presolve
pass does not have before the reduced solve, so the naive elimination can
manufacture infeasibility in a feasible model. The families fire only at
cost 0, where no choice exists.

**The second restriction came from running, not from the design.** The
free-column-singleton fires only when its row is also a singleton — a mutual
singleton. Two findings forced it:

1. The row-pass always wins the race for a degree-1 row, so the non-mutual
   case was dead code, reachable only through a same-round race. The mutual
   check now runs inside the row-pass; the column-pass's own check covers the
   race.
2. Mutuality is what lets postsolve recover the column's value from the
   row's own already-shifted bounds alone, with no dependency on any other
   column's final value and therefore no arena-replay-ordering hazard.

A third rule travels with them: a row that had a bounded singleton column
relaxed out of it is frozen against every other row-removing family for the
rest of the run — its bounds then describe a range the removed column still
needs, not a determined value.

**Evidence.** 02-03's diff (`9aba410`); five round-trip tests with
must-fail-first siblings in `tests/test_presolve.c`; the scope stated in
`src/presolve.c`'s file header. Left open, unmeasured: whether a
dual-informed elimination after the reduced solve could lift the cost-0
restriction. Nothing currently needs it.

## D96 — Presolve's gate: the off-build must reproduce the baselines bit for bit, and the on-build is judged on verdicts, not bit-identity

**The question.** What does "no regression" mean once presolve deliberately
changes the model the simplex sees, on a gate whose baselines were committed
before presolve existed?

**The policy, with a measurement on each side.**

- `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` must reproduce all three committed
  baselines exactly. It does — confirmed by 02-01 when the guard was new and
  reconfirmed since. This is the regression detector, and it is bit-exact.
- Presolve-on is judged on verdict, objective against Koch's reference,
  checker acceptance and determinism. The baseline's own 2x work-ratio rule
  predates presolve and does not apply to instances presolve touches: a
  reduction that removes structure moves work on purpose. At `d861b22` that
  is 26 standard-set lines (`etamacro` work 2.7x, `greenbeb` 2.0x,
  `pilotnov`'s suboptimality bound 9930x among them), `bgindy` on the
  infeasible set (work 2.0x, iterations unmoved), and 4 Kennington lines.
  Which instances move swims as families land — 02-03's snapshot had
  `greenbea` 3.1x and `pilot4i` 2.3x instead of `bgindy` — and that is the
  design, not drift to chase.

**What the policy does not cover.** Checker rejections. The rejections
standing at HEAD are a defect (`TODO.md` #1, which owns the count), not
tolerated movement, and no baseline is rewritten while the gate is red. The deliberate
three-baseline rewrite happens once the phase's families are in and the gate
passes, by the `*-baseline` targets and confirmed by a following gate run.

## D97 — Bound tightening is refused: every design returns INFEASIBLE on models that have an optimum

**The question.** The activity range's fourth reading — imposing on a column
the bound the rest of its row implies — was the family 02-04 was written
around, because it is what lets forcing, redundant and fixed-column cascade.
Six designs were built and measured against the standard set.

**The measurement.**

| design | solved | objective ok | checker ok | refused |
|---|---|---|---|---|
| tightening off (what ships) | 94/94 | 94 | 79 | — |
| bound reasoned with, never published | 92/94 | 92 | 75 | `pilot`, `pilot87` |
| bound published | 90/94 | 90 | 52 | `agg`, `pilot`, `pilot87` |
| published, outward rounding at `DBL_EPSILON` | 89/94 | 89 | 48 | `agg`, `maros`, `pilot`, `pilot87` |
| published, no collapse-to-fixed | 91/94 | 91 | 48 | `pilot`, `pilot87` |
| published, row window at `DBL_EPSILON` | 89/94 | 89 | 48 | `agg`, `maros`, `pilot`, `pilot87` |

Nine epsilon settings from 1e-12 to 1e-4 moved none of it. The last row is
what settles the attribution: with every window reduced to the arithmetic's
own error, four models still come back INFEASIBLE, so the implied bounds
themselves over-tighten on those models — not the comparisons around them.

**The worked case.** `pilot` row 1095, an equality row on one column. A
forcing row pinned column 3554 at 1.15, its own upper bound, while row 1095
needed it at 0; the row went empty holding -1.15 and presolve correctly
declared the model those boxes describe infeasible. The boxes were wrong
because tightening had narrowed them first.

**What ships instead.** The three other readings of the same range —
infeasible, forcing, redundant — measuring better than the tree they came
from on all three sets. `jm_presolve_counts.tightened_bound` stays declared,
stays zero, and is pinned at zero by a test, so relighting the family
without redoing the campaign fails.

**For whoever tries again, in the order the evidence puts it:** first a
derivation of why the implied bound over-tightens on `pilot`, `pilot87`,
`agg` and `maros` specifically — it is not the epsilon and not the rounding,
both measured on both sides; second, a dual postsolve for an imposed bound —
a column resting at a presolve-derived bound is interior to the caller's own
box, where the checker requires a zero reduced cost that only the implying
row's multiplier can pay, which is sound in exact arithmetic and not
otherwise; third, both under a campaign, because every design in the table
looked right when it was written. Raw readings: `bench/measurements/02-04/`.

## D98 — The planning layer is retired: the record is five documents, and the process is the loop in CLAUDE.md

**The question.** The GSD layer under `.planning/` — ROADMAP, REQUIREMENTS, a
37 KB STATE file, ten plan files of 20-30 KB, summaries, research, validation
matrices and two index systems — existed to keep this project coherent. Did
what it cost buy that?

**The measurement, in costs.**

- 3.5 MB and 194 files of process, several times the solver's own source.
- STATE.md carried "Plan: 4 of 9 (01, 02 complete)" in its body while its own
  frontmatter recorded 02-03 complete — same file, same day.
- Token estimates missed by 3.6x to 10x on four of the seven executed plans
  (01-04: 7,400 realized against 70,000 estimated; 02-04: 23,500 against
  85,000), because WSL machine time — which set every real duration — appears
  in no estimate field.
- Inserting one diagnostic plan required renumbering six files, because a
  wave is an integer.
- The apparatus did not prevent the one failure it existed to prevent: 02-03
  ran one instance set of three while a 24 KB plan, a validation matrix and
  an orchestrator all watched, and 15 checker rejections crossed a wave
  unseen. What found them was the standing habit of running all three sets,
  which lives in a skill, not in a plan.

**The decision.** Five documents are the record: `SPECS.md` for what,
`TODO.md` for what is next, `DECISIONS.md` for why closed questions closed,
`CHANGELOG.md` for what landed, and `docs/` plus `bench/` for the contracts
and the evidence — raw readings under `bench/measurements/<id>/`. The
per-change process is the loop in `CLAUDE.md`. GSD is not used in this
repository. `docs/archive/PLAN.md` stays archived because 88 comments cite
it by section.

**What was kept from the layer, and how.** Every open item and standing debt
moved to `TODO.md`; D94-D97 above were written from the phase summaries
before deletion; the raw measurement records moved to
`bench/measurements/02-04/`. Everything deleted remains reachable in git
history.

## D99 — The singleton column was judged against bounds that had stopped describing its row, and that is the whole of the row-residual defect

**The question.** 02-03's diff took the standard set from 93/94 to 78/94 and
Kennington from 16/16 to 8/16; 02-04's one repair returned 79/94 and 12/16.
Every objective still matched its published reference — the standard set's 94
against Koch, Kennington's 16 against netlib's own values, and the 29
infeasible models have no objective to match — and all 139 instances stayed
deterministic, so the defect was in what postsolve published rather than in
the simplex (D96's off-build reproduces the pre-presolve baselines bit for
bit). Which of the five families publishes a point that misses its own row
bounds by up to 1.9e+04, and what makes four Kennington instances read
`rowrel` exactly 1/3? A ratio of exactly one third is structure, not
rounding.

**The measurement.** Raw readings: `bench/measurements/02-05/`.

*Attribution.* A build carrying one runtime switch per family, 16 rejected
instances against 8 settings (`attribution-02-03/out-<instance>.txt`). Empty
rows, empty columns, the free-column-singleton and the activity pass are
exonerated: with each off, no term moves on any instance. Disabling the
bounded singleton column alone cleans every row residual in the set.

*The site.* `ps_replay_one`'s `JM_PS_SINGLETON_COL` case read
`rest = sol_row[i]` and judged the recovered value against the row's ORIGINAL
bounds. The replay is strictly LIFO (D-07), so a record pushed earlier
replays later: every column removed before this one had not yet added its
share. Traced on `ken-07`, all six violated rows: the violation equals
exactly the sum of the contributions replayed after that record. Row 2413,
`rl = ru = 1506` — the record publishes 1506 against `rest = 0`, and 53 fixed
columns then add 753. The correct value was 753
(`attribution-02-03/trace-ken07.txt`).

*The 1/3.* With `R` the equality's right-hand side and `F` the later-replayed
sum, published activity is `R + F` and the violation is `F`, every term
positive, so `rowrel = F/(R+F)`. On `ken-07`'s six rows `F` is exactly `R/2`
(753/1506, 707/1414, 742/1484, 706/1412, 729/1458), and
`(R/2)/(3R/2) = 1/3`. The half is a property of that model family; what the
code contributes is that the violation IS `F`.

*The fix.* The record carries `row_lo`/`row_hi`: the row's own bounds at the
moment the column left, read before its own relaxation shifted them. Partial
activity and recorded bounds then describe the same set of columns.
`JM_PS_FREE_COL_SINGLETON` already recorded shifted bounds this way and the
sweep exonerated it, which is the pattern this follows.

*What it cost.* All three sets at `J=12`, run twice on the final tree,
byte-identical records both times. Standard set 89/94 checker ok, from 79/94
— ten instances move REJECTED to ok (`czprob finnis lotfi perold pilot-ja
pilot87 pilotnov pilot-we share1b tuff`). Kennington 16/16, from 12/16, with
the runner reporting no field differing from its pre-defect committed
baseline, digests included. The infeasible set unchanged in all 29 lines.
Work units, iteration counts and presolve dimensions moved on no instance of
any set. `geomean.py --metric work` against `02-04/final-*.txt` reads best
and worst 1.0000x on each of the three sets, which is stronger than a mean of
1.0000x: no instance moved at all. `record_diff.py` counts 78 of 94, 29 of 29
and 12 of 16 bit-identical, 119 of the 139, and reports no regression on any
set. Twenty solution digests moved. Four of
those changed no checker term at all, and dumping `x` and `y` separately
splits them: `y` is bit-identical in all four, and `pilot4`, `standata` and
`standmps` publish a genuinely different feasible point (8, 6 and 8 entries
of `x` move, by up to 3.75, 10 and 10), which a cost-0 column can do without
touching the objective. `nesm` is not that: one entry moves by 4.26e-14, so
it is the same point to fourteen digits and its digest moved on rounding. Independently
re-derived, ACCEPT, by `jaos-measurer` in a context that did not produce
these numbers.

**What was refuted.**

- *That the stale-status class explained it.* 02-04 repaired one such
  recovery, and the reading that its siblings covered the rest was wrong:
  they explain no row residual at all. `bnl1`, `bnl2` and `e226` are
  bit-identical across this change — it does not touch them.
- *That `BASIC` is the right status for the surviving frozen row.* Written
  into the code on the reasoning that every column had left at a bound.
  Refuted by building the same model with `-DJAOS_NO_PRESOLVE`: the reference
  publishes `AT_LOWER` and a basis of the promised size. The reasoning had
  missed that a recovered column can land strictly inside its own box.
- *That a postsolve change can only move `x`.* The singleton row's replay
  decides its multiplier from the published column value (`zero_works`),
  which this change moves, so `y` moves with it — and it did, measurably, not
  as a possibility left unexercised. The checker's `dual` figure fell to
  exactly 0 on five instances (`finnis` from 66.2, `pilot-we` from 6.82e+03,
  `perold` from 7.87, `lotfi` from 0.001, `pilot87` from 0.000739), all five
  now passing. On `25fv47`, `y[338]` went from 0.5927293667749798 to exactly
  0 while `max_dual_violation` stayed bit-identical at 6.1221772946311033:
  the vector moved and the figure that summarises it did not. No verdict
  moved anywhere.
- *That `rowrel = 1/3` was a coincidence of scale.* It is exact, and derived
  above.

**Two latent risks stopped being latent.** The attribution report closed with
two unconfirmed suspicions, and both are now confirmed and carry a test.
Several bounded singleton columns can share one row — the family checks
`!free_col` and never `row_frozen[i]` — which was measured as one record per
violated row across all 16 instances, so it was a shape with no live case;
it is now `test_two_singleton_cols_on_one_row`, the only one of the three new
tests that produces a column-bound violation as well as a row one, and the
shape that aborts the old assert. And `jm_postsolve_solved` seeds no
activity at all, which is now tested and is where the uninitialised basis
status surfaced.

**What is left open**, handed to `TODO.md`:

1. Five rejections remain (`25fv47 bnl1 bnl2 e226 vtp-base`), a dual-recovery
   defect independent of this one. Evidence in `no-presolve/`, with the
   canary: all five are checker ok with `dual=0` compiled without presolve,
   and carry `dual` 0.0705, 6.12, 9.81, 1.16 and 1.32e+03 with it. On the two
   that had both terms the primal residual collapses while the dual stays
   identical to the digit (`25fv47` row 4.32 to 3.5e-13, `vtp-base` row
   1.9e+04 to 2.23e-11).
2. The basis the singleton-column family publishes breaks `jaos.h`'s
   row-count promise whenever the column is recovered strictly inside its own
   box, on both postsolve paths.
3. An infeasible model can be published OPTIMAL, because a frozen row is
   never rechecked after being relaxed. `netlib-infeas` has no instance of
   the shape, so the set cannot currently see it.
4. The reduced-cost-on-an-interior-point mechanism. Both instances the
   attribution recorded are closed here, and the arithmetic accounts for
   each: `perold`'s column 1325 sits in a row surviving as an equality at
   -0.1401, and with the recorded pair the intersection collapses to a point,
   putting the column on its own lower bound where a reduced cost of
   +7.8697698235111009 is legal; `pilot-we`'s column 81 lands the same way
   through the negative-coefficient branch, with +6817.1056233617956. Both
   published values are exactly 0.0 and both statuses are `AT_LOWER`, which
   was predicted from the mechanism before it was checked. Both had
   `rec->lo` equal to the column's own lower bound, which is why the checker
   reads the landing as a bound. The residual form is the one neither has: a
   singleton column whose bounds were TIGHTENED before removal lands on a
   tightened bound that is interior in the original box. No instance of that
   shape appears in any of the three sets, so the measurement says nothing
   about it, and a set with no instance is not a proof. `pilot87` was
   recorded as fitting by shape, never verified, and passes now too.

## D100 — Several rows can fold into one column, and the multiplier is owed to the one whose bound the column rests on

**The question.** D99 closed the row-residual half of the postsolve defect and
left five standard-set instances refused on a dual term: `25fv47`, `bnl1`,
`bnl2`, `e226`, `vtp-base`. The primal point was certified on all 94, every
objective matched its published reference, all 139 instances were
deterministic, and all five came back checker ok with `dual = 0` under
`-DJAOS_NO_PRESOLVE` (D96's off-build), which placed the defect in presolve's
own dual recovery. Four sites can produce a `max_dual_violation` and each
leads to a different repair, so the label had to be established before
anything was changed. The recorded lead was the `zero_works` test reading a
`sol_redcost[j]` three other producers can have written first.

**The measurement.** Raw readings: `bench/measurements/02-06/`.

*Attribution.* A `JAOS_DIAG` build printing one line per `JM_PS_SINGLETON_ROW`
record in replay order — every input to every predicate that decides its
multiplier — with `src/check.c` instrumented to name the worst dual term.
The build reproduces all five `max_dual_violation` figures at 17 digits,
which is what says the instrument is faithful. Every offending term is a row;
none is a column.

The shape is the same in all five, and it is not the recorded lead. Two
`SINGLETON_ROW` records fold into one column. The replay is LIFO, which is not
the order they tightened in, and `zero_works` reads the column and never the
row, so it is true or false for both alike. The record replayed first takes
the whole reduced cost whether or not its own row produced the bound `x_j`
rests on; the row that did produce it then finds `d0` already zeroed:

| instance | row that took it | its tl/th | row that owns the bound | its tl/th | published |
|---|---|---|---|---|---|
| `25fv47` | 696 | 0/0 | 695, an equality at 17 | 1/1 | -6.1221772946311033 |
| `bnl1` | 638 | 0/1 | 636, lower at 110 | 1/0 | +0.070451146994220337 |
| `bnl2` | 13 | 0/0 | 6 | 0/1 | +9.8102420146394795 |
| `e226` | 14 | 0/0 | 13 | 0/1 | -1.1645190903954061 |
| `vtp-base` | 176 | 0/0 | 175 | 0/1 | +1320.1986408 |

*The fix.* The record carries `lo`/`hi`: the bounds the column is left with
after that fold. The replay asks the question directly — does `x_j` rest on
the bound THIS row produced, on the side `d0`'s sign needs? The comparison is
exact and that is not an approximation standing in for a tolerance: a nonbasic
column rests on its bound bit for bit, and `rec->lo`/`hi` is the same
computation that produced it. No constant is introduced. Both numbers are in
the original space, since presolve runs before scaling. A row whose fold was
later overwritten by a tighter one compares unequal and declines, which leaves
the reduced cost intact for the record that does own the bound. The order
stops mattering.

*What it cost.* All three sets at `J=12`. Standard set 94/94 checker ok, from
89/94; the five move REJECTED to ok and their `rsub` collapses (`vtp-base`
4.23 to 1.81e-15). 127 of the 139 records are bit-identical to the committed
record and `netlib-infeas` is entirely so. **No work unit, iteration count or
presolve dimension moved on any instance of any set** — geometric mean
1.0000x on all three. Twelve records move their digest: the five, plus
`bore3d`, `d2q06c`, `fffff800`, `finnis`, `standmps`, `cre-a` and `cre-c`.
The change writes `sol_dual`, `sol_redcost` and `sol_col_status` and never
`sol_col`, so `x` is unchanged on all 139 and only `y` moved; the checker
recomputes `d_j` from `y`, where a wrong multiplier would land on the column,
and reads `dual = 0` exactly on all seven before and after. `make warm`: the
same five go checker-refused to ok, every other field on their lines is
bit-identical, and `make warm-kennington` is identical line for line.

*The basis, which no gate reads.* `jaos_basis` compared across all 110 optimal
instances: three move, each losing exactly one basic column (`d2q06c`,
`fffff800`, `cre-c`). The `basic == num_row` invariant was already broken on
66 of the 94 standard and 12 of the 16 Kennington instances, **with the same
counts on both sides of the change** — none that held is broken and none that
was broken is fixed. `warm` is unmoved on all three, which is where the cost
of a moved basis is measured.

**What was refuted.**

1. *Reading `row_tightens_lo/hi` alone.* `bnl1`'s row 638 has
   `row_tightens_hi` set and is still not the owner, because the column ends
   up resting on the lower bound row 636 imposed. Two rows can tighten and
   only the one whose bound survives is owed the multiplier.
2. *An assert at the decline site*, proposed by the review: fire when a record
   declines with `d0 != 0` and `v0` strictly inside the column's original
   bounds. It fires on a legitimate decline — `bnl1`'s row 638 declines with
   `d0 = 0.070451146994220337` and `v0 = 110`, interior to `[0, inf)`, and row
   636 pays immediately after. The postcondition it wants is global to the
   whole replay, and `jaos_check_solution` already computes it.
3. *Reading a campaign by the checker's own reported terms.* An exploratory
   sweep comparing the five report fields over the 94 found exactly the five
   and would have supported "the other 89 are unchanged". Seven of those 89
   had moved their duals. Only the digest sees it, and this is the second time
   after D99 that an unchanged figure was nearly read as an unchanged vector.

**What is left open**, handed to `TODO.md`: the collapsed fold whose midpoint
is no row's implied bound, refused by this code and by the code it replaced
alike; `assert(want_lo <= want_hi)` firing on `bnl1` and `finnis` when
assertions are live, which predates this change and which no gate can see
because the release build carries `-DNDEBUG`; and the `warm` record last
written before presolve existed, which makes any diff against it unreadable.

## D101 — The last three presolve families have 0.15% left to remove on this instance set, which defers them rather than refusing them

**The question.** `REQ-presolve` scopes five reduction families that are live
and three that were never built: duplicate rows, duplicate columns and
dominated columns. The expectation going in was that they were simply
unfinished work. Before writing any of them, two things were established: what
the literature actually specifies, and how much they would find on the models
JAOS is tested against.

**The measurement.** Raw readings: `bench/measurements/02-07/`.

*The literature first* (`02-07/literature.md`, citations verified against
publisher or Crossref). Three parts of the work have **no published source**
and would be ours to derive: the dual postsolve for parallel rows, the primal
split rule when two columns are merged into one, and the parallel-with-
mismatched-cost case, which §6.4's Definition 6 does not capture because it
carries no scale factor. No source gives a tolerance for deciding two rows are
parallel. And HiGHS omits parallel rows and columns deliberately — Galabova
§3.2.3, quoted in full there — which matters because JAOS's field value is
measured against HiGHS (D81). Achterberg §6.3 does settle one thing `TODO.md`
asked for: the duplicate/dominance boundary is `c_k = lambda * c_j`, written
in the source, and the bound conditions beside it are entirely about
integrality and do not apply to continuous LP.

*The count.* A counter compiled into presolve under `-DJAOS_DIAG`, reading the
model presolve publishes, so it sees what is left after the five live families
reach a fixed point. Calibrated on a five-by-five model with a known answer —
three mutually parallel rows and one parallel column pair — where it reports
exactly the two and the one (`02-07/validate.c`).

| set | live rows | removable | live cols | removable | dual fixing |
|---|---|---|---|---|---|
| netlib (94) | 78445 | 151 | 157858 | 1450 | 1053 |
| kennington (16) | 205651 | 298 | 844890 | 4 | 0 |
| infeasible (29) | 13204 | 22 | 26267 | 72 | 30 |

471 rows of about 297000 and 1526 columns of about 1029000: **0.15% of each**.
Concentrated rather than spread — `cre-a` holds 222 of Kennington's 298
removable rows, and `d6cube` holds 735 of netlib's 1450 removable columns,
about 12% of that one model.

*The tolerance nobody publishes turns out to decide almost nothing here.* Each
count was taken at tau = 0, 1e-12, 1e-9 and 1e-6 in one pass. Netlib's
removable rows move 142 to 151 across that whole range and its columns 1450 to
1471; on Kennington nothing moves at all. The pairs that exist are exactly
parallel.

**What was refuted.**

1. *That the benefit here is unmeasurable.* It was argued first that 0.15% sits
   far below this host's 6.27% repeatability (D93) and so could not be
   measured. That is wrong and worth writing down: 6.27% is the repeatability
   of the **clock**. Work units and problem dimensions are exact integers, and
   removed rows are counted, not timed. The benefit would be measured
   precisely. It is small, which is a different statement.
2. *That a count on these 139 models settles the question.* It does not, and
   this is why the entry defers rather than refuses. netlib is old and curated
   and Kennington is a few network families. Gurobi's own figures, on their
   customer library, put parallel rows in more than half the models of their
   slowest tranche, and Tomlin and Welch wrote a paper in 1986 on finding
   duplicate rows precisely because auto-generated industrial models carry
   them. A measurement on this set is not a statement about that population.
3. *Two versions of the counter, both caught by being too clean.* The first
   hung off a label most exit paths skip, so five instances produced no line,
   and it read the reduced model unconditionally, reporting zero on every
   instance presolve does not touch. The second counted parallel pairs rather
   than removable rows — k mutually parallel rows are k(k-1)/2 pairs and k-1
   removals, so `d6cube` read 3048 where the answer is 735 — and tested dual
   fixing without reading each row's sense, calling 421615 Kennington columns
   fixable, which would have collapsed models that solve normally.

**What is left open**, handed to `TODO.md`'s deferrals table: the three
families, with the reopen condition being a model population where the counter
reports a non-trivial share. The counter is committed at
`bench/measurements/02-07/` so that condition is executable rather than a
matter of opinion. Presolve is otherwise complete at five families; nothing is
removed by this entry.

## D102 — A relaxed row is skipped by every pass that could refuse it, so an infeasible model was published OPTIMAL

**The question.** A review found that `min x0 s.t. x0 + x1 = 100, x0 in [4,4],
x1 in [0,3]` — which has no feasible point — came back OPTIMAL with `x1 = 96`
against a box of `[0,3]`. The checker read a column violation of 93. The
expectation was that this was one defect with one repair. It was two, sharing
one assert.

**The measurement.** Raw readings: `bench/measurements/02-08/`.

*Reproduced in three builds.* `-DJAOS_NO_PRESOLVE` says INFEASIBLE, which is
the correct answer and the only oracle for it. The shipping build says OPTIMAL
with the violation. A build with assertions live aborts at
`assert(want_lo <= want_hi)` in `ps_replay_one`. The mechanism is exact rather
than numerical: `lo_j = (100 - 4)/1 = 96` intersected with the column's own
`[0, 3]` is empty, and the replay published `want_lo` regardless.

*The same assert has a second, unrelated trigger.* Instrumenting it to print
instead of abort: 11 of the 94 standard instances reach it with a gap of an
ulp — `bnl1`, `finnis`, `80bau3b`, `bandm`, `cycle`, `dfl001`, `nesm`,
`perold`, `pilot-ja`, `pilot`, `pilotnov`, gaps 2.2e-16 to 1.3e-15. `bnl1`
row 581 wants 2.1850000000000005 from a column whose own upper bound is
2.1850000000000001. Those publish a value about 4e-16 outside the box and the
checker's tolerance absorbs it. **That is why the assert could never be
enabled**: it would abort on eleven real instances, and it cannot tell a
rounding gap from a gap of 93.

*The repair, and why it is where it is.* Both the row pass and the activity
pass skip a frozen row, for the same correct reason — its bounds no longer
describe a determined value — so between them nothing asks whether the row can
still be satisfied. A pass after the round loop applies the test the activity
pass already applies to every other row. Once, not per round: a box only
narrows, so the final boxes are the tightest.

*What it cost.* All three sets at `J=12`, `gate: PASS` on each. 94/94, 29/29
and 16/16. **No digest moved on any of the 110 optimal instances**, which is
the strong claim: this change can turn OPTIMAL into INFEASIBLE and can do
nothing else. Work rises on 60 of 94 netlib (+111057), 5 of 16 Kennington
(+68894) and 8 of 29 infeasible (+3732), because the pass charges a nonzero
per stored entry of every frozen row whether or not it fires. That bound is
structural and was checked on all 139: no instance's delta exceeds its own
nonzero count. Two infeasible instances now leave in presolve rather than the
simplex — `pilot4i` from 408 iterations and 7063304 units to 0 and 13185,
`galenet` from 8268 units to 26.

**What was refuted.**

1. *That `ps_row_tol` was the right window.* It scales by the LIVE traffic,
   and a frozen row's live traffic is routinely zero, so the window collapsed
   to 1.776e-15 absolute against bounds carrying rounding proportional to what
   had been subtracted from them. 117 rows of the standard set sat at exactly
   zero margin, `greenbea` row 57 with 660 of shift. They passed because the
   cancellation happened to be exact. Replaced with
   `PRESOLVE_TIGHTEN_EPS * ps_bound_scale`, whose coverage ratio and its
   3679x measured margin are in `docs/tolerances.md`.
2. *Rescaling by `row_traffic` instead*, which was the obvious fix and is a
   no-op: `row_traffic[i]` saturates to `+inf` the first time a half-bounded
   column is relaxed out of the row, so the window would become infinite on
   precisely the rows this test exists for. Carried to `TODO.md`; the pass
   avoids it by using `ps_bound_scale`, which skips infinities by design.
3. *Charging the live degree*, which the activity pass does. There every row
   has degree 2 or more; here rows whose live degree collapsed are the common
   case, and a row emptied to degree 0 was charged nothing for walking all its
   stored entries. 98339 charged against 111057 walked.
4. *A cost figure taken from the `presolve=` field.* The first reading of the
   work delta was 98339 with `fit2p` at +47284; both are that instance's
   presolve output-nnz, not its delta. The real figures are 111057 and +50284,
   and they coincide closely enough with the wrong ones to be believed.

**What is left open**, handed to `TODO.md`: the ulp-sized empty intersection,
which is the other half of the assert and wants the published value clamped to
the column's own box — deliberately after this entry, because clamping first
would have masked a gap of 93 as if it were rounding; and `row_traffic`'s
saturation.

## D103 — Presolve was written for one objective sense, and one tolerance answered a question that has no tolerance

Two defects, found by reviewing the whole presolve diff as one rather than
family by family, and both confirmed by running a model against the
`-DJAOS_NO_PRESOLVE` reference before either was believed. Each published
OPTIMAL where the reference gives the right verdict. The raw readings are in
`bench/measurements/02-09/`.

### The first question: what does presolve do with a MAXIMIZE model?

Nobody had asked. `src/presolve.c` did not contain the word `sense`. The
canonical form the rest of the solver works in is minimise — `src/check.c` and
`src/simplex.c` each build `sigma = (sense == MAXIMIZE) ? -1 : 1` and apply it
to every multiplier and every reduced cost — and presolve was the one stage
that skipped the conversion. Every rule in it that reads a cost sign or a dual
sign was therefore inverted on such a model.

Measured, against the reference build:

| model | presolve | `-DJAOS_NO_PRESOLVE` |
|---|---|---|
| `max x1`, `x1` an empty column, cost 1, box `[0, 5]` | OPTIMAL, obj **0** | OPTIMAL, obj **5** |
| the same, box `[-inf, 5]` | **UNBOUNDED** | OPTIMAL, obj 5 |
| `tests/test_simplex.c`'s `test_maximise_without_column_upper_bounds` | dual `[2, 0]`, redcost `[1, 0]` | dual `[2, 1]`, redcost `[0, 0]` |

The third row is a test the project already shipped, and it passed. The
checker's own implied bound on `x` made the sign condition true regardless of
which side the reduced cost sat on, so a wrong dual satisfied it.

**Why no gate could see it.** Netlib is entirely MINIMIZE — all 139 models of
all three sets — and `tests/test_presolve.c` had zero MAXIMIZE cases. A green
gate and a green suite were both compatible with half of the public
`jaos_sense` enum being wrong.

The repair applies sigma to the questions and not to the stored values:
`d_j = c_j - a_ij y_i` holds in the model's own space whatever the sense, so a
multiplier is derived unflipped and canonicalised only where a sign is
compared. On a MINIMIZE model sigma is 1.0 and multiplying a double by 1.0 is
exact, so the change is bit-identical there by construction — which the
campaign then confirmed rather than assumed.

### The second question: is 1e-9 a tolerance, or a number somebody picked?

`PRESOLVE_TIGHTEN_EPS` was the window at three sites. All three ask the same
thing: this residue came out of a running difference of terms, is it a number
or is it what is left of cancellation? That question has no knob. The answer
is `DBL_EPSILON` times the traffic that produced the residue, which is what
`ps_row_tol` has said since 02-04 and what its own comment in the file argues
at length. 1e-9 is 5.6e5 times wider than that.

The window is relative, so the gap is not academic at model scale. Three
models, each minimal, each refused by the reference build:

| model | what presolve published |
|---|---|
| `1e9*x0 + 1e9*x1 == 2e9 + 1.5`, `x0` and `x1` fixed at 1 | OPTIMAL, row missed by 1.5 |
| `min x1 s.t. x1 >= 1e9 + 0.4`, `x1 in [0, 1e9]` | OPTIMAL, `x1 = 1000000000.2` |
| `min x0 s.t. x0 + x1 == 1e9 + 1`, `x0` fixed at 0.5, `x1 in [0, 1e9]` | OPTIMAL, `x1` 0.5 above its own bound |

The second publishes a value a fifth of a unit outside a bound the caller
stated. The third is the frozen-row test D102 landed the day before: the same
half D102 closed, escaping through the window instead of around the test.

### The measurement that set the constant, and why the sweep could not

`PRESOLVE_ROUND_ULPS = 8` replaces it at all three sites. The old constant is
deleted; `-Werror` refused to keep an unread `constexpr`, which is the right
answer, because a tunable that moves nothing is D82's shape exactly.

**And at two of the three the SCALE changed as well, which "replaces the
constant" hides.** Only the frozen-row test is a straight constant swap. The
emptied-row test falls back to `ps_bound_scale(cur_rl, cur_ru)` when
`row_traffic[i]` is not finite, which is new behaviour the constant has
nothing to do with. The fold collapse takes
`max(ps_bound_scale(new_lo, new_hi), row_traffic[i] / fabs(a))`, and that
traffic term is a new term rather than a rescaling: where it exceeds 5.63e5
times the bound scale **the new window is wider than the old one**. So this
change can move a verdict in either direction, not only toward refusing more.
`src/presolve.c` says so beside the code; this entry said "replaces the
constant at three sites" and that was a simplification the code does not
support. Corrected after an independent re-read asked what the other two sites
actually do.

**A sweep was the wrong instrument here and had already run.** The nine-decade
plateau recorded against `PRESOLVE_TIGHTEN_EPS` (1e-12 to 1e-4, nothing
moving) was a true reading and it could not have found any of this: its canary
conflicts by 1e-8 on a unit-scale model, so it calibrates the window's
absolute floor and says nothing about a model whose scale multiplies the
window up to 1.0.

What answered it was measuring the residues themselves. A patched copy of
`src/presolve.c`, built outside the repository, printing at each site the
residue it is about to judge divided by `DBL_EPSILON * scale` — so the printed
number is the residue in ulps of that site's own scale, directly comparable
with the ulp count in the window. Over all three sets:

| site | probe lines | residue > 0 |
|---|---|---|
| emptied row | 13150 | 0 |
| fold collapse | 8 | 8 |
| frozen row | 19082 | 4 |

**"Probe lines", not "sites reached", and the difference is only in the
middle row.** The emptied-row and frozen-row probes print on every reach; the
fold probe prints only when the fold actually collapsed, so its 8 is
collapses. Independent instrumentation counts that site 18388 times on netlib
and the infeasible set alone. The residue reading is unaffected — the probe
measures the violating direction by construction — but a column headed "sites
reached" claimed a coverage the number does not describe.

All twelve positive residues are on `netlib-infeas`, and the smallest is
3.69e8 ulps. So no feasible model among the 139 puts a residue anywhere in
`(0, 3.69e8 ulps]`, the constant may be set anywhere in that interval, and 8
is taken because it is where `ps_row_tol` already is and because it counts
roundings rather than naming a magnitude. Swept 1 through 256 for the record:
flat, with four canary flips inside the grid and seven distinct binaries.

### What the campaign said

Every record bit-identical to the one committed before the change: 94, 29 and
16 instances, 0 digests moved. The `-DJAOS_NO_PRESOLVE` control reproduces all
three pre-presolve baselines with 0 regressed, 0 improved, 0 new.

**The bit-identity is carried by the campaign and not by the residue
argument**, and the difference is worth stating because the entry first read
the other way round. The residue run was taken on the candidate tree — this
directory's README says so — so what it measures is the inputs to the NEW
windows. It establishes that the new windows are set where nothing on these
139 models is near them. It cannot on its own establish that the old and new
windows agree, because it never ran under the old ones. Two runs of the three
sets, one per tree, are what establishes that, and they are what was done.

**And a no-op can be true trivially, which the campaign also cannot rule
out.** If the changed branches never execute, identical records prove only
that dead code is dead. The independent re-read settled it by instrumenting a
copy of `presolve.c` that computes BOTH windows at each site and counts every
decision where they would disagree. Over all three sets:

| | decisions |
|---|---|
| emptied row | 13150 |
| fold collapse | 100034 |
| frozen row | 19082 |
| **old and new window disagree** | **0** |
| traffic term widens the window | **912** |
| non-finite-traffic fallback | 0 |

**132266 window decisions, zero disagreements, and the new formula fires 912
times with a genuinely different scale.** The no-op is measured rather than
inferred, and it is not an artefact of dead code. That is the statement this
entry should have rested on from the start. The fallback firing zero times
matches the source calling it unreachable today.

The two instruments agree to the unit on the sites both count, 13150 and
19082, written independently in different sessions. That is what validates
each of them.

The first attempt at the second instrument read all zeros, and that was a
broken instrument rather than a result: `bench/run.c` forks per instance and
the children `_exit()`, so nothing that runs at destruction time survives.
Recorded because any future counter hung off the runner meets it, and because
a zero there looks exactly like a clean pass.

The same campaign carries what presolve itself is worth, which `TODO.md` had
open: geometric means of per-instance ratios, presolve on over off, work
0.810x standard / 0.651x Kennington / 0.084x infeasible, and seconds about 0.3x
over the six instances presolve removes the most from against a negative
control that reproduces at 1.0. The per-instance tables are in
`bench/measurements/02-09/`.

### The verdict, from a context that did not produce the numbers

**ACCEPT.** `jaos-measurer` built the parent in its own worktree, ran both
sides itself and reproduced every claim: 139 of 139 records bit-identical with
`cmp` IDENTICAL on all three, the `-DJAOS_NO_PRESOLVE` control at 0 regressed
on all three, the work ratios to four figures, and every probe row rebuilt
three ways with distinct binaries.

It added two checks this entry did not have. The 132266-decision count above
is one. The other is a regression the gate cannot see at all: the shipping
build is `-DNDEBUG`, so `assert(want_lo <= want_hi)` is compiled out, and
built with assertions on **both trees abort on the same 11 netlib instances**
— `80bau3b bandm bnl1 cycle dfl001 finnis nesm perold pilot pilot-ja
pilotnov`, 0 on the other two sets. Unmoved, and matching the count `TODO.md`
already owns.

It also found three figures in the write-up that did not follow from the
readings, all since corrected: four iteration counts carrying `--plus-one`'s
offset, a column headed "sites reached" that counted collapses, and the
infeasible set described as passing a checker predicate it does not have.

### What was refuted

1. *Sharing one function between `ps_round_tol` and `ps_row_tol`.* They have
   the same shape and, today, the same number, so the first repair unified
   them — and thereby put the three activity-range readings on the
   `EXTRA_CFLAGS` hook, which `docs/tolerances.md` says in as many words must
   not happen. `make netlib EXTRA_CFLAGS=-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=64`
   reproduced 02-04's failure: `pilot` INFEASIBLE, column 3554 pinned. Caught
   by review, not by the suite. Two constants that happen to be equal are not
   one constant.
2. *Trusting the coverage proxy in `docs/tolerances.md` for the third site.*
   It bounds accumulated shift against `ps_bound_scale` and reported a 3679x
   margin, which reads like comfort and is a one-sided statement: it says the
   window is not too tight. The model that broke the site has a ratio of about
   1.0, deep inside the allowed 5.6295e5.
3. *Reading the residue measurement as "the residues are small".* It is not
   that. The probe measures the violating direction only, so a feasible row
   reads exactly 0 by construction. The claim it supports is the interval one
   above, and nothing stronger.
4. *A canary alone proving a sweep reached the binary.* Four conflicts do not
   separate every adjacent pair of a grid that steps by 2 in places and by 4
   in others: 2 and 4 read alike, 8 and 16 read alike. Seven distinct md5s is
   what settles it.
5. *That this host's timing floor is 1%.* The negative control read 0.9934x
   over a 2.2% spread, which looked like a much tighter floor than D93's
   6.27%. Re-run under the identical protocol in a fresh session it reads
   1.0012x over **8.2%**, 3.7x wider. The centre reproduces and the tightness
   does not, so the narrow band was that session. The mechanism is runtime:
   `maros-r7` at 23.1 s reproduces to 0.07% and `vtp-base` at 0.0003 s to
   22.8%, because a sub-millisecond solve is timing its own process startup.
   **The seconds figure is quoted as about 0.3x from here**; four significant
   figures were never supported, and the direction and size still are — every
   mover sits 2.3x to 5.2x below 1.0.

### What is left open

Handed to `TODO.md`: `grow22` and `grow7`, which presolve makes 11.16x and
8.56x more expensive in work and 7.5x and 8.8x in iterations. They arrived
with presolve rather than with this change — the records here are
bit-identical to the ones committed before it — and nothing has yet asked why
a reduced model is so much worse for the simplex than the model it replaced.
The two window guards against an infinite `row_traffic` are unreachable today
and stated as such in the source; the saturation itself is unchanged and still
carried.

## D104 — The ladder is recalibrated, and JAOS's presolve is worth what the others' are worth while HiGHS presolves the instances that decide the set

`bench/compare/README.md` had said since it was written what would happen when
JAOS gained a presolve: presolve moves into the bottom rung for everyone. JAOS
gained one (D103), so this closes the recalibration `TODO.md` carried as
undecided, and reports what the new rung reads.

### The rung

**P0**: JAOS as it ships — dual simplex, presolve, no crash basis, one thread
— against each competitor with presolve on, the dual forced, no crash basis,
one thread, tolerances equalised at 1e-7. It is T0 with exactly one line
changed per competitor. T0 through T3 keep their definitions and their
records; nothing was renamed underneath a stored number, and T0 becomes a
historical rung because running it against a presolving JAOS would put
presolve on one side only.

| P0 | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 4.13x | **1.18x** | 3.50x |
| iterations | 1.81x | **0.68x** | 1.46x |
| **time per iteration** | **2.29x** | **1.73x** | **2.39x** |
| JAOS faster on | 1 of 18 | **10 of 21** | 1 of 14 |
| worst | `maros-r7` 72.5x | `maros-r7` 20.9x | `maros-r7` 48.1x |

Against T0's 3.72x / 1.34x / 3.77x (D83): closer to SoPlex and to Clp, further
from HiGHS.

### Why the gap to HiGHS widened, read against each solver rather than through JAOS

The README's own rule. Each competitor's binary is pinned by checksum and
identical at both rungs, so T0 → P0 times that binary twice and the difference
is its presolve. Geometric mean of per-instance time ratios, over whatever
each has above the 0.05 s floor:

| solver | its own presolve | its iterations |
|---|---|---|
| JAOS | 0.739x | 0.820x |
| HiGHS | 0.692x | 0.717x |
| SoPlex | 0.906x | 0.904x |
| Clp | 0.670x | 0.789x |

**JAOS's presolve is worth about what the others' are worth.** The gap to
HiGHS widened anyway, and two instances carry most of it:

| instance | JAOS's presolve | HiGHS's presolve |
|---|---|---|
| `maros-r7` | 1.065x, and 1.000x on iterations | 0.378x, 0.548x on iterations |
| `stocfor3` | 0.965x, 1.017x | 0.198x, 0.342x |

Those two are the worst instances in the whole comparison. So the finding is
not that presolve was a poor investment. **It is that HiGHS presolves the
instances that dominate this set and JAOS removes nothing from them.** Handed
to `TODO.md` as a question about which families fire on which models.

### The rung believes itself for two reasons

First, it reproduces two figures measured a different way. HiGHS's 0.692x
inverts to 1.445x against D81's 1.417x and D83's 1.421x; SoPlex's 0.906x to
1.104x against D83's 1.111x. Those were taken as T2 − T1 with algorithm choice
free; this is T0 → P0 with the dual forced.

Second, the whole summary arithmetic was recomputed by a second
implementation sharing no code with the harness, and it reproduces T0 exactly
— 3.72x, 1.34x, 3.77x, `maros-r7` at 25.7x. D83's table stands as published.

### What was refuted, and one thing that was broken the whole time

1. *Re-running T0.* The obvious move and it is not a rung: JAOS presolves now
   and the T0 competitor files do not. A rung whose two sides differ in two
   features attributes the difference to neither.
2. *Reading the widened HiGHS gap as a regression in JAOS.* JAOS is faster at
   P0 than it was at T0 on every instance where its presolve fires. The ratio
   moved because the other side moved further.
3. **Clp's summary block was silently absent from T1, T2 and T3 from
   2026-08-11 until this entry.** Clp reports zero iterations on `d6cube`,
   `maros-r7` and `woodw` whenever it is free to choose its algorithm.
   Dividing by that count is a fatal error in awk rather than a NaN, so the
   block aborted before printing its first line and the loop moved on to the
   next solver. The records held the data throughout. Nobody noticed for three
   days because no Clp rung figure was ever quoted — a gap that is invisible
   exactly as long as nobody needs it. `run-compare.sh` now keeps such an
   instance in the time row, drops it from the two iteration rows, and prints
   how many it dropped and which. Recomputed, Clp reads 3.79x, 5.88x and 5.92x
   at T1, T2 and T3, which prices its presolve at 1.55x against the 1.49x its
   own T0 → P0 reading gives.

### What is left open

The gate this is aimed at is unchanged and still cannot be closed here: D17
excludes a WSL number, and this host repeats to 6.27% (D93). Every figure
above is a development number and the record says so on every line.

`pilot87` is worth watching. At P0 both SoPlex and Clp publish 301.7106806
where JAOS and HiGHS agree on 301.7103474, so the harness discards their
times. JAOS is on the correct side of a disagreement between two mature
solvers, on the instance whose suboptimality bound `TODO.md` records as not
understood.

## D105 — What HiGHS finds in `maros-r7` is the implied free column singleton, and it needs an implied bound rather than a tightened one

D104 left this open and said so: HiGHS removes 31% of `maros-r7`'s rows where
JAOS removes nothing, `truss` has the same coarse shape and neither solver
reduces it, and none of the eight families JAOS has scoped could account for
it. Identified now, and counted. Readings in `bench/measurements/02-10/`.

### The technique, and the count that identifies it

**Implied free column singleton substitution.** A column with exactly one
matrix entry, whose row implies a box on it that already lies inside its own
box. Its own bounds can then never bind, so it can be eliminated exactly.

Row `i` implies, for a column `j` appearing only there:

```
a_ij * x_j  in  [ rl_i - maxact_-j ,  ru_i - minact_-j ]
```

divided by `a_ij`, ends swapped when `a_ij < 0`, where `minact_-j` and
`maxact_-j` are the row's activity range excluding `j`'s own term. Fire when
that box is contained in `[l_j, u_j]`; an infinite own bound satisfies its
side for free.

`bench/measurements/02-10/implied_free.c` counts it. The prediction was stated
before the counter was written and it came out exactly:

| instance | rows this rule hits | what HiGHS removes |
|---|---|---|
| `maros-r7` | **984** | **984 rows** |
| `truss` | **0** | nothing, "Not reduced" |

984 against 984 is what closes the question. Every one of the 984 carries a
nonzero cost.

### Why JAOS reads zero, and it is not a defect

`src/presolve.c` fires its singleton-column family only at `col_cost[j] == 0`.
That restriction is D95's and its reasoning is sound: a nonzero-cost singleton
needs to know which bound the cost points at. All 984 of `maros-r7`'s
singletons have cost exactly 1, so the family correctly declines all of them
and the counter correctly reads zero. The instance is outside what JAOS has
built, not mishandled by it.

### What separates `maros-r7` from `truss`, which no structural count could

`truss` has the same singleton pairs: `X10005` and `X10006` are `+1` and `-1`
singletons in one row, both at cost 10. Same shape, same signs, same nonzero
cost. Every structural count taken in D104 reads identically on the two.

The difference is the sign pattern of the **other** coefficients. `maros-r7`
is an L1 image restoration written as a goal program: a non-negative blur
kernel, every column `x >= 0` by MPS default, so a row's minimum activity is
exactly 0 — finite, and something the predicate can compare against. `truss`
rows carry `-0.8944` and `+0.4472` together over unbounded-above columns, so
the minimum activity is `-inf` and nothing is implied free.

**The separating quantity is the row's finite minimum activity.** No count of
degrees, bound types, row senses, doubletons, duplicates or dominated columns
can see it, which is exactly why D104's counts failed to separate the two.

### What it is worth on these sets

`implied_free.c` over both feasible sets, on the model as loaded:

| set | hits | distinct rows | nonzeros in those rows | cost 0 | cost != 0 | instances |
|---|---|---|---|---|---|---|
| netlib | 3321 | **3315** | 87621 | 557 | **2764** | 56 of 94 |
| Kennington | **0** | 0 | 0 | 0 | 0 | 0 of 16 |

The 2764 is the part outside what JAOS could reach today. Kennington reads
zero throughout, so this family is worth nothing there and the 29.36% figure
D104 recorded for doubletons on that set remains its own separate question.

`stocfor3` reads 1025 rows, and it is the other instance where HiGHS beats
JAOS by a factor the comparison notices.

### It lands beside D97, not behind it

This is the part worth being careful about, because "implied bound" and
"tightened bound" read alike.

D97 refused **publishing a narrowed box**. Its reason, written at
`src/presolve.c`, is that removing a row because its range sits inside its
bounds is a statement about the tightened box, so the simplex has to be given
that box, and every one of the six designs that did so returned INFEASIBLE on
a model with an optimum.

Implied free substitution narrows nothing and publishes nothing. It uses the
implied bound **only as a predicate** — is my own bound already redundant? —
and then performs an exact algebraic elimination. The row it reasoned over is
deleted, so no surviving row's redundancy depends on a derived box. D97's
second stated obstacle, a dual postsolve for an imposed bound, does not arise
either: the eliminated column was free, so its reduced cost must be zero and
the multiplier follows by division, `y_i = c_j / a_ij`. On `maros-r7` that is
`-1`, and the paired `v_i` column then reads `d = 1 - (+1)(-1) = 2`, dual
feasible.

**The direction of a mistake reverses, and gets worse.** D97 over-tightened
and refused feasible models — loud, and caught by the first campaign. A false
positive here **drops a bound that was real**, which relaxes the model and
returns an objective that is too good. That is a wrong answer, and no digest
comparison against a presolve-off build would fail to catch it, but nothing
about it announces itself. The margin has to go the conservative way:
`ilo >= l_j + margin` and `iup <= u_j - margin`, so a borderline case is
declined rather than taken.

**The margin's canary is already in the instance.** Four of the 984 rows have
`b_i` exactly 0, so `ilo` equals `l_j` exactly. Any margin strictly above zero
drops them and the count reads 980 instead of 984. That is a sweep with a
built-in flip, which is what `docs/tolerances.md` asks of every constant.

### What was refuted

1. *That the explanation would be structural.* It is not, and D104's counts
   could not have found it however many were added. The separator is a
   computed activity range, not a shape.
2. *That `grow22` might be the same mechanism.* It is not. `grow7`, `grow15`
   and `grow22` read **0** on this counter, so their 20 singleton columns are
   genuinely not implied free and their relaxation is necessary. Section 1b of
   `TODO.md` is a separate problem and substituting instead of relaxing would
   not touch it. Checked before it was claimed.
3. *Dependent-row removal*, the other obvious candidate for a 31% row cut.
   SuiteSparse reports both `lp_maros_r7` and `lp_truss` at full structural
   and numerical row rank, 3136 of 3136 and 1000 of 1000, so no row of either
   is a combination of others.
4. *Sparsify*, which removes nonzeros by row combination and never removes a
   row at all. It cannot produce a 31% row reduction whatever else it does.

### What is not explained, and is not claimed

HiGHS removes **2803 columns** and **64654 nonzeros** from `maros-r7`. The 984
substitutions remove 984 columns, and each paired `v_i` then becomes an empty
column its own family fixes — 1968 accounted for, **835 not**. The counter
reads 44362 nonzeros in the 984 rows, against 64654. So the row count is
identified exactly and the column and nonzero counts are not.

The hypothesis for the remainder, recorded as a hypothesis: the `+1/-1`
singleton pair at equal cost bounds that row's own dual to `[-1, 1]` with no
primal reasoning at all, and a dual box admits dominated-column fixing. That
is a different family and it needs its own counter before anyone believes it.

### The sources, with what was actually reached

The record carries the verification state because a citation nobody opened is
not a citation.

- **Gondzio, J.**, "Presolve Analysis of Linear Programs Prior to Applying an
  Interior Point Method", *INFORMS J. Computing* 9(1), 1997, 73–91, DOI
  `10.1287/ijoc.9.1.73`. **Record and abstract reached; full text paywalled.**
  The abstract names the technique in words — "or relax them to find implied
  free variables" — which makes it the most defensible of the three without
  the paywall.
- **Andersen, E. D. & Andersen, K. D.**, "Presolving in linear programming",
  *Math. Programming* 71, 1995, 221–245, DOI `10.1007/BF01586000`. **Record
  and abstract reached; full text paywalled.** The family name is attributed
  from consistent field usage and **not** from its text. Before this entry is
  cited as the source of the condition, someone should reach the text.
- **Brearley, Mitra, Williams**, *Math. Programming* 8, 1975, 54–83, DOI
  `10.1007/BF01580428`. **Record and abstract reached.** Origin of the
  activity-bound arithmetic and of "freeing" variables.
- **Galabova, I. L.**, "Presolve, crash and software engineering for HiGHS",
  PhD thesis, Edinburgh 2023, DOI `10.7488/era/2974`. **Record reached; the
  PDF could not be read on this machine** (no PDF text extraction here, a
  standing limitation). It is HiGHS's presolve described by its author and
  open access, and it is the document most likely to confirm both the rule and
  its postsolve. Worth reading on a machine that can.

No solver source was read; D12 stands.

### What is left open, handed to `TODO.md`

The reduction itself is not built and this entry does not schedule it. Two
things have to exist first that do not: presolve must be able to modify
`col_cost`, which it never has — `src/presolve.c` copies costs through
unchanged — and the substitution makes the objective denser, turning cost-0
columns into cost-carrying ones, which changes what the existing cost-0
families see. Order of application becomes a question that does not exist
today.
