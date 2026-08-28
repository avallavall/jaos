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
- **[D106](#d106--the-implied-free-column-singleton-buys-64x-on-maros-r7-and-the-row-activity-it-reads-was-short-in-two-older-families)** — The implied free column singleton buys 64x on `maros-r7`, and the row activity it reads was short in two older families
- **[D107](#d107--the-inequality-half-of-the-implied-free-count-is-a-tenth-not-two-thirds-and-building-it-is-refused-on-the-count)** — The inequality half of the implied-free count is a tenth, not two thirds, and building it is refused on the count
- **[D108](#d108--greenbeb-pays-d106s-overcost-in-iterations-and-scfxm3-in-the-ratio-test-and-no-refuse-rule-is-built-on-an-exact-reductions-trajectory)** — greenbeb pays D106's overcost in iterations and scfxm3 in the ratio test, and no refuse rule is built on an exact reduction's trajectory
- **[D109](#d109--the-implied-free-windows-floor-declines-nothing-the-set-can-measure-and-the-margin-ships-exactly-as-it-is)** — The implied-free window's floor declines nothing the set can measure, and the margin ships exactly as it is
- **[D110](#d110--maros-r7s-cheaper-iteration-is-the-factor-fill-collapsing-and-the-instrument-reproduced-d46s-figure-before-being-believed)** — maros-r7's cheaper iteration is the factor fill collapsing, and the instrument reproduced D46's figure before being believed
- **[D111](#d111--the-postsolve-recovery-is-compensated-nine-digests-move-where-rounding-lived-and-1c-closes)** — The postsolve recovery is compensated, nine digests move where rounding lived, and §1c closes
- **[D112](#d112--the-widening-rule-cannot-tell-grow15-from-grow22-and-2s-refusal-closes-on-its-own-counter)** — The widening rule cannot tell grow15 from grow22, and §2's refusal closes on its own counter
- **[D113](#d113--stocfor3s-presolve-gap-is-the-aggregator-and-the-prize-lands-behind-d97-again)** — stocfor3's presolve gap is the aggregator, and the prize lands behind D97 again
- **[D114](#d114--d97s-over-tightening-is-derived-a-window-scaled-by-the-activity-certified-586-of-slack-as-zero)** — D97's over-tightening is derived: a window scaled by the activity certified 5.86 of slack as zero
- **[D115](#d115--the-fourth-set-exists-and-small-models-understate-the-iteration-exponent-by-16x-while-the-work-unit-holds-to-6)** — The fourth set exists, and small models understate the iteration exponent by 1.6x while the work unit holds to 6%
- **[D116](#d116--directed-rounding-on-the-activity-readings-is-refused-because-the-forcing-test-detects-an-equality)** — Directed rounding on the activity readings is refused, because the forcing test detects an equality
- **[D117](#d117--d106-fires-on-none-of-fomes-candidates-because-d95-takes-every-one-of-them-first-and-freezes-121-of-the-rows)** — D106 fires on none of fome's candidates because D95 takes every one of them first, and freezes 12.1% of the rows
- **[D118](#d118--giving-the-implied-free-family-first-refusal-is-refused-pilotnov-publishes-a-suboptimal-point-as-optimal)** — Giving the implied free family first refusal is refused: pilotnov publishes a suboptimal point as optimal
- **[D119](#d119--pilotnovs-wrong-answer-is-the-refactorization-interval-and-the-termination-test-never-re-reads-dual-feasibility)** — pilotnov's wrong answer is the refactorization interval, and the termination test never re-reads dual feasibility
- **[D120](#d120--the-same-reduced-lp-solved-twice-both-points-dual-feasible-by-the-solvers-own-reading-and-29-apart)** — The same reduced LP solved twice: both points dual-feasible by the solver's own reading, and 29% apart
- **[D121](#d121--the-shift-round-trip-is-not-bit-exact-and-on-pilotnov-it-destroys-67-costs-one-by-5511)** — The shift round trip is not bit-exact, and on pilotnov it destroys 67 costs, one by 55.11
- **[D122](#d122--a-repayment-restores-the-cost-instead-of-subtracting-the-loan-and-costs-10001x)** — A repayment restores the cost instead of subtracting the loan, and costs 1.0001x
- **[D123](#d123--no-loan-is-outstanding-when-the-duals-are-published-on-any-of-the-128-instances-an-assert-enabled-build-can-run)** — No loan is outstanding when the duals are published, on any of the 128 instances an assert-enabled build can run
- **[D124](#d124--the-186-loans-were-never-lost-02-29-compared-one-accumulator-against-a-sum-of-partial-sums)** — The 186 loans were never lost: 02-29 compared one accumulator against a sum of partial sums
- **[D125](#d125--no-loan-swamps-a-real-cost-anywhere-in-the-gate-and-one-lend-in-six-sets-d-to-zero-on-a-cost-that-never-moved)** — No loan swamps a real cost anywhere in the gate, and one lend in six sets `d` to zero on a cost that never moved
- **[D126](#d126--refused-the-zero-d125-called-a-fabrication-is-what-stops-the-breach-compounding-and-removing-it-costs-six-orders-of-magnitude)** — **Refused**: the zero D125 called a fabrication is what stops the breach compounding, and removing it costs six orders of magnitude
- **[D127](#d127--refused-the-wrong-signed-dual-step-is-holding-pilot87-up-and-clamping-it-costs-3228x)** — **Refused**: the wrong-signed dual step is holding pilot87 up, and clamping it costs 3.228x
- **[D128](#d128--the-shift-record-says-what-the-cost-moved-by-and-it-skips-two-re-pricings-out-of-290)** — The shift record says what the cost moved by, and it skips two re-pricings out of 290
- **[D129](#d129--the-basis-count-defect-costs-25-of-netlib-and-55-of-kennington-their-warm-start-outright)** — The basis-count defect costs 25% of netlib and 55% of Kennington their warm start outright
- **[D130](#d130--six-instances-publish-more-basic-variables-than-rows-not-three-and-the-three-with-no-basis-at-all-are-named)** — Six instances publish more basic variables than rows, not three, and the three with no basis at all are named
- **[D131](#d131--jaos_basis-publishes-something-that-is-not-a-basis-on-70-of-solves-and-presolves-mapping-is-exact)** — `jaos_basis` publishes something that is not a basis on 70% of solves, and presolve's mapping is exact
- **[D132](#d132--singleton_row-restores-half-of-netlibs-removed-rows-and-leaves-78-of-them-nonbasic)** — `SINGLETON_ROW` restores half of netlib's removed rows and leaves 78% of them nonbasic
- **[D133](#d133--the-two-singleton-families-are-the-whole-of-the-basis-count-error-and-the-sum-closes-exactly)** — The two singleton families are the whole of the basis-count error, and the sum closes exactly
- **[D134](#d134--the-pair-sums-to-one-only-by-accident-and-the-repair-is-a-swap-rather-than-a-status)** — The pair sums to one only by accident, and the repair is a swap rather than a status
- **[D135](#d135--the-exchange-the-reduction-suggests-is-available-and-valid-on-97-of-firings)** — The exchange the reduction suggests is available and valid on 97% of firings
- **[D136](#d136--the-singleton-rows-rule-falls-out-of-its-own-dual-and-the-defect-is-a-status-decided-on-a-partial-activity)** — The singleton row's rule falls out of its own dual, and the defect is a status decided on a partial activity
- **[D137](#d137--the-counting-rule-is-published-and-highs-turned-off-the-family-that-costs-us-5902)** — The counting rule is published, and HiGHS turned off the family that costs us 5902
- **[D138](#d138--every-under-count-is-gone-kenningtons-worst-error-falls-100x-and-the-sum-was-the-wrong-target)** — Every under-count is gone, Kennington's worst error falls 100x, and the sum was the wrong target
- **[D139](#d139--kennington-publishes-a-valid-basis-on-every-solve-and-netlibs-worst-error-falls-from-596-to-23)** — Kennington publishes a valid basis on every solve, and netlib's worst error falls from 596 to 23
- **[D140](#d140--the-80-are-an-exact-degenerate-tie-the-recovery-division-rounds-off-and-the-swaps-guard-reads-a-status-another-family-rewrites)** — The 80 are an exact degenerate tie the recovery division rounds off, and the swap's guard reads a status another family rewrites
- **[D141](#d141--a-within-row-demotion-cannot-pay-for-the-residue-152-of-the-232-declines-have-no-basic-column-at-a-bound)** — A within-row demotion cannot pay for the residue: 152 of the 232 declines have no basic column at a bound
- **[D142](#d142--the-remember-basis-count-guard-is-refused-the-non-basis-it-would-clear-is-the-warm-starts-raw-material)** — The remember-basis count guard is refused: the non-basis it would clear is the warm start's raw material
- **[D143](#d143--d138d139s-correct-basis-maps-short-and-netlibs-warm-ratio-paid-37x-the-mapping-owes-the-reverse-of-the-swap)** — D138/D139's correct basis maps short and netlib's warm ratio paid 3.7x: the mapping owes the reverse of the swap
- **[D144](#d144--the-mappings-balance-is-multi-family-so-the-count-is-repaired-at-the-consumer-not-in-the-mapping)** — The mapping's balance is multi-family, so the count is repaired at the consumer, not in the mapping
- **[D145](#d145--the-count-repaired-warm-start-publishes-wrong-optima-through-the-termination-hole-and-the-warm-prize-waits-behind-it)** — The count-repaired warm start publishes wrong optima through the termination hole, and the warm prize waits behind it
- **[D146](#d146--a-hostile-basis-makes-head-publish-a-wrong-optimum-through-the-public-api-alone)** — A hostile basis makes HEAD publish a wrong optimum through the public API alone
- **[D147](#d147--the-solver-measures-the-violation-it-publishes-the-best-point-ends-with-bstdv--3534-and-nothing-reads-it-against-a-tolerance)** — The solver measures the violation it publishes: the best point ends with bstdv = 35.34 and nothing reads it against a tolerance
- **[D148](#d148--the-certificate-guard-lands-0-wrong-of-80-where-head-published-26-and-the-gate-is-bit-identical)** — The certificate guard lands: 0 wrong of 80 where HEAD published 26, and the gate is bit-identical
- **[D149](#d149--the-retried-warm-repair-is-correct-now-and-refused-on-cost-dfl001-pays-172x-for-a-doomed-attempt-the-guard-then-throws-away)** — The retried warm repair is correct now and refused on cost: dfl001 pays 172x for a doomed attempt the guard then throws away
- **[D150](#d150--the-gate-sees-the-basis-every-optimal-line-carries-its-hash-det-covers-it-and-all-139-instances-hold)** — The gate sees the basis: every optimal line carries its hash, det covers it, and all 139 instances hold
- **[D151](#d151--the-warm-repair-lands-behind-a-shortfall-cap-of-4-chosen-at-the-end-of-a-plateau-because-the-mean-is-flat-there-and-the-worst-case-is-not)** — The warm repair lands behind a shortfall cap of 4, chosen at the end of a plateau because the mean is flat there and the worst case is not
- **[D152](#d152--the-replay-clamps-into-the-columns-own-box-and-the-assert-that-could-not-be-enabled-is-removed-rather-than-widened)** — The replay clamps into the column's own box, and the assert that could not be enabled is removed rather than widened
- **[D153](#d153--the-row-activity-check-becomes-an-invariant-and-four-wrong-versions-of-it-each-reported-a-defect-that-was-not-there)** — The row-activity check becomes an invariant, and four wrong versions of it each reported a defect that was not there
- **[D154](#d154--three-of-the-five-build-configurations-did-not-build-and-make-configs-is-what-will-say-so-next-time)** — Three of the five build configurations did not build, and `make configs` is what will say so next time
- **[D155](#d155--row_traffic-accumulates-only-what-a-still-finite-end-absorbed-and-the-assert-that-says-so-rests-on-measured-headroom-rather-than-on-the-argument-that-looked-available)** — `row_traffic` accumulates only what a still-finite end absorbed, and the assert that says so rests on measured headroom rather than on the argument that looked available
- **[D156](#d156--the-destroyed-row-width-is-refused-as-a-defect-because-the-width-that-dies-was-already-below-one-ulp-of-the-activity-it-constrains)** — The destroyed row width is refused as a defect, because the width that dies was already below one ulp of the activity it constrains
- **[D157](#d157--the-two-silent-fallbacks-become-checked-claims-and-the-check-that-catches-them-is-the-sweep-rather-than-either-read)** — The two silent fallbacks become checked claims, and the check that catches them is the sweep rather than either read
- **[D158](#d158--the-collapsed-folds-midpoint-is-clamped-into-the-columns-box-which-bounds-the-last-unbounded-item-and-the-branch-runs-0-times-in-100018-folds)** — The collapsed fold's midpoint is clamped into the column's box, which bounds the last unbounded item, and the branch runs 0 times in 100018 folds
- **[D159](#d159--the-frozen-row-window-is-scaled-by-what-the-comparison-is-made-of-and-presolve-stops-refusing-a-model-the-solver-can-solve)** — The frozen-row window is scaled by what the comparison is made of, and presolve stops refusing a model the solver can solve
- **[D160](#d160--clause-1-of-the-activity-pass-gets-its-own-window-and-the-bound-scale-that-looked-like-symmetry-published-a-wrong-answer)** — Clause 1 of the activity pass gets its own window, and the bound scale that looked like symmetry published a wrong answer
- **[D161](#d161--the-frozen-row-window-drops-the-far-bound-too-and-that-defect-predates-d159-which-widened-around-it)** — The frozen-row window drops the far bound too, and that defect predates D159, which widened around it
- **[D162](#d162--a-row-bound-is-a-running-difference-so-the-window-counts-the-terms-and-the-end-it-is-testing-comes-back-in-multiplied-by-that-count)** — A row bound is a running difference, so the window counts the terms — and the end it is testing comes back in, multiplied by that count
- **[D163](#d163--the-singleton-rows-fold-is-a-fourth-read-of-that-running-difference-and-a-count-cannot-cover-an-error-that-arrives-inside-a-value)** — The singleton row's fold is a fourth read of that running difference, and a count cannot cover an error that arrives inside a value
- **[D164](#d164--carrying-that-error-into-the-window-is-refused-because-it-publishes-a-point-violating-two-rows-by-75-times-check_tol)** — Carrying that error into the window is refused, because it publishes a point violating two rows by 7.5 times CHECK_TOL
- **[D165](#d165--the-row-bounds-keep-their-residue-which-removes-the-error-four-windows-were-widened-to-cover-and-moves-fourteen-digests)** — The row bounds keep their residue, which removes the error four windows were widened to cover — and moves fourteen digests
- **[D166](#d166--the-shift-counts-come-out-and-the-tests-they-were-built-for-passing-without-them-is-the-evidence)** — The shift counts come out, and the tests they were built for passing without them is the evidence
- **[D167](#d167--the-published-basis-count-had-been-stale-for-a-day-and-nothing-in-the-gate-could-have-said-so)** — The published-basis count had been stale for a day, and nothing in the gate could have said so
- **[D168](#d168--the-simplex-accumulates-its-right-hand-side-with-compensation-and-the-reference-build-stops-calling-a-feasible-model-infeasible)** — The simplex accumulates its right-hand side with compensation, and the reference build stops calling a feasible model infeasible
- **[D169](#d169--the-published-objective-is-summed-with-compensation-over-the-point-it-is-published-with-and-81-of-94-now-agree-with-the-checker-exactly)** — The published objective is summed with compensation over the point it is published with, and 81 of 94 now agree with the checker exactly
- **[D170](#d170--the-published-reduced-costs-contradict-the-published-basis-on-five-netlib-instances-and-every-one-of-them-is-2-rather-than-a-new-defect)** — The published reduced costs contradict the published basis on five netlib instances, and every one of them is §2 rather than a new defect
- **[D171](#d171--the-refinement-residual-is-compensated-too-and-the-argument-that-refused-it-was-sound-about-the-terms-and-wrong-about-the-consequence)** — The refinement residual is compensated too, and the argument that refused it was sound about the terms and wrong about the consequence
- **[D172](#d172--the-published-objective-recovers-what-each-product-lost-and-109-of-110-now-agree-with-the-checker-exactly)** — The published objective recovers what each product lost, and 109 of 110 now agree with the checker exactly
- **[D173](#d173--the-published-objective-is-the-correctly-rounded-exact-one-on-all-110-so-finnis-is-refused-and-pilot-is-the-instance-with-a-wrong-point)** — The published objective is the correctly rounded exact one on all 110, so `finnis` is refused and `pilot` is the instance with a wrong point
- **[D174](#d174--pilots-wrong-answer-is-dual_tol-and-nothing-else-and-the-repair-that-fixes-it-turns-the-gate-red-on-six-instances)** — `pilot`'s wrong answer is `DUAL_TOL` and nothing else, and the repair that fixes it turns the gate red on six instances
- **[D175](#d175--the-sum-that-ranks-two-rounds-decides-which-point-is-published-so-it-is-compensated-too--and-no-solve-on-the-three-sets-can-reach-the-case)** — The sum that ranks two rounds decides which point is published, so it is compensated too — and no solve on the three sets can reach the case
- **[D176](#d176--refused-presolves-objective-offset-is-compensated-for-nothing-because-poisoning-it-with-nan-moves-not-one-byte-on-any-of-the-139)** — REFUSED: presolve's objective offset is compensated for nothing, because poisoning it with NaN moves not one byte on any of the 139
- **[D177](#d177--the-gates-suboptimality-predicate-watched-4-solves-of-110-and-the-floor-that-excluded-the-other-106-is-refuted-by-d171s-own-numbers)** — The gate's suboptimality predicate watched 4 solves of 110, and the floor that excluded the other 106 is refuted by D171's own numbers
- **[D178](#d178--refused-scsd1-and-degen2-do-not-lose-the-same-way-so-3-asks-for-a-predictor-of-something-that-happens-once-in-twenty)** — REFUSED: `scsd1` and `degen2` do not lose the same way, so §3 asks for a predictor of something that happens once in twenty
- **[D179](#d179--a-rule-wider-than-the-firing-row-has-a-supply-on-19-of-24-instances-and-none-at-all-on-two-so-it-improves-the-residue-and-cannot-close-it)** — A rule wider than the firing row has a supply on 19 of 24 instances and none at all on two, so it improves the residue and cannot close it
- **[D180](#d180--refused-the-refactorization-interval-stays-at-64-although-32-is-86-cheaper-and-the-sweep-that-says-so-also-reaches-pilots-optimum-without-touching-a-tolerance)** — REFUSED: the refactorization interval stays at 64 although 32 is 8.6% cheaper, and the sweep that says so also reaches `pilot`'s optimum without touching a tolerance
- **[D181](#d181--the-fourth-set-does-not-reopen-3--the-mapped-basis-arrives-short-by-56-of-rows-and-the-repair-never-runs--and-it-prices-2-at-three-of-four-warm-starts)** — The fourth set does not reopen §3 — the mapped basis arrives short by 5.6% of rows and the repair never runs — and it prices §2 at three of four warm starts
- **[D182](#d182--plato-nug-solves-one-of-three-and-presolve-reaches-a-median-of-zero-rows-on-every-plato-set-against-nine-per-cent-on-netlib)** — `plato-nug` solves one of three, and presolve reaches a median of zero rows on every plato set against nine per cent on netlib
- **[D183](#d183--pilot87s-suboptimality-bound-moves-because-its-dual-solution-is-not-unique-and-its-priced-primal-answer-does-not-move-at-all)** — `pilot87`'s suboptimality bound moves because its dual solution is not unique, and its priced primal answer does not move at all
- **[D184](#d184--dual_tol-is-1e-9-and-all-four-wrong-points-are-gone-at-10339x-work-on-netlib-and-10976x-on-kennington-which-d174-had-not-measured)** — `DUAL_TOL` is 1e-9 and all four wrong points are gone, at 1.0339x work on netlib and 1.0976x on Kennington, which D174 had not measured
- **[D185](#d185--the-gate-has-an-absolute-bar-on-suboptimality-at-1e-6-placed-on-123-solves-across-five-sets-and-it-rejects-the-answer-this-project-shipped-two-days-ago)** — The gate has an absolute bar on suboptimality at 1e-6, placed on 123 solves across five sets, and it rejects the answer this project shipped two days ago
- **[D186](#d186--refused-no-mapped-basis-arrives-long-in-101-calls-so-a-demotion-rule-in-build_warm_basis-has-nothing-to-act-on-and-35-of-90-netlib-warm-starts-fall-back-to-cold)** — REFUSED: no mapped basis arrives long in 101 calls, so a demotion rule in `build_warm_basis` has nothing to act on and 35 of 90 netlib warm starts fall back to cold
- **[D187](#d187--the-primal-clean-up-priced-its-row-the-expensive-way-and-the-saving-is-32-on-wood1p-against-10000017x-lost-on-pilot87)** — The primal clean-up priced its row the expensive way, and the saving is 3.2% on `wood1p` against 1.0000017x lost on `pilot87`
- **[D188](#d188--the-primal-simplexs-first-version-and-both-defects-it-shipped-with-were-invisible-to-a-green-suite)** — The primal simplex's first version, and both defects it shipped with were invisible to a green suite
- **[D189](#d189--the-primal-published-a-value-outside-a-declared-bound-as-optimal-and-stage-1s-own-pricing-rule-is-what-made-it-reachable)** — The primal published a value outside a declared bound as OPTIMAL, and stage 1's own pricing rule is what made it reachable
- **[D190](#d190--the-primal-phase-1-lands-and-takes-the-reach-from-0-of-94-to-64-and-a-loan-of-exactly-10-is-what-found-the-defect-it-shipped-with)** — The primal phase 1 lands and takes the reach from 0 of 94 to 64, and a loan of exactly 1.0 is what found the defect it shipped with
- **[D191](#d191--the-primals-64-of-94-was-54-and-the-difference-was-a-guard-that-was-documented-believed-and-never-applied)** — The primal's "64 of 94" was 54, and the difference was a guard that was documented, believed, and never applied
- **[D192](#d192--blands-rule-now-reaches-the-primals-leaving-variable-and-it-arms-nowhere-on-netlib)** — Bland's rule now reaches the primal's leaving variable, and it arms nowhere on netlib
- **[D193](#d193--refresh-is-the-third-place-a-cost-is-lent-it-fires-30-times-inside-the-primal-phase-1-and-guarding-it-trades-pilot4-for-pilot-ja-and-pilotnov)** — `refresh` is the third place a cost is lent, it fires 30 times inside the primal phase 1, and guarding it trades `pilot4` for `pilot-ja` and `pilotnov`
- **[D194](#d194--605-of-the-primal-campaigns-iterations-are-the-duals-the-primals-phase-2-runs-exactly-one-iteration-on-80-of-94-and-pilot4-is-not-a-primal-regression)** — 60.5% of the primal campaign's iterations are the dual's, the primal's phase 2 runs exactly one iteration on 80 of 94, and `pilot4` is not a primal regression
- **[D195](#d195--the-bound-flips-1e10-delta-fires-on-nothing-and-chasing-it-found-that-d194-counted-phase-1-from-a-log-line-printed-only-on-success)** — The bound flip's 1e10 delta fires on nothing, and chasing it found that D194 counted phase 1 from a log line printed only on success
- **[D196](#d196--the-iteration-cap-really-is-shared-across-both-primal-phases-and-the-dual-and-phase-1-spends-at-most-168-of-it-so-only-the-guards-message-needed-fixing)** — The iteration cap really is shared across both primal phases and the dual, and phase 1 spends at most 1.68% of it, so only the guard's message needed fixing
- **[D197](#d197--the-primal-campaign-reports-which-method-did-the-work-and-two-instruments-now-agree-that-phase-2-is-00-of-it)** — The primal campaign reports which method did the work, and two instruments now agree that phase 2 is 0.0% of it
- **[D198](#d198--phase-1-was-under-billed-by-nvar-on-every-iteration-and-charging-it-honestly-costs-two-instances-and-exposes-an-onvar-clear-that-should-be-onrow)** — Phase 1 was under-billed by `nvar` on every iteration, and charging it honestly costs two instances and exposes an `O(nvar)` clear that should be `O(nrow)`
- **[D199](#d199--the-phase-1-clear-stops-sweeping-every-variable-to-undo-the-basis-and-the-campaign-returns-to-its-pre-d198-verdicts-at-042-more-work)** — The phase-1 clear stops sweeping every variable to undo the basis, and the campaign returns to its pre-D198 verdicts at 0.42% more work
- **[D200](#d200--two-of-phase-1s-four-refusals-now-refresh-before-refusing-neither-is-reached-on-the-standard-set-and-phase-1-can-finally-be-watched-and-stopped)** — Two of phase 1's four refusals now refresh before refusing, neither is reached on the standard set, and phase 1 can finally be watched and stopped
- **[D201](#d201--the-hand-overs-zero-margin-is-55000x-in-practice-and-s-cols-contract-stops-being-a-comment-with-five-other-writers)** — The hand-over's zero margin is 55000x in practice, and `s->col`'s contract stops being a comment with five other writers
- **[D202](#d202--an-abandoned-solve-published-the-previous-solves-iteration-total-and-the-column-added-to-expose-the-split-is-what-exposed-it)** — An abandoned solve published the previous solve's iteration total, and the column added to expose the split is what exposed it
- **[D203](#d203--d199s-scatter-clear-buys-no-seconds-costs-none-and-needs-no-density-fallback)** — D199's scatter clear buys no seconds, costs none, and needs no density fallback
- **[D204](#d204--phase-1-is-395-of-the-campaign-is-a-statement-about-two-instances-and-the-median-instance-is-573)** — Phase 1 is 39.5% of the campaign is a statement about two instances, and the median instance is 57.3%
- **[D205](#d205--the-most-common-primal-failure-published-a-verdict-with-no-sentence-on-31-of-94)** — The most common primal failure published a verdict with no sentence, on 31 of 94
- **[D206](#d206--a-refusal-had-expired-unnoticed-the-record-was-checked-by-nothing-and-the-instrument-that-could-not-see-05-now-can)** — A refusal had expired unnoticed, the record was checked by nothing, and the instrument that could not see 0.5% now can
- **[D207](#d207--the-primal-ratio-tests-pivot-floor-was-absolute-and-one-ulp-of-the-columns-own-largest-entry-is-the-value)** — The primal ratio tests' pivot floor was absolute, and one ulp of the column's own largest entry is the value
- **[D208](#d208--the-pivot-floor-does-not-weaken-blands-rule-and-the-reason-pilot87-stalls-is-that-its-phase-1-has-already-diverged)** — The pivot floor does not weaken Bland's rule, and the reason `pilot87` stalls is that its phase 1 has already diverged
- **[D209](#d209--pivotmin-on-the-pricing-row-is-a-stability-floor-not-a-noise-floor-and-the-noise-floor-it-was-mistaken-for-was-missing)** — `PIVOT_MIN` on the pricing row is a stability floor, not a noise floor, and the noise floor it was mistaken for was missing
- **[D210](#d210--the-last-absolute-pivot-floor-stays-absolute-it-decides-nothing-on-139-instances-and-the-only-way-to-move-it-is-the-unsafe-one)** — The last absolute pivot floor stays absolute: it decides nothing on 139 instances, and the only way to move it is the unsafe one
- **[D211](#d211--pilot87s-phase-1-diverges-because-the-primal-ratio-test-has-no-rule-against-a-tiny-pivot-and-a-stop-on-the-objective-rising-is-refused-because-a-solve-that-finishes-rises-25x)** — `pilot87`'s phase 1 diverges because the primal ratio test has no rule against a tiny pivot, and a stop on the objective rising is refused because a solve that finishes rises 25x
- **[D212](#d212--harriss-two-pass-ratio-test-in-primal-form-60-of-94-agree-against-56-wood1p-publishes-a-different-vertex-for-22-less-work-and-pilot87-is-untouched)** — Harris's two-pass ratio test in primal form: 60 of 94 agree against 56, `wood1p` publishes a different vertex for 22% less work, and `pilot87` is untouched
- **[D213](#d213--the-harris-width-is-half-primaltol-and-the-measurement-did-not-choose-it-one-flat-plateau-on-the-campaign-and-a-gate-that-does-not-move-at-all)** — The Harris width is half `primal_tol`, and the measurement did not choose it: one flat plateau on the campaign and a gate that does not move at all
- **[D214](#d214--canmoves-units-are-a-rate-read-in-both-spaces-and-d27s-cautionary-pds-20-costs-a-fifth-of-the-work)** — `can_move`'s units are a rate read in both spaces, and D27's cautionary `pds-20` costs a fifth of the work
- **[D215](#d215--d211s-refusal-expired-at-d212-and-the-script-that-proves-it-measured-the-main-tree-from-inside-three-separate-worktrees)** — D211's refusal expired at D212, and the script that proves it measured the main tree from inside three separate worktrees
- **[D216](#d216--eight-of-lucs-prose-contracts-are-asserts-now-and-a-control-proves-they-catch-the-defect-they-exist-for)** — Eight of `lu.c`'s prose contracts are asserts now, and a control proves they catch the defect they exist for
- **[D217](#d217--every-measurement-script-derives-the-repository-root-it-was-48-scripts-and-not-28-and-the-check-that-proves-it-said-stop-twice-for-the-right-reason)** — Every measurement script derives the repository root, it was 48 scripts and not 28, and the check that proves it said STOP twice for the right reason

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

## D106 — The implied free column singleton buys 64x on `maros-r7`, and the row activity it reads was short in two older families

D105 decided to build it and TODO.md §1 wrote the plan. This is what it cost,
and the defect it uncovered on the way, which was older than it is.

**What was built.** A column with exactly one matrix entry `a_ij`, in an
equality row whose other terms already confine it strictly inside its own
box, is substituted out exactly and the row is removed with it. Eliminating
`j` pushes its cost onto the row's other live columns, `c_k -= (c_j/a_ij) *
a_ik`, which is what `cur_cost[]` was landed for one commit earlier. The
postsolve is forced rather than searched: the column is interior, so `d_j` is
zero, so `y_i = c_j / a_ij` in one division.

Four restrictions, each with its own reason, all in the source beside the
code. The column's ORIGINAL degree is 1, not just its live one, because
`d_j = 0` is an equation over every row the column touches. The row is an
equality, because an inequality leaves `x_j` undetermined and puts a sign
condition on `y_i` that `c_j/a_ij` has no reason to satisfy. The row is not
frozen. And the margin declines borderline cases.

**The margin, and why it is a separate constant.** The other two windows in
this file answer "is this residue rounding?", where being wrong is loud: too
wide and a feasible model comes back INFEASIBLE. This one answers "does the
implied box lie inside the column's own box?", and being wrong is silent — it
drops a bound that was real, relaxes the model, and publishes an objective
that is too good. No digest comparison against `-DJAOS_NO_PRESOLVE` would
look wrong, because the reference build would be the one refusing. So
`PRESOLVE_IMPLIED_FREE_ULPS` is subtracted from the column's own bounds and
the family declines at exact equality.

**What it removes.** 17 of the 94 standard instances, 1041 rows, 2040 columns
and 47043 nonzeros. The counter in `bench/measurements/02-10/` predicted 3315
rows over 56 instances on the model as loaded; the difference is the
equality-row restriction the plan asked for as a starting point, and the
remainder is the next step rather than a shortfall.

On `maros-r7` it removes **980 rows**, which is the number stated in advance:
the counter read 984 candidates, 4 of them sit at exact equality, and any
margin above zero declines those 4. 1960 columns and 44198 nonzeros go with
them.

| set | what moved |
|---|---|
| netlib | 17 instances' `presolve` field, 17 digests, 57 records in some field |
| Kennington | **nothing — 16 of 16 bit-identical** |
| infeasible | 29 of 29 still refused; `gosh` 1.282x work, `pang` 0.822x |

Kennington is the negative control and it earns the name. The counter read 0
candidates there before the family existed, and the set came back
bit-identical afterwards.

**The cost, per instance and as a geometric mean of per-instance ratios
(D46, never a set total).**

```
  GEOMETRIC MEAN       0.9527x
  best  maros-r7       0.0156x    21010708013 -> 328053926 work
                                       10479 -> 2576      iterations
  worst greenbeb       1.5126x      379164967 -> 573519868
  ratio of totals      0.6105x    <- NOT the result
```

Eleven instances improve, six get worse, and 57 are unchanged to the bit. The
geometric mean is almost entirely `maros-r7`: it contributes 0.0443 of the
0.0484 that `-ln(0.9527)` is made of. That is stated rather than hidden,
because a mean that one instance carries is a statement about that instance.

`maros-r7`'s iterations fall 4.07x and its work 64.0x, so the cost of an
iteration falls 15.7x while the model shrinks by 31% of its rows and 31% of
its nonzeros. **That is more than the size accounts for and it is not
explained here.** `maros-r7`'s factors carry 4.801x its basis nonzeros, the
worst ratio in the set (D46), so a hypothesis exists; it has no measurement
and TODO.md carries it.

**What got worse, and it is not proportional to what was removed.**
`greenbeb` loses 3 rows and 10 columns and costs 1.5126x. `scfxm3` loses 3
rows and costs 1.3557x. `forplan` loses 5 rows and costs 1.1648x. A handful
of firings can hurt an instance badly, which is the same shape TODO.md §2
already carries for the cost-0 singleton column on `grow22`. None of the
three crosses the gate's own 2.0x work bar, so **the gate reports
`0 regressed` and that is not the same statement as "nothing got worse"** —
the summary line cannot see any of this and the per-instance diff is where it
came from.

**The defect it uncovered, which was not its own.** The first campaign came
back with three checker failures: `greenbeb`, `modszk1` and `tuff`, reporting
row violations of 900, 1.67e5 and 27.7 against column violations of 1.4e-27,
4.6e-13 and 4.7e-30. A published point inside every column's own box that
misses a row by 900.

`JM_PS_SINGLETON_COL` and `JM_PS_FREE_COL_SINGLETON` write the activity of
their own row and stop. Each fires on a column of LIVE degree one, so every
other row that column touches is already dead, and the share owed to those
rows was never added. `jaos_solution` has been publishing short row
activities for them since the families landed.

Nothing noticed because nothing read them. `bench/run.c` hashes the columns
and the duals, and the checker recomputes every row activity from the columns
it is handed. This family is the first reader: it recovers its own column
from `sol_row[i]` of the row it removed, so every gap in that accumulation
became a published value that is wrong and still inside its own box.

Repaired in `ps_add_to_other_rows`. All three are checker ok again and the
standard set reads `0 regressed, 0 improved, 0 new`.

**Refuted along the way.** The first design set `col_pending_dual` on every
column the removed row left behind, on the precedent of the other
row-removing families. It is not needed and it is not set: a column removed
after this one carries `c_k - (c_j/a_ij) * a_ik` in its record and reads
`sol_dual[i] == 0` at replay, so `rec->cost - sum_l a_lk y_l` lands on the
true `d_k`. The cancellation is bit-exact because the replay divides the same
two numbers the forward pass does. Setting the flag would have cost every
later forcing row on those columns for nothing.

**Also repaired, and unrelated.** `make_frozen_row_infeasible_model` in
`tests/test_presolve.c` had no fault-build guard, so `make test
EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` did not compile at all and no
negative test in that file could run. The plain build and the reference build
are the two the loop actually runs, so nothing announced it.

**The margin's sweep, and it is a switch rather than a dial.** Swept 0, 1, 8,
64, 4096 with `make clean` between settings. Rows removed across the whole
standard set read **9992, 8639, 8639, 8639, 8639**; `maros-r7` reads 984, 980,
980, 980, 980; and solved, objective ok and checker ok are 94 at every one.
One step, at zero, and four decades of nothing above it. The firing is
bimodal: an implied box is either comfortably inside the column's own box or
exactly at its bound, and across 94 instances almost nothing lands in a 1e-12
relative band.

The canary separates 0 from the rest and nothing else, the same gap D103
recorded for `PRESOLVE_ROUND_ULPS`, so the plateau rests on the second check:
five settings, five distinct md5s of `presolve.o`.

**Zero is refused on cost, not on correctness, and that is worth stating
precisely.** At zero the set still reads 94 `objective ok` against Koch's
exact rationals, which is the predicate an objective that is too good would
trip. What refuses it is `d2q06c` at **2.2163x** work, which crosses
`bench/run.c`'s own `WORK_REGRESSION_FACTOR` of 2.0 — against a geometric mean
of 0.9627x and `bore3d` at 0.2524x. So 8 ships, and what is left open is
narrower than the constant: it is whether the window's `max(1, scale)` floor
should exist at all, since that floor is what declines the exact-equality
cases where nothing cancelled and the comparison carries no error to protect
against. `TODO.md` §1b.

**Left open, and handed to TODO.md.** Inequality rows, which is the other
two thirds of the counted opportunity. `greenbeb`, `scfxm3` and `forplan`.
The margin's floor, above. And `maros-r7`'s 15.7x per-iteration drop, which wants a cause. Readings in `bench/measurements/02-12/`.

## D107 — The inequality half of the implied-free count is a tenth, not two thirds, and building it is refused on the count

**The question, as §1a asked it.** Before building the inequality half of the
implied free column singleton, count what a sign-respecting version would
actually reach. The expectation on record was large: 02-10's counter read
3315 rows as loaded against the 1041 the shipped equality family removes,
D106 called the remainder "the next step rather than a shortfall", and §1a's
own heading called inequality rows two thirds of the counted opportunity.

**The measurement, 2026-08-17.** `bench/measurements/02-13/` extends the
02-10 counter with the row-sense split and the dual sign condition:
eliminating the column forces `y_i = c_j / a_ij`, and an inequality row
admits that multiplier only when it points at a finite row end, in the
minimize-canonical convention `src/check.c` judges published duals in. The
instrument refuses to report until it reproduces a hand model that fires
every branch — equality, range row, zero cost, negative coefficient, three
declines — and 02-10's committed values: `maros-r7` 984, `truss` 0, netlib
3321 hits over 3315 distinct rows. Both reproduced exactly.

| set | hits | in equality rows | in inequality rows | sign-ok | declined |
|---|---|---|---|---|---|
| netlib | 3321 | 2980 | 341 | **341** | **0** |
| Kennington | 0 | 0 | 0 | 0 | 0 |

The 341 sign-ok rows carry 14094 nonzeros over 12 instances: `ship12l` and
`ship12s` 77 each, `ship08l` and `ship08s` 50 each, `ship04l` and `ship04s`
25 each — 304 of the 341 on the six `ship*` models, every one below the
comparison harness's 0.05 s floor — then `80bau3b` 14, `bnl2` 9, `pilot87`
9, `scorpion` 3, `25fv47` 1, `finnis` 1. `stocfor3`, the worst instance in
the comparison since the P0 re-take, carries zero. Kennington carries zero.

**What was refuted, one premise per direction.**

The two-thirds premise is wrong. Inequality rows hold 341 of the 3315
as-loaded rows, 10%. Equality rows hold 2980, so the shipped family's 1041
is short of its own half's count by margin and interaction, not by row
sense: §1b already owns 1353 of that gap (the margin's absolute floor), and
the rest is presolve-time interaction. There is no large inequality prize
waiting behind D106.

The sign condition, the part of §1a that looked like the dangerous design
work, filters nothing on this population — and that is derived, not
observed. A one-sided inequality hit is declined only when its forced
multiplier points at the infinite row end; the containment test already
forces the column's own bound on that side to be infinite; a declined
candidate's cost therefore improves along a ray the row never cuts, and the
model was unbounded. All four sense-and-sign cases reduce to this, so on a
feasible bounded model the declined count must be 0, and on all 94 it is.
The decline branch is exercised by the calibration model, so the zero is not
an instrument that cannot fire.

**Refused: the inequality extension is not built at this population.** 341
rows is 0.83% of the standard set's rows, concentrated on instances the
comparison cannot even time, with none on the instance that decides the
comparison and zero on Kennington. D101 deferred three families at 0.15%
with an executable reopen condition; this is the same shape at five times
the share and with a worse concentration. The reopen condition is
executable: a model population where
`bench/measurements/02-13/run-sign-count.sh` reports a non-trivial sign-ok
share. `TODO.md` §4's fourth instance set is the standing candidate.

**Left open, and handed to TODO.md.** If a population reopens this: the 19
zero-cost hits (9 `bnl2`, 9 `pilot87`, 1 `80bau3b`) need a postsolve that
picks a value from the implied interval instead of computing one, and a
declined candidate on a feasible model is an unboundedness witness that D19
says must still be published off a ray, so the family declines and leaves
the certificate to the solve. Nothing else. §1b, §1c, §1d and §1e stay open
and are untouched by this.

## D108 — greenbeb pays D106's overcost in iterations and scfxm3 in the ratio test, and no refuse rule is built on an exact reduction's trajectory

**The question, as §1d asked it.** After D106, `greenbeb` costs 1.5126x,
`scfxm3` 1.3557x and `forplan` 1.1648x in work units, against 3 to 5 rows
removed on each — out of proportion, and the same shape §2 carries for
`grow22`. The missing piece was named in the entry: nothing measured says
which way a firing goes on a model that has not been run, and a refuse rule
cannot be designed without that.

**The measurement, 2026-08-17, in `bench/measurements/02-14/`.** Two
readings. First the committed records on both sides of D106, which cost
nothing:

| instance | iterations | work | work per iteration |
|---|---|---|---|
| `greenbeb` | **1.3779x** | 1.5126x | 1.0978x |
| `scfxm3` | 1.0539x | 1.3557x | **1.2864x** |
| `forplan` | 1.0659x | 1.1648x | 1.0928x |

Then a callgrind instruction-count attribution: one diagnostic build per
tree, identical flags, each binary required to reproduce its own record's
`iters=` and `work=` exactly before being profiled — all six matched, so the
profiled trajectories are the recorded ones. On `greenbeb` every kernel
scales with the iteration count (per-function 1.34–1.57x around a 1.378x
iteration ratio; per iteration nothing beyond `ftran_prefix` 1.14x and
`jm_lu_factor` 1.11x): the overcost is the path. On `scfxm3` the growth is
localized with iterations near flat: `update_dual` 1.71x,
`shift_to_feasible` 1.68x, `admit_candidate` 1.57x, `pivot` 1.48x, against
the LU side at 1.05–1.11x: each iteration admits and processes more
candidates. `forplan`'s largest mover is `jm_dual_simplex` at 1.093x, which
is two trajectories differing and nothing more.

**What was refuted.** That the three share a mechanism: one pays in
iteration count, one in per-iteration candidate volume, and the label
"three firings costing 51%" covered both. And that the reduction site could
carry a predictor: the substitution is exact, the reduced models are
equivalent, and nothing at the site separates these three from the 14
instances the same family made cheaper — §2's own record already shows the
identical 20 firings halving `grow15` while inflating `grow22` sevenfold.
A rule that refuses a firing on a predicted trajectory outcome would be
fitted to named instances, which is the practice this project's own rules
exclude.

**Refused: a refuse rule for the implied free column singleton on
trajectory grounds.** The family's set-wide price already contains these
three (geometric mean 0.9527x, D106), none of the three crosses the gate's
own 2.0x work bar, and both measured mechanisms are downstream of an exact
reduction rather than of anything the family did wrong. Reopens if an
instance crosses the gate's 2.0x bar from this family's firings, or if a
measured mechanism ever predicts trajectory direction from the reduction
site. §2 is untouched: its family relaxes rows rather than substituting
exactly, it pays 0.810x set-wide for its worst cases, and its candidate
rule (refuse an unbounded relative widening) remains its own open item.

**Left open.** `scfxm3`'s localized reading names the machinery but not the
cause: what turned more columns into ratio-test candidates — the pushed
costs, or the changed basis path — was not separated, and separating it
buys nothing until some instance crosses the bar. Recorded in
`bench/measurements/02-14/README.md` beside the profiles.

## D109 — The implied-free window's floor declines nothing the set can measure, and the margin ships exactly as it is

**The question, as §1b asked it.** `ps_implied_free_margin`'s window is
`ULPS * DBL_EPSILON * max(1, scale)`, and §1b proposed that the `max(1, …)`
floor is what declines the exact-equality candidates — cases where the
bound and the implied end are exactly representable and nothing cancelled,
so the comparison carries no error to protect against. Removing the floor
would then take those 1353 rows and leave the rest declined. The blocker,
`d2q06c`'s 2.2163x at margin zero, was cleared the same day
(`bench/measurements/02-15/`): it is D108's trajectory class, with no
correctness or relaxation defect behind it.

**The reasoning, stated before the run.** The containment test is
`ilo >= cl + margin`, so an exact-equality candidate passes only when the
margin is absorbed: `margin < ulp(cl)/2`. Two floors stack under the margin
(`ps_bound_scale` already returns at least 1), the outer floor changes the
margin only where `|a| > max(1, |b|, traffic)`, and a zero bound absorbs
nothing at any scale. Prediction: `maros-r7`'s four exact-equality
candidates sit on zero bounds and stay declined; movement, if any, needs a
large exactly-met bound under a large coefficient.

**The measurement, 2026-08-17, in `bench/measurements/02-16/`.** A copy of
the tree with the outer floor removed, the repository untouched. The copy
proves itself first: at margin zero it reproduces the 02-12 sweep's
`maros-r7` record exactly (iters=2544, work=316766250), so the patched
build is real and the family is live in it. At the shipping margin 8, the
floor-less binary then runs the whole standard set: **all 94 instance lines
are bit-identical to the committed record** — presolve counts, iterations,
work units, objectives, solution digests. Digest equality is the strongest
no-op proof available here.

**What was refuted.** §1b's premise, in the direction that keeps the code:
the floor declines nothing. The 1353 rows between margin 8 and margin 0 are
declined by any nonzero window, because their bounds are zero or too small
to absorb a margin of any scale. The one setting that takes them is margin
zero, and the D106 sweep already priced and refused that (`d2q06c` 2.2163x
across the gate's own bar, geometric mean 0.9627x beside it).

**Closed: the window ships exactly as it is** — `ULPS = 8`, both floors in
place. §1b closes with it; nothing of it remains open. Reopens if a model
population makes `bench/measurements/02-16/run-floorless.sh` report a moved
instance line, which is executable, or if the D106 sweep's own question
reopens. §1c and §1e are untouched.

## D110 — maros-r7's cheaper iteration is the factor fill collapsing, and the instrument reproduced D46's figure before being believed

**The question, as §1e asked it.** D106 made `maros-r7`'s work fall 64.0x
while iterations fell 4.07x, so the cost of an iteration fell 15.7x, and
the model only shrank 31%. The hypothesis on record: the factors carried
**4.801x** the basis nonzeros, the worst ratio in the set (D46), 980 of the
removed columns were singletons, and if the fill collapsed with them the
fact belongs to the factorization. It had no measurement, and §1e said to
take one before believing either half.

**The measurement, 2026-08-17, in `bench/measurements/02-17/`.** A
throwaway print in `jm_lu_factor`'s success path — dimension, basis
nonzeros, L and U nonzeros, once per refactorization — patched into two
tree copies, HEAD and `b40fe74`, the repository untouched. Three
calibrations passed before anything was read: each binary reproduces its
committed `maros-r7` record exactly (2576/328053926 and 10479/21010708013);
`adlittle`, bit-identical across D106, gives identical traces on both
binaries; and the pre side's mean fill ratio reads **4.801**, reproducing
D46's committed figure from an instrument that never saw it.

Means over all refactorizations (326 pre, 82 post; one refactorization per
~31 iterations on both sides, so the cadence is unchanged):

| | pre-D106 | post-D106 | ratio |
|---|---|---|---|
| dimension | 3136 | 2156 | 0.69x |
| basis nonzeros | 64526 | 26865 | 0.42x |
| L nonzeros | 90523 | **3172** | **1/28.5** |
| U off-diagonal | 216157 | 33815 | 1/6.4 |
| whole factor | 309816 | 39143 | **1/7.9** |
| fill ratio | **4.801** | **1.457** | 1/3.3 |

**Closed: the hypothesis is confirmed and quantified.** A 31% smaller model
carries a 7.9x smaller factor, and every FTRAN, BTRAN and update walks that
factor — which is where the 15.7x per-iteration drop lives, with the
remainder in the 0.42x basis itself. `maros-r7` goes from the worst fill
ratio in the set (4.801x against the 2.673x set mean, D46) to 1.457x. The
fact belongs to §5's factorization item and is recorded there: the fill
problem D46 named has lost its worst example to a presolve reduction, and
the standing worst instance in the comparison, `stocfor3`, has no fill
measurement yet.

**Left open.** Nothing of §1e. The set-wide fill picture after D106 (D46's
2.673x mean predates it) is unmeasured and belongs to §5's factorization
item when that work starts; `stocfor3`'s own fill is the first number to
take there.

## D111 — The postsolve recovery is compensated, nine digests move where rounding lived, and §1c closes

**The question, as §1c asked it.** The implied-free margin is sized on the
forward sum's error, which it covers with slack; the recovery is a different
quantity and nothing sized it. `x_j` came back from `sol_row[i]`, a plain
running double accumulated in replay order, whose error grows as
`n·eps·traffic` against a margin promising `8·eps·traffic`.
`bench/measurements/02-18/` made that concrete: a degree-2001 row published
its recovered column 4.06e-6 outside the bound the caller stated, 11.4x the
margin's promise, predicted bit for bit. Two settlements were candidates:
degree-scale the margin, or compensate the accumulation.

**The decision: compensate.** Degree-scaling declines reductions to avoid an
error the arithmetic can simply not make; Neumaier compensation removes the
error class at the cost of one carry array per walk and a two-sum per
accumulation, portable and deterministic (`-ffp-contract=off` makes the
error recovery exact, the same argument `ps_acc` already carries).
Clamping stayed excluded — it hides a residue, D103's own refused shape.
Every accumulation into `sol_row` during the replay goes through
`ps_row_add`, every reader reads sum plus carry, assignments reset the
carry, and the walkers fold once at the end. `numerics-reviewer` read the
diff before any campaign; its chain finding (the cost-0 singleton re-base
discarded the carry once per record) was fixed by accumulating there too,
its ordering finding (allocation after the status publication) was fixed by
moving the allocation, its contract findings were fixed in the comments and
with the test's vacuity guard, and its saturating-activity finding
(inf becomes NaN through the carry at activities near 1e308) is refused
with its own reason: no realistic LP reaches it and `ps_acc_add` in the
forward pass has the same property.

**The measurement, 2026-08-17.** The 02-18 model is now
`test_the_recovered_column_respects_the_bound_it_was_promised`, and it was
validated the required way: green on the repaired tree, failing on its
bound assertion on an unrepaired copy carrying the same test. `make test`
and `make sanitize` green. All three sets, twice — once by this session,
once independently by `jaos-measurer`, bit-identical to each other:

- netlib: gate PASS, verdicts, iterations, work units and printed
  objectives identical on all 94; **85 bit-identical, 9 moved digest only**
  (`bandm`, `beaconfd`, `capri`, `greenbeb`, `maros`, `maros-r7`,
  `standmps`, `tuff`, `vtp-base` — every one with nonzero presolve
  reductions, so the replay accumulated on it; `truss`, which presolve
  does not touch, did not move). The unjudged residuals mostly fell:
  `greenbeb` row 2.12e-11 to 9.85e-12, `standmps` 3.32e-13 to 1.05e-13,
  `vtp-base` 8.82e-12 to 5.73e-12.
- netlib-infeas: 29 of 29 bit-identical (the infeasible walker replays
  nothing, and the record proves it).
- Kennington: 16 of 16 bit-identical.

`jaos-measurer` returned **ACCEPT** from its own context, re-confirming the
reject case itself. The netlib baseline was rewritten deliberately after
the verdict and confirmed by a following run reading
`0 regressed, 0 improved, 0 new`; the other two baselines are untouched
because their records are bit-identical.

**What was refuted.** That the defect class needed a wider or
degree-scaled margin: the margin was never the wrong size, the accumulation
was the wrong instrument, and fixing the instrument moved no verdict, no
iteration and no work unit anywhere.

**Left open.** Nothing of §1c, which closes §1 entirely — every question
D106's own measurements opened (§1a, §1b, §1c, §1d, §1e) is now closed by
D107 through D111. The reviewer's refused finding above is the only carried
note, recorded here with its reason.

## D112 — The widening rule cannot tell grow15 from grow22, and §2's refusal closes on its own counter

**The question, as §2 asked it.** The cost-0 bounded singleton column
family turns twenty `== 0` rows into ranges of up to 5e5 on each of
`grow7`, `grow15` and `grow22`; `grow22` and `grow7` inflate 11.16x and
8.56x in work while `grow15` halves its iterations (02-11). The candidate
rule was to refuse a firing whose relaxation widens the row beyond some
multiple of its own scale, and it needed a sweep on both sides and a
campaign.

**The measurement, 2026-08-17, in `bench/measurements/02-19/`.** A
throwaway print at the firing site, one line per firing over the standard
set, calibrated against 02-11's committed `grow22` count of 20 per solve —
the calibration caught the runner's two-solve determinism check doubling
every trace, and the aggregator now requires the two passes identical,
which they are on all 60 firing instances. The distribution: **8617
firings over 60 instances, 8096 of them (94%) on equality rows**, 8495
past the row's own scale, 4934 past 100x it, and the typical relaxation
removes one row end entirely (a half-infinite column box makes the
absorbed range `[0, inf)`).

**What was refuted, twice over.**

The discriminator does not discriminate: `grow7`, `grow15` and `grow22`
carry the same maximum relative widening, **5.524e5**, and one of them is
helped while two are hurt. No threshold on the widening separates them —
which is D108's finding again, arriving from the structural side: nothing
at the firing site predicts trajectory direction.

And any threshold that catches the `grow*` firings catches the family:
98.6% of all firings widen past the row's scale, because absorbing a
singleton's slack is what the family is for. The rule is not a filter on
this population; it is the family's off switch with extra steps, and
turning the family off to repair two instances is a different, larger
question that would need its own no-family campaign to even be priced.

**Refused: the unbounded-relative-widening refusal rule.** §2 closes with
it. What stands: `grow22` and `grow7` remain the standard set's worst
cases against `-DJAOS_NO_PRESOLVE`, below the gate's own bar. Reopens on
the condition D108 already carries — a measured mechanism that predicts
trajectory direction from the firing site — or on an instance crossing
the gate's 2.0x work bar from this family's firings.

**Left open.** Nothing else of §2. The 02-19 instrument is reusable for
any future population question about this family.

## D113 — stocfor3's presolve gap is the aggregator, and the prize lands behind D97 again

**The question, as §5's presolve item asked it.** `stocfor3` is the worst
instance in the comparison since the P0 re-take (30.0x HiGHS), its factor
fill is 1.036 (D110), and HiGHS's presolve removes 50% of its rows where
JAOS removes 58 of 16675. Which families do it was uncounted; 02-10's
`maros-r7` count was the pattern to repeat.

**The measurement, 2026-08-17, in `bench/measurements/02-20/`.** HiGHS
1.15.1 at the P0 options, one presolve rule suppressed per run via the
documented `presolve_rule_off` option. The calibration found the
instrument's limit before anything was believed: `maros-r7`'s known
984-row reduction is untouched by every suppression, so HiGHS runs the
implied-free elimination in its base rules (0–5), which the option cannot
turn off — ablation attributes the suppressible rules only, marginally
and with interactions.

Eleven of twelve single-rule ablations on `stocfor3` change nothing.
Turning off the **Aggregator** collapses the reduction from 8416 rows to
2859 and lifts HiGHS from 6404 to **14788 iterations, 2.31x**. The
doubleton rule alone reads idle (−9 rows) because the aggregator subsumes
it; free col substitution carries −216 rows and +5% iterations.

**Closed: what HiGHS removes on `stocfor3` is equality substitution at
any degree — the aggregator — and it alone owns the iteration half of the
30.0x.** With it off, HiGHS needs 14788 iterations against JAOS's 18431,
1.25x, near parity. What remains of the gap is the per-iteration cost the
M2 split already tracks.

**Where it lands.** Aggregation beyond the free and implied-free cases
transfers bounds onto the survivor, which is the machinery D97 refused
six designs of. §3 already weighed the doubleton population behind D97;
this puts a third and larger prize there: the worst instance in the
comparison, whole. D97's reopen condition is unchanged and what it
unlocks keeps growing. Nothing else is left open here.

## D114 — D97's over-tightening is derived: a window scaled by the activity certified 5.86 of slack as zero

**The question, as D97 posed it.** Its first reopen precondition: a
derivation of why the implied bounds over-tighten on `pilot`, `pilot87`,
`agg` and `maros` specifically — not the epsilon and not the rounding,
both already measured on both sides across nine settings.

**The measurement, 2026-08-17, in `bench/measurements/02-21/`.** The
minimal failing design lives at commit `7c7375c` and was excavated whole:
built with the diagnostic flags, it reproduces D97's table (`pilot` and
`pilot87` INFEASIBLE at zero iterations, `agg` and `maros`
checker-rejected, the refusals landing on the exact rows and residues of
D97's worked case). Throwaway prints then walked the chain backward from
`pilot` row 1095's empty -1.15 to its source, every link with its number:

1. Tightening materializes `[0, 6.687e10]` onto column 3556's original
   `[0, inf)` — a valid implication that is now a finite magnitude.
2. Row 1094 (`rl = ru = 0`) computes its range over the tightened boxes:
   `min_act = -9.42e11`, `max_act = 5.8644`, and its single per-row window
   becomes `rtol = 1e-9 × 9.42e11 = 941.58`.
3. `force_lo` asks `5.8644 <= 0 + 941.58` and fires on a row with 5.86 of
   genuine slack, pinning column 3554 at 1.15, its own upper bound.
4. Row 1095, an equality needing that column at 0, goes empty holding
   -1.15, and presolve correctly refuses the model those boxes describe.

**The derivation.** The forcing verdict's window took its scale from the
activity range's magnitude, where its claim is about the row bound's
scale — the same judgement-constant failure 02-09 later found for the
emptied-row test, compounded here by tightening feeding materialized
magnitudes into the scale. No epsilon reaches it: on this row every
setting down to ~6e-12 still windows past 5.86, and larger materialized
bounds survive smaller settings, which is why D97's sweep moved nothing.
The implied bounds' values were never the defect. The repair's form
already ships: today's forcing family windows by
`ps_bound_scale(cur_rl, cur_ru)`, ~1e-15 on that row, and is green.

**What this closes and what it opens.** D97's first precondition is met;
its refusal stands until the second — the dual postsolve for an imposed
bound — exists, and any future tightening design must window pin verdicts
by the row bound's scale, pin only within the arithmetic's own error
(`eps × traffic`, the D103 form), and keep materialized bounds out of
verdict scales. The requirements are in `bench/measurements/02-21/`'s
README beside the exhibit.

## D115 — The fourth set exists, and small models understate the iteration exponent by 1.6x while the work unit holds to 6%

**The question, as TODO.md §4 asked it.** Every verdict in this repository had
been taken on 139 models: 94 netlib standard, 16 Kennington, 29 netlib
infeasible. Three entries already said the population was doing more of the
deciding than it should — D46 (two instances are 74% of the standard set's
total work), D101 (three families deferred on 0.15% *on these 139 models*), and
§1's own counter (3315 rows on netlib, 0 on Kennington). The section named a
fourth set as the executable form of that concern and then blocked it behind
§1. §1 closed 2026-08-17 with D107 through D111, and nobody re-read §4.

**What was built, 2026-08-17/18.** `bench/fetch.sh` gained a `bz2-emps` mode;
`bench/run.c` gained a third expectation, `EXPECT_OPTIMAL_NOREF`, for a set with
no published optimum (`bench/measurements/02-22/`); fifteen instances are pinned
across three manifests from Mittelmann's mirror. The instances are fetched and
never redistributed, which is the position already taken for netlib and
Kennington and is what makes the absence of a licence statement on that host
survivable.

**The measurement, `bench/measurements/02-23/`.** `fome` 4/4 and `pds` 8/8, all
`shape=ok checker=ok det=ok`, both `gate: PASS`. `nug08-3rd` solved; `nug20` and
`nug30` are unmeasured, not unsolvable.

The `pds` family already ran from `pds-02` to `pds-20` inside Kennington, so the
ladder is **twelve points over a 52.9x range in rows**, four of which had been
in the repository since M1 and had never been read as a sequence. `ladder.py`
beside the record derives every figure from the manifests and the baselines.

Split the range at `pds-20`, which is where netlib and Kennington stop:

| | iteration exponent | work exponent |
|---|---|---|
| `pds-02` … `pds-20` | 1.30, 1.14, 1.38 — mean **1.27** | **2.61** |
| `pds-20` … `pds-90` | 1.83, 2.16, 1.84, 2.01, 2.59, 2.34, 1.78 — mean **2.08** | — |
| whole range | 1.69 | **2.77** |

**Two conclusions, and they point opposite ways.** Measuring in the small range
understates how iteration count grows by a factor of **1.6**. It understates how
work units grow by **6%**. So the unit D16 made a public contract holds its
shape across a 53x change in model size and the iteration count does not, which
is the first evidence for that choice rather than an argument for it.

**What was refuted.** Mittelmann's LPopt benchmark itself, which was §4's most
attractive candidate because `bench/compare` would read directly against
published figures. Its smallest instance carries 3x `dfl001`'s nonzeros, its
median is around 1.5M, `dlr2` is 7.1M x 38.9M, and SoPlex solves 31 of it while
JAOS reads 0.95x SoPlex per solve at P0. Sixteen of its instances are
undisclosed. It is the set to aim at and the wrong one to adopt. `fctp` was
declined for the opposite reason — 2.2K to 111K compressed, netlib's own problem
again.

**The cost, because it decides how this set can be used.** `pds` alone is 23016 s
of solve time and 6.4 hours of wall clock at `J=4`; `pds-100` costs 6.425e11
work units, **twenty times the entire netlib standard set**, and the eight `pds`
together are 62x it. The three `netlib*` targets stay the gate and `plato` is
deliberately not part of it.

**One step is reported as one step.** `pds-90` to `pds-100` grows iterations
1.886x for a 1.094x model while the cost of an iteration falls 30%. Their
product is a work ratio in line with its neighbours, so total work stayed smooth
while the split jumped — the same finding from the other side, and not a trend.

**Left open, in `TODO.md`.** How often `plato` should run. `nug20` and `nug30`.
Whether D101's and D107's reopen conditions — both written as "a model
population" — are now satisfiable; their scripts exist and have not been pointed
at this set.

## D116 — Directed rounding on the activity readings is refused, because the forcing test detects an equality

**The question.** Fourer & Gay 1994 report AMPL's presolve discarding
constraints that kept netlib's `maros` from being unbounded, and reporting
inconsistent constraints on `greenbea`, `greenbeb`, `perold` and `woodw`. Their
fix was not a tolerance: they computed the activity bounds with IEEE directed
rounding, so a deduced bound is valid by construction. `maros` is one of D97's
four failing instances and `greenbeb` one of D108's three, so the question was
whether JAOS is in the same position.
`docs/research/dual-postsolve-imposed-bound.md` §13 predicted the change would
be "a no-op or a small gain".

**The measurement, `bench/measurements/02-24/`.** Built in a git worktree while
the `plato-pds` campaign held the main tree. `candidate.diff` is beside the
record. Nothing landed.

**What was refuted, first design.** Widening both activity ends outward and
dropping every window is sound by construction and dies on `make test` in under
a minute. The objection generalises and is the entry: **the FORCING reading
detects an EQUALITY, not an inequality.** Outward rounding makes an inequality
proof survive rounding; it destroys an equality detection. The test suite's own
`make_forcing_row_model` is `x0 + x1 <= 0` with both columns in `[0, 10]` —
minimum activity exactly 0 against an upper bound of exactly 0 — and one ulp
declines it. A `<= 0` row over non-negative columns is the shape the family
exists for, not a corner case.

**What was refuted, second design.** Leaving FORCING alone and making only
REDUNDANT sound. That is a real defect being fixed: the shipped form drops a row
whose minimum activity is within `err` *below* `rl`, a row that can still bind,
which is exactly AMPL's `maros` failure. It passes `make test` and `make
sanitize` clean and then fails the gate. **`pilotnov` goes from 86587427 to
2378158900 work units, 27.5x**, against `bench/run.c`'s own 2.0x bar, alone
among 94. Every other instance is unmoved.

The mechanism was named rather than inferred, by running the one instance
through both binaries: presolve removes **101 rows at HEAD and 69 in the
candidate**, columns identical at 1811 on both sides, so nothing else moved.
Thirty-two rows that survive instead of being dropped cost 60866 iterations.
This is D108's and D112's class — a reduction whose effect is on the trajectory
rather than at the reduction site.

**The cost of the refusal is stated too.** The answer is bit-identical to the
last digit on both builds, and the candidate's residuals are *better*: row
`3.09e-10` against `1.93e-07`, relative row `7.9e-14` against `9.32e-13`. So the
32 rows buy a numerically cleaner answer at 27.5x the price. This is a cost
question, not a correctness one, and the refusal says so.

**Refused.** The unsoundness that remains is bounded by `ps_row_tol` — 8 ulps of
the row's traffic — nothing in this repository has been shown to give a wrong
answer because of it, and removing it costs 27.5x on one instance.

**What this does not refute.** Fourer & Gay's result. Their `maros` failure was
real and directed rounding fixed it. JAOS is not in that position because D103
already replaced the judgement constant with the error bound, and their pre-fix
tolerance was orders wider than 8 ulps. Reading a published fix is not the same
as needing it.

**Left open, in `TODO.md`.** The reopen conditions, in `02-24`: an instance
where a wrongly-dropped redundant row produces a wrong answer, a wrong verdict
or a checker rejection; or a rule that separates the 32 rows from the 69 that
still fire, which is D108's condition in a new place and D108 refused that rule
once already.

---

## D117 — D106 fires on none of fome's candidates because D95 takes every one of them first, and freezes 12.1% of the rows

**The question.** D115 built the fourth set and the first thing it found was
open. 02-13's counter reads **166, 332 and 664** implied-free column singleton
candidates on `fome11`, `fome12` and `fome13` — exactly proportional to the
family's doubling — and D106 fires on **none** of them, while every one of the
three has rows removed by nobody. 02-25 ruled out `PRESOLVE_IMPLIED_FREE_ULPS`
with a canary that moves: at margin 0 `maros-r7` goes 980 → 984 rows and the
two `presolve.o` differ by md5, while `fome11` is identical on both builds down
to its 46026 iterations. `TODO.md` §4a named the frozen row as the leading
suspect and asked for a diagnostic build that says which condition declined
each candidate.

**The measurement, `bench/measurements/02-26/`.** A decline reader is compiled
into a copy of `src/presolve.c` under `-DJAOS_DIAG`; the repository tree is
read and never written. It records two things per candidate: what actually
became of the column, and what D106's own four conditions say about it, read at
the top of the column pass before any family in that round can act.

**The answer is the order of two families, not any of D106's conditions.**
`JM_PS_SINGLETON_COL` — D95's cost-0 bounded singleton column — takes **100%**
of `fome`'s candidates in round 0. Its branch sits in the same column pass as
D106 and above it. D106 never sees them.

| | candidates | taken by D95 | of those, D106 would have fired on |
|---|---|---|---|
| `fome11` | 166 | 166 | **18** |
| `fome12` | 332 | 332 | **36** |
| `fome13` | 664 | 664 | **72** |

The rest are declined by the margin anyway: their implied bound sits exactly on
the column's own bound, which is the bimodal shape 02-12 recorded when it made
the margin a switch rather than a dial.

**The frozen row is refuted, not merely unproven.** Over the whole 94-instance
standard set exactly **2** candidates are declined for that reason. On `fome`
none are.

**And it explains 02-25's canary.** D95 takes all 166 at any margin, so no
setting of `PRESOLVE_IMPLIED_FREE_ULPS` can change what D106 does with them.
`maros-r7` moves and `fome11` cannot.

**What D95 freezes is the larger number.** A frozen row is closed to every
row-removing family for the rest of the run.

| | rows | frozen by D95 | rows presolve removes |
|---|---|---|---|
| `fome11` | 12142 | 1468 (12.09%) | 0 |
| `fome12` | 24284 | 2936 (12.09%) | 0 |
| `fome13` | 48568 | 5872 (12.09%) | 0 |
| `fome21` | 67748 | 0 | 3174 |

The same share at all three sizes. `fome21` carries no such column, freezes
nothing, and is the one instance of the four where rows go. On netlib the same
family freezes **8309 rows over 60 of the 94 instances**, against the 8639 rows
every family together removes; `fit2p` freezes all 3000 of its own. `pds` and
`nug` carry no candidates and freeze nothing.

**It is not a `fome` peculiarity.** Over netlib, 524 of the 3321 candidates go
to D95, and **55 of those are ones D106 would have fired on** — `ganges` 12,
`czprob` 11, `dfl001` 9, `pilotnov` 7, `pilot-ja` 7, `perold` 6, `seba` 1,
`scrs8` 1, `d2q06c` 1. That is 5.3% of what D106 removes today.

**Three calibrations, and the reader is checked against the code.** `maros-r7`
reproduces 984 candidates and 980 firings; netlib reproduces 3321 candidates
and 02-12's 8639 rows removed; and zero netlib candidates end on `WOULDFIRE`,
which is what a disagreement between the reader and D106 would leave behind.
02-12's other figure, the 1041 this family "adds", is a delta against the 7598
the set read before D106 and is not this counter. D106's own firing count is
**1044**, so three rows other families used to remove are no longer removed
once D106 takes theirs first — the same ordering effect, from the other side.

**Nothing here says D106 is wrong, and nothing here changes the order.** Both
families are exact. On a cost-0 column D106's cost transfer is zero, so D106
would remove the same column, remove its row as well, and leave no cost behind.
That makes it the larger reduction on the columns both can take, and says
nothing about its cost: D108 and D112 both measured this family's price landing
on the trajectory rather than at the reduction site, and neither found a rule
that predicts the direction. The 18 / 36 / 72 and the 55 are lower bounds on
what a reordering moves, not predictions — D106 firing removes a row, and every
later round sees a different model.

**Left open, in `TODO.md` §4b.** Whether D106 should be preferred over D95 on a
column both can take, measured under the loop: `numerics-reviewer` on the diff,
all three sets, and a verdict from `jaos-measurer`. `fome` is not in the gate,
so the netlib 55 and the Kennington side are what would decide it.

---

## D118 — Giving the implied free family first refusal is refused: pilotnov publishes a suboptimal point as optimal

**The question.** D117 measured that `JM_PS_SINGLETON_COL` — D95's cost-0
bounded singleton column — takes every column the implied free column singleton
(D106) could also take, because its branch sits above D106's in the same column
pass. On a cost-0 column D106's cost transfer is exactly zero, so D106 removes
the same column, removes its **row** as well, and leaves nothing behind, where
D95 keeps the row, widens it and freezes it. The candidate moves only that one
branch below D106's; the free cost-0 cases keep their old precedence, so
exactly one thing changes. Built in a worktree; nothing landed but one test.

**The prediction held, per instance.** D117 counted the columns from a
read-only instrument. The solver removed exactly those rows and no others:
`ganges` 12, `czprob` 11, `dfl001` 9, `pilotnov` 7, `pilot-ja` 7, `perold` 6,
`seba` 1, `scrs8` 1, `d2q06c` 1 — 55 rows over nine instances, nine for nine.

**The measurement, `bench/measurements/02-27/`.** `make netlib` exits 1 with
four regressions, all on `pilotnov`:

```
parent    obj=-4497.2761882188706  objective=ok  checker=ok
          iters=2374   work=86587427    dual=0     rsub=8.16e-13
candidate obj=-3169.5271937202242  objective=OUT-OF-TOLERANCE  checker=REJECTED
          iters=87432  work=2616239810  dual=0.89  rsub=117
```

**The published objective is wrong by 29% and the solver reports `optimal`.**
The point is primal-feasible (`col=1.11e-17`, `row=6.8e-08`,
`rowrel=2.67e-12`), so this is not a containment failure. It is a feasible but
suboptimal point published as optimal, with a dual violation of 0.89. The
checker caught it; no digest and no work bar would have. `netlib-infeas` and
`netlib-kennington` both read `0 regressed, 0 improved, 0 new`.

**What the refusal costs, because a refusal owes both sides.** Four instances
get cheaper — `ganges` **0.8429x**, `dfl001` **0.8951x**, `czprob` 0.9227x,
`scrs8` 0.9837x — and three get dearer: `d2q06c` 1.037x, `perold` 1.0675x,
`pilot-ja` 1.1669x. Geometric mean over 94 instances **1.0358x**. `dfl001` is
one of the two instances that are 74% of the set's total work (D46), so its
0.8951x is a real prize.

**The band between 1.0000x and 1.0015x on about fifty instances is billing, not
cost**, and so are Kennington's four moves (0.9977x to 0.9999x, digests
identical): D106's block now runs on every cost-0 bounded singleton column and
bills its range charge there. `numerics-reviewer` named this before the
campaign ran.

**What this exposed about the shipped code, which is the entry.** D106 has
never been handed a cost-0 bounded singleton column, on any instance of any of
the four sets, because D95's branch takes all of them first. This candidate is
the first thing that ever gave it that population: **55 columns, 48 correct and
7 wrong, all seven on `pilotnov`.** D106's four stated restrictions are
therefore not sufficient for that population, and nothing in the repository
says which fifth one is missing. The shipped code is not affected, because the
order it ships with never asks the question — a safety margin nobody chose,
now written down.

**Refused.** A one-line reordering that makes a feasible model publish a 29%
wrong objective is refused whatever it buys elsewhere.

**What was refuted along the way.** The claim that the reorder repairs the
basis-count promise. `test_singleton_col_between_two_removals_solved_path`
did read the correct 2 under the candidate, and that is the pin being moved off
the defective path rather than the defect being fixed. `numerics-reviewer`
proved it by probing `TODO.md`'s own named minimum case against the candidate's
own library: still 2 basic against `num_row = 1`.

**What survived.** `test_the_basis_count_promise_breaks_on_a_declined_column`,
which pins that minimum case — 2 basic against `num_row = 1`, and 1 in the
reference build — on a column the implied free family declines by **margin**
rather than by order, so no future change of order can quietly retire it.

**Left open, in `TODO.md` §4b.** Why `pilotnov`'s seven and not `pilot-ja`'s
seven. Settling it needs `jaos-debug`'s procedure on those seven columns.
`pilotnov` is already the instance D116 found sensitive to a different presolve
change, at 27.5x and with a correct answer. The reopen condition is a fifth
restriction on D106 that declines those seven; the 55 are worth re-asking then,
because `ganges`, `dfl001` and `czprob` are a real gain.

---

## D119 — pilotnov's wrong answer is the refactorization interval, and the termination test never re-reads dual feasibility

**The question.** D118 refused a one-branch reordering of presolve's column
pass on `pilotnov` alone: it published an objective 29% wrong as `optimal`,
primal-feasible with a dual violation of 0.89, at 30.2x the work. `pilot-ja`
loses seven rows to the same change and is untouched. Either presolve cut off
the optimum, which would mean D106's substitution is unsound on a population it
never meets, or the reduced model is fine and the solve failed on it. Those
lead in opposite directions and §4c asked which.

**The measurement, `bench/measurements/02-28/`.** The same candidate, the same
reduced model, four values of `REFACTOR_EVERY`, each compiled into its own
binary with its own md5 printed beside it.

```
HEAD, no candidate    2374 iters    86587427 work   -4497.2761882188706   dual 0
candidate,  64        87432       2616239810        -3169.5271937202242   dual 0.89
candidate,  16        28859       1345562616        -4497.2761882188715   dual 0
candidate,   8         2741         89348539        -4497.2761882188752   dual 0
candidate,   4         2287         84697175        -4497.2761882188743   dual 0
```

**At 16 the candidate reaches Koch's published optimum to the last bit.** So
presolve did not cut off the optimum, and D106's substitution on those seven
columns is sound.

**And the reduced model is not intrinsically harder.** At interval 8 it costs
1.032x HEAD's work on that instance, where at 64 it costs 30.2x. The 30x is the
interval, not the model.

**What goes wrong, named rather than inferred.** At 64 the solve reports
**43041 weight restarts and 156 stability rebuilds**, against HEAD's 1042 and 0
on the same instance, and `pilot-ja`'s 0 and 0 under the same candidate.
`pilotnov` was already the sensitive one and the candidate pushes it over. Then
the solve stops and calls it optimal, and the checker rejects the answer at a
dual violation of 0.89.

The checker caught it. No digest comparison and no work bar would have: the
answer is feasible and deterministic.

> **CORRECTION, the same day, `bench/measurements/02-29/`.** This entry first
> said the termination test reads primal feasibility only, and that "dual
> feasibility is an invariant the dual method maintains and nothing re-reads
> before the verdict is published". **That is false, and measurement refutes
> it.** `dual_breach`, `published_breach`, `breached`, `arm_reentry` and the
> whole re-entry loop read it, and the loop keeps the best point it finds by
> `settled_dual_violation` (D25, D89). On `pilotnov` under this candidate that
> loop **ran six rounds and its own dual violation read exactly zero**. The
> solver does check; its check disagrees with the independent checker's. That
> is a different defect from the one named here, and D120 carries it. Nothing
> else in this entry moves: the sweep, the 1.032x and the refusal all stand.

**Postsolve is ruled out by derivation, not by hope.** All seven columns have
cost exactly zero, so D106 sets `y_i = c_j / a_ij = 0` on each of their rows
and its transfer `c_k -= y_i * a_ik` subtracts nothing from any surviving
column. Those records contribute zero to every dual, so they cannot produce a
violation of 0.89.

**What this does not say.** It does not say `REFACTOR_EVERY` should be lowered.
One instance is not a population, the interval is a global constant and every
instance pays it; the sweep is a diagnostic on one model. And it does not
un-refuse D118: at the interval that ships, the candidate still publishes a
wrong answer and the gate is still red.

**What it changes.** D118's reopen condition was written as "a fifth
restriction on D106", and that is now known to be the wrong place to look.
Nothing is wrong with D106 here. The condition is restated in the refusals
table.

**Left open, in `TODO.md` §5a, and carried by D120.** Why the solver's own dual
reading and the checker's disagree on this point. Whether the refactorization
interval should adapt to an instance rather than being one constant is a second
question and is not costed either. `TODO.md`'s standing debt already records
that the `REFACTOR_EVERY` trajectory sweep is manual and that three of M1's four
defect closures came from it. This is the fourth.

---

## D120 — The same reduced LP solved twice: both points dual-feasible by the solver's own reading, and 29% apart

**The question.** D119 said the termination test reads primal feasibility only
and that nothing re-reads dual feasibility before a verdict is published. That
sentence was written from the code and not measured, and §5a set out to cost
the repair it implied. The first reading refuted it instead.

**The measurement, `bench/measurements/02-29/`.** Four probes, each compiled
into a copy of the tree under `-DJAOS_DIAG`. The state is reachable only
through D118's refused candidate, applied from its own recorded diff.

**1. The solver does check, and its check says clean.** `dual_breach`,
`published_breach`, `breached`, `arm_reentry` and `reenter_after_settling` all
read dual feasibility, and the loop keeps the best point by
`settled_dual_violation` (D25, D89). On `pilotnov`:

```
REENTRY no-work rounds=6 dviol_now=0 dviol_best=0 obj=-2115.3928900690385
```

**Six rounds, and its own dual violation reads exactly zero**, on the point the
independent checker rejects at 0.89. `pilot-ja` exits after one round, same
zero, right answer. D119's sentence is corrected in place.

**2. The basics' reduced costs are not the gap.** `compute_duals` assigns
`s->d[v] = 0.0` to every basic variable and never verifies it, which is exactly
the class `jaos-debug` step 4 names. Recomputing `d = c - A'y` from the
solver's own duals for every variable, unscaled: `pilotnov` reads **3.82e-14**
worst over the basics and **3.75e-14** worst over the nonbasics — the cleanest
of the three instances probed, `pilot-ja` reading 1.14e-09 and `dfl001`
1.11e-09. `B'y = c_B` holds and every nonbasic sign is right.

**3. No column rests on a bound dual phase 1 lent it.** Such a column is
interior in the caller's box, so a nonzero reduced cost there is a violation to
the checker and legal to the solver — the exact shape of a 0.89. `pilotnov`
lent 72 and **none is resting on one** at the exit; `pilot-ja` lent 7, same.

**4. The reduced model is identical at both intervals.** Read rather than
assumed, because the whole conclusion rests on it: `975/2172/13057 ->
867/1811/11676` with `fixed_col=230 empty_row=24 empty_col=0 singleton_row=27
singleton_col=94 rounds=8` at both `REFACTOR_EVERY = 64` and `16`. Family by
family, the same.

**What is left is a contradiction, and it is the entry.** On one and the same
reduced LP, the interval-16 solve postsolves to Koch's published optimum to the
last bit, and the interval-64 solve stops at a point that is primal-feasible by
its own test verified against a fresh factorization (D20), whose every reduced
cost recomputed from its own duals is correctly signed to 3.8e-14, with no
column on a lent bound — and whose postsolved objective is 29% worse. A basis
that is primal-feasible and dual-feasible is optimal. Both cannot be right, so
**one of the solver's own optimality readings is measuring something other than
what it claims**, and one reordering inside presolve reaches it from a green
tree.

**What was not tested, named so it is not redone.** `s->status[v]` going stale
after 156 stability rebuilds and whatever `repair_singular_basis` evicted —
every reading above trusts it to say which bound a variable sits at, so if it
lies they all agree with each other and with nothing else. D20's second opinion
refactorizes and re-reads the same carried `x_B` rather than comparing it
against an independent `B^-1(b - N x_N)`. And postsolve, which is the same code
at both intervals and right at 16, so it is not wrong on its own but could be
faithfully reproducing a reduced point that was never optimal.

**Nothing here changes the shipping code, and no instance of the 139 reaches
this state.** That is why the gate is green and why this is an open question
rather than a defect with a repair. It is written down because it was reached,
reproducibly, and because three plausible explanations are now closed.

**Left open, in `TODO.md` §5a.**

---

## D121 — The shift round trip is not bit-exact, and on pilotnov it destroys 67 costs, one by 55.11

**The question.** D120 recorded a contradiction it could not resolve. On one
reduced LP the `REFACTOR_EVERY = 16` solve reaches Koch's published optimum to
the last bit, and the `REFACTOR_EVERY = 64` solve stops 29% short at a point
that is primal-feasible against a fresh factorization, whose every reduced cost
recomputed from its own duals is correctly signed to 3.8e-14, with no column on
a lent bound. A primal- and dual-feasible basis is optimal, so one of those
readings had to be measuring something other than what it claimed.

**It was measuring the wrong objective, and the two remaining probes say so.**
`bench/measurements/02-29/`.

**The carried `x_B` is not it.** D20's second opinion runs *before* the
re-entry loop, and the loop then updates `x_B` incrementally
(`src/simplex.c:2367`) with nothing re-verifying it. Recomputed from the
factorization at the exit, `pilotnov` drifts **7.22e-10** absolute and
`pilot-ja` and `dfl001` drift zero — four orders too small to be a 29%
objective error.

**`s->cost` is.** Against the vector the scaling pass built:

| | costs moved | shift pending | worst absolute | worst relative |
|---|---|---|---|---|
| `pilotnov` | **67** | 0 | **55.110016** | **1.0** |
| `pilot-ja` | 0 | 0 | 0 | 0 |
| `dfl001` | 236 | 0 | 3.64e-12 | 2.22e-16 |

Sixty-seven of `pilotnov`'s costs are permanently different from the ones the
solve was handed, one by 55.11 on a cost of magnitude at most one, and every
`shift` record reads zero. `dfl001` moves 236 costs and stays at 2.2e-16, which
is what rounding looks like.

**The books balance and the arithmetic does not.** Tallying every lend
(`shift_to_feasible`) and every repayment (`repay_shifts`, `primal_cleanup`)
per variable, `pilotnov`'s worst-drifted column v=1050 reads **lent
1.61113965389807e+32, repaid 1.61113965389807e+32, shift 0**. The bookkeeping
is exact. The arithmetic is not: `s->cost[v] += need` then `s->cost[v] -=
shift` is `x += d; x -= d`, which does not restore `x` — the trap `jaos-debug`
names in as many words. A cost of order one plus 1e32 *is* 1e32, and
subtracting 1e32 back leaves nothing of it. 55.11 is the residue.

**And 186 variables never balanced at all**, the worst by 256, so loans are
being lost as well as rounded away. That is a second defect in the same
machinery and it is not the one that produced the wrong answer here.

**What it explains.** Every optimality reading D120 took is correct about the
objective `s->cost` holds, which is no longer the caller's. The solver is dual
feasible for its own perturbed problem; the checker judges the model's true
costs and reports 0.89. The 29% is the distance between the two objectives.
`settled_objective` computes `(s->cost[v] - s->shift[v]) * x` and calls the
subtraction "belt and braces rather than arithmetic that matters"; with `shift`
at zero and `cost` off by 55.11 it reports the perturbed objective.

**What this is not.** It is not a defect any of the 139 instances reaches. At
HEAD `pilotnov` runs 1042 weight restarts and 0 stability rebuilds and answers
correctly; D118's refused candidate is what drives the loans to 1e32. And it is
not a tolerance — no threshold in the file decides any of it, so nothing here
is fixed by moving one.

**Nothing is repaired here and nothing is costed.** A repair has to keep the
shift mechanism, which the method needs, while making the round trip exact or
making the loan bounded relative to the cost it lands on. Both are design
questions with their own measurements, and `fp-numerics` says plainly that a
sum is known no more finely than its terms.

**Left open, in `TODO.md` §5a**, now with the mechanism named rather than the
symptom.

---

## D122 — A repayment restores the cost instead of subtracting the loan, and costs 1.0001x

**The question.** D121 located a defect and did not repair it. The dual method
borrows costs to keep dual feasibility, and repaying subtracted the recorded
loan back out — `x += d; x -= d`, which does not return `x`. On `pilotnov`
under D118's refused presolve candidate the loans on one column total 1.6e+32
against a cost of magnitude at most one, 67 costs end permanently wrong with
the worst off by 55.11, and every `shift` record reads zero. The solve then
priced an objective nobody asked for, was dual feasible against it, and
published a result 29% off as `optimal`. `TODO.md` §5a offered two repairs and
said to take the cheaper one first because it cannot change a trajectory.

**The change.** A write-once array `cost0` holds the model's own scaled cost,
and both repayment sites restore from it. `settled_objective` reads `cost0`
rather than `cost - shift`.

**`numerics-reviewer` read the diff before any campaign, and three of its four
findings are in the landed change.**

- `primal_cleanup` moved `d[q]` by the recorded loan. `d` is `cost - y·M_q` by
  definition, so it must move by the amount the **cost** actually moved —
  otherwise the two disagree by exactly the drift this repair removes, on the
  one quantity that then decides the pivot. It now moves by
  `cost[q] - cost0[q]`.
- Both sites gated the restore on the record alone, so a column whose cost
  moved while its record came back to zero kept its drift for ever, and
  `repay_shifts` returning `false` meant `settle_shifts` never re-priced it.
  That is the case D121 measured **186** of. Both now test the cost.
- The comment claimed a `cost[v] == cost0[v] + shift[v]` invariant. **There is
  no such invariant**: the two arrays accumulate separately and round apart at
  the first lend large against the cost. What makes the restore exact is only
  that `cost0` is the model's own by construction, and the comment says that
  instead.
- Its fourth was a suggestion and is taken: `settled_objective`'s precondition
  was a comment and is now a debug-only assert. It does not fire anywhere in
  the suite.

The review also confirmed the change's main risk was absent: every write to
`s->cost` was enumerated — the two in the scaling pass, `shift_to_feasible`,
and the two repayments — so no write is silently discarded by restoring.

**Validated against the case it exists for**, both binaries built in one run
with distinct md5s (`bench/measurements/02-30/run-validate.sh`):

```
without   obj=-3169.5271937202242   checker REJECTED   dualviol 0.89   gap 0.295
with      obj=-4497.2761882188706   checker ok         dualviol 0      gap 2.02e-16
```

87052 iterations against 87432 and 156 stability rebuilds either way, so it
repairs the answer and not the difficulty — which is what it claims and no
more.

**The campaign, `bench/measurements/02-30/campaign/`.** All three sets at
`J=12` against the committed baselines: **`0 regressed, 0 improved, 0 new` on
all three.** No `objective`, `checker`, `shape` or `det` predicate moves
anywhere.

| set | bit-identical | moved | digests moved | record's own age |
|---|---|---|---|---|
| netlib (94) | 70 | 24 | 24 | **current** |
| infeasible (29) | **29** | 0 | 0 | 3 `src/` commits behind |
| Kennington (16) | 6 | 10 | 10 | **7 `src/` commits behind** |

**The last column nearly cost the attribution and one control restores it.**
`record_diff.py` compares against the record as committed, and only
`netlib.txt` was committed after the last `src/` change, so Kennington's ten
moved digests could on the face of it belong to the seven commits its record
misses. They do not: **the parent binary reproduces all three committed records
bit-identically**, 94/94, 29/29, 16/16, so those seven were no-ops there, the
committed record is the parent on this host, and every figure here is this
change alone. That is the distinction a staleness count cannot make by itself,
and it is why `preflight.sh` reports one as a count rather than a verdict.

Digests move because an arithmetic result changes on every instance whose round
trip was not exact, and only there. **`pilot-ja` is bit-identical**, which
`numerics-reviewer` named before the run as the check that would catch anything
else having changed: D121 measured it at zero costs moved.

**The cost is a geometric mean of 1.0001x on netlib** — worst `pilot` 1.0078x,
then `ganges` 1.0007x, every other instance 1.0000x — and **0.9975x on
Kennington**, best `pds-06` 0.9769x, with nothing there getting dearer.
**Iterations move on one instance out of 139**, `pilot`, 23265 → 23331.

**`jaos-measurer` returned ACCEPT** from its own binaries, with the controls
that make the figures readable: the parent reproducing the committed records,
the candidate repeating byte for byte from `make clean`, `make sanitize` clean,
and warm measured against a same-tree parent rather than a record that turned
out to be 21 `src/` commits behind. Residuals over all three sets: 26 changed,
21 better, 5 worse, and all five that worsened sit at 1e-14 or below, decades
under `RSUB_FLOOR`.

**The new assert never runs in a release build, and was checked separately.**
Rebuilt with `EXTRA_CFLAGS=-UNDEBUG`, `settled_objective`'s precondition never
fired on the 128 instances that completed. The other 11 abort first on a
pre-existing `assert(want_lo <= want_hi)` at `src/presolve.c:2127` — the same
eleven `TODO.md` already names under that debt, and the parent aborts on an
identical list. The consequence had never been written down: **no
assert-enabled build can run those 11 instances**, so every assert in the solve
is untested on them, this one included.

**A record's age is now asked before a campaign.** `jaos-measurer` found the
warm records 21 `src/` commits behind while judging this candidate, and
`preflight.sh` only ever read the three netlib ones. It asks every record in
`bench/results/` now, and that is what turned up the other two.

**Residuals mostly improve**: `sctap1` 7.66e-17 → 2.98e-17, `dfl001` 3.01e-13 →
1.9e-13, `bnl1` 8.99e-15 → 7.09e-15, `boeing1` 5.84e-16 → 3.87e-16. Two go the
other way inside the same decade, `degen3` 3.29e-16 → 7.01e-16 and `ganges`
1.03e-14 → 1.17e-14.

**The gate does not reach the failure, and the entry says so.** `pilotnov` is
**bit-identical on the standard set**, on both sides, `optimal` and
`objective=ok`. `jaos-measurer` raised this against the change's own
description and was right to: the defect is reached only under the presolve
reordering D118 refused, and nothing in the 139 reaches it. The source comments
said "on `pilotnov`" without that condition and now carry it.

So the case is not that a gate instance was wrong. It is that the failure is
real, reachable and demonstrated with a negative control in one run; that the
same inexactness is present all over the gate at a harmless size, which is what
34 moved digests and improved residuals are; and that it costs 1.0001x. A
defect that is small on every instance anyone has measured and catastrophic on
the first one that pushes it is repaired rather than bounded.

**What it does not repair.** The 186 lost loans, which are a separate defect in
the same machinery — this makes them harmless wherever a settle runs, because
the cost is restored whatever the record says, and does not explain them. The
loan's size: a `need` of 1e32 on a cost of one is the sign condition being
overwritten rather than repaired, and bounding it is §5a's second candidate,
uncosted. And `pilotnov`'s 30x under D118's candidate: the answer is right now,
the work is not, so D118 stays refused.

**The three baselines were rewritten deliberately after the diff was read and
accepted**, and confirmed by a following gate run.

## D123 — No loan is outstanding when the duals are published, on any of the 128 instances an assert-enabled build can run

**The question.** `numerics-reviewer` raised it while reviewing D122, and it
was kept out of D122 so one change did one thing. `refresh` re-runs
`shift_to_feasible` over every variable when `repair_singular_basis` fired
(`src/simplex.c:1335`). Both `take_best_if_better` and `restore_settled` call
`refresh` **after** their own `repay_shifts`. On that path
`reenter_after_settling` returns with loans back in the costs, and nothing
settles them before `classify_optimum` and `publish`. The published objective
is safe whatever the answer is — `publish` builds it from `m->col_cost` — but
`sol_dual` is a BTRAN of `s->cost` and `sol_redcost` is `s->d`, so both would
carry the loan. Expected: a live defect in published duals, or the suspicion
removed.

**The instrument.** `assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v])`
over every variable, on the OPTIMAL branch of `publish` and only there. A
solve that ends anywhere else never calls `settle_shifts`, is entitled to
carry loans, and publishes four arrays of zeros instead. The cost is compared
as well as the record for D122's reason: a column whose cost moved while its
record cancelled back to zero is the case D121 measured 186 of on `pilotnov`,
and the record alone would not see it. Same pair `settled_objective` asserts.

**The measurement**, `bench/measurements/02-32/`, `EXTRA_CFLAGS=-UNDEBUG` over
all three sets at `J=12`:

| set | answered | aborted | `publish` assert fired |
|---|---|---|---|
| netlib | 83 | 11 | **0** |
| infeasible | 29 | 0 | **0** |
| Kennington | 16 | 0 | **0** |

**It never fires.** The suspicion is removed and the assert stays. All eleven
aborts are the same pre-existing `assert(want_lo <= want_hi)` at
`src/presolve.c:2127`, the standing debt D122 already named; it is reached
before the solve publishes anything, so those eleven say nothing either way.
128 is the count D122 predicted would answer.

**The negative control, because the clean result is the suspicious kind.** A
copy of the tree gets one line on the OPTIMAL path, immediately before
`publish` is called — `s.cost[0] += 1.0; s.shift[0] += 1.0;` — and the first
instance aborts inside `publish` on this assert. The instrument is reached on
the branch it guards and fires on the state it exists to catch. `src/` is read
and never written.

**What was refuted.** Nothing was, and that is the entry. Reading the code
alone, the path is there and nothing on it settles a loan; only the run says
no model in three sets takes it. **This is not a proof that the path cannot be
taken** — the assert stays precisely for that, so a harder model reaching it
stops instead of publishing duals nobody can defend.

**The cost is nothing, and it is checkable.** With `NDEBUG` back on the assert
compiles to nothing: 94, 29 and 16 instances bit-identical to the committed
record, 110 solution digests and 29 infeasibility verdicts unmoved.
`make test`, `make sanitize` clean with the assert live in both.

**Left open, in `TODO.md` §5a.** The 186 lost loans, and the missing bound on
a loan's size relative to the cost it lands on. Neither is touched here.

## D124 — The 186 loans were never lost: 02-29 compared one accumulator against a sum of partial sums

**The question.** 02-29 tallied every lend against every repayment per
variable and found 186 variables on `pilotnov`, under D118's refused presolve
candidate, whose totals differ — the worst by 256. `TODO.md` §5a carried it as
*"186 loans go missing, and nothing explains it"*, and D122 recorded that it
made them harmless without explaining them. Expected: a leak in the shift
machinery, found by attributing each missing loan to a site and a round.

**What the tally could not distinguish.** `g_lent[v]` is one accumulator over
every loan of the whole solve. `s->shift[v]` is reset to zero at every
repayment, so what a repayment hands to `g_repaid[v]` is a partial sum, and
`g_repaid[v]` is the sum of those partial sums. Adding *k* terms in one
accumulator and adding them in segments are not the same number in floating
point, so **`lent != repaid` is what a lost loan looks like and what
re-association looks like**, and 02-29 measured only the difference.

**The discriminator**, `bench/measurements/02-33/`: a third tally accumulating
the same loans the way the repayments are accumulated, and a fourth counter
that checks at every repayment that the segment accumulator equals
`s->shift[v]` exactly — which is the one thing reading the source cannot
settle, because a fourth write to `shift` or a memcpy would break it and grep
does not see a memcpy.

| tally | unbalanced | worst |
|---|---|---|
| one accumulator (02-29's) | 221 | −0.5 at v=1791 |
| by segment | **0** | 0 |

```
shift written elsewhere: 0 mismatch(es)
loans still outstanding:  0
worst one-accumulator v=1791:
    lent=4369593160644541.5  repaid=4369593160644542
    lends=125  repays=6  ulp(lent)=0.5
```

**Nothing is lost.** Every loan is repaid bit for bit, `shift` is written at
exactly three sites, and the worst disagreement in the solve is **one ulp** of
the total: 125 loans in one accumulator against the same 125 in six segments,
on 4.37e15 whose ulp is 0.5. A lost loan of 0.5 would be a coincidence; one
ulp is the signature of re-association. `pilot-ja` and `dfl001` are balanced
on both tallies, and at HEAD `pilotnov` is balanced on both too — no loan on
the shipping trajectory reaches a magnitude where the effect is visible.

The count reads 221 rather than 186 because D122 changed what a repayment
does, which changes the trajectory. The shape did not change.

**The negative control.** `-DJAOS_NEGCTL` drops one variable's loan the moment
it is lent, so the cost keeps the money and the record forgets it. Both
counters find it: `by-segment unbalanced=1, worst=5.01e+30 (v=7)`,
`shift written elsewhere: 11 mismatch(es)`, 269 lends against 0 repaid. The
instrument can find a lost loan and finds none.

**What was refuted, and it is this entry's own premise.** The leak does not
exist. `TODO.md` §5a item 1 asked for site attribution for a defect that was
never there, and building the attribution first would have produced a table of
sites for rounding.

**A number was also wrong in two source comments and in D122.** 02-29's
`cost-drift.txt` reports `moved=67 shift_still_pending=0` — 67 columns whose
cost moved while the record came back to zero. Its `loan-balance.txt` reports
`unbalanced=186`, a different property of a different set of columns. D122 and
two comments in `src/simplex.c` cited 186 for the cost-moved-record-zero
shape. **The number for that shape is 67.** The comments carry 67 now and say
where the wrong one came from. It is the same failure `jaos-record` names: a
figure copied out of the record it belongs to, into prose that then owns it.

**The cost is nothing, and it is checkable.** Comments only: 94, 29 and 16
instances bit-identical to the committed record, 110 solution digests and 29
infeasibility verdicts unmoved. `make test` and `make sanitize` exit 0.

**Left open, in `TODO.md` §5a.** One item, unchanged: nothing bounds a loan
relative to the cost it lands on.

## D125 — No loan swamps a real cost anywhere in the gate, and one lend in six sets `d` to zero on a cost that never moved

**The question.** `TODO.md` §5a's last item asked for a bound on how large a
loan may be relative to the cost it lands on, because a `need` of 1e32 on a
cost of one replaces the model's cost rather than repairing a sign condition
(D121). It named a second defect in passing: `shift_to_feasible` also sets
`s->d[v] = 0.0` unconditionally, which asserts the cost moved by exactly
`need` and is false whenever `need` is below the ulp of the cost. Both were
measured over all three gate sets before any number was proposed
(`bench/measurements/02-34/`).

**The bound is refused, and the ratio is the wrong measure.**

| set | lends | worst \|need\|/\|cost\| | at those magnitudes | both sides > 1e-6, swamped by 1e6 |
|---|---|---|---|---|
| netlib | 1006960 | 1.11e+50 | need 2.78e-17 on cost 2.49e-67 | **0** |
| infeasible | 697766 | 2.36e+22 | need 4.44e-15 on cost 1.88e-37 | **0** |
| Kennington | 2802762 | 8.46e+35 | need 2.81e-08 on cost 3.32e-44 | **0** |

Not one lend on any of the three sets has a loan and a cost that are both
numbers with the loan swamping the cost. Every extreme ratio is a tiny loan
landing on a cost that is already nothing, and 2.49e-67 is not a cost being
overwritten. The largest loan anywhere in netlib is **100.07**, and it lands
on a cost of exactly zero where the addition is exact and the ratio has no
value — 56118 netlib lends are of that kind. **A ceiling on the ratio would
fire on nothing that matters.** D121's 1e32 stands and stays reachable through
D118's refused candidate; no instance in the gate reaches it.

**The second defect is the live one, and it is not rare.**

| set | lends | cost did not move | solves with at least one |
|---|---|---|---|
| netlib | 1006960 | **167816 (16.7%)** | 134 of 188 |
| infeasible | 697766 | 29784 (4.3%) | 12 of 38 |
| Kennington | 2802762 | **400204 (14.3%)** | 28 of 32 |

On one lend in six the cost is unchanged and `d[v]` is set to zero anyway, so
dual feasibility is asserted rather than repaired and the next ratio test
reads a reduced cost that was never computed. This is the shipping
configuration, on 134 of the 188 standard solves. `below_ulp` — the same
question asked on the inputs instead of the outcome — reads 175832 / 33014 /
421872, larger by a few percent, which is what round-to-nearest does with a
`need` between half an ulp and one ulp.

**What was refuted.** The item's own premise. `TODO.md` said none of §5a's
three items is reached by any of the 139 instances; that is true of the
overwrite and false of the fabrication, which fires hundreds of thousands of
times per campaign. Reaching for the ratio bound first would have produced a
constant, a sweep and a `docs/tolerances.md` row for a case that does not
occur.

**The instrument carried the defect it was looking for, and it was caught by
repeating the run.** Two passes over the same tree read 188 solves and then
187. Twelve children share one `stderr` and each wrote its line with fourteen
`fprintf` calls, so two interleaved and the reader dropped the mangled one. It
is one `snprintf` and one `write(2, …)` now, atomic against the shared offset.
The script carries both checks: no mangled line on any of the four logs, and
netlib run twice from the same tree gives a byte-identical aggregate. The
figures above are the ones both original passes agreed on.

**Nothing changed and nothing is proposed.** No source file was touched. What
the evidence supports is a floor on `need` rather than a ceiling — a `need`
below the noise of the sum that produced `d[v]` is not a number, which is
`fp-numerics`' rule and already what `can_move` applies through
`NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v)`.

**Left open, in `TODO.md` §5a**, two preconditions before any of that is code:
what refusing 16% of lends costs, measured on both sides, since that is a
trajectory change on most of the set rather than a no-op; and whether
`column_traffic` may be read on the ratio-test path at all, since it reads
`s->y` — the duals of some basis rather than necessarily the current one —
and is a full column scan in the solver's hottest loop.

## D126 — Refused: the zero D125 called a fabrication is what stops the breach compounding, and removing it costs six orders of magnitude

**The question.** D125 measured `shift_to_feasible` writing `s->d[v] = 0.0` on
167816 of netlib's 1006960 lends where the cost does not move at all, and
called it a fabrication: `d` is `cost[v] - y·M_v` by definition, so the zero
claims the cost moved by exactly `need`. The candidate reads the move back off
the cost and stops when there was none
(`bench/measurements/02-35/candidate.diff`):

```c
const double before = s->cost[v];
s->cost[v] += need;
const double moved = s->cost[v] - before;
if (moved == 0.0)
    return;
s->shift[v] += moved;
s->d[v] += moved;
```

Expected: the same answers, an honest `d`, and a residue too small to matter.

**The argument for it, and where it leaked.** The residue was said to be
bounded — the cost did not move, so `|d|` is below half an ulp of `cost[v]` —
and `admit_candidate` already clamps the ratio-test numerator with
`rnum[k] = dist > 0.0 ? dist : 0.0`. The clamp does cover the **choice**:
`bfrt_walk`, `jm_harris_pick` and `jm_bland_pick` read only `rnum`, `rden` and
`rrange`, so no wrong-signed `d` reaches any of them. It does not cover the
**step**. Both exits of `dual_ratio_test` compute
`*theta_out = s->d[best] / s->alpha[best]` from the raw `d`, and
`admit_candidate` only requires `|alpha| > PIVOT_MIN`, which is `1e-9`. The
division amplifies by up to a billion.

**The measurement**, both binaries built in one run with distinct md5s:

| | picks | wrong-signed | worst \|dist\| past zero | worst \|theta\| from one |
|---|---|---|---|---|
| netlib, HEAD | 477562 | 248 (0.052%) | 4.81e-10 | 8.37e-09 |
| netlib, candidate | 712218 | **8840 (1.24%)** | **5.84e-05** | **2.21e-03** |
| Kennington, HEAD | 435418 | 170 (0.039%) | 5.22e-10 | 5.35e-10 |
| Kennington, candidate | 448132 | **5588 (1.25%)** | 4.59e-10 | 4.31e-10 |

**The half-ulp bound is false, by five orders of magnitude on the breach and
six on the step.** 2.21e-03 arriving at the dual step is a number, not an
artefact. And the trajectory says it from the other side: 712218 ratio-test
picks against HEAD's 477562 on the same 188 solves, 49% more.

**Why the bound was wrong.** It assumed the breach is whatever one lend could
not repair. `update_dual` runs `d[v] -= theta_dual * alpha[v]` every iteration
and then calls `shift_to_feasible`; today's zero resets the breach each time,
so it cannot accumulate. Remove the zero and the next iteration pushes the same
variable further the same way. That is the hazard the function's own header
names in advance: *"a reduced cost pushed a tolerance past zero stayed there,
and the next iteration could push it further."* The header was right and the
candidate did not read it as a claim about accumulation.

**So `d[v] = 0.0` is not a fabrication.** It rounds a quantity to zero when
that quantity is below the resolution of the cost that produced it, which is
what `fp-numerics` prescribes for a sum below the noise of its terms. D125
measured it correctly and this entry corrects what D125 implied about it. The
comment in `shift_to_feasible` is unchanged and stays right.

**The process note, because it decided the outcome.** `numerics-reviewer` was
spawned twice on this diff, as the loop's step 3 requires, and neither instance
delivered a report — both signalled idle without content and were stopped. The
review was done in the main context instead, which is the wrong context by
this repo's own rule, and it is what found the clamp at `admit_candidate` and
then the uncovered division two lines further on. **The finding that refuted
the candidate came from reading the consumers of `d` one by one, not from the
diff.** A campaign was never run: the probe answered it first, for a fraction
of the cost.

**What is left open, in `TODO.md` §5a.**

- `shift[v] += need` still records a loan that was never made, on the same
  16.7% of lends. Recording `moved` is **not** a no-op: `repay_shifts` returns
  whether anything moved and `settle_shifts` skips `compute_duals` and
  `repair_dual_infeasibility` when nothing did, so a phantom loan forces a
  re-pricing that would otherwise be skipped. Its own measurement.
- **At HEAD, 248 netlib picks and 170 Kennington picks already compute the
  dual step from a wrong-signed reduced cost**, worst step 8.37e-09. The clamp
  at `admit_candidate` does not reach the division and nothing else does. A
  standing fact, not a proposal.

## D127 — Refused: the wrong-signed dual step is holding pilot87 up, and clamping it costs 3.228x

**The question.** D126's probe established that both exits of
`dual_ratio_test` compute `*theta_out = s->d[best] / s->alpha[best]` from the
raw `d`, while `admit_candidate` had already clamped the numerator the pick
was made on. At HEAD that reaches **248 of netlib's 477562 picks and 170 of
Kennington's 435418**, worst step 8.37e-09. The code contradicts its own
stated design: the comment above `dual_ratio_test` says an already-infeasible
cost *"blocks at once, and the step that follows repairs it exactly"*, and a
step of `d/alpha` with `d` past zero does not repair it — it carries the
breach into every other reduced cost through `update_dual`, amplified by an
`alpha` required only to exceed `PIVOT_MIN = 1e-9`.

**The change** (`bench/measurements/02-36/candidate.diff`): a `blocking_cost`
helper returning the same number the pick used, read at both exits. A free
nonbasic left alone, because `admit_candidate` calls its distance zero for a
different reason — it may travel either way, so no sign is the wrong one.

**The gate refuses it**, `bench/measurements/02-36/netlib-diff.txt`:

```
94 instances compared: 72 bit-identical, 22 moved, 14 digest change(s)

-- REGRESSIONS --
  WORK   pilot87   18818789905 -> 60754965471   (3.228x)
  RSUB   pilot87   suboptimality bound 2.58e-06 -> 3.59e-05  (13.9x, D91 threshold)
```

`pilot87` runs 108973 iterations against 40246. `25fv47` at 1.109x and
`d2q06c` at 1.069x move the same way inside the bar. Every answer is still
`optimal` with `checker=ok`, so this is a cost and not a wrong answer.

**It is entirely the Harris exit.** Reverting the Bland exit and keeping the
Harris one reproduces the regression to the digit — work 60754965471, iters
108973, digest `93805090cdc362f8`, `25fv47` and `d2q06c` unchanged. Bland's
rule is a rare fallback and contributes nothing.

**Why it costs that much, which is the finding.** Today's tiny wrong-signed
step is not doing nothing. `update_dual` applies `d[v] -= theta_dual *
alpha[v]` to every other reduced cost, so a `theta` of 8e-09 perturbs the
whole dual vector slightly. Clamped to exactly zero the iteration becomes
fully degenerate, with no dual movement at all. `pilot87` is the instance this
project already knows oscillates — D25 and D89 were written for it and D74
measured 2.372x on its iterations from a related change. **The accidental
perturbation is what keeps it moving.**

That is the second candidate in a row where the apparently-sloppy thing is
load-bearing, on the neighbouring line of the same machinery. D126 was the
first. The pattern is worth naming: **this loan-and-shift code looks careless
and is not, and a repair argued from the source alone has now been wrong
twice.** Both were caught by measuring, and the second was caught only by the
gate — no probe would have predicted 3.228x.

**What was refuted.** Not the inconsistency, which is real and stays on the
record with its size. What is refuted is a zero as the replacement. It does
not follow that no replacement works: a step small enough to keep the
perturbation and signed correctly would be a new constant, needing a sweep on
both sides and a row in `docs/tolerances.md`. Nothing here proposes one.

**Process.** `numerics-reviewer` was unavailable for this diff as it was for
D126 — see that entry. The review was done in the main context and found the
`-0.0` question (`s->d[leaving] = -theta_dual` with a zero step; `published`
normalises it and no comparison distinguishes it) and confirmed nothing
divides by `theta_dual`. Neither of those was what refused the change.

**Left open, in `TODO.md` §5a.** The phantom loan in the record, unchanged
from D126. And this inconsistency, now with a measured price for the obvious
repair.

## D128 — The shift record says what the cost moved by, and it skips two re-pricings out of 290

**The question.** `shift[v] += need` records a loan that was never made
whenever `need` is below half an ulp of the cost, because the addition leaves
the cost where it was. D125 measured that on **167816 of netlib's 1006960
lends, 16.7%**, and 14.3% of Kennington's. §5a's last open item.

**What made it not free**, and it is one line in `settle_shifts`:

```c
if (!repay_shifts(s))
    return;
```

`repay_shifts` reports whether anything was outstanding and `settle_shifts`
skips `compute_duals` and `repair_dual_infeasibility` when nothing was, so a
phantom loan forces a re-pricing. The two readings differ only when **every**
outstanding record is phantom: a column whose cost really moved has
`cost != cost0`, which both see.

**The blast radius was measured before the change, not after**
(`bench/measurements/02-37/`), with a shadow record accumulating `moved`
beside the real one:

| set | solves | `repay_shifts` calls | flips | phantom lends |
|---|---|---|---|---|
| netlib | 188 | 258 | **2** | 167816 of 1006960 (16.7%) |
| infeasible | 38 | **0** | 0 | 29784 of 697766 (4.3%) |
| Kennington | 32 | 32 | **0** | 400204 of 2802558 (14.3%) |

Two calls out of 290 across the whole gate. The infeasible set never reaches
`settle_shifts`, which is D123's fact from the other side. The phantom
percentages reproduce D125 exactly.

**The change.** `s->shift[v] += s->cost[v] - before` instead of `+= need`.
`s->d[v] = 0.0` is untouched: removing it was measured and refused (D126),
because the breach then compounds across iterations.

**The gate agrees with the prediction, instance for instance:**

| set | bit-identical | moved | digests moved |
|---|---|---|---|
| netlib (94) | **93** | 1 | **0** |
| infeasible (29) | **29** | 0 | 0 |
| Kennington (16) | **16** | 0 | 0 |

```
WORK   fit1d   1694739 -> 1681313   (0.9921x)
```

188 solves is 94 instances run twice, so two flips on two solves is one
instance. `fit1d` is it, and it costs **0.9921x** — the re-pricing that no
longer runs. **110 solution digests and 29 infeasibility verdicts unmoved.**
`make test` and `make sanitize` exit 0. `bench/netlib.baseline` was rewritten
deliberately for that one line and a following run reads `0 regressed, 0
improved, 0 new`.

**What was refuted.** Nothing was tried and rejected here. What the probe
refuted in advance was the assumption that a 16.7% change to the record is a
16.7% change to the solve: it is 0.7% of the calls that read it, because the
reading is a disjunction over every column and one real loan carries it.

**What it does not claim.** `shift[v]` still cannot be read as how far a cost
moved. It accumulates separately from `cost`, so the two round apart at the
first lend large against the cost (D122), and D124 showed their totals differ
by re-association alone. This removes one specific dishonesty: a record of a
move that provably did not happen.

**Process.** `numerics-reviewer` was unavailable, as on D126 and D127. The
review was done in the main context. §5a is now closed.

## D129 — The basis-count defect costs 25% of netlib and 55% of Kennington their warm start outright

**The question.** `bench/results/warm.txt` and its Kennington sibling were
last written at `44c0ef6`, **21 `src/` commits ago**, which `TODO.md` carried
as a standing debt: a diff against them reports the whole of presolve and
cannot isolate a later change. The gate was green and the tree clean, so they
were rewritten. Expected: a ratio that moved because presolve shrank the cold
solve it is divided by.

**The headline got much worse than that.**

| | old record | new record |
|---|---|---|
| netlib, work units warm/cold, geometric mean | 0.0164 | **0.0696** |
| netlib, worst instance | `afiro` 0.5768 | `80bau3b` **1.0000** |
| Kennington, work units warm/cold | 0.0041 | **0.0873** |
| Kennington, worst instance | `pds-06` 0.0329 | `cre-a` **1.0000** |

A ratio of exactly 1.0000 is a warm re-solve doing **bit-identical** work to
the cold one — `80bau3b` at 3511 iterations and 64249140 units either way,
`dfl001` at 21985 and 2744690896. **26 of netlib's 92 measured instances read
it, and 6 of Kennington's 11.** No instance did in the old record. Three of
netlib's 26 are branches taking zero iterations on both sides and identical
for a legitimate reason.

**Where it goes**, `bench/measurements/02-38/`. The driver solves three times
per instance — anchor, warm, cold after `jaos_clear_basis` — so calls 0 and 2
are expected to find no basis and call 1 is the question:

| set | call 1 accepted | count mismatch | no basis stored |
|---|---|---|---|
| netlib | 66 | **23** | 0 |
| Kennington | 5 | **6** | 0 |

**23 and 23, 6 and 6.** The count mismatches equal the non-trivial 1.0000
instances exactly, on both sets, so the attribution is measured rather than
inferred. `no-basis` never fires on the warm solve, so nothing is lost for any
other reason.

`nbasic != s->nrow` is the standing debt `TODO.md` already carries with a
named minimum case and a pinned test. What it did not carry is a price. **The
price is 25% of the standard set and 55% of Kennington.**

**The mismatch is usually by one and is not always short.** Thirteen of
netlib's 23 read `nbasic = nrow - 1`. Three publish *more* basic variables
than rows — by 10, 11 and 2 — which is a different shape from a missing one
and is not explained by the same argument. Kennington's worst is `nrow=3074
nbasic=3051`.

**What was refuted.** The expectation this entry started with. Both
explanations — the defect, and presolve shrinking the denominator — predict a
worse ratio, so they were separated by taking the geometric mean over the
instances that kept their warm start:

| | all | kept the warm start | lost it |
|---|---|---|---|
| netlib | 0.0696 | **0.0244** (66) | 1.0000 (26) |
| Kennington | 0.0873 | **0.0047** (5) | 1.0000 (6) |

Kennington's survivors read 0.0047 against the old record's 0.0041, unchanged
for practical purposes; netlib's read 0.0244 against 0.0164, worse but the
same order. **The jump is the lost warm starts and almost nothing else.**

**It is not a wrong answer and not a gate regression.** The fallback is
correct by construction: `build_warm_basis` refuses a basis it cannot trust,
and the cold start is always right. No checker and no digest reads a starting
status, which is exactly why 21 commits passed with nobody noticing — and why
`preflight.sh` asking every record its age is what surfaced it (D122). The
three gate sets are unaffected.

**Left open, in `TODO.md`.** The repair itself, which now has a price to be
weighed against. And the three instances publishing more basic variables than
rows, which the minimum case does not describe.

## D130 — Six instances publish more basic variables than rows, not three, and the three with no basis at all are named

**The question.** D129 counted 23 netlib instances losing their warm start to
`nbasic != s->nrow` and printed each mismatch's dimensions, but could not name
them: the driver forks a child per instance and twelve share one stderr, so a
name printed by the driver cannot be matched to a line printed by the library.
Run at `-j 1` the driver does not fork, and the name can be printed
immediately before the warm solve.

**The full tally**, netlib's 92 measured instances
(`bench/measurements/02-39/`):

| outcome on the warm re-solve | count |
|---|---|
| accepted | 66 |
| short (`nbasic < nrow`) | 17 |
| **over (`nbasic > nrow`)** | **6** |
| nothing was stored at all | 3 |

`17 + 6 = 23` is D129's mismatch count and `23 + 3 = 26` is its count of
instances at a work ratio of exactly 1.0000. **Both totals hold; the split did
not.**

**Correction one. It is six over, not three.**

```
80bau3b    nrow=2022  nbasic=2043  over by 21
finnis     nrow=399   nbasic=411   over by 12
standmps   nrow=407   nbasic=418   over by 11
standata   nrow=300   nbasic=310   over by 10
vtp-base   nrow=52    nbasic=54    over by 2
boeing1    nrow=298   nbasic=299   over by 1
```

D129 named three, over by 10, 11 and 2. **The reading came from a truncated
terminal output** — `tail -40` on a run that printed 23 mismatch lines — so
the first seven were never seen. Its three are real and are the last three of
six. The shape spans 1 to 21, so it is not a small-model artefact: `80bau3b`
is over by 21 on 2022 rows.

Sixteen of the seventeen short are short by exactly one; `maros` is short by
five.

**Correction two. `no-basis` does fire, on three, and it explains them.** D129
said it never fires on the warm solve. It fires on `pilotnov`, `scrs8` and
`share1b` — exactly the three it set aside as *"identical for a legitimate
reason"*. The reason is now measured rather than assumed: **the anchor solve
stored no basis at all**, so there was never anything to start from. Their
1.0000 is the absence of a warm start rather than the loss of one. D129's 23
is unaffected, because those three were already outside it.

**What was refuted, and it was this probe twice before it was right.** Both
failures read as clean results:

- **A call counter does not survive `-j 1`.** The first version identified the
  warm solve as the process's second `build_warm_basis` call. At `-j 12` the
  driver forks and the counter resets per instance; at `-j 1` it does not
  fork, so only the run's first instance ever had a second call. It reported
  `seen=1`.
- **The marker went on the cold solve.** The second version inserted the name
  before the second `st = jaos_solve(m);`, which is the cold one — the anchor
  above them is written `if (jaos_solve(m) != JAOS_OK)` and does not match. It
  reported 92 instances, every one `no-basis`, every field empty.

The script now carries a **proportion** canary rather than a presence one:
every warm solve reporting `no-basis` means the marker is on the wrong call,
while three of ninety-two reporting it is a fact about those three.

**Left open, in `TODO.md`.** The repair, with the shape this adds to D129's
price: **a repair aimed at the missing-one case answers sixteen of the
twenty-three**, and says nothing about `maros`'s five short or the six that
are over.

## D131 — `jaos_basis` publishes something that is not a basis on 70% of solves, and presolve's mapping is exact

**The question.** D130 named six instances whose stored basis has more basic
variables than the reduced model has rows. `src/presolve.c:1635` maps a
starting basis into reduced indices and its comment contemplates one direction
only — *"dropping it undercounts the basic total … safe, never wrong, only
colder"*. Over-counting is contemplated nowhere, and the obvious explanation
was that removing a row whose logical is NONBASIC costs a basis position and
no basic variable.

**That explanation is refuted** (`bench/measurements/02-40/`). Written as an
identity it reads `over = rows_removed - drop_row - drop_col - adj`, and it
**fails on 61 of 88 solves**: `80bau3b` is over by 21 where it predicts −5.

**The identity that holds is stated on what the mapping actually reads:**

```
nbasic_out = basic_in - drop_row - drop_col - adj      0 failures of 88
```

The mapping is arithmetically exact. The first formula assumed
`basic_in == nr`, which is what a basis on the caller's model has by
definition, and that assumption is the whole difference:

| instance | over by | `basic_in` | `nr` | already off by |
|---|---|---|---|---|
| `80bau3b` | 21 | 2288 | 2262 | **+26** |
| `finnis` | 12 | 512 | 497 | **+15** |
| `standata` | 10 | 344 | 359 | **−15** |
| `standmps` | 11 | 456 | 467 | **−11** |
| `vtp-base` | 2 | 164 | 198 | **−34** |
| `boeing1` | 1 | 351 | 351 | 0 |

**Two mechanisms, and `boeing1` is the control that separates them.** Its
stored basis counts exactly right and the mapping still lands it over by one,
which is the original hypothesis and accounts for one of the six. The other
five inherit a count that was already wrong before presolve saw it.

**Confirmed on the gate, on the caller's own model after postsolve**
(`bench/measurements/02-41/`) — the array `jaos_basis` returns verbatim:

| set | optimal solves | count exact | **wrong** | over | under | worst over | worst under |
|---|---|---|---|---|---|---|---|
| netlib | 188 | 56 | **132 (70%)** | 72 | 60 | 596 | 169 |
| Kennington | 32 | 8 | **24 (75%)** | 8 | 16 | **12104** | 406 |
| infeasible | 0 | — | — | — | — | — | — |

The infeasible set publishes no basis, which is correct: `publish` memsets the
arrays on every path but OPTIMAL and `jaos_basis` refuses unless the status is
OPTIMAL. `12104` too many on a Kennington model is not an off-by-one; it is a
different object from a basis.

**It is a defect and not a convention, because the file says so twice.**
`src/model.c`: *"a model with n rows needs n basic variables … it is
structural — no later event makes a wrong count right"*, and *"Checked here:
… that exactly num_row of them are basic. Those are structural."*
`jaos_set_basis` refuses a basis whose count is wrong; `basis_survives_or_goes`
clears one that becomes wrong; **`jm_model_remember_basis` checks nothing** and
`memcpy`s the published statuses straight in. **The solver publishes, and
stores for its own next solve, something it would refuse from a caller.**

**No answer is wrong and the gate is green.** Values, objective, duals,
reduced costs, the independent checker and every solution digest are
unaffected: the basis sits beside the answer and nothing in the solve reads it
back except `build_warm_basis`, which refuses it. That is why 21 `src/`
commits passed with nobody noticing (D129).

**And it is the source of D129's lost warm starts rather than a second
defect.** `build_warm_basis` rejecting the count is the symptom.

**Left open, in `TODO.md`**, and it is two questions rather than one:

- **`publish` and postsolve should produce a basis.** That is where the count
  is decided; presolve's mapping is exact and nothing there needs repair.
- **`jm_model_remember_basis` should check.** A one-line guard makes the
  invariant honest and on its own changes nothing measurable — a stored basis
  failing the count is already rejected by `build_warm_basis`, so clearing it
  earlier reaches the same cold start. It belongs with the repair above rather
  than instead of it.

The standing debt names one postsolve family and a minimum case of one status.
**The measurement is 132 solves and a worst error of 12104**, so that case is a
corner of this rather than a description of it.

## D132 — `SINGLETON_ROW` restores half of netlib's removed rows and leaves 78% of them nonbasic

**The question.** D131 established that the published basic count is wrong on
132 of netlib's 188 optimal solves and that presolve's basis *mapping* is
exact, which puts the defect in postsolve. Which family?

**The arithmetic a correct postsolve satisfies.** The reduced solve leaves
exactly `rrow` basic variables and `jm_postsolve_expand` copies those onto the
surviving rows and columns, so the replay must add exactly one basic variable
per row it restores.

**Two things are established** (`bench/measurements/02-42/`).

**Every removed row and column is claimed by a record**: `ORPHANED` reads
`0/0` on all 204 solves. The defect is not an entity nobody restores.

**The row-restoring families balance and one does not:**

| family | basics | rows restored | drift | solves off |
|---|---|---|---|---|
| `REDUNDANT_ROW` | 1138 | 1138 | 0 | 0 |
| `FORCING_ROW` | 3672 | 3672 | 0 | 0 |
| `EMPTY_ROW` | 1758 | 1758 | 0 | 0 |
| `IMPLIED_FREE_COL` | 2088 | 2088 | 0 | 0 |
| **`SINGLETON_ROW`** | **1898** | **8622** | **−6724** | **130 of 172** |

Kennington is the same shape and larger: 81646 rows restored, 29174 basic,
**−52472**, on 24 of 32 solves. **`SINGLETON_ROW` restores half of netlib's
removed rows and leaves 78% of them nonbasic.**

**The basic-ness is not lost, it moves to a column.** 9770 restored columns
come out BASIC on netlib where the rows are short by 6724, and
`JM_PS_SINGLETON_ROW`'s own replay writes
`orig->sol_col_status[j] = JAOS_BASIS_BASIC` for the column its row folded
into (`src/presolve.c:2037`). That is the migration.

**What is NOT established, and it is the probe's fault.** Which family wrote
each of those column basics. The probe attributes an entity to the record that
*restored* it, read off the arena, and that is not the record that *wrote its
status last*: `SINGLETON_ROW` assigns a status to a column a different record
restored, so a column counted under `FIXED_COL` may carry a status
`SINGLETON_ROW` wrote. **The row numbers do not have this problem** — no
family writes a row another family restored — which is why the `SINGLETON_ROW`
deficit stands and the column split does not. Settling it needs a last-writer
probe, and that is the next step rather than a guess.

**Two probe errors, both caught before publication.** Ownership was read
forwards while the replay is strictly LIFO (D-07), so the lowest arena index
writes last; walking forwards records the first writer instead. Fixing it
returned **identical** numbers, which is itself the finding that no entity is
claimed by two records. And a helper placed before the includes does not
compile, because the tag enum is declared in `jaos_internal.h`.

**Nothing changed and nothing is proposed.** No source file was touched, and
no repair is costed. The gate is unaffected: this is published output that
nothing in the solve reads back except `build_warm_basis` (D131).

**Left open, in `TODO.md`.** The last-writer probe, then the repair. The
repair now has a family to aim at and a number to beat.

## D133 — The two singleton families are the whole of the basis-count error, and the sum closes exactly

**The question.** D132 attributed each restored entity to the record that
*restored* it and said plainly that this is not the record that *wrote its
status last*, because `JM_PS_SINGLETON_ROW` assigns a status to a column a
different record restored. Its row numbers stood and its column split did not.
This settles the split (`bench/measurements/02-43/`).

**The instrument.** Every access to `sol_col_status` and `sol_row_status`
inside `ps_replay_one` — six and eight sites, all with a simple `[i]` or `[j]`
index — goes through a helper that stamps the arena index currently replaying
and returns the slot, so the call site stays an assignment. **The writer is
recorded at the moment of the write**, so a branch not taken records nothing.
The rewrite asserts it found exactly 6 and 8 sites, so a future site it misses
fails the run rather than being dropped.

| last writer | netlib basics | rows | drift | Kennington basics | rows | drift |
|---|---|---|---|---|---|---|
| `REDUNDANT_ROW` | 1138 | 1138 | 0 | 296 | 296 | 0 |
| `FORCING_ROW` | 3672 | 3672 | 0 | 10272 | 10272 | 0 |
| `EMPTY_ROW` | 1758 | 1758 | 0 | 11150 | 11150 | 0 |
| `IMPLIED_FREE_COL` | 2088 | 2088 | 0 | — | — | — |
| **`FIXED_COL`** | **0** | **0** | **0** | **0** | **0** | **0** |
| **`EMPTY_COL`** | **0** | **0** | **0** | — | — | — |
| **`SINGLETON_COL`** | 5902 | 0 | **+5902** | 482 | 0 | **+482** |
| **`SINGLETON_ROW`** | 6624 | 8622 | **−1998** | 106818 | 81646 | **+25172** |
| survivors (unwritten) | 148294 | 148294 | 0 | 411412 | 411412 | 0 |

**The sum closes.** netlib `5902 − 1998 = +3904` against published basics
169476 for 165572 rows, off by 3904. Kennington `482 + 25172 = +25654` against
540430 for 514776, off by 25654. **Nothing is unaccounted for**, and the
survivors balance exactly, so the reduced solve's basis is a basis and nothing
upstream of the replay is wrong.

**Three corrections to D132.**

- **`FIXED_COL` and `EMPTY_COL` contribute exactly nothing.** D132 read 3620
  and 18; both were misattribution. `src/presolve.c:1846` writes `AT_LOWER` or
  `AT_UPPER` and never `BASIC`, which is what the code said all along and now
  what the measurement says.
- **`SINGLETON_ROW` on netlib is −1998, not −6724.** It writes 6624 basics for
  its 8622 rows, not 1898.
- **`SINGLETON_COL` is the larger contributor on netlib**, +5902 on 96 of 172
  solves, and D132 did not name it at all.

**The mechanism, now readable.** `JM_PS_SINGLETON_COL`
(`src/presolve.c:2131`) writes
`(xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC`, and **its row
survives** — the record's `index` names a row that stays, relaxed. So it
restores a column, marks it `BASIC` whenever the value is not at `hi`, and no
row comes back to pay for the basis position. That is +1 per firing and the
whole of netlib's `SINGLETON_COL` drift.

`JM_PS_SINGLETON_ROW` restores a row and writes both that row's status and the
status of the column its row folded into. **The sign differs between the
sets** — −1998 on netlib, +25172 on Kennington — so a repair has to handle
both directions.

**What was refuted.** D132's column split, which D132 had already marked as
unestablished for exactly this reason. Flagging it is what made the correction
cheap: the row numbers were reusable and only the split had to be redone.

**Nothing changed and nothing is proposed.** No source file was touched.

**Left open, in `TODO.md`.** The repair, with two families, an exact
per-family price, and a closing sum for any candidate to be checked against.
`SINGLETON_COL`'s shape is one line of code; `SINGLETON_ROW` changes sign
between the sets and needs its branches counted first.

## D134 — The pair sums to one only by accident, and the repair is a swap rather than a status

**The question.** D133 traced the whole published basic-count error to
`SINGLETON_COL` and `SINGLETON_ROW`, and recorded that `SINGLETON_ROW` changes
sign between the sets — −1998 on netlib, +25172 on Kennington — so its
branches had to be counted before anything was proposed.

**`SINGLETON_ROW` decides two things with two tests that never look at each
other.** It restores one row, so it is worth exactly one basic variable. It
sets the column `BASIC` only when `!zero_works && this_row_owns`, and it sets
the row `AT_LOWER`/`AT_UPPER` when the activity lands exactly on a bound and
`BASIC` otherwise. So `drift = (row BASIC) + (column set BASIC) − 1`, and only
two of the four combinations are right
(`bench/measurements/02-44/`):

| combination | drift each | netlib | Kennington |
|---|---|---|---|
| row at a bound, column not set | **−1** | 2524 | 3886 |
| row at a bound, column set BASIC | 0 | 4200 | 48586 |
| row BASIC, column not set | 0 | 1372 | 116 |
| row BASIC, column set BASIC | **+1** | 526 | 29058 |
| | **net** | **−1998** | **+25172** |

**The sign flip is which wrong combination dominates**: netlib is mostly the
first, Kennington mostly the last. **A repair that handles one direction fixes
one set and worsens the other.** 6726 of netlib's 8622 firings are already
right, and 48702 of Kennington's 81646.

**`SINGLETON_COL` is wrong every time it publishes BASIC**, because its row
survives and nothing is restored to pay for the basis position: 5902 of
netlib's 17234 firings (34%) and 482 of Kennington's 602 (80%).

**The canary closes.** `+3904` and `+25654`, identical to D133's per-set sums
and to the published error. Every basic in the wrong place is traced to a named
branch of a named family.

**What this refutes is the obvious repair.** "Stop writing BASIC" does not
work: `AT_LOWER` and `AT_UPPER` are claims that the variable rests on that
bound, and `SINGLETON_COL`'s `BASIC` branch is reached precisely when it rests
on neither. **A strictly interior variable cannot be nonbasic.** The status is
not the error — the basis has one member too many, and correcting it means
taking a different variable *out*.

So this is a **swap**, not a status choice: mark the interior column basic and
move some row's logical out of the basis, chosen so the result is still a
basis. Nothing in postsolve does that today. The same argument applies to
`SINGLETON_ROW`'s "row BASIC, column BASIC" combination; its "row at a bound,
column not set" combination is the opposite problem, one member too few, and
needs a variable brought *in*.

**Nothing changed and nothing is proposed.** No source file was touched.

**Left open, in `TODO.md`.** The repair, with two families, four named
branches, an exact count per branch per set, and a closing sum for any
candidate to be checked against.

## D135 — The exchange the reduction suggests is available and valid on 97% of firings

**The question.** D134 established that `JM_PS_SINGLETON_COL` publishing a
`BASIC` column is a basis one member too large rather than a status error: the
column lands strictly inside its own bounds, so it cannot rest on one and
cannot be nonbasic, and its row **survives**, so nothing is restored to pay for
the position. The exchange the reduction itself suggests is that the column
enters and the logical of the row it was substituted out of leaves — which is
what `JM_PS_IMPLIED_FREE_COL` already does, and that family reads drift 0
(D133). Two things must hold: the partner must be in the basis, and the row
must rest on a bound or making its logical nonbasic claims a bound the row is
not on.

**Both hold, nearly always** (`bench/measurements/02-45/`):

| | netlib | Kennington |
|---|---|---|
| firings publishing a `BASIC` column | 5902 | 482 |
| row logical `BASIC` — partner available | 5822 (98.6%) | **482 (100%)** |
| of those, row activity exactly on a bound — **swap valid** | **5714 (96.8%)** | **482 (100%)** |
| partner available, row not on a bound | 108 | 0 |
| row logical already at a bound — no partner | 80 | 0 |

The canary closes: 5902 and 482 are D134's counts exactly.

**The reading that had to be redone, and it inverted the answer.** The first
pass judged tightness *inside* the replay and read **0** rows on a bound,
which would have said the swap is never valid and killed the design. **A row's
activity is not final until every record touching it has replayed** —
`ps_row_add` accumulates into it — so that pass was comparing partial sums
against bounds. The probe now tallies per row during the replay and classifies
afterwards. The number went from 0 to 5714.

**What is left open, and it is why nothing lands yet.** A repair handling the
5714 leaves netlib wrong by **188** — 108 firings where the partner is in the
basis but the row is not on a bound, and 80 where the logical is already out —
so the closing sum does not reach zero. Those two shapes need their own answer.

**And `SINGLETON_ROW` is untouched by this.** D134's four combinations stand:
netlib is dominated by "row at a bound, column not set" (2524, one member too
few) and Kennington by "row BASIC, column BASIC" (29058, one too many). A
family short a member needs the mirror exchange — a variable brought *in* —
and no candidate has been measured for it.

**Nothing changed and nothing is proposed.** No source file was touched.

## D136 — The singleton row's rule falls out of its own dual, and the defect is a status decided on a partial activity

**The question.** D134 counted `JM_PS_SINGLETON_ROW`'s four combinations and
found two wrong, with the sign differing between the sets, and said the
branches had to be understood before a repair. D135 handled `SINGLETON_COL`
and left this family without a candidate.

**The rule is not a choice.** The case already computes both halves of
complementary slackness: `y_i = 0.0` when `zero_works || !this_row_owns` and
the column is left alone, `y_i = d0 / rec->coef` otherwise and the column is
set `BASIC`. A basic logical requires a zero dual, so `y_i == 0` means the
row's logical may be basic and `y_i != 0` means it must be nonbasic.

**Measured against what is published, the rule agrees with exactly the two
balanced combinations and disagrees with exactly the two wrong ones**
(`bench/measurements/02-46/`):

| | netlib | Kennington |
|---|---|---|
| rule agrees with what is published | 5572 | 48702 |
| rule wants `BASIC`, published on a bound (D134's −1) | 2524 | 3886 |
| rule wants a bound, published `BASIC` (D134's +1) | 526 | 29058 |

5572 is D134's 4200 + 1372 and 48702 its 48586 + 116. Derived, not fitted.

**The −1 case is free.** A basic variable is allowed to sit exactly on a
bound — that is degeneracy — and the dual on that branch is exactly `0.0`,
which is what a basic logical requires.

**The +1 case is a status decided too early.** The rule wants a bound where
`BASIC` is published, and the case marks `BASIC` precisely because the
activity matched no bound. The gap to the nearest bound, relative to the row's
own traffic:

| | netlib | Kennington |
|---|---|---|
| **exactly 0** | **486** | **29058** |
| 1e-16 | 40 | 0 |

**The rows are on their bounds; the test is not.** The case assigns
`orig->sol_row[i] = ps_published(rec->coef * xv)` — its own term alone — and
compares that against the row's bounds immediately. Records replaying later
add through `ps_row_add`, folded in at the end of `jm_postsolve_expand`. The
test sees a partial sum. **Deciding the status after the replay closes the
whole +1 combination** bar 40 netlib firings.

**What was refuted, and it was this probe's own first two readings.** Reading
the gap *inside* the replay gave 498 of 526 at a relative gap of order **1.0**,
which would have concluded that the published point violates complementary
slackness — a much larger and false claim. And the histogram clamped at 1e-8
and was labelled one decade optimistic, so the first corrected pass still could
not separate rounding from a real gap. **This is the third time in three probes
that reading a row activity during the replay produced a wrong number** (D135
was the first).

**Nothing changed and nothing is proposed.** No source file was touched.

**Left open, in `TODO.md`.** The repair now has a shape: decide the singleton
row's status after the replay, publish the row `BASIC` when `y_i == 0`, and
answer the 40 firings at 1e-16 plus `SINGLETON_COL`'s 188 from D135. Any
candidate is checked against +3904 on netlib and +25654 on Kennington, which
must go to zero.

## D137 — The counting rule is published, and HiGHS turned off the family that costs us 5902

**The question.** D131 to D136 derived a basis-recovery rule from scratch and
measured it. Before building anything on it, is it the published one?
`literature-scout` searched and **could not open a single PDF** — its report
says so at the top and marks its per-family table as derivation. Its headline
was "no published source states the basis-size invariant as a counting rule".

**The thesis was then read directly**, `pdftotext -layout` in WSL, and the
headline is wrong. Galabova 2023 (`10.7488/era/2974`, University of Edinburgh)
states all of it, in `docs/research/postsolve-basis-recovery.md`:

> a point returned by postsolve must also be a basic feasible solution (BFS) in
> order to hot-start the simplex algorithm

> **At each step of postsolve where a new row is introduced, a variable must be
> identified as basic.**

> Basic variables can have primal values between their lower and upper bounds
> but must have a zero dual value. Nonbasic variables must be at a bound but
> can have nonzero duals.

> If the eliminated variable is strictly between bounds it must be ensured that
> it is basic in the postsolved problem.

**That is D132's accounting identity, D136's complementary-slackness rule and
D134's interior-implies-basic argument, all three, in print.** Every one was
derived here first and independently; none needs restating as this project's
own invention.

**What the literature adds that was not derived.** HiGHS does not solve for the
assignment; it attempts one and falls back: *"an attempt is made to set it to
basic. If that assignment of values is infeasible, the remaining column `x_j`
is selected for the basis."* And postsolve is followed by re-optimisation —
*"Additional simplex iterations after postsolve ensure that the solution
returned to the user is feasible"* — so **the bar is a valid starting basis,
not the optimal one**: `num_row` members and nonsingular, without having to
reproduce the duals. JAOS's only consumer is `build_warm_basis`, which
re-optimises anyway.

**And the family that costs the most here was measured and disabled there.**

> The zero cost column singleton rule was added to HiGHS, however, enabling it
> led to a reduction in elimination counts for some test problems, and it was
> disabled by default. … Thus confirming that the zero cost column singleton
> rule should not be included in the default presolver list.

That is `JM_PS_SINGLETON_COL`, which D133 measured at **+5902 on netlib**, the
larger of the two contributors. **It is not an argument to disable it here** —
D95 and D106 are this project's own measurements of its value and they say
something different — but it is a published datum about the same rule and it
belongs beside them.

**What was refuted.** The scout's own headline, by reading the document it
could not open. The lesson is in the tooling rather than the agent: the memory
note saying this machine has no PDF tooling was **stale**. `pdftotext` 24.02.0
is installed in WSL and extracted 2542 usable lines in seconds. The scout was
told the same thing and worked around it for a whole search.

**No source carries a per-family basis table.** Brearley/Mitra/Williams 1975,
Tomlin & Welch 1983, Fourer & Gay 1994, Andersen & Andersen 1995, Gondzio 1997
and Achterberg et al. 2020 recover values and duals and assign no status. The
table in `docs/research/postsolve-basis-recovery.md` is derived and is labelled
so.

**Nothing changed and nothing is proposed.** No source file was touched.

**Left open, in `TODO.md`.** The repair, unchanged in shape by this, and now
with the published rules to check it against. One new lead nobody has read:
Tomlin & Welch, *A pathological case in the reduction of linear programs*, ORL
1983, `10.1016/0167-6377(83)90036-6`.

## D138 — Every under-count is gone, Kennington's worst error falls 100x, and the sum was the wrong target

**The change.** `ps_singleton_row_status` decides a restored singleton row's
status from its own dual and its **final** activity, in a second pass after
the replay, on both postsolve paths. The replay no longer decides it. Two
rules, both derived in D136 and both published (D137): `y_i == 0` → `BASIC`,
because the replay publishes a zero dual exactly when it leaves the folded
column alone; `y_i != 0` → the row rests on a bound, so `AT_LOWER`/`AT_UPPER`
from the final activity.

**What it did** (`bench/measurements/02-47/`):

| | before (02-41) | after |
|---|---|---|
| netlib, exact count | 56 | **88** |
| netlib, wrong | 132 | **100** |
| netlib, under-counting | 60 | **0** |
| Kennington, exact count | 8 | **24** |
| Kennington, wrong | 24 | **8** |
| Kennington, under-counting | 16 | **0** |
| Kennington, worst error | +12104 / −406 | **+119** / −0 |

**Every under-count on both sets is gone**, and Kennington's worst published
error falls from 12104 to 119.

**The gate is bit-identical on all three sets** — 94, 29, 16 — which is the
proof that no value moved, and also the reason the gate cannot judge this:
`bench/run.c` states that the digest covers x and y and not the basis, so a
change that moves only a status is invisible to all three sets. `make test`
and `make sanitize` exit 0.

**What was refuted: the target this project had set itself.** netlib's total
went from **+3904 to +5942** — worse — while every other measure improved. The
difference is exactly +2038, which is D134's `SINGLETON_ROW` net of −1998
removed plus the ~40 firings that still fall through. **The under-count was
cancelling part of the over-count.** `TODO.md` asked for "the closing sums …
must go to zero", and that target is satisfied by a change making both halves
worse in equal measure. It is dropped. **The measure is the count of solves
publishing a wrong basis**: 132 → 100 and 24 → 8.

**And the first attempt was refuted by a test written to catch it.**
`ps_replay_one` is shared by both postsolve paths; the first version removed
the status write from the replay and added the second pass to
`jm_postsolve_expand` only, so on the `jm_postsolve_solved` path the status
fell back to that path's `memset`, which is `BASIC`.
`test_singleton_col_between_two_removals_solved_path` pins the basic count as
a change detector, reads 3, and carries a comment predicting the repair takes
it to 2. It read **4** and failed at once, and its own model comment says why:
*"Every column leaves, so `rcol == 0` and postsolve runs on the
`jm_postsolve_solved` path."* That test still reads 3, so this change is
neutral on it; its 3 comes from a different defect the same comment names.

**Process.** `numerics-reviewer` was unavailable, as on D126, D127 and D128.
The review was done in the main context: the second pass runs in forward order
with one record per row so nothing depends on order (D8), `ps_restore_index`
is applied exactly as the replay applies it so the fault-injection build still
moves both together, `ps_published` normalises a negative zero so the
`== 0.0` test is not sign-sensitive, and no tolerance, libm call or
`jm_work_add` was added.

**Left open, in `TODO.md`.** `SINGLETON_COL`, which is the whole of the +5942
and +482 remaining and has a rule from D135 that is valid on 96.8% and 100% of
firings. And the 40 netlib singleton rows whose final activity misses its
bound by about 1e-16 of the row's traffic.

## D139 — Kennington publishes a valid basis on every solve, and netlib's worst error falls from 596 to 23

**The change.** `ps_singleton_col_swap` performs the exchange the reduction
owes. A restored cost-0 bounded column singleton that lands strictly inside
its own bounds must be `BASIC` — a nonbasic variable rests on a bound and this
one rests on neither — but its row **survives**, so nothing was restored to
pay for the basis position. The swap takes the row's own logical out. It runs
in the same second pass as D138's, after the replay, on both postsolve paths.

**The partner is forced, not chosen.** If the column came back interior, the
reduced activity was strictly inside the widened row bounds, so row `i`'s
logical was basic in the reduced solve, and it is the only other variable the
record touches. The exchange removes `e_i` and inserts a column whose one
nonzero is `a_ij`, so the pivot is `a_ij`, which presolve already required
non-zero: no rank test, no fallback. And it moves no numbers — `c_j = 0`, so a
basic `x_j` needs `y_i = 0`, which the reduced solve already had.

**The whole chain** (`bench/measurements/02-48/`), measured the same way each
time:

| | D131, before | D138, row only | **now** |
|---|---|---|---|
| netlib, exact count | 56 | 88 | **140** |
| netlib, wrong | 132 (70%) | 100 | **48 (26%)** |
| netlib, worst error | +596 / −169 | +596 / −0 | **+23** / −0 |
| Kennington, exact count | 8 | 24 | **32 — all** |
| Kennington, wrong | 24 (75%) | 8 | **0** |
| Kennington, worst error | +12104 / −406 | +119 / −0 | **0** |

**Kennington publishes a valid basis on every solve.**

**Both pinned change detectors fired in the direction they predicted.**
`test_singleton_col_between_two_removals_solved_path` read 3 against
`num_row = 2` with a comment saying *"expect this 3 to become 2 … re-pin
there, deliberately"*, and now reads 2.
`test_the_basis_count_promise_breaks_on_a_declined_column` read 2 against
`num_row = 1` and now reads 1. **Both now agree with `-DJAOS_NO_PRESOLVE`**,
this project's only oracle for output no predicate reads. Each test's `#if`
existed to state the gap between presolve and the oracle; the gap is closed
and both collapse to one assertion.

**The gate is bit-identical on all three sets**, which proves no value moved
and is also why it cannot judge this: `bench/run.c` states the digest covers x
and y and not the basis. `make test`, `make test
EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` and `make sanitize` all exit 0.

**A consequence worth acting on.** `bench/run.c` declined to widen the digest
to cover the basis because *"a published basis that breaks the row-count
promise is a live defect (TODO.md), so a basis hash would pin today's wrong
answer."* Kennington no longer breaks it on any solve. Widening the digest is
now a smaller question than it was.

**Process.** `numerics-reviewer` was unavailable, as throughout this run. The
review was done in the main context: forward order with one record per row so
nothing depends on order (D8), `ps_restore_index` applied exactly as the
replay applies it, no tolerance, no libm call and no `jm_work_add` added, and
the two declining shapes written into the source beside the code.

**Left open, in `TODO.md`**, and it is 48 netlib solves. Two shapes decline
the swap, both counted in D135: **80 firings whose row logical is already
nonbasic**, which contradicts the derivation and is unexplained — D135 read
the *published* status rather than the reduced one, so re-measure before
believing it is a second defect; and **108 whose row is not on a bound**,
where making the logical nonbasic would claim a bound the row does not rest
on. Plus D136's **40 singleton rows** whose final activity misses its bound by
about 1e-16 of the row's traffic.

## D140 — The 80 are an exact degenerate tie the recovery division rounds off, and the swap's guard reads a status another family rewrites

**The question, as asked.** D139 left 80 netlib firings declining the swap
because the row logical is "already nonbasic", which contradicts the swap's
own derivation (an interior recovery forces the reduced activity strictly
inside the widened row bounds, so the logical had to be basic). D135 had read
the *published* status and assumed the row's survival without checking
either, so `TODO.md` ordered a re-measure before believing it is a second
defect. Expected: some of the 80 would turn out to be rows that never
survived, with the status written by whichever family removed them.

**The measurement** (`bench/measurements/02-49/`, two probes, predictions
stated in each script before its run). The first probe failed its canary —
6132 records read a BASIC column at the guard against D134's 5902 — and the
failure located a second finding before the first question was answered:
`JM_PS_SINGLETON_ROW`'s replay writes `sol_col_status[j] = BASIC` for its own
column (`src/presolve.c:2132`), and that column can be one a `SINGLETON_COL`
record restored earlier in the same LIFO walk, so the guard at
`src/presolve.c:1881` reads the rewrite, not the recovery. The second probe
stores the column's status at its own replay write and separates the
populations: netlib true 5902 exactly, phantom 230, lost 0; Kennington
482 / 0 / 0, all four canaries exact.

Of the 5902: **5670 swapped, 152 declined with the row interior, 80 declined
with the logical nonbasic — and every one of the 80 survived** with the
reduced solve resting the logical **exactly on the widened row bound**, 58 on
the widened lower as `AT_LOWER`, 22 on the widened upper as `AT_UPPER`,
matching one for one. The sum closes: 232 unpaid plus D136's 40 is +272,
02-48's published netlib SUM exactly.

**What the 80 are.** A legitimate degenerate vertex of the reduced model. At
activity exactly `rl − a·hi`, the exact recovery is `(rl − rest)/a = hi` —
the column AT its bound, nonbasic, no drift. The replay's division rounds it
a few ulps interior, the interior test calls the column BASIC, and the basis
gains an unpaid member. After the full replay 74 of the 80 rows land exactly
ON their original bound: the row is binding, its nonbasic logical is correct,
and the interior `xv` is rounding noise from the recovery. **The derivation
is not wrong and there is no second defect; its premise fails only on the
exact tie.**

**What was refuted.** The row-did-not-survive explanation: zero of the 80.
An earlier swap of the same family taking the logical: zero. A surviving
row's reduced-BASIC status overwritten by anything: zero. And D135's 5714/108
split of the tight/loose boundary: its probe classified after the replay loop
but before the final carry fold, so 44 rows it called tight are loose on the
folded activity the shipped swap actually reads — 5670/152 are the operative
numbers, and this is the fourth partial-activity misread in five probes on
this machinery.

**The phantom 230** (202 `AT_LOWER`, 28 `AT_UPPER` at their own replay) fired
the swap zero times on the current sets, so the guard defect costs nothing
today. It stays a live premise violation: a firing would remove a logical
whose basis slot `SINGLETON_ROW` already paid with its own restored row. The
value test `sol_col[j] == rec->lo || == rec->hi` reproduces the replay's
decision bit for bit and cannot be rewritten; it is the hardening candidate.

**Left open, in `TODO.md`.** The 80's repair candidate — at the singleton-col
replay the surviving row's status still holds the reduced solve's answer, so
a nonbasic status names the exact bound and the exact `xv`; snapping moves
published values by ulps and digests with them, so it is a value change with
the full gate, not a status edit. The 152, now the largest class, with no
local exchange. And the guard hardening, behaviour-identical today by this
measurement.

## D141 — A within-row demotion cannot pay for the residue: 152 of the 232 declines have no basic column at a bound

**The question, as asked.** D140 left netlib's published over-count at +272:
80 declines on the exact degenerate tie, 152 with the row interior, 40
singleton rows (D136). The swap already takes the row's logical where it
can, so both declining classes need a different variable taken out. Two
candidates existed. The snap D140 sketched for the 80, and the demotion both
classes share: some OTHER basic column of the same row resting exactly on
its own bound, whose demotion claims nothing false and moves no value.
Expected: unknown; the probe question was whether `cands = 0` dominates.

**The snap was refuted before the probe ran, on the arithmetic.** 02-49
measured 74 of the 80 rows landing exactly ON their original bound with the
interior `xv`. The exact recovery `(rl − rest)/a = hi` holds in real
arithmetic only; publishing `xv = hi` perturbs the replayed activity by ulps
and takes the row OFF the bound its nonbasic logical claims. The repair
would trade a column-status defect for a row-status one. Do not rebuild it.

**The measurement** (`bench/measurements/02-50/`, availability only, the
D135 pattern: is the partner even there before designing the exchange).
Canaries all exact — 5902 true firings, B = 80, L = 152, Kennington zero
declines. Of the 80: 66 have no demotion partner, 14 a forced one, 0 a
choice. Of the 152: 86 none, 18 forced, 48 a choice. No row lacks basic
columns — 8.03 and 10.59 on average — but a basic variable rests strictly
inside its bounds almost everywhere, and the degenerate basic-at-bound
member the rule needs is rare.

**Refused.** A within-row rule reaches at most 80 of 232 and leaves 152
with nothing. No rule that only looks at the firing row can close the
residue, so none should be built. Reopen condition: a demotion design whose
candidate set is wider than the firing row AND that carries a rank argument
for the demoted member — the attempt-and-fallback shape Galabova 2023
describes, whose fallback is accepting the residue.

**Left open, in `TODO.md`.** Accepting the residue has one measurable
price: the 48 solves publish a count `build_warm_basis` rejects, so they
lose their warm start and nothing else. The `warm` re-measure prices it.

## D142 — The remember-basis count guard is refused: the non-basis it would clear is the warm start's raw material

**The question, as asked.** TODO item 2: `jm_model_remember_basis` should
enforce the count `jaos_set_basis` enforces, on the premise that "on its own
it changes nothing measurable — a stored basis failing the count is already
rejected by `build_warm_basis`". The candidate was built (clear instead of
store, OPTIMAL publishes only, so the deliberate partial stores of the two
interrupted paths stay exempt), its reject-case test validated to fail on
the unguarded tree, `make test` and `make sanitize` green. Expected: three
sets and both warm campaigns bit-identical.

**The premise is false, and the review found it before the campaign did.**
`numerics-reviewer` (second delivery in this run — the step is live again):
`build_warm_basis` at `src/simplex.c:923` counts the MAPPED basis on the
reduced model, and `jm_presolve_run`'s mapping at `src/presolve.c:1651`
drops every stored member whose row or column presolve removes. A basis
wrong in orig space can therefore map exact and warm-start; the guard would
clear exactly those.

**The measurement** (`bench/measurements/02-51/`). The gate is genuinely
unaffected — `bench/run.c` clears the basis before every re-solve — and
Kennington's warm campaign is bit-identical. netlib's is not: the guard
costs `capri` its warm start (1 → 273 iterations, 12.3x work) and
`fffff800` its (7 → 945, 127x), moving the warm work geomean 0.2553 →
0.2766. The 02-51 mapping probe counts exactly 2 netlib warm solves in the
"orig wrong, mapped exact" class, the two the guard clears.

**Refused.** The stored publish is not a contract any consumer reads as a
basis in orig space; the one consumer counts after the mapping, and its
check is already in the right place. The candidate and its test are kept at
`bench/measurements/02-51/remember-guard-candidate.diff`. Reopen: a consumer
of `start_*` appears that reads the orig-space count as a claim, or warm
starting stops going through presolve's mapping.

**Left open.** Nothing of item 2 survives; the mapping question it exposed
is D143's.

## D143 — D138/D139's correct basis maps short and netlib's warm ratio paid 3.7x: the mapping owes the reverse of the swap

**The question, as asked.** TODO item 4: re-measure the `warm` records now
that D138/D139 publish a valid basis on every Kennington solve. Expected:
the warm ratio moves, Kennington's by a lot, in the good direction.

**The measurement** (`bench/measurements/02-51/`, records rewritten
deliberately from these runs). Kennington improves, 0.0873 → 0.0572 work
geomean. **netlib regresses 3.7x, 0.0696 → 0.2553**, iterations geomean
0.0250 → 0.1381; the cold fallbacks go from 23 of 92 (D129) to 54 of the
88 warm solves that map a stored basis. A dozen-plus instances that
warm-started in 0–6 iterations (`25fv47`, `adlittle`, `bandm`, `blend`,
`bnl1`, `bnl2`, `brandy`, `cycle`, `czprob`, `d2q06c`, `etamacro`, …) now
run warm equal to cold; `fffff800` gained its warm start.

**The mechanism, measured** (`run-warm-mapping.sh`, one line per mapped
stored basis): 35 netlib warm solves and 5 Kennington ones read **orig
exact, mapped SHORT** — worst shortfall 596 members on one instance — and
fall back cold on the count `build_warm_basis` judges. Pre-D138/D139 the
published error and the mapping cancelled: the extra BASIC members sat on
rows and columns presolve removes again on the re-solve, so dropping them
restored the count (2 netlib solves still run warm on exactly that
accident). Post-D139 the swap rests the surviving row's logical on a bound,
which maps through as nonbasic, while the restored basic column is removed
again and dropped: short by one per firing. The correctness fix was right
and stays; what it exposed is that **the mapping performs no reverse
exchange**.

**What was refuted along the way.** That the regression is a defect in
D138/D139 themselves: the published basis is exact on 140 of 188 netlib
solves and all 32 Kennington ones (02-48), and the warm loss follows from
the mapping alone. And item 4's own expectation of a uniform improvement.

**Left open, in `TODO.md`, and it is the new head of the warm work.** The
mapping owes the reverse of what postsolve's second pass writes: for a
`JM_PS_SINGLETON_COL` record whose stored column is BASIC and about to be
dropped, promote the surviving row's stored-nonbasic logical back to BASIC
in the reduced start — the mirror of `ps_singleton_col_swap`, same forced
pivot `a_ij`. Derive the balance for every family the same way before
building. The prize is bounded: up to 35 netlib and 5 Kennington warm
starts. 4 of the 92 measured netlib warm solves print no mapping line and
are unattributed.

## D144 — The mapping's balance is multi-family, so the count is repaired at the consumer, not in the mapping

**The question, as asked.** D143 named the un-swap — promote the surviving
row's stored-nonbasic logical when its stored-basic singleton column is
dropped — and ordered the per-family balance derived before building it.
Expected: the shortfall dominated by `SINGLETON_COL`, the un-swap closing
most short solves.

**The measurement** (`bench/measurements/02-52/`, one exact decomposition
per mapped stored basis; the closing identity held on all 99 solves —
BROKEN=0 — which is the canary everything below rests on). netlib: 54
solves short by 2803 in total; un-swap candidates 2939 on those solves;
the un-swap closes **30 of 54 exactly, cannot close 14, and would
overshoot 10**. The dropped stored-BASIC columns split 3003
`SINGLETON_COL`, 1757 `FIXED_COL`, 1042 `IMPLIED_FREE_COL`, offset by 3313
removed rows stored nonbasic and 426 correction-pass demotions.
**Kennington refutes the single-family design alone**: its 5 short solves
are short by 1 each with zero un-swap candidates — `dcSC = 0` there, the
shortfall being five more stored-BASIC `FIXED_COL` drops (910) than
nonbasic-removed-row offsets (905).

**What was refuted.** Per-family exactness in the mapping: the mapped count
is a difference of many family terms, so correcting one family chases a
moving sum and leaves 24 netlib and all 5 Kennington fallbacks standing —
and on 10 netlib solves the correction would push the count past `rrow`.
Do not build the un-swap as a mapping pass.

**The design this selects.** Repair the count at the consumer:
`build_warm_basis` promotes, while the mapped count is short, the logical
of a row that has no basic member, in fixed row order (D8), and trims the
mirror way when long, instead of rejecting. Rank stays where it already
lives — `repair_singular_basis` — and the weights already restart at one.
One measured fact says the repaired basis is good rather than merely
valid: for the `SINGLETON_COL` family the promotion reconstructs exactly
the pre-D139 mapped basis, the one that warm-started `25fv47`, `adlittle`,
`bandm` and the rest in 0–6 iterations.

**Left open, in `TODO.md`.** Build and measure that count repair: the warm
campaigns are the judge (netlib's geomean should recover toward 0.0696 and
Kennington's 5 shortfall-1 solves should warm-start), the gate must stay
bit-identical, and the promote-then-trim rule needs its reject case built
in a test before it is believed.

## D145 — The count-repaired warm start publishes wrong optima through the termination hole, and the warm prize waits behind it

**The question, as asked.** D144's selected repair, built and judged:
`build_warm_basis` promotes logicals — uncovered rows first, then fixed
row order — while the mapped count is short; LONG still refused (deviating
from D144's trim sketch, deliberately: no long map has been measured).
Expected: gate bit-identical, netlib's warm geomean recovering toward
0.0696, Kennington's five shortfall-1 solves warming.

**The measurement** (`bench/measurements/02-53/`, candidate kept whole
beside it). The gate is bit-identical on all 139 instances. Kennington is
a clean win: 0.0572 → 0.0070 work geomean, all five recovered, `osa-60`
from 7061 iterations to 1, none worse. netlib's geomean improves 0.2553 →
0.1636 **and the campaign refuses the candidate underneath it**: 8 solves
publish `optimal` with a wrong objective from the warm trajectory
(`dfl001` 3.099e8 against the true 1.127e7; `modszk1` 1135456 against
321; `cycle`, `d2q06c`, `degen2`, `greenbea`, `maros`, `woodw`), 2 more
have their warm point refused by the checker (`pilot87`, `scsd1`), and 13
of 82 cost more warm than cold, worst `pilot-ja` 8.34x. Every one of
those counters read zero before the candidate.

**What was refuted, precisely.** The candidate at HEAD — not the design.
The promoted basis is structurally valid, and the solve that starts from
it can stop at a suboptimal vertex and publish `optimal`: that is §5a's
termination defect (D119, "the termination test never re-reads dual
feasibility"), now with eight named reproductions driven from a valid
basis. The refusal ordering follows: **the termination defect blocks the
warm prize**, and any retry starts from the kept candidate only after a
solve that starts badly can no longer end wrongly. `numerics-reviewer`'s
four findings on the candidate were all dispositioned before the
campaigns; the process failed nothing — the judge did its job.

**Also opened.** `jaos.h` promises a hostile basis "costs time and cannot
produce a wrong verdict". The candidate manufactured count-valid bases
and eight wrong verdicts. Whether `jaos_set_basis` alone can do the same
at HEAD is one probe away; `TODO.md` carries it as a correctness question
ahead of any warm work.

**What landed.** The LONG-map pinned test
(`test_a_long_mapped_basis_falls_back_cold`), premise asserted, so any
future count repair moves a test deliberately. Nothing else: the source
was reverted and the working tree re-validated green on all three test
builds.

## D146 — A hostile basis makes HEAD publish a wrong optimum through the public API alone

**The question, as asked.** D145 left it one probe away: `jaos.h` promises
a caller basis "costs time and cannot produce a wrong verdict" — does the
promise hold at HEAD with nothing but `jaos_read_mps`, `jaos_set_basis`
and `jaos_solve`? Expected: plausible that it does not, after D145's eight
manufactured reproductions.

**The measurement** (`bench/measurements/02-54/`, five instances × sixteen
deterministic hostile bases, every `optimal` judged against the same
binary's cold reference and by `jaos_check_solution`). 80 trials: **26
wrong optima published as `JAOS_SOLVE_OPTIMAL`, 5 more with the point
checker-refused, 0 errors.** `degen2` fails all 16 trials, worst
−1352.64 published against a true −1435.178 (5.7%); `scsd1` fails 10 of
16 and is checker-refused on 15 of 16. `cycle`, `modszk1` and `woodw`
held on every trial.

**Refuted: the header's promise, at HEAD, by construction.** No candidate
code is involved. The mechanism is §5a's termination hole (D119): the
termination never re-reads dual feasibility, so a solve that starts badly
can end wrongly — and it now has seconds-cheap deterministic
reproductions on a 444-row instance in place of a 278003-iteration warm
campaign. `jaos.h` now states the defect beside the promise until the
repair lands.

**What this reorders.** This is the largest open correctness item in the
repository: a public-API caller gets `OPTIMAL` and a wrong objective with
no signal. It takes over `TODO.md`'s head; the published-basis residue
(D140/D141) and the warm retry (D145's kept candidate) queue behind it.

**Left open, in `TODO.md`.** The diagnosis: instrument the termination on
`degen2` shift 1 (jaos-debug's trajectory-first discipline), find what the
final test reads instead of dual feasibility, and why `cycle`, `modszk1`
and `woodw` survive the same hostility. Then the repair, judged by this
probe reading 0 of 80, the three sets bit-identical, and the D145
candidate as the warm retry behind it.

## D147 — The solver measures the violation it publishes: the best point ends with bstdv = 35.34 and nothing reads it against a tolerance

**The question, as asked.** D146's step 1: where does the hostile `degen2`
solve lose the true dual, and what did the best-point loop see when it
kept the wrong vertex? Expected, loosely: a dual feasibility nobody
re-reads.

**The measurement** (`bench/measurements/02-55/`, one trajectory line per
`settle_shifts` call plus `run()`'s exit and `classify_optimum`'s verdict,
cold `degen2` as the control). Cold: 578 iterations, lends at 1e-14, zero
violations. Hostile: `run()` declares its shifted-space optimum at 636
iterations with **402 outstanding lends (sum 3254) and 186 true-dual
violations (max 35)**; the settling loop repays every lend — `nlend = 0`
from round 1 on — and the true violation count never drops below 100
through all 32 rounds, oscillating with `d0max` between 40 and 495. The
best point is fixed at round 1 with `settled_dual_violation = 35.34` and
objective −1352.64, no round beats it, the rounds run out and
`take_best_if_better` publishes it as `OPTIMAL`. `classify_optimum` passes
trivially: no lends outstanding, and invented bounds are all it asks
about. The published point's true `d0max` equals `bstdv` bit for bit —
the solver's own number and the instrument agree.

**What was refuted.** "The termination never re-reads dual feasibility" as
the literal mechanism: the settling loop measures it every round and
stores the best. What no code does is compare that stored number — eight
orders of magnitude past any tolerance here — against one before
publishing. D89's "publish the best point instead of failing" is where the
verdict laundering happens: benign on `pilot87`'s 8.37e-09 residue, wrong
by 5.7% of the objective on a hostile start.

**The repair shape selected, and its precondition.** When settling ends
with the best point's `bstdv` above the dual tolerance, do not publish
OPTIMAL; restart once, cold, from the slack basis — the header's own
contract, time and never correctness. No new constant: `dual_tol` is the
bar and `bstdv` the number, both already in the code. **Measure first,
both sides**: the distribution of `bstdv` at publish across all 139 gate
instances at HEAD. Every legitimate solve at or under the tolerance means
the guard has orders of magnitude of margin and the gate stays
bit-identical by construction; any legitimate instance above it names the
margin question before anything lands.

**Left open, in `TODO.md`.** The distribution probe; the guard plus cold
restart with its tests (hostile `degen2` flipping to the correct optimum
at cold cost, 02-54 reading 0 of 80); why `cycle`, `modszk1` and `woodw`
survive the same hostility is subsumed — predicted: their settling either
converges under the tolerance or their best point is genuinely optimal —
and the distribution probe reads it directly.

## D148 — The certificate guard lands: 0 wrong of 80 where HEAD published 26, and the gate is bit-identical

**The question, as asked.** D147's repair, built: read the settled dual
violation before publishing; an uncertified warm start restarts once, cold;
an uncertified cold start is `NUMERICAL_ERROR`. Expected, from 02-56's
margin: 02-54 reading 0 of 80 and the gate bit-identical.

**The measurement** (`bench/measurements/02-57/`). The 02-54 hostile probe
on the candidate: **80 trials, 0 wrong, 0 refused** — run twice, before
and after the review fixes — against HEAD's 26 + 5. The gate: 94 + 29 + 16
instances bit-identical to the committed records. `make test`,
`make sanitize` and the `-DJAOS_NO_PRESOLVE` variant green.

**What the review added, and it was load-bearing.** `numerics-reviewer`'s
fourth delivery this run found the guard's own blind spot before any
campaign: a restore exit whose refresh fired `repair_singular_basis`
re-runs `shift_to_feasible`, and the guard would then read `d[v] = 0.0` on
exactly the breached columns — the lend arranging the evidence, the D146
defect through a rarer door. The driver settles once more before reading,
which is a no-op on a settled state (`repay_shifts` finds nothing, bills
nothing) and truth on the rare one. Its other findings, all dispositioned:
the reduced path offered warm memory on `NUMERICAL_ERROR` against
`publish()`'s own whitelist (fixed in `jm_postsolve_expand`, whose "Null
only for NUMERICAL_ERROR" comment was already false for a caller basis);
`rowc` leaked on the interrupted early return (fixed); the abandoned warm
attempt's counters vanished from the summary (logged in the restart line
now). Carried, low: `m->err` can hold a first-attempt message behind a
final OPTIMAL — a pre-existing class with one new route.

**What was refuted along the way.** Nothing about the design; the sequence
held: D147 named the number, 02-56 measured both sides, the guard used no
new constant, and the gate could not move because the guard cannot fire on
a population measured at exactly zero.

**What this restores and unlocks.** `jaos.h`'s promise is true again and
enforced rather than assumed; the header says so. D145's reopen condition
is met: the warm count-repair candidate at
`bench/measurements/02-53/warm-count-repair-candidate.diff` is live again,
judged by the warm campaigns, with its Kennington win (0.0572 → 0.0070)
waiting.

## D149 — The retried warm repair is correct now and refused on cost: dfl001 pays 172x for a doomed attempt the guard then throws away

**The question, as asked.** D145's retry behind D148: the 02-53 count
repair re-applied on the guarded HEAD, judged by the warm campaigns with
the bar written in TODO before the run — `disagreed=0, rejected=0` where
02-53 read 8 and 2, the recovered warm starts kept, the geomean re-read.

**The measurement** (`bench/measurements/02-58/`; composition review
clean, suite green in three variants, gate bit-identical on 94 + 29 + 16).
**The correctness bar is met**: `disagreed=0, rejected=0` on both warm
campaigns — the certificate guard catches every doomed trajectory and the
cold restart answers correctly. **The cost refuses it**: netlib work
geomean 0.2605 against the repair-less 0.2553 (iterations improve, 0.1381
→ 0.0752), and `dfl001` goes from a cold fallback at ratio 1.0 to
**172.03x** — its repaired basis (shortfall 596) launches a ~2e6-iteration
trajectory to an uncertifiable vertex, the guard fires, and the honest
work accumulator hands the caller the whole bill for one changed bound.
`bnl2` shows the second cost shape, certified but 7.8x through a worse
vertex path. Kennington stays a clean win, 0.0070, nothing worse.

**What was refuted.** The blanket repair: its value concentrates in small
shortfalls (Kennington's five at 1 each; `adlittle` 80→2, `blend` 97→12,
`boeing1` 391→24) and its cost in large ones, and no rule that repairs
both lands without a threshold. A threshold on the shortfall is a new
constant, and D8 gives a constant exactly one way in: a sweep on both
sides, for which the material is already measured — 02-52's per-instance
shortfalls joined against 02-58's per-instance outcomes.

**Left open, in `TODO.md`.** The sweep, if the warm prize is pursued:
shortfall-capped repair, judged by the same campaigns. The candidate is
kept whole at `bench/measurements/02-58/warm-retry-candidate.diff`. The
refusals table carries the condition.

## D150 — The gate sees the basis: every optimal line carries its hash, det covers it, and all 139 instances hold

**The question, as asked.** `bench/run.c`'s own comment: the digest covers
x and y and not the basis, "so a change that moves only the basis is
invisible to all three sets" — left open because a basis hash would pin a
wrong answer. Kennington stopped breaking the count promise (D139) and
netlib's residue became a measured, named 48 (D140, D141), so the
objection expired. Expected: a `basis=` field per optimal line, `det`
covering it, no predicate moving.

**The measurement** (`bench/measurements/02-59/`). The instrument was
validated against the two cases it must reject before any campaign: a
solver flipping one status on its SECOND publish reads `det=DIVERGED`, and
one flipping EVERY publish reads `det=ok` with the line moving in exactly
the `basis=` field. The three-set campaign then read `0 regressed, 0
improved, 0 new` on every baseline and **`det=ok` on all 110 optimal
solves — a new measured fact: the published basis is bit-deterministic
across the cold re-solve on every gate instance.** The three committed
records were rewritten deliberately with the new field; the `*.baseline`
predicate files did not change.

**What was refuted.** Nothing; the objection this closes was its own
author's and had expired on the record's own measurements.

**What this ends.** Basis repairs judged only by hand-built probes: D138
and D139 were judged by 02-47/02-48 because the gate was basis-blind. From
this record on, the gate itself sees a basis change per instance, and
netlib's 48-solve residue is pinned deliberately so its future repair
moves the record visibly instead of invisibly.

## D151 — The warm repair lands behind a shortfall cap of 4, chosen at the end of a plateau because the mean is flat there and the worst case is not

**The question, as asked.** D149's refusals-table condition, in its own
words: a shortfall cap swept on both sides from the measured material,
judged by the same warm campaigns. D149 refused the blanket repair on
cost — correct behind D148's guard, and `dfl001` paying 172x for a
596-short repair the guard then threw away. Expected: a threshold exists
that keeps the small-shortfall gain and drops the large-shortfall cost.

**The measurement** (`bench/measurements/02-60/`). 02-52 had saved only
aggregates, so the per-instance shortfalls were re-measured;
`run-shortfall.sh` reproduces 02-52's aggregates exactly — netlib 88
mapped / 54 short / 2803 total, Kennington 11 / 5 / 5 — on an instrument
written against a different decomposition, which is its validation.

The curve, netlib work geometric mean against cold, at caps 0, 1, 2, **4**,
5, 6, 7, 8 … 596: 0.2553, 0.2089, 0.2047, **0.1916**, 0.1886, 0.1895,
0.1874, 0.1938 … 0.2605. The worst per-instance ratio over the same caps:
1.00, 4.65, 4.65, **4.65**, 4.70, 4.70, **15.48**, 15.48 … 172.03. **The
mean is flat across 1..7 and the worst case is not.** 1 → 4 buys 8.3% and
moves the worst case by nothing; 4 → 5 buys 1.6% and costs `brandy` 4.70
and `bnl1` 2.87; 5 → 7 buys 0.6% and costs `greenbea` 15.48. So 4, at the
end of a plateau rather than at the minimum — 7 is 2.2% better on the mean
for a worst case 3.3x larger. Kennington does not vote: all five of its
short solves are short by exactly 1, so every cap at or above 1 gives it
the whole gain.

The capped tree was then built and run, and the campaign matches the
sweep's prediction on **103 of 103 instances** across both sets, warm and
cold, iterations and work (`verify-prediction.py`). netlib 0.2553 →
**0.1916**, Kennington 0.0572 → **0.0070**, worst ratio 4.65 against the
blanket repair's 172.03, and `disagreed=0, rejected=0, errors=0` on both —
D145's correctness bar, met.

The three gate sets are unmoved, and that was run rather than argued:
`gate: PASS` and `0 regressed, 0 improved, 0 new` on all three, with
`record_diff` reading **94 + 29 + 16 bit-identical, 0 digest changes**.
The gate solves each instance once from a fresh load and never reaches
`build_warm_basis`. D150, landed the day before, is what makes this
check strong — every optimal line now carries `basis=` and `det` covers
it, so a change moving only a basis would show here, and last week it
would not have.

**Both subagents delivered, late, and both were also done in the main
context in the meantime — so this entry has two independent reads of the
same change rather than none.**

`jaos-measurer` returned **ACCEPT** from its own worktree campaign: 139
of 139 bit-identical, and stronger than the main context's own run —
`cmp -s` says the three result **files** are byte-identical to the
committed records. It also gave the structural reason the gate cannot
reach the change (`bench/run.c:653` clears the basis and the runner never
sets one, so `build_warm_basis` returns at its first null check) and the
empirical one (the repair bills `covnz * JM_WORK_NONZERO`, so any firing
would have moved work; none did). And it validated the new test by
mutation independently: `WARM_REPAIR_MAX_SHORT` 4 → 8 makes it fail,
restoring 4 makes it pass.

`numerics-reviewer` returned **no findings** in all four defect classes,
having confirmed the promotion order reads only `cov[i]`, `want_arr` and
the loop index — `cov` is filled from `m->a_index[k]`, a row index, so no
matrix value reaches the decision — and that all four exits after the
allocation free `want_arr` exactly once. It added one argument the main
context did not have: **no path can bill work and then refuse.** Writing
`c` for basic columns and `l` for basic logicals, the shortfall is
`nrow - c - l` and the promotable supply is `nrow - l`; since `c >= 0`
the supply always covers the shortfall, so once the block runs
`nbasic == nrow` holds and the refusal below it cannot fire.

**It also measured the test's premise instead of inferring it**, which is
the better evidence: instrumented with the cap raised to 1000000, the
k-block construction reads a shortfall of exactly k for k = 1..8, because
presolve removes the k singleton columns and no rows. The main context
had only the boundary landing at 4/5 as indirect evidence for the same
claim.

Three low-severity notes from that review are **fixed** in the same
commit: the OOM comment said "an 8·nvar-byte allocation" where the type
measures 4 bytes and two allocations can fail; `repair_fires_at` checked
one of its ten allocations for null and now checks all of them; and the
helper carries a note that a failing assert longjmps out of it, so an
ASan run will attach about eleven LSan reports to a test failure that is
not a memory defect.

**Those fixes landed after the campaigns, so what they changed was
checked rather than assumed.** In `src/` the edit is comment-only: every
changed line in `src/simplex.c` sits inside one `/* */` block, and the
compiled objects at the measured commit and after the edit share an md5,
so the gate campaign stands verbatim rather than approximately.
`tests/test_presolve.c` also changed, and **that part is not comments** —
eight `TEST_ASSERT_NOT_NULL` calls were added inside `repair_fires_at`.
The distinction is written out because "a comment-only edit" is the
sentence a later reader would use to skip re-measuring, and it is true of
`src/` alone. Test code cannot reach the solver binary or the gate, and
the assertion the cap's mutation test turns on is untouched — only its
line number moved. Caught by `jaos-measurer` re-reading the three commits
on top of the one it had measured, rather than accepting the claim.

**What was refuted.** The relative cap, which is the better-shaped
constant on the argument that a shortfall of 5 means different things on a
25-row and a 6000-row model. Swept over every distinct ratio in the set,
it reaches a best mean of only 0.2081 against the absolute cap's 0.1874,
and it meets the 15.48 cliff with **8** instances admitted where the
absolute cap admits **31** before reaching it. One instance is the
reason: `greenbea`
is 7 short of 1954 rows, **0.36%** — the smallest relative shortfall in
the set and one of the two worst outcomes — while `seba` is 69% short and
costs 2.87x. The shortfall's absolute size separates these cases and its
size relative to the model does not, which is the opposite of what the
shape argument predicts.

Also refuted, as an instrument: an iteration-based test of the cap. On
models small enough to unit-test, warm and cold counts coincide by
accident — the test's own construction reads `warm == cold` at k = 2 and
k = 4 while the repair fires at both. The test counts the repair's DETAIL
log line through the public callback instead, and was validated against
the two trees it must reject (cap removed, cap raised to 5), failing on
both and passing on the shipping tree.

**What is left open, in `TODO.md`.** Two instances still lose real work at
this setting, `scsd1` 4.65x and `degen2` 4.09x, both with warm iterations
exactly equal to cold — D148's guard rejecting the repaired trajectory and
charging the caller the attempt plus the whole cold solve. A rule that
predicts a doomed trajectory before paying for it is not this entry's, and
the shortfall does not separate them: both are short by 1, the same
shortfall as the sixteen instances that win. The refusals table carries it.

## D152 — The replay clamps into the column's own box, and the assert that could not be enabled is removed rather than widened

**The question, as asked.** TODO's standing debt, the one it called worth
more than it looks: eleven of the 94 standard instances reach
`ps_replay_one` with `want_lo` above `want_hi` by 2.2e-16 to 1.3e-15 and
publish `want_lo`, so a value sits outside a bound the caller declared and
`assert(want_lo <= want_hi)` cannot be enabled — which means **no
assert-enabled build can run those eleven at all**. Expected: clamp the
published value into `[rec->lo, rec->hi]`, and the build configuration
comes back.

**The measurement** (`bench/measurements/02-61/`). The eleven reproduce
exactly at the parent commit, one process per instance so no abort hides
another. **One number in the debt was wrong and the probe corrected it**:
of the 138 empty intersections behind those eleven instances, only **10
records on 2 instances — `bnl1` and `finnis` — ever published outside the
column's box.** On the other 128 `want_lo` equals `rec->lo` with the box
open above it, so the intersection is empty while the published value was
inside all along. The debt's worked example is one of the real ten and is
right as written.

The clamp lands. All 94 standard instances now run under `-UNDEBUG`, 0
aborts. The gate moves on exactly the two instances the control named and
nowhere else: 92 of 94 bit-identical with `bnl1` and `finnis` changing
digest, basis and row-relative residual; netlib-infeas 29 of 29 and
Kennington 16 of 16 bit-identical; all three `gate: PASS` with `0
regressed, 0 improved, 0 new`. The basis moves in the right direction — a
clamped value equals `rec->hi` exactly, so the status publishes `AT_UPPER`
where it published `BASIC`, which is D137's interior-implies-basic rule
read the other way round.

**What was refuted.** Two windows for keeping the emptiness assert with a
tolerance, both built and both measured:

- eps times the division's own inputs, `(|rest| + |row bounds|)/|coef|`,
  takes the eleven aborting instances to two. It fails for the reason
  `fp-numerics` states: the rows that remain are equalities at zero whose
  partial activity has cancelled, so `rl = ru = 0` and `rest` **is** the
  residue — the scale collapses onto the quantity it exists to bound.
  Worst gap 4.72e-14 against a window of 1.78e-15.
- eps times the row's accumulated traffic, which is what a sum is known
  to. It moves 22 uncovered records to 18 and reads a traffic of **exactly
  zero on 86 of the 138**, worst case included. The reason is structural
  and is two lines of `src/presolve.c`: `sol_row[i]` arrives by direct
  copy from the reduced solve at line 2707, and two families assign it
  outright. The residue is the **simplex's**, so its error budget is not
  in the replay to be read at all.

So the assert is removed rather than widened, and a third window would be
a constant fitted to two instances. What the site asserts instead is what
the clamp establishes and `jaos.h` promises: the published value lies
inside the column's own box. That is enforceable and is enforced.

**What is left open, in `TODO.md`.** Detecting a genuinely infeasible
model — the `x0 + x1 = 100` case whose gap is 93 rather than 1e-14 — is
unchanged and stays where it was. This site cannot tell the two apart
without an error budget it has no access to; the old assert only appeared
to because it was never enabled. The debt's entry is rewritten with the
corrected count.

## D153 — The row-activity check becomes an invariant, and four wrong versions of it each reported a defect that was not there

**The question, as asked.** `numerics-reviewer`'s own standing proposal:
two replay producers assign `sol_row[i]` outright where every other
producer accumulates, both correct today by an argument about arena order
that nothing checks, and the class has already cost one campaign (D106).
Recompute every row's activity from `sol_col` at the end of postsolve and
assert it matches. **D152 is what made it runnable** — before it, eleven
of the 94 aborted an assert build before reaching any new assert.

**The answer.** The two assignments are fine. With the predicate stated
correctly all 139 instances pass, and the check is now an invariant of
every debug build. The debt is closed.

**The measurement** (`bench/measurements/02-62/`, `02-63/`). The
instrument was validated against two injected faults before it was
believed: an empty row overwriting a share that already arrived fires on
45 of 94, every basic row's activity moved by 1.0 fires on 81 of 94, and
both are silent with asserts off. Clean tree: **0 of 139** — netlib
0/94, netlib-infeas 0/29, Kennington 0/16. D152's property survives, all
94 still running under `-UNDEBUG`. The release objects keep the parent's
md5, so the gate campaign carries over untouched.

**What was refuted, and this is the entry's value: four versions of the
check, each of which reported a defect that was not there.**

- **No OPTIMAL gate** — fires on all 29 netlib-infeas instances. Both
  call sites run the replay whatever the verdict, because the index
  mapping is owed even for a stopping point, and there `sol_col` and
  `sol_row` are not required to agree.
- **A fixed multiple of eps times the traffic** — fires on `osa-30` and
  `osa-60`, whose rows carry **72554 and 173365 nonzeros**. A naive sum
  of n terms is bounded by `(n-1)·eps·Σ|t|`, not by a constant times eps.
- **Comparing every row** — fires on `pilotnov`, 18 rows, worst 131x.
  **This one was written up as a genuine defect and committed before the
  correction.** The split by basis status is total: all 18 of
  `pilotnov`'s have a nonbasic logical and none is basic, while the only
  disagreeing rows on `osa-30` and `osa-60` are basic. A nonbasic logical
  means the basis asserts the constraint is tight, so the published
  activity is the tight value and the column sum is a different quantity
  carrying the **basis solve's primal residual** — bounded by
  conditioning, which nothing at that site bounds. The row trace
  (`02-63/`) confirms it from the other side: only one producer touches
  `pilotnov` row 931, the copy from the reduced solve, so the two
  suspected assignments are exonerated on the very row that looked worst.
- **Asserting a nonbasic row's activity equals its ORIGINAL bound** —
  fires on 44 of the 94. The replay adds restored columns on top of a
  reduced activity, so the original bound is not what is left there.

A fifth false alarm was the harness rather than the check: a missing
`-e infeasible` made every infeasible instance look like a failure.

**What is left open.** Nothing from this entry. `pilotnov`'s numbers are
explained and are not a defect; the TODO item the first version of this
entry opened is withdrawn, with the reason recorded there so it is not
re-opened from the same evidence.

## D154 — Three of the five build configurations did not build, and `make configs` is what will say so next time

**The question.** `TODO.md` §5 asked something small: the singleton-column
replay's comment says a frozen row is never revisited for infeasibility and
names a model that "publishes x1 = 96", and that model reports INFEASIBLE at
HEAD. Find a shape that still reaches the empty intersection, or delete the
claim. The expectation was a comment edit.

**The claim is stale, and the answer is the first of the two branches the item
offered.** `git log -S` puts the claim in `541f7dd` and its repair in
`7587ecd`, both on 2026-08-14, the repair second. The repair is the
frozen-row feasibility test at the end of `jm_presolve_run`, which
`test_a_frozen_row_that_cannot_be_satisfied_is_infeasible` and three
neighbours already pin. The comment is corrected rather than deleted: the
replay site still cannot detect an infeasible model, and naming the site that
now does is worth more than saying nothing. The residue that DOES still reach
the replay is named with it — a model infeasible by less than the frozen-row
test's own window of 8 ulps of the row's bound scale, which is the case D152's
clamp handles, 93 against 1.3e-15.

**Then the check for it did not compile.** `make test
EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` fails at HEAD, and so do both fault builds.
`repair_fires_at` in `tests/test_presolve.c` is defined and never called under
any configuration that ignores the warm-repair cap test, and
`-Werror=unused-function` is fatal. Blamed to `6db3bc6` (D151).

**Why nothing said so, and it is a property of `make` rather than of anyone's
attention.** `make` decides what to rebuild from timestamps and does not track
a change in `EXTRA_CFLAGS`. Run right after a plain `make test`, with no
source file changed, the reference build re-runs the plain binaries and exits
0. `TODO.md` stated all three of `make test`, the reference build and `make
sanitize` exit 0 at HEAD; two of the three claims were true.

**This is the second occurrence of the same shape.** D-10's record says
`make_frozen_row_infeasible_model` broke `-DJAOS_PRESOLVE_FAULT_OFFBYONE` the
same way until 2026-08-15, and names the reason nothing noticed: "the plain
build and the reference build are the two the loop actually runs". This time
the reference build broke as well, which is the build `jaos-testing` calls the
only oracle for output no predicate of the three sets reads.

**The measurement**, `bench/measurements/02-65/stages.txt`. The three repairs
live in three different files, so a stage is a `git stash` of a subset:

| stage | plain | reference | OFFBYONE | WRONGDUAL |
|---|---|---|---|---|
| 0 — HEAD | 221 pass | **no compile** | **no compile** | **no compile** |
| 1 — + the unused-function guard | 221 pass | 220 pass | **3 aborts**, 69 ran | **14 fail** |
| 2 — + the row-activity check's fault skip | 221 pass | 220 pass | **16 fail** | **14 fail** |
| 3 — + the positive-test guards | 221 pass | 220 pass | 163 pass, 0 fail | 160 pass, 0 fail |

None of the three is redundant, and stage 1 is why the other two were found:
a configuration that does not compile hides every defect behind it.

The second repair is D153's own check. It compares published row activities
against published columns, a fault build makes the replay wrong on purpose, so
it fired and aborted three of the eight test binaries — 69 of 236 tests ran.
It is skipped under either fault build and not weakened; the predicate is
untouched on every build that ships. The third is `tests/test_simplex.c`,
which carried **no** fault guard at all where `tests/test_presolve.c` carries
thirty. Fifteen of its sixteen failures went through one helper, so the guard
is in the helper.

**The green is checked, because green alone proves nothing here.** A guard
that swallowed a negative test would leave a fault build green for the wrong
reason. Under `-DJAOS_PRESOLVE_FAULT_OFFBYONE`, 8 of 8 off-by-one negative
tests PASS and none is ignored; under `-DJAOS_PRESOLVE_FAULT_WRONGDUAL`, 2 of
2 wrong-dual negative tests in `tests/test_presolve.c` PASS and neither is
ignored. The control is the plain build, where all ten are IGNORED.

**The cost is nothing the gate can see, and that is measured rather than
argued.** `presolve.o` shares an md5 at HEAD and on the repaired tree under
the release flags, so the campaign at `01aca61` carries over. The changed code
is inside `#ifndef NDEBUG`, which `-DNDEBUG` removes.

**What was refuted — three instruments, before one of them gave a usable
answer.** Comparing object md5s is the obvious way to prove a change cannot
reach the release build, and it reports a difference that is not one in three
separate ways. `bench/measurements/02-65/build-reproducibility.txt`:

- **`-flto`, which is the default.** Two builds of ONE unedited tree produce
  **12 of 12 different object md5s**; the `.gnu.lto_*` sections carry a
  per-compilation seed. At `LTO=0` the same two builds are byte-identical. So
  under the shipping flags `md5sum build/release/*.o` across two trees
  compares nothing. Nothing in the repository said so, and `02-62`'s README
  asserts objects "have the same md5 as the parent's" with no script beside it.
- **`-g`.** A comment LINE added or removed shifts every line after it, so
  `.debug_line` differs while `.text` does not. `02-30`'s one-off already knew
  this one.
- **The source file's basename**, which GCC writes into the object as an
  `STT_FILE` symbol. Compiling the reference copy as `ref.c` differs from
  `presolve.c` on identical code. Found by dumping sections one at a time
  after the disassembly came back identical and the md5 did not.

`.claude/skills/jaos-measure/scripts/comment_only.sh` handles all three and is
validated on both sides: it reports the release object unchanged for this
edit, and names the changed line when `rtol` is forced to 0.0.

**What is left open.** Nothing from this entry. `make configs` builds all five
configurations, each after `make clean`, and is validated both ways — 0 on the
repaired tree, 2 with `tests/test_presolve.c` reverted. It is deliberately not
part of `make test`: it costs five full rebuilds, so it belongs before landing
anything that touches `tests/` or a guarded block in `src/`.

## D155 — `row_traffic` accumulates only what a still-finite end absorbed, and the assert that says so rests on measured headroom rather than on the argument that looked available

**The question.** `TODO.md`'s first smaller item. The cost-0 bounded singleton
column's relaxation adds `max(|cmax|, |cmin|)` to `row_traffic[i]`. Such a
column need only be non-free, so one of its bounds may be infinite and the
product with the coefficient is then infinite. The item already stated the
repair — "what should accumulate is the finite part actually subtracted from
`cur_rl`/`cur_ru`" — and asked for a measurement rather than a patch.

**The measurement**, `bench/measurements/02-66/traffic.txt`. The probe carries
a second accumulation beside the shipped one, so one run reports both and
changes nothing.

| | netlib (94) | infeas (29) | Kennington (16) |
|---|---|---|---|
| saturating sites | 9556 | 1408 | 2 |
| rows reaching the frozen-row test | 16618 | 1894 | 602 |
| of those, `row_traffic == inf` | **9008** | 1344 | 2 |
| infinite value READ by either consumer | **0** | **0** | **0** |
| worst repaired traffic on a zero-margin row | **660** | 1 | — |

The instrument is dead on more than half the rows it exists for. No consumer
ever reads a saturated row on any of the three sets, which the file argued and
this measures. And the repaired accumulation recovers the number the row
really carries: `greenbea` row 57 reads **660**, the figure the frozen-row
test's own comment names for that row, where the shipped form reads `+inf`.

**It is two repairs, not one, and the second has nothing to do with
infinity.** An end that was ALREADY infinite is not subtracted from at all, so
the magnitude aimed at it moved nothing and does not belong in the budget: a
`<=` row taking `cmax = 5` against `cur_rl = -inf` used to charge 5 for a
subtraction that never happened. That failure mode is this site's alone — the
other two producers subtract the same term from both ends, so charging it in
full is exact there whichever end was finite. Only this site aims different
magnitudes at the two ends.

**The cost is nothing, on all three sets.** 94, 29 and 16 instances
bit-identical to the committed record, 0 digest changes, `gate: PASS` with
`0 regressed, 0 improved, 0 new` on each. The expected result for a change
with no reachable read, run as the check rather than assumed.

**A claim in `TODO.md` was wrong and is corrected there.** It said "**all** 117
standard-set rows that reach the frozen-row test at exactly zero margin carry
`row_traffic == inf`". It is **110 of 117**. The item's substance is
unaffected; the word "all" is not.

**What was refuted, and it is the entry's value.** The assert this repair
carries had two wrong versions before the shipped one, both caught by
`numerics-reviewer` on the diff:

- **Placed at the site the repair touched.** After the repair that site's own
  contribution is finite by construction, so the assert could only fail
  because of a term added at one of the other two producers — and a row
  poisoned by those need never host a cost-0 singleton column. "0 aborts on
  139" would have meant "the traffic was finite at rows that had a cost-0
  bounded singleton column". It is a sweep over every row after the round loop
  now, and the predicate is what a consumer needs: if either end is still
  finite, the budget that bounds it is a number.
- **Defended by a structural argument that does not hold.** An overflow at the
  other two producers was said to drive BOTH ends to the same infinity, so the
  antecedent would be false and the assert could not fire on a legal model.
  `row_traffic` sums magnitudes while the bounds sum signed values, so two
  terms of opposite sign cancel in the ends and add in the budget. From
  `rl = ru = 0`, terms of `+1e308` and `-1e308` leave both ends at 0 and the
  traffic at `+inf`, and the assert fires. `min x3 s.t. 1e308*x0 - 1e308*x1 +
  x2 + x3 == 0` with x0 and x1 fixed at 1 is a model producing it, and every
  value in it passes jaos's validation. Confirmed by compiling it here rather
  than on the reviewer's word.

So the assert rests on **measured headroom**: the largest traffic any row of
the three sets carries is 1e7, against a `DBL_MAX` of 1.8e308. The measured
number is what survives someone changing the fixed-column site; the argument
would not have.

Validated both ways, with the negative control re-run against the shipped
placement rather than inherited from the earlier one: 0 aborts over all 139
instances with the repair, and **45 of 94** netlib instances abort with the
accumulation reverted and the sweep kept. It fires wherever an end is finite,
which is not everywhere — a row whose two bounds were already infinite absorbs
nothing, so the old form saturated it and the sweep stays quiet there.

**The assert was moved after the campaign ran, and that was checked rather
than waved through.** `comment_only.sh src/presolve.c df2054e` reports the
release object unchanged, because `-DNDEBUG` removes an `assert`. The campaign
carries over verbatim.

**What is left open**, handed to `TODO.md`:

- **The third consumer needs two traffics, not one.** Replacing the
  frozen-row test's `ps_bound_scale` with the traffic repeats on one side the
  mistake the file already names on the other: that test compares
  `min_act`/`max_act` against `cur_ru`/`cur_rl`, which are two sums with two
  different traffics. `row_traffic` covers only the bound half; the activity
  half is `rg.traffic`, the quantity `ps_row_tol` already uses. The window has
  to cover the larger, the shape the singleton-row fold already has. The
  repaired traffic is up to **153** times the bound scale on netlib, so this
  is a real change needing its own campaign.
- **The sweep does not cover the two live reads**, which happen inside the
  round loop. Traffic only grows, so the sweep is strictly stronger on that
  half; the antecedent goes the other way, and a row whose end was finite when
  it was read and is infinite by the end passes the sweep.
- **Two silent fallbacks guard a condition the file calls unreachable.** Both
  consumers substitute or skip when the traffic is not finite. An assert in
  each turns two comments into checked claims, and it matters more now: the
  repair makes the traffic finite where it was not, so those guards change
  from "never taken" to "never taken for a different reason".

## D156 — The destroyed row width is refused as a defect, because the width that dies was already below one ulp of the activity it constrains

**The question.** `TODO.md`'s second smaller item, and `src/presolve.c` said
the same thing beside the implied-free family's guard: two of the three sites
that shift a row's bounds subtract the same term from both ends, so the width
`ru - rl` is invariant in exact arithmetic, but `1 - 1e17` and `2 - 1e17` are
the same double. A row the caller wrote as `[1, 2]` can reach the simplex as
an equality. Both places said the answer is then "wrong by up to that width".
The expectation was a repair: refuse the shift that destroys a width.

**Refused, and the claim is what changes.** Nothing in the solve is touched.

**The instrument first, because a zero from it is the whole finding.**
`width-case.c` is two models one line apart — `1e17*x0 + x1 in [1, 2]` with x0
fixed, and the same at `1e3`. `ulp(1e17)` is 16 so the first collapses and the
second does not. The probe reads `lost=1 destroyed=1` on the first and
`lost=0 destroyed=0` on the second.

**It never happens on the three sets.** 320, 30 and 8592 same-term shift
events on finite-width rows across netlib, netlib-infeas and Kennington, and
**0** of the 8942 changed a width by any amount. The subtraction is exact
whenever the bounds and the shift are within a factor of two of each other,
which they always are here.

**And when it is forced to happen, the answer does not move.** Seven shapes
run on the normal build and on `-DJAOS_NO_PRESOLVE`: width destroyed with the
surviving column at the shift's scale, the same with a 1e17 coefficient, an
inequality with no width to lose, the `1e3` control, and two amplifying shapes
with the surviving singleton at `a = 1e-6` and `a = 1e-12`. **Six of seven are
bit-identical.** The seventh differs in one variable's value at the same
objective and the same other variable: alternate optima on a degenerate model.

**The bound, which is what the item was missing.** The width dies only when
`fl(rl - t)` and `fl(ru - t)` are the same double, so `ru - rl` is below one
ulp of `rl - t`. And `rl - t` is the activity the surviving columns must
produce. It cannot be small: a shift close enough to the bounds' own scale to
leave `rl - t` small subtracts exactly, by Sterbenz, and loses nothing. So
width loss requires `|rl|, |ru| << |t|`, hence `|rl - t| ≈ |t|`, and **the
width that dies was already below the resolution of the quantity it
constrains**.

**What was refuted along the way**, and it is the half that had to be
measured. The obvious objection is that a surviving singleton with a tiny
coefficient amplifies: dividing by `a = 1e-12` turns a row-space width of 1
into a column-space width of 1e12. That is exactly the mechanism that makes
§1's collapsed fold unbounded, so it is not a hypothetical. It does not apply
here, because the same division multiplies the ACTIVITY by `1/|a|` too and the
relative error is unmoved. Cases F and G measure it and are bit-identical to
the reference build.

**Why §1's collapsed fold is not the same shape**, since the two look alike
and only one has a bound. The fold's error is
`4 * DBL_EPSILON * row_traffic[i] / |a|`, relative to the row's **traffic**,
and cancellation can put the traffic far above the activity — a row summing to
1 out of terms of size 1e9 carries a traffic of 1e9. There the division
amplifies an error never tied to the value it perturbs, and §1 records a case
reading 0.89. Here the error is relative to the **activity** and the division
scales both. That is the whole difference.

**The repair that was NOT made**, stated so nobody builds it. Refusing a
width-destroying shift would be exact — no tolerance, no sweep — and free on
the gate, 0 firings of 8942. It buys nothing, because the shape it refuses
does not produce a wrong answer, and it would leave a fixed column in the
reduced model on the one class of input where presolve is most useful.

**What is left open.** Nothing from this entry. §1's collapsed fold keeps its
unbounded error and stays the largest open item; this entry sharpens why the
two are different rather than closing it.

## D157 — The two silent fallbacks become checked claims, and the check that catches them is the sweep rather than either read

**The question.** D155 left two things, both handed to `TODO.md` by
`numerics-reviewer`. The empty-row test and the singleton-row fold each
substitute or skip silently when `row_traffic[i]` is not finite, on a branch
the file calls unreachable — the shape that stops being true without anyone
noticing. And D155's own sweep runs after the round loop, so it cannot speak
for either read: traffic only grows, but an end that was finite when it was
read can be infinite by the end, and the sweep would pass.

**Both are the same predicate**, so both are one change.
`ps_traffic_usable(rl, ru, traffic)` is now asserted at each of the three
places that depend on it: the budget has to be a number wherever an end it
bounds still is, and a row whose two ends are both infinite constrains nothing
and is exempt. That exemption is also what keeps it quiet on an overflowed
`a * v`, since the two producers that can overflow subtract one term from both
ends without a guard.

**The measurement, and it says something the sweep alone did not.** With the
accumulation reverted to its pre-repair form, all three asserts live, netlib
aborts on 45 of 94 — and **it aborts at the sweep, not at either read**. So
the "unreachable" claim the two fallbacks carried is true from the assert side
as well, not only from the counter that measured 0 infinite reads in D155.
On the repaired tree: 94, 29 and 16 instances under `-UNDEBUG`, 0 aborts, and
`make configs` passes all five configurations.

**Nothing reaches the gate.** `comment_only.sh` reports the release object
unchanged, because `-DNDEBUG` removes an `assert` and the helper with it.

**What is left open.** Nothing from this entry. Both of D155's smaller
leftovers are closed; its third, the frozen-row test's scale needing two
traffics, is untouched and stays in `TODO.md`.

## D158 — The collapsed fold's midpoint is clamped into the column's box, which bounds the last unbounded item, and the branch runs 0 times in 100018 folds

**The question.** `TODO.md` §1, described there as the only open item whose
error has no stated bound. When a singleton row's implied interval collapses
inside the fold's rounding window, the midpoint of the two ends goes into both
folded bounds. `new_lo` is at or above `cur_cl[j]` and `new_hi` at or below
`cur_cu[j]`, but the midpoint of a **collapsed** pair need not lie between
them: the branch admits a gap of up to `btol`, so the midpoint sits up to half
of it past whichever bound it crossed. `btol` carries `row_traffic[i] / |a|`
and nothing caps that. The stated size was
`4 * DBL_EPSILON * row_traffic[i] / |a|`, with a shape reading **0.89** — a
published value nearly a whole unit outside a bound the caller declared,
which `jaos.h` promises does not happen.

The item said it needed a decision rather than a patch, because the midpoint
is symmetric in the two ends and whatever replaces it has to keep that.

**The repair keeps it.** The midpoint is unchanged and then clamped into
`[cur_cl[j], cur_cu[j]]`. The clamp reads the box and not which end was
tightened, so mirroring the model mirrors the result. That is what makes a
clamp admissible here where choosing one end would not be.

**Which end gives way is D152's argument, with one qualification that entry
did not need.** On the FIRST fold into a column, `cur_cl[j]` and `cur_cu[j]`
are the caller's own numbers while `implied_lo` and `implied_hi` came out of
`cur_rl[i] / a`, a running difference divided by a coefficient — the derived
end carries the error. On a SECOND fold into the same column the box is itself
a previous fold's `rl/a`, so both ends are derived and that argument does not
apply. The clamp is still right there for a different reason: the box is what
every other rule in the file has already been told, and a value outside it is
a value no later reduction can reason about. Raised by `numerics-reviewer`.

**The instrument first, because its zero is the whole finding.**
`fold-case.c` is one shape at three magnitudes — `min x0 s.t. x0 >= rl0,
x0 in [0, 1e9]`. At `1e9 + 5e-7` the probe reads `collapse=1 out_orig=1
worst_out_orig=2.38e-7`, which is the figure `tests/test_presolve.c` already
carried for that model, arrived at independently. At `1e9 + 0.4` the fold is
refused before it happens; at `1e9 - 1` it folds without collapsing. One
control folds and does not collapse, so the probe is not counting folds.

**The branch never runs on the three sets.** 8622, 9750 and 81646
singleton-row folds on netlib, netlib-infeas and Kennington, and **0**
collapses in all 100018. So the repair is provably a no-op on the gate before
the campaign, and the campaign is the check rather than the question: 94, 29
and 16 instances bit-identical, 0 digest changes, `gate: PASS` with
`0 regressed, 0 improved, 0 new` on each, and `make configs` passes all five
configurations.

**The collapse itself is unchanged**, which is the half that could have gone
wrong. The reproducing model stays `OPTIMAL` rather than becoming
`INFEASIBLE`; only where the point lands moved, from `1e9 + 2.4e-7` to `1e9`.
A repair that turned a solvable model into a refused one would be the
mirror-image catastrophe this file names elsewhere, and it is checked rather
than assumed.

**The pinned test moved deliberately, and it asked to be.**
`test_a_fold_onto_the_box_at_scale_still_collapses` asserted
`x[0] <= 1e9 + window` and said in as many words that x0 is not inside the box
and that this was the open item. Its own comment named this repair as the
condition for tightening: "If the containment item is closed later, this
assertion still holds and the one above it can be tightened to the box." It is
`x[0] <= 1e9` now. What the test still turns on is the collapse happening —
`1e9 + 5e-7` is four ulps inside the eight-ulp window, so a
`PRESOLVE_ROUND_ULPS` below four stops the collapse and fails the `OPTIMAL`
assertion above first.

**It closes §1's DUAL half as well, which was not the claim and is the
entry's best result.** That half is: a collapsed record leaves a dual bound no
record owns, so when the reduced cost's sign points at the other side no
record pays and the cost is left on a column strictly inside its own box. The
mechanism turns out to be the same one. Two singleton rows folding into one
column leave the second fold's midpoint strictly inside the box the first fold
left, so no record's recorded bound equals the published value; the clamp puts
that value back ON the first fold's bound and restores ownership. Measured on
`x0 >= 5, x0 <= 5 - g, x0 in [0, 10]`: at `g = 4e-15`, `max_dual_violation`
goes 1 to 0 and `dual_feasible` false to true. Found by `numerics-reviewer`.

**`TODO.md`'s own recorded model for that half no longer reproduces
anything**, on either tree. It used `g = 1e-13`, and the window at scale 5 is
`8 * DBL_EPSILON * 5 = 8.88e-15`, so the gap is eleven times too wide and both
builds refuse the model outright. It is replaced there with a gap that reaches
the branch. A reproducing model that stopped reproducing is worse than none,
because the next reader runs it and concludes the defect is gone.

**What it costs: the residue moves onto the row, multiplied by |a|.** It is
not removed. The column violation is in x units and the row's in a*x units, so
for an admitted gap `g` in x units the midpoint splits it `g/2` and `|a|*g/2`
while the clamp puts `0` and `|a|*g`. The worst of the two therefore changes
by `2|a| / max(1, |a|)`: it doubles at `|a| >= 1` and **shrinks** below
`|a| = 0.5`. Measured at one gap and two coefficients rather than derived — at
`a = 4` the worst side goes 3.34e-6 to 6.20e-6, and at `a = 0.25` it **halves**,
7.15e-7 to 3.87e-7.

At `a = 1` that costs a verdict: `x0 >= 1e9 + 1.5e-6` against `x0 <= 1e9` reads
col 7.15e-7 / row 8.34e-7 before and col 0 / row 1.55e-6 after, so
`primal_feasible` at an absolute `CHECK_TOL` of 1e-6 goes from true to false.
**That is the honest reading and not a regression**: the model is short by the
whole gap whatever is published, the midpoint was not reducing the violation
but keeping both sides under a tolerance neither deserved to pass, and the
caller now gets a point inside the box they declared with the residue
reported.

**The first statement of this cost was wrong and it is worth recording why.**
It said "the model is infeasible by the whole 1.5e-6 whichever point is
published", which is the `|a| = 1` special case — and both models in the first
cost table had `a = 1`, so the table could not have shown it. Caught by
`numerics-reviewer`, who measured the two coefficients above. It was generous
to the clamp below `|a| = 0.5` and harsh above 1, in a sentence that read like
a general bound.

**What was refuted — the first version of this repair aborted on legal
input.** `include/jaos.h` says an inverted column box (`xl > xu`) is legal and
is to be reported infeasible rather than refused at load. With the bounds
crossed, `new_lo > new_hi` holds for any row at all, so the collapse branch is
reached on a model that has nothing to do with rounding, and the first
version's `assert(fold_lo >= cur_cl[j] && fold_hi <= cur_cu[j])` fired there —
in a configuration `make test` and `make sanitize` both build. Nothing in the
suite covered it: the only other inverted-box model, `tests/test_model.c`'s
`[5.0, 1.0]`, has a gap about 4.5e14 times the window and takes the INFEASIBLE
branch without ever reaching the collapse, which is why 221 of 221 passed.
The clamp is skipped on an inverted box now — there is no point to clamp into,
and the ternary would have returned whichever end it tested first, which is
also where the mirroring symmetry broke. Found by `numerics-reviewer`;
`test_a_collapse_on_an_inverted_box_keeps_the_midpoint` pins it, validated by
removing the guard and watching the binary abort.

**What is left open.** Nothing from §1. Both halves are closed.

## D159 — The frozen-row window is scaled by what the comparison is made of, and presolve stops refusing a model the solver can solve

**The question.** D155 made `row_traffic` a live quantity, and `TODO.md` asked
whether the frozen-row feasibility test should use it. The expectation was a
tolerance improvement against a latent risk.

**It is a wrong answer, not a latent risk.** The test compares
`min_act`/`max_act` against `cur_ru[i]`/`cur_rl[i]`. Those sides carry error
from different places — the activities are a sum over surviving columns, the
bounds are running differences every removed column shifted by its own `a*v` —
and the window was the MAGNITUDE OF ONE OPERAND, which bounds neither. On

```
min x1  s.t.  1e9*x0 + x1 + x2 <= 1e9,  x0 in [1,1],
              x2 in [0,1] cost 0 (freezes the row),  x1 in [1e-10, 10]
```

presolve reported INFEASIBLE and `-DJAOS_NO_PRESOLVE` reports OPTIMAL. `1e-10`
is about a thousandth of one ulp of 1e9, an infeasibility the arithmetic
cannot represent. A solvable model refused is the shape this file elsewhere
calls the mirror-image catastrophe, and there is nothing downstream to recover
it.

**The campaign cannot see it, which is why it sat.** Over 19114 frozen rows on
the three sets the candidate window exceeds the shipped one on 6934 of them by
up to 45930x, and **flips no verdict on any set**. Both shapes were swept
rather than the wider one assumed correct: the bound half alone reaches 153x
over 842 rows, and adding the activity half reaches 45930x over 6092 more.
Neither moves a verdict. The four genuine infeasibilities in netlib-infeas
stand at 5.63e14 times the shipped window.

The gate is bit-identical on all 139 and all five build configurations pass.

**The absolute window is the figure that says how far the widening can go, and
every other number is a ratio.** The widest window this produces is **6.5e-8**
on Kennington, against `PRIMAL_TOL` 1e-7 and `CHECK_TOL` 1e-6 — under both, by
a factor of 1.5 rather than by decades. netlib and netlib-infeas do not move in
absolute terms at all. A set carrying a larger `rg.traffic` than Kennington's
3.66e7 is the reopen condition.

**What was refuted, and two of the three changed the change.**
`numerics-reviewer` returned four findings, all reproduced here before being
accepted.

- **The negative-half test could not fail.** Its first version used a model
  whose frozen row keeps a live column, so the simplex refused it at
  `PRIMAL_TOL` whatever the window did: at `ROUND_ULPS = 1e12` it still read
  INFEASIBLE. It was reading the pipeline rather than the window, so **the
  widening had no guard at all**. Replaced with a model whose row is EMPTIED,
  where the frozen-row test is the last word — `1e9*x0 + x1 == 1e9 + 100` with
  x1 in [0,3] and cost 0. Infeasible on every build, and OPTIMAL at
  `ROUND_ULPS = 1e12`, so it can fail.
- **`8 * DBL_EPSILON` is a scale claim and not a bound.** `cur_rl[i] -= a*v`
  is a plain running sum with no compensation, so after k removals the error
  goes with `k * eps * scale`; eight ulps covers k of about three, and a row
  with a hundred removed columns is understated by roughly 12x. This file says
  so for the same quantity at `ps_verify_row_activities`, which multiplies by
  `nnz - 1`. The direction is the loud one, so it is carried rather than
  blocking.
- **The measurement is a ratio between windows, not a measurement of the
  error.** The first version of the comment said the latter. Computing the
  error would need a higher-precision recomputation the probe does not do.
- **A paragraph three lines above the change said the opposite of it**, from
  before D155 removed its premise.

**And a second live wrong answer, in the same shape, at a site this change does
not touch.** The activity pass uses `ps_row_tol(&rg)`, which is
`8*eps*rg.traffic` alone, so the bound side is uncovered there in mirror image.
The same model with one cost changed from 0 to 1 — which stops the row
freezing — reads INFEASIBLE on the shipping build and OPTIMAL on the reference
build. The repair is not a copy of this one, because that window is shared with
FORCING and REDUNDANT and widening the forcing window is what cost 02-04 a
campaign. It is `TODO.md`'s, with its model.

**The instrument was wrong, and that is the entry's other half.** `bench/run
-j N` forks children sharing one stderr; `fprintf` with many conversions
issues several writes, so a line is torn, `grep` still counts it, and `awk`
parses the fragment only — **the sums come out low**. The same source rebuilt
and re-run gave 4858, 4844 and 4798 for one counter with the line count intact
at 188 every time. A count that can only be undercounted is exactly the wrong
shape for a probe whose finding is `0 verdicts flip`. Fixed with one `write(2)`
per record, verified across four optimisation levels and both `-j` settings.
**D156's and D158's readings were re-taken with the fixed instrument** because
both had landed and both reported zeros; both stand, identically at `-j 12` and
`-j 1`.

**What is left open**, both handed to `TODO.md`: the activity pass's own
window, and the missing k factor in `PRESOLVE_ROUND_ULPS` on a row with many
removals.

## D160 — Clause 1 of the activity pass gets its own window, and the bound scale that looked like symmetry published a wrong answer

**The question.** D159's defect at the site that is not frozen, found by
`numerics-reviewer` while reviewing D159. The activity pass computed
`rtol = ps_row_tol(&rg)` once — the ACTIVITY half — and used it for all three
clauses. Clause 1 compares `min_act` against `cur_ru[i]`, a running difference
every removed column shifted by its own `a*v`, which nothing covered.

D159's model with one cost changed from 0 to 1, so the row never freezes,
read INFEASIBLE where `-DJAOS_NO_PRESOLVE` reads OPTIMAL. A second solvable
model refused.

**Only clause 1 can take a wider window**, and the reason is direction. Clause
1 tests `min_act > ru + rtol`, so wider fires LESS. FORCING tests
`min_act >= ru - rtol` and REDUNDANT its mirror, so wider fires MORE — and
widening the forcing window is what pinned `pilot` column 3554 and cost 02-04
a campaign. Clauses 2 and 3 keep `rtol`.

**The measurement.** 3307656 rows reach this pass over the three sets. The new
window is wider on 81376 of them and **flips 0 verdicts**, and the widest
ABSOLUTE window is unchanged on every set. The eight genuine infeasibilities in
netlib-infeas fire under both. The gate is bit-identical on all 139 and all
five build configurations pass.

**That measurement is a no-op result and says nothing about the rescued
band**, which no row on the three sets is in. The evidence for that band is
constructed, and the first version of it was wrong.

**What was refuted — the first version published a wrong answer, and it was
one commit from landing.** All three found by `numerics-reviewer`, all three
reproduced here on four builds before being accepted.

- **`ps_bound_scale(rl, ru)` in the window.** Added "for symmetry with D159".
  On `-1e12 <= x0 + x1 <= 0` with x0 in [1e-3, 1] — infeasible by 1e-3, with
  nothing ever removed — it gave a window of 1.78e-3 taken **entirely from the
  row's LOWER bound for a test on the UPPER side**, and published `optimal`
  with an objective of 0.001. `ps_bound_scale`'s own comment says it is for a
  comparison between two BOUNDS; clause 1 compares a computed activity against
  one. Dropped outright rather than narrowed, because there is no third error
  term for it to cover at any size: `min_act` carries `eps * rg.traffic`, `ru`
  carries `eps * row_traffic[i]`, and both are already in the max.
- **`ps_round_tol` put clause 1 on the `EXTRA_CFLAGS` sweep hook**, which
  `ps_row_tol`'s comment and `docs/tolerances.md` both forbid for the
  activity-range readings. 02-09 did it for a few hours and review caught it,
  so this is the second occurrence. Behavioural rather than tidy: `itol` could
  fall BELOW `rtol` at a lower setting and invert the clause ordering, and the
  case above flipped between `ULPS=8` and `ULPS=4`. A literal 8 is immune at
  1, 4, 8 and 64.
- **"A wider window fires less, so it can only stop a refusal" is the
  incomplete half that let the first one through.** Every rescued row reaches
  FORCING with its condition already true, by construction, so it is pinned
  and deleted — and D159's safety argument does not transfer, because there
  the row survives for the simplex to re-test and here it does not.

**Three tests, where the first version had one.** The accept case asserts the
ANSWER rather than the status, because the model reaches OPTIMAL through
FORCING pinning two columns and a wrong pin would leave the status green. The
`-1e12` case must read INFEASIBLE and fails on the version that was reviewed.
And the same model at a shortfall of 1.0 pins the window from the tight side,
which an accept test cannot.

**`make configs` caught a fourth thing the plain build could not**: the accept
test asserts an exact answer and so needs the fault-build guard every other
positive test in the file carries. That is its second catch this session.

**What is left open.** One, and it is the same term at D159's own site.
`TODO.md` carried it with the model; D161 closed it.

## D161 — The frozen-row window drops the far bound too, and that defect predates D159, which widened around it

**The question.** D160 dropped `ps_bound_scale` from the activity pass's
clause 1 after it published a wrong answer. `numerics-reviewer` then asked
whether the same term does the same thing at the frozen-row test, which is
where D159 kept it.

**It does, and it has since that window was written.**

```
min 0  s.t.  -1e12 <= x0 + x1 <= 0,  x0 and x1 both cost 0 in [1e-4, 1]
```

Both are cost-0 bounded singletons, so both relax and freeze the row, which is
then empty. Infeasible by 2e-4, and the frozen-row test is the last word,
because an emptied frozen row is deleted with everything else and the simplex
never sees it. `ps_bound_scale(-1e12, 0)` is 1e12, so the window was 1.78e-3
and swallowed it — **taken entirely from the row's LOWER bound for a test on
the UPPER side**.

| tree | verdict |
|---|---|
| before D159 (`0ac44fd`) | **optimal**, x = {1e-4, 1e-4} |
| the parent of this change (`0078244`) | **optimal** |
| the working tree | INFEASIBLE |
| reference build, the oracle | INFEASIBLE |

**The control is what makes it a measurement.** The same model with
`rl = -INFINITY` reads INFEASIBLE on all four trees, which is only explicable
if the finite lower bound was supplying the number.

**Why D159 missed it, stated rather than glossed.** `ps_bound_scale` was the
shipped window there, and D159 widened around it — keeping a term that can only
add width is safe by the reasoning D159 was using, so it never asked whether
the term belonged. D160 was forced to ask two entries later, when the same term
was a NEW source of width and published `optimal` on a model infeasible by
1e-3. The question was available at D159 and was not asked.

**The repair is a subtraction and it narrows the window**, which is the
direction that refuses good models, so it is the direction that had to be
checked. 94, 29 and 16 instances bit-identical, 0 digest changes, `gate: PASS`
on each, five build configurations, 227 tests. Every one of D159's own
frozen-row tests still passes, including the one that exists to catch a window
that has become too narrow and the one that sits on the boundary with zero
slack.

Dropped rather than narrowed, for D160's reason: there is no third error term
for it to cover at any size, since `min_act`/`max_act` carry `eps *
rg.traffic` and `cur_rl`/`cur_ru` carry `eps * row_traffic[i]` and both are
already in the max.

**That last sentence is wrong and D162 corrects it.** There is a third error
term: `cur_rl`/`cur_ru` carry `k * eps` times the partials they walked through,
not `eps * row_traffic[i]`, and the magnitude of the end being tested is part
of that. D162 puts it back multiplied by the shift count, which is zero on this
entry's model and leaves every reading above unchanged.

**What is left open.** Nothing from this entry.

## D162 — A row bound is a running difference, so the window counts the terms — and the end it is testing comes back in, multiplied by that count

**The question.** `numerics-reviewer` raised it while reviewing D159 and
`TODO.md` §4 has carried it since: `cur_rl[i] -= a*v` is a plain running sum
with no compensation, so after k removals the error goes with `k * eps * scale`
and not with `eps * scale`. Three windows judge one of those numbers and all
three counted a fixed eight ulps, which covers a k of about three.
`ps_verify_row_activities` already multiplies by `nnz - 1` for the same
quantity and is the shape to copy.

Expected: a widening nothing measures, landed for coherence. What it turned out
to be is a live wrong answer, plus two wrong shapes on the way to the right
one.

**The count is real.** Over the three sets
(`bench/measurements/02-72/shifts.txt`), the largest number of shifts on one
row is **250** at clause 1 of the activity pass, on netlib, and **325** at the
frozen-row test, on Kennington. **804 rows** carry more than the eight ulps the
window paid for.

**The model, and it is a false INFEASIBLE.**

```
row R:  x0 + x1 + (k smalls) + w1 + w2  ==  k*2^-25 + 1e-7
row S:  x1 + z                          == -1e9

x0      fixed at +1e9        x1  in [-1e9-1, -1e9+1], not fixed at load
smalls  fixed at 2^-25       w1, w2 in [0, 2e-7], cost 1
z       fixed at 0, which is what delays x1 by one round
```

Presolve removes x0 and all k smalls in round 1 while x1 is still free, so each
small is a quarter of an ulp of an accumulator of magnitude 1e9 and rounds
away. Round 2 folds row S, fixes x1 and only then subtracts it. `cur_rl` comes
back to `k*2^-25 + 1e-7` where the truth is 1e-7.

| k | the parent (`4c5f58f`) | with the count |
|---|---|---|
| 128 | not refused | not refused |
| **256** | **INFEASIBLE** | **not refused** |
| 512 | INFEASIBLE | not refused |

A pin, not one reading: 128 and 256 are one step apart in what separates the
two windows. The control — the same shape 1e-2 from any feasible point — is
refused on every build at every k.

**The oracle cannot arbitrate this one, and the entry says so rather than
dressing it up.** `-DJAOS_NO_PRESOLVE` refuses the model at every k, including
the k where the shipped window already accepts, because the solver sums the row
in column order and loses the same terms presolve lost. What settles it is that
the feasible point is exactly representable and checkable by hand: `x0 = 1e9`,
`x1 = -1e9`, every small at `2^-25`, `w1 = T - 2^-17`, `w2 = 0`, and the
activity is exactly `T`. The published objective is not the true optimum on any
build either, so the test asserts presolve's outcome and nothing else.

**Two shapes were built and both are wrong. This is the part that pays.**

| | the count multiplies | verdict |
|---|---|---|
| A | the row's traffic | **short**, and no measurement says so |
| B | `ps_end_scale(the end being tested)` + the traffic | **ships** |
| C | `ps_bound_scale` — the larger end — + the traffic | **refused by D161's own test** |

**A is short.** The rounding at each step is half an ulp of the PARTIAL SUM,
and a partial is bounded by `|row_lower[i]| + traffic`. The argument for A was
that the comparison only comes near firing when `cur_rl[i]` is small; it is
false, because at the activity pass and the frozen-row test `cur_rl[i]` is near
the ACTIVITY there, which can be any magnitude. A row of activity 1e9 with 300
removals totalling 0.9 of traffic carries about 1.8e-5 of error against a
traffic-only window of 6.8e-14. Refuted by working the case, because no row on
the three sets is near enough to any window for the sets to separate the two.

**C brings D161's defect back through the count**, and `make configs` caught
it: on `-1e12 <= x0 + x1 <= 0` the two cost-0 singleton relaxations are two
shifts, so `2 * eps * 1e12 = 4.4e-4` of window lands on the UPPER side against
an infeasibility of 2e-4, and
`test_a_frozen_rows_window_ignores_the_far_bound` went red. The two ends walk
through different partials, so anything scaled by an end has to say which end.
That is `ps_end_scale`, and it is the whole difference between C and B.

All three vanish at k = 0, which is what keeps D161 for a row nothing was ever
removed from.

**B and C are the same number on every row of all three sets** — worst C/B
ratio exactly 1 at all nine site-set pairs. So the population cannot separate
the shape that ships from the shape that is wrong, and no row anywhere had a
positive residue that passed on its window. A green campaign says nothing about
the shape of a window here; only the constructed models do.

**The cost.** `gate: PASS` on all three sets with `0 regressed, 0 improved, 0
new`; 94, 29 and 16 instances **bit-identical to the committed records**, 0
digest changes; the twelve genuine infeasibility firings in `netlib-infeas`
fire under every shape. Five build configurations. The widest ABSOLUTE window
moves 1.776e-08 → 2.247e-08 on netlib's frozen-row test and 6.494e-08 →
6.587e-08 on Kennington's, both under `PRIMAL_TOL` 1e-7; clause 1's widest does
not move on any set, because the rows carrying it are not the rows carrying the
shifts.

**A shift of exactly zero is not counted.** `x - 0.0` is exact, so charging it
would widen a window for an error that was never made.

**`numerics-reviewer` delivered after this entry was committed**, and the
sentence here first said it had not delivered at all. Four requests went
unanswered while the change was open, so the read was done in the main context
and it is what found both A and C. The review then arrived with two findings
the main context had missed, both wrong answers, and both are D163. The lesson
is the timing and not the quality: a review that lands after the commit costs a
second campaign, and this is the second time it has landed late here (D126,
D127 and D128 were the first, where it returned nothing at all).

**What is left open**, both to `TODO.md`:

- **The solver's own row activity loses terms the same way.** It sums in
  column order, so it refuses the model above on every build. That is the
  reason this entry has no reference-build disagreement to show, and it is a
  defect in the feasibility test rather than in any presolve window.
- **The reopen condition is the absolute window, not a ratio.** A set carrying
  a larger `rg.traffic` than Kennington's 3.66e7, or a shift count far above
  325, is where these windows stop being comfortably under `PRIMAL_TOL`.

**Two things in this entry are wrong and D163 corrects them**, both found by
the review that arrived after the commit. The count reached three reads of the
running difference and there are four; the singleton row's fold judges
`cur_rl[i] / a` and did not carry it. And this entry says the shift term is
zero at k = 0 "and that is what keeps D161", which is true of `ps_shift_excess`
and not of the window: the base moved from `8*eps*max(act, traffic)` to
`8*eps*act + 8*eps*traffic` as well, which is 8 ulps of 1 wider at k = 0.

## D163 — The singleton row's fold is a fourth read of that running difference, and a count cannot cover an error that arrives inside a value

**The question.** `numerics-reviewer` delivered on D162 after that commit had
landed. It confirmed both of D162's own repairs and returned two findings
against them, each with a model built from the source. Both are wrong answers
and both were confirmed here by running them
(`bench/measurements/02-73/`).

**And this time the oracle arbitrates.** D162's model removed every column of
its row, so the solver's own summation made the same error and
`-DJAOS_NO_PRESOLVE` refused it too. These models keep a live column, so the
reference build reaches the feasible point and disagrees with the shipping
build directly.

### The fourth read — repaired

```
x_big + (256 columns fixed at 2^-25) == 1e9,   x_big in [0, 1e9 - 2^-17]
```

`2^-25` is a quarter of an ulp of 1e9, so each of the 256 subtractions rounds
back and `cur_rl` stays at 1e9 against a truth of `1e9 - 7.6294e-6`. Round 2
folds the row onto `x_big` and asks whether `[1e9, 1e9]` meets
`[0, 1e9 - 2^-17]`: `1e9 > (1e9 - 2^-17) + 1.77636e-6` fires.

| | the parent (D162) | this tree | the oracle |
|---|---|---|---|
| feasible | **INFEASIBLE** | optimal, 999999999.99999237 | optimal, **999999999.99999237** |
| control, 1e-3 out | INFEASIBLE | INFEASIBLE | infeasible |

The repaired objective matches the oracle to the last bit. The scale at that
site was already right — `ps_bound_scale` of the fold's own pair or
`row_traffic[i] / |a|`, whichever is larger — so only the count was missing,
and it takes the end `tightens_lo`/`tightens_hi` says the running difference
supplied.

**What it costs, stated because it is the only thing this widening buys
against.** The gap the collapse branch below admits is whatever the refusal let
through, so a wider window leaves a larger residue on the row: `|a|` times the
gap, since D158's clamp puts the column back in its own box. On this model
5.86e-5 where it was 1.78e-6. D158 measured 0 collapses in 100018 folds over
the three sets and this leaves that at 0.

### The term D162's own test never exercised

D162 added `ps_end_scale` in its second revision, and **its test did not cover
it**: `cur_rl` lands at 7.75e-6 there, so `ps_end_scale` reads its floor of 1
and the traffic half carried the whole window. Replace `ps_end_scale` with a
constant 1.0 and that test stays green. Confirmed by doing exactly that.

The missing half is the same shape with two live cost-1 columns, so clause 1
judges the row instead of the fold. `row_traffic` is `2^-17`, below the floor,
and the bound is 1e9 — the whole window is `ps_end_scale`. Neutered it reads
1.776e-6 against a residue of 7.391e-6 and goes red.

**A control near the edge, which D162 also did not have.** Its control sat 1e-2
out against a window of 1.18e-4, a factor of 85. The new one is 2e-4 out
against 5.862e-5, a factor of 3.5, and it is refused on every build. That
matters at clause 1 specifically: a row rescued there is pinned by FORCING and
deleted, so an infeasibility missed at that clause is never re-tested.

### What is refused — a wider window for the chained error

```
row S:  x1 + (256 y_s fixed at 2^-25) == 1e9         x1 in [1e9-1, 1e9+1]
row R:  x1 + w1 + w2 == 1e9 - 63*2^-23               w1, w2 in [0, 2^-23]
```

Feasible exactly at `x1 = 1e9 - 2^-17`, `w1 = 2^-23`, `w2 = 0`. Round 2 folds
row S and **fixes x1 at 1e9**, wrong by 7.6294e-6, passing the fold's own test
because `new_lo == new_hi` there. Row R is then charged **one** shift at its own
traffic and clause 1 refuses it. Shipping build INFEASIBLE, oracle optimal at
1.1920928955078125e-07.

The count is right and the scale is right. The window is short because the
VALUE was wrong, and nothing in `ps_shift_excess` knows that. **Widening is
refused as the repair**: no window scaled by this row's own quantities can see
an error that arrived from another row. What it needs is an error weight
carried instead of a count — `row_err[i] += |a| * err(v)`, with the fold
recording `err` when it fixes a column — and that is a design. `TODO.md` has it
with this model as its statement.

### The cost

`gate: PASS` on all three sets, `0 regressed, 0 improved, 0 new`, 94, 29 and 16
instances bit-identical with 0 digest changes, five build configurations. Both
new tests validated against the tree that must fail them.

**Two smaller things from the same review.** The counted event and the performed
event are one expression now at both counting sites, rather than
`m->a_value[k] * v` written twice. And the probe printed `presolve=` under
`-DJAOS_NO_PRESOLVE`, where `jm_presolve_run` is the same code and only
`jaos_solve` stops consulting it — the shipping verdict was appearing in the
oracle's row. It reads `not consulted` there now.

**What is left open**, to `TODO.md`: the chained error above, and the solver's
own row activity losing terms in column order, which D162 opened.

## D164 — Carrying that error into the window is refused, because it publishes a point violating two rows by 7.5 times CHECK_TOL

**The question.** D163 refused "a wider window" for the chained error on an
argument and handed the repair to `TODO.md` as an error weight: `col_value_err`
set where a fold writes a derived end, `row_inherited_err[i] += |a| *
col_value_err[j]` at each subtraction, and every window adding it. This built
it and measured it.

**It works, and it is refused.** The measurement is
`bench/measurements/02-74/`.

**First, the route is real on real models and not only on the constructed
one.** Over the three sets: 8622, 9750 and 81646 folds write a derived end;
5408, 3706 and 75896 of those carry an error; and **324826 window reads on
Kennington alone** see a non-zero inheritance. The worst inheritance is
**1.143e+05 times the shipped window** on a netlib row. So the window is short
by five decades on instances that ship, not just on a model built to break it.

**And it flips nothing.** 0 verdicts spared on any of the 139, all 20 genuine
infeasibility firings in `netlib-infeas` intact, and **no absolute window moves
at all** at any of the four sites on any set — the row carrying the widest
window is never the row carrying the inheritance.

**Then the answer, which is what refuses it.** On D163's CHAIN model, feasible
exactly at `x1 = 1e9 - 2^-17`, `w1 = 2^-23`:

| tree | status | objective | row S residual | row R residual |
|---|---|---|---|---|
| the parent | **infeasible** | — | — | — |
| **the error weight carried** | **optimal** | **0** | **7.629e-06** | **7.51e-06** |
| reference build, the oracle | optimal | 1.1920928955078125e-07 | 0 | 0 |

**Both rows violated by 7.5 times `CHECK_TOL`, published as optimal.** The
parent's answer is wrong too, but a false INFEASIBLE is loud and does not break
`jaos.h`'s promise. This is the silent direction, and it is the failure
`src/presolve.c` names at the empty-row test reached from the other side.

**A wider window cannot repair a value that is already wrong.** The window
decides whether to refuse; it has no way to correct `x1 = 1e9` back to
`1e9 - 2^-17`. Widening it only stops the refusal and lets the wrong value
reach the answer. That is the whole of the refusal and it took building the
thing to see it — the argument in D163 said "a wider window is not the repair"
and could not say what it would do instead.

**What was learned about the instrument.** The probe's first reading printed
the fold's shipped window as `0 -> 5.536e-08`, a maximum of zero beside a
non-zero sum, which is impossible. The awk classifier matched extremes by the
suffix `_w$`, which does not match `D_wnow`, so that field was summed and
printed from the empty extremes table. **02-69 found this exact failure and
this script carries 02-69's warning as a comment.** Anchor every extreme by
name; a suffix rule silently reclassifies the next field anyone adds.

**What is refused, precisely.** Carrying the error into any of the four windows
that judge a row's bounds. Not refused, and neither built nor measured:

- **Compensating `cur_rl`/`cur_ru`.** They are the only uncompensated running
  sums in the file, while `ps_row_range` has used Neumaier for activities since
  02-04. If the bounds carry no error the fold's value is right, nothing
  inherits anything, and **D162's and D163's shift counts stop being needed** —
  it subsumes the class instead of adding to it. `long double` is unavailable
  (D34); Neumaier is portable and already here.
- **Widening the folded BOX rather than the window**, so the column stays a
  range and the simplex judges it. It reduces less and needs its own campaign.
  It is also §11b of `docs/research/dual-postsolve-imposed-bound.md`, the
  deliberate-slack direction, arrived at from a different question.

**The cost.** Nothing in `src/`: the mechanism was reverted and
`src/presolve.c` is byte-identical to D163's, so **no campaign is owed and none
was run**. What landed is one pinned change-detector,
`test_a_folds_value_carries_its_rows_error_into_the_next`, which asserts the
wrong answer JAOS gives today AND the right one the reference build gives, in
the same test — the repair announces itself there. `make configs` exits 0 on
all five configurations.

**What is left open.** Both directions above, in `TODO.md`, and the pin is what
will notice when either lands.

## D165 — The row bounds keep their residue, which removes the error four windows were widened to cover — and moves fourteen digests

**The question.** D164 named two directions and said the first was the one to
take: `cur_rl[i]` and `cur_ru[i]` were the only running sums in
`src/presolve.c` with no compensation, while `ps_row_range` has used a Neumaier
accumulator for activities since 02-04. Compensating removes the error rather
than covering it.

**The shape, because it is what made this cheap.** `ps_bound_shift` keeps the
residue and writes `sum + comp` back into `cur_rl[i]`, so **no read site
changed at all** — there are about fifteen, and a value compensated at some of
them and not others would be worse than no compensation. An infinite end is
left exactly as the uncompensated subtraction left it, because `(inf - inf)` is
a NaN and a NaN turns every comparison false, which is the failure mode where
an infeasible model is quietly accepted.

**What it changes, measured before building** (`bench/measurements/02-75/`).
The correction is non-zero on 5260 of netlib's 581826 window reads, 250 of
82726 on netlib-infeas, and **0 of Kennington's 2775394**. Worst correction
2.526e-12 absolute. **0 window verdicts move in either direction** on any set —
both directions measured, because a compensated bound can newly refuse as well
as spare and only one of those is safe.

**Kennington's zero is worth reading beside 02-74**, which reported a worst
inherited error of 2.08e-11 on the same set. That was the WINDOW's bound on the
error; this is the error. The gap between them says D162's and D163's windows
are far wider than these instances need.

**The probe under-predicted the cost and the reason is instructive.** It
measured window verdicts and folded values — 2 folds move, by one ulp — and did
not measure the reduced model's row bounds. That is the channel that carried
almost all of it: `p->reduced.row_lower[ri2] = cur_rl[i]` hands the compensated
bound to the simplex, so all 5260 corrected rows reach it. **A probe over a
value that is later COPIED somewhere has to follow the copy.**

**The diff is against a record twelve `src/` commits old, and that is only
legitimate because two earlier campaigns closed it.** `preflight.sh` warns on
exactly this and it is right to: a diff against a stale record credits every
commit since to your change. Raised by `jaos-measurer`, and the chain that
answers it, with R(t) the netlib record tree t produces and C the committed
record:

1. `git diff 52bdbbc cd68630 -- src/` is empty — D164 changed no source at
   all — so R(cd68630) = R(52bdbbc).
2. D163's campaign measured a worktree whose `src/` is byte-identical to
   52bdbbc's and read **94 bit-identical, 0 moved, 0 digest changes** against
   C. So R(52bdbbc) = C.
3. Therefore R(cd68630) = C and the diff below is parent-versus-candidate.

A tree at 52bdbbc reproducing C exactly is also what covers the ten earlier
commits, which this session did not run itself. **The chain only exists because
D162 and D163 each ran a full campaign that landed on C** — three consecutive
bit-identical campaigns are what makes a fourth one attributable.

**The cost.** netlib **79 bit-identical, 15 moved, 14 digest changes**;
netlib-infeas and Kennington bit-identical. `gate: PASS` and
`baseline: 0 regressed, 0 improved, 0 new` on all three, `record_diff.py` reads
no regression on all three, five build configurations.

**Work: geometric mean 1.0000x on every set**, best `capri` 0.9993x, worst
`bandm` 1.0000x. **Iteration counts are identical on all 15 moved instances,
and so are the presolve reduction counts** — the same trajectory of the same
length over the same reduced model, landing on different bits.

Four objectives moved in their last one or two ulps, each still `objective=ok`
against its Koch reference: `bandm` and `pilot87` toward it, `ship08s` and
`pilot` away, against gaps unchanged in every significant digit. `basis=`
changed on `bandm`, `capri`, `czprob` and `finnis`, all `det=ok`; the gate has
covered the basis since D150.

**The chained model, which is what this was for.** D164's model publishes
`optimal` at 1.1920928955078125e-07 with `x1 = 1e9 - 2^-17`, `w1 = 2^-23` and
**both rows at residual zero** — bit-identical to the reference build. D164's
refused window repair reached `optimal` too and got there with both rows
violated by 7.6e-06. The pinned change-detector D164 left behind fired on the
first build (`Expected 2 Was 1`) and now asserts the answer.

**The record moved and the baseline did not.** `bench/results/netlib.txt` is
rewritten in this commit because it is the record of what the tree produces;
`bench/netlib.baseline` is untouched, because the gate reads
`0 regressed, 0 improved, 0 new` against it and a baseline is rewritten only by
its own target, deliberately.

**`jaos-measurer` returned ACCEPT, after this landed, and it is the most
valuable of the three late deliveries in this session.** The main context had
already reached the same verdict — geometric mean from `scripts/geomean.py`,
per-instance read from `scripts/record_diff.py`, the four moved objectives
checked against the Koch reference on their own lines — and the entry first
said the review had not delivered. Four things it added that the main context
did not have:

- **It ran the parent.** All three sets at `cd68630`, md5-identical to the
  committed records, which turns the three-step argument above into a
  measurement. Its candidate `netlib.txt` is byte-identical to this one.
- **`ship08s` was an EXACT match to Koch and is now one ulp off.** The main
  context read it as "away by one ulp" without noticing it had been exact. The
  predicate is `|got - ref| <= 1e-6 * scale`, 1.92 absolute against a move of
  2.33e-10, so it is eight orders inside — but "exact to one ulp" is a
  different sentence from "one ulp to two".
- **`pilot`'s 2-ulp move is meaningless** against a pre-existing gap of 2.3e-5,
  which is 203 million ulps. Reporting it as "moved away" would have implied a
  precision that disagreement does not have.
- **The basis counts on the four instances whose hash moved**, below.

It also ran the negative control rather than reasoning about it: the candidate
test file on the parent source fails on exactly one test, at exactly the
assertion the repair moves.

**The basis, and one number in `TODO.md` is no longer exact.** Counting
`JAOS_BASIS_BASIC` through the public API on the four instances whose `basis=`
hash moved:

| instance | rows | before | after | excess |
|---|---|---|---|---|
| `bandm` | 305 | 328 | 323 | +23 → **+18** |
| `capri` | 271 | 277 | 277 | +6 → +6 |
| `czprob` | 929 | 932 | 930 | +3 → **+1** |
| `finnis` | 497 | 498 | 499 | +1 → **+2** |

**The count was already wrong on all four before this change** — it is D131 to
D139's open item, not this one's, and the reference build holds the contract on
both sources. By the measure `TODO.md` insists on, the count of solves
publishing a wrong basis, nothing moved: four wrong before, four wrong after,
and no other instance can have moved because `basis=` hashes exactly those
statuses. But `TODO.md`'s table records netlib's worst over-count as 23, that
23 is `bandm`, and `bandm` is now 18. The cell is annotated rather than
replaced, because the other 47 wrong-basis solves were not re-counted and
inventing a new worst from four instances would be worse than saying the old
one is stale.

**Closed by D166**, one commit later.

**What is left open**, to `TODO.md`: **the shift counts are now redundant and
still ship.** `row_shifts`, `ps_shift_excess` and `ps_end_scale` widen four
windows to cover an error that no longer exists. Removing them NARROWS those
windows, which is the direction that refuses feasible models, so it is its own
change with its own measurement — and D162's and D163's tests are what it has
to keep passing. Nothing here says those windows are wrong; it says they are
covering something that has been removed underneath them.

## D166 — The shift counts come out, and the tests they were built for passing without them is the evidence

**The question.** D165 left one thing open and named it: `row_shifts`,
`ps_shift_excess` and `ps_end_scale` widen four windows by `k` ulps to cover a
drift that D165 stopped happening. Two mechanisms covering one error, and one
of them with nothing left to cover.

**Removing them NARROWS four windows**, which is the direction that refuses
feasible models, so it needed its own measurement rather than riding along.

**Why it is not tidying.** At three of the four sites a window wider than its
error is recoverable — the row survives into the reduced model and the simplex
meets the infeasibility again. **At the emptied-row test it is not**: that test
is the last word, an emptied row is deleted with everything else, and nothing
re-asks. Too wide there accepts an infeasible model silently. Taking the count
out narrows it, and by construction rather than by measurement: each window was
`base + ps_shift_excess(...)` with that term non-negative.

**The evidence is D162's, D163's and D165's own tests.** The suite holds
exactly the models the counts were built for, each validated against a tree
that fails it, and all five pass with the counts gone:
`test_the_window_counts_the_shifts_and_not_only_their_scale`,
`test_the_shift_count_scales_by_the_end_it_is_testing`,
`test_the_singleton_fold_counts_the_shifts_too`,
`test_a_folds_value_carries_its_rows_error_into_the_next` and
`test_a_frozen_rows_window_ignores_the_far_bound`.

**A test going green when its repair is deleted normally means the test is
weak.** Here it means the defect was removed a second time, upstream, and what
distinguishes the two readings is D165's campaign: 15 instances moved, which is
the compensation reaching the reduced model. Without that, "the tests still
pass" would prove nothing.

**They had to be named individually.** `make configs` prints five section
headers and an exit status and nothing else, so "configs exits 0" cannot say
which tests ran — they are listed from `make test` in
`bench/measurements/02-76/counts-removed.txt`.

**The cost.** `gate: PASS` on all three sets, `baseline: 0 regressed, 0
improved, 0 new`, **139 of 139 bit-identical with 0 digest changes**. Five
build configurations. `src/presolve.c` loses 196 lines and gains 65: one array,
two functions and four window terms.

**And the 139 is not the safety evidence, which is worth stating because it
looks like it is** (`jaos-measurer`). A window that got NARROWER is only tested
by an instance landing in the band that was removed, and bit-identical says no
instance did. It is evidence of no cost, not evidence of no harm. The evidence
of no harm is the five tests above, because they are the models that land in
that band. The distinction matters here more than usual: at the emptied-row
test, being wrong is silent.

**No staleness question, and the contrast is the point.** `preflight.sh`
reports the netlib record one `src/` commit behind, and that commit is
`bd3b136` — the direct parent, because D165 rewrote the record. D165 itself
needed a three-step argument and an independent parent run from
`jaos-measurer` to establish the same thing. **A record rewritten when it moves
is what makes the next diff readable.**

**What this entry found that is not about tolerances.** `build/diag/wt-*` is
inside what `make clean` deletes, and `make clean` is what `make configs` runs
between its five configurations. A `jaos-measurer` campaign lost its entire
worktree to this change's `make configs`, mid-run, with no error on its side.
**44 of this repository's measurement scripts use that location**, from 02-28
onward; it is safe only while one thing runs at a time, and `CLAUDE.md` routes
campaigns to a subagent while the main context keeps working. The warning is in
`.claude/skills/jaos-measure/SKILL.md`, which is what `CLAUDE.md` says to load
before running or believing any campaign, and this directory's scripts use
`mktemp -d`. The older ones are not converted.

**What is left open.** Nothing from this entry. The class D159 opened —
a running difference judged by a window that did not know how it was
computed — is closed at all four sites, by compensation rather than by
tolerance.

## D167 — The published-basis count had been stale for a day, and nothing in the gate could have said so

**The question.** `jaos-measurer`, judging D165, counted the published basis on
the four netlib instances whose `basis=` hash moved and found `bandm` at +18
where `TODO.md` records netlib's worst over-count as +23. It said the cell was
no longer exact and that four instances cannot replace it. This re-ran
`bench/measurements/02-48/run-verify-count.sh`, the instrument that owns the
number, at three trees.

| tree | exact | **WRONG** | worst over | sum |
|---|---|---|---|---|
| recorded as D139 | 140 | **48** | +23 | +272 |
| `4c5f58f`, this session's starting point | 142 | **46** | +23 | +262 |
| `cd68630`, D164, before the compensation | 142 | **46** | +23 | +262 |
| `HEAD` | 142 | **46** | **+18** | **+250** |

**The table was already wrong when this session started.** Two solves were
fixed somewhere in D140–D161 and nobody re-read the number. It sat wrong for a
day in the file that calls it the largest open correctness item, and five
places in `TODO.md` cited it.

**Nothing in the gate could have caught it, and that is the part worth
keeping.** `bench/run.c`'s `basis=` is a hash: it detects a CHANGE and never
reports a COUNT. D150 widened the digest to cover the basis, which makes a
change visible per instance, and the count itself exists only when 02-48's
probe is run by hand. **A figure whose owner is a script nobody runs is a
figure that drifts** — the same shape D46 warns about for derived totals,
reached from the other side.

**D165 moved the worst and the sum, not the count.** `4c5f58f` and `cd68630`
read identically, so D162, D163 and D164 changed nothing here, which is what
three bit-identical campaigns predict. Between `cd68630` and `HEAD` the worst
goes +23 → +18 and the sum +262 → +250 while WRONG stays at 46 — four
instances' bases differ and two are less wrong. **By the measure this table
insists on in bold, that is not progress**: "the measure is the count of solves
publishing a wrong basis", and it did not move.

**The cost.** No source change. `TODO.md`'s table gains a row, its four other
citations of 48 become 46, and the D139 row is kept as history rather than
edited.

**What is left open.** The 46 are not attributed per instance here; the probe
reports set totals and the split is 02-49's. And the item itself is unchanged:
46 solves publish a basis that is not one, every local repair is refused
(D141), and what remains is a design wider than the firing row.

## D168 — The simplex accumulates its right-hand side with compensation, and the reference build stops calling a feasible model infeasible

**The question.** D162 built a model whose feasible point is exactly
representable and found that `-DJAOS_NO_PRESOLVE` refused it at every removal
count, including the counts where presolve accepts. That record could not use
its own oracle as a result, and said why: *"the solver's own row activity loses
terms the same way, which is §4 above and is now `TODO.md`'s"*. This closes it.

**Where the terms go.** `compute_primal` builds `-N x_N` by walking every
nonbasic variable in column order and adding its entries into the rows it
touches. A row is a slot that many columns write into, so the order the row
sees is the column order. A row that meets a large term before many small ones
loses the small ones outright: each is below half an ulp of the running total,
so each addition returns the total unchanged and the whole tail is dropped. On
D162's model 256 columns fixed at a quarter of an ulp of 1e9 vanish, the
activity comes back short by `2^-17 = 7.63e-6`, and nothing left in the model
can make that up — so the solve reads INFEASIBLE.

**It is D165's repair one layer out**, and the accumulator is the one presolve
already ships. `long double` would buy the same accuracy and break the
cross-machine determinism claim (D34); Neumaier is portable and its two-term
error recovery is exact under `-ffp-contract=off`. The compensation lives in an
owned `[nrow]` array rather than a borrowed one — every other `[nrow]` scratch
in `sx` belongs to a producer that runs inside the same refresh.

**The reading** (`bench/measurements/02-78/lost-terms.txt`), D162's model at
four counts:

| | k = 64 | 128 | 256 | 512 |
|---|---|---|---|---|
| reference build, parent | **infeasible** | **infeasible** | **infeasible** | **infeasible** |
| reference build, compensated | optimal | optimal | optimal | optimal |
| shipping build, compensated | optimal | optimal | optimal | optimal |

The control — the same shape 1e-2 away from any feasible point — is refused on
every build at every count. **The defect is visible in the reference build and
that is not a quirk of the model**: presolve removes those columns before the
simplex sees them and, since D165, subtracts them with the residue kept. The
build that was answering wrong is the oracle every presolve entry in this
repository is judged against.

**The cost.** `gate: PASS` on all three sets with `0 regressed, 0 improved,
0 new` and 139 of 139 `checker=ok`. netlib: 69 bit-identical, 25 moved, 23
digest changes; netlib-infeas and Kennington bit-identical. Work geometric mean
**0.9996x**, best `pilotnov` 0.9096x, worst `pilot87` 1.0372x — the ratio of
totals is 1.0216x and is not the result (D46). Five instances changed
trajectory: `pilot87` 40246 → 41281 iterations, `pilotnov` 2374 → 2390,
`pilot-we` 4172 → 4178, `pilot-ja` 1402 → 1371.

**The seconds are the evidence here and the work units are not**, because the
Neumaier step is arithmetic `jm_work_add` does not bill: the same nonzero count
is charged either way. Four of the six timed instances come back bit-identical
on the gate, so their ratio is the arithmetic alone. Across two independent
runs of the `-j 1` protocol they span **0.9501x to 1.0302x** while doing byte
for byte the same work, which is this host's 6.27% repeatability (D93). The
added arithmetic is not measurable on it. `pilot87` reads 1.0353x and 1.0528x
and moved 3.72% in work, which is the explanation the counter already gives.

**The published basis was re-read rather than assumed** (D167 is the entry
that says why). Six netlib `basis=` hashes moved, and 02-48's probe on the
whole tree reads `exact=142 WRONG=46 worst +18 SUM=+248` against the parent's
`+250`, with Kennington at `WRONG=0` on both. By the measure that item insists
on, the count of solves publishing a wrong basis, nothing moved.

**The residuals move both ways and this entry does not claim them.** Over
netlib's 94, `rsub` is better on 8 and worse on 2, `row` better on 7 and worse
on 8, `rowrel` better on 9 and worse on 7. Two moves are large and in the right
direction — `pilot-ja` 6.03e-12 → 1.62e-14 and `pilotnov` 8.16e-13 → 5.21e-14 —
and the rest is a few ulps either way, which is what a changed summation order
does to a converged point. **A more accurate sum is not a smaller residual on
every instance.** What the gate says is that no verdict moved on any of the 139.

**The test is validated against the tree that has the defect.**
`test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total` carries no
build guard, because OPTIMAL is the right answer in every configuration; built
against the parent's `src/simplex.c` it fails under `-DJAOS_NO_PRESOLVE` and
passes in the shipping build, and the control passes on both. `make configs`
exits 0.

**What is left open.** `subtract_basis_times` is still an uncompensated sum,
and `apply_flips` is a third of the same shape. Both are named below with the
reason and the condition that reopens them.

### What `numerics-reviewer` added, after the commit

The review landed after this entry was written. It found nothing wrong with the
change — reproducibility, the owned scratch, the guard, the tolerance space and
the `refine` ordering all check out, and it confirmed that `rhs[i] -= av*val`
becoming `t = -(av*val); rhs[i] = rhs[i] + t` is bit-identical in IEEE, signed
zeros included, so the 23 netlib digest moves are the recovered residue and
nothing else. Six things it corrected are folded in here.

**1. `0 improved` is a predicate count and not an accuracy measure.** The sets
show **no harm**; the constructed model shows the **benefit**; and no instance
of the 139 demonstrates the benefit. The gate line must not be read as saying
this helped on the sets.

**2. The work counter cannot see this change at all, by construction.** The
fold pass bills nothing, while the identical-shape `nrow` loop seventeen lines
later bills `s->nrow * JM_WORK_NONZERO`; and the inner loop went from two
operations per nonzero to about eight, still billed at one `JM_WORK_NONZERO`.
So `pilot87`'s 1.0372x and the 0.9996x geometric mean are **trajectory, not
arithmetic**. The arithmetic is what the `-j 1` protocol measured, and it is
under this host's noise.

**3. `refresh()`'s own comment records D29 measuring this exact lever, and it
fired.** A more accurate `x_B` mid-solve feeds a different steepest-edge pick;
D29 measured refining every refresh and got `pilot-ja` back INFEASIBLE and
`pilot87` at 4.5x the work. Here `pilot87` moved 1.0372x with +1035 iterations
and `pilot-ja` did not regress. The mechanism is different — a recovered
residue rather than a refinement solve — and the size is two orders apart. The
variant of putting the fold behind `if (refine)` was considered and is not
taken: the campaign says it is not needed.

**4. D29's primal-dual symmetry is crossed and this is the sentence.**
`compute_duals`'s refinement residual is an uncompensated sum over one column's
nonzeros and is untouched, so the primal right-hand side is accurate now and
the dual side is as it was. The asymmetry is much smaller than it looks: that
sum runs over one column, while this one runs over every nonbasic column
touching a row.

**5. `subtract_basis_times` stays out, and the reason is numerical rather than
about scheduling.** On the constructed model every product is exact — `av` is
1.0 and `val` is a power of two — so the accumulation was the entire error and
compensation recovered all of it. `subtract_basis_times` sums products of
`x_B`, an FTRAN output already carrying the factorization's error, and no
accumulator reaches an error that is already inside a term. It also publishes
directly, being on the `refine` path, so it would move published values rather
than trajectories and needs its own read against the checker and the basis
count. **Reopen condition: a model where the refinement residual loses a term
that changes a published value.** `apply_flips` is the same shape and is left
out with it; the final `refine = true` refresh rebuilds `x_B` from scratch, so
what it loses is mid-solve only. If either ever lands it gets its own `[nrow]`
array — `s->rhsc` is idle by then, which is exactly the borrowed-scratch shape
this project has paid for before, and `src/simplex.c` says so beside the field.

**6. Two test corrections, both real.** The positive test asserted OPTIMAL and
nothing else, and 02-72 records this same model publishing `obj = 4e-07` on an
earlier tree and 0 at three of four counts before D165 — so a repair reporting
OPTIMAL with a wrong objective would have passed. It pins the value now.
**The reviewer's figure for that value was wrong and the measurement is here**:
it said every feasible point has `obj = 1e-7` exactly, but `x1` is a variable,
and the shipping build publishes 1.0000000000000074e-07 while
`-DJAOS_NO_PRESOLVE` publishes 1.1920928955078125e-07, which is 2^-23 — one ulp
of `x1`'s own magnitude. The window admits both and rejects 0, 2e-7 and 4e-7.
And the 1e-2 control separates nothing, being 1300 times the 7.63e-6 recovered;
a second control at 5e-6 is added, which is infeasible by 4.7e-6 and therefore
INSIDE a window widened to cover the loss, so a widening repair accepts it and
an accurate sum refuses it. Both read INFEASIBLE on both builds at both trees.

The published objective on D162's model is still not pinned to the true
optimum, for D162's reason: `x1` sits where one ulp is 1.19e-07, so the last
step of the ratio test is not on the model's grid.

## D169 — The published objective is summed with compensation over the point it is published with, and 81 of 94 now agree with the checker exactly

**The question.** D168 compensated the row activity the simplex computes.
`settled_objective` and `publish()` are the same shape one step further on, so
the objective was asked the same question — and the answer was worse than
expected, because there were two defects rather than one.

`jaos.h` promises *"objective value of the solution held by the model,
including the constant term"*. Neither producer kept it.

**1. The sum was naive.** Costs of `+1e16`, then `k` costs of 1, then `-1e16`,
on columns fixed at 1, publish an objective of **0** where the answer is `k`.
One ulp of 1e16 is 2, so every middle term is below half an ulp of the running
total and none of them moves it. Reproduced at k = 64 and k = 256, on the
shipping build and on `-DJAOS_NO_PRESOLVE`, while `jaos_check_solution` — the
same library, accumulating in `long double` — reads `k` on all four
(`bench/measurements/02-79/objective.txt`).

**2. It was not a sum over the point the caller reads.** Both presolve
postsolve paths reported the reduced model's objective, or the accumulated
offset alone. `jm_model_publish_objective` in `src/model.c` replaces all three
producers: `obj_offset` plus a Neumaier sum of `col_cost[j] * sol_col[j]`, on
the model that publishes it, after its values are written.

**The measurement, and the oracle is the checker.** `jaos_check_solution`
judges the model as loaded, in the original space, independently of every
solver bookkeeping, so `|jaos_objective − primal_objective|` on the same point
is the promise itself, measured:

| netlib, 94 instances | parent | D169 |
|---|---|---|
| **exact agreement with the checker** | 34 | **81** |
| closer / further / unchanged | — | 57 / 4 / 33 |

The four that move the wrong way are `sierra` 0 → 1.86e-09, `kb2` 0 → 2.27e-13,
`afiro` 0 → 5.68e-14 and `tuff` 0 → 5.55e-17 — last-bit, each a naive sum whose
errors happened to cancel onto the long-double value.

**The cost.** `gate: PASS` on all three sets, `0 regressed, 0 improved, 0 new`,
139 of 139 `objective=ok checker=ok`. **0 digest changes on any set**: netlib
moved 62 instances, Kennington 13, netlib-infeas none, and every one of them
moved on `obj` and on nothing else — no work unit, no iteration count, no
basis. The digest covers x and y, so zero across 139 instances says this change
touched the number reported and no part of the solve.

**What is left is the product rounding, and the entry says so rather than
claiming the objective is now exact.** On `finnis`, the worst cancellation in
the set (terms summing to 1.7e5 with magnitudes summing to 3.2e12), the four
numbers for one point are: naive `double` 172791.0657497762, Neumaier `double`
172791.06569834377, the same rounded products added in `long double`
172791.06569833743, and the products themselves in `long double`
172791.06567182826. **The accumulation is exact to 6.3e-09 now, against
5.1e-05 naive**; the remaining 2.65e-05 is each `c_j x_j` rounding to a double,
where one term of 6.5e11 rounds by up to 7.2e-05 on its own. No accumulator
reaches that — it needs a two-product, which is its own change.

`finnis` is also why the weaker measure disagrees: against the manifest's
reference the 94 read closer on 54 and further on 8, with `finnis` going
4.93e-05 → 1.03e-04. The parent's number came from the reduced model and was
nearer the true optimum by luck. `finnis` publishes a point with
`row = 8.44e-07` and `gap_positive = 1.05e-04`, and D169 reports the objective
of that point.

**Two positive tests began failing under `-DJAOS_PRESOLVE_FAULT_OFFBYONE` once
the objective started reading the published values, and both are guarded.**
That is the fault build working: it corrupts a postsolved value, and until now
the objective did not read those values, so tests designed to be broken by it
passed. Guarding at `solved_objective` follows `solve_and_verify`'s precedent
in `tests/test_simplex.c`.

**`-ffp-contract=off` is load-bearing here in a way it was not before.**
`u = a + t` with `t = cost * sol` is exactly the multiply-add GCC would fuse,
and a fused `u` would no longer equal `a + t`, so the Neumaier correction
would compute a wrong residue in silence. A future change to that flag breaks
this without any test saying so (`numerics-reviewer`).

**The three questions this entry put to review, answered in the main context
before the review delivered.** `CLAUDE.md` wants that read done somewhere
else. It was done here first and the review arrived afterwards; it agreed on
all three and added the four items below.

- **Is `sol_col` complete at all three call sites?** Yes, and the measurement
  answers it rather than an argument about the source. `jm_model_publish_objective`
  is the last statement that touches the model on both postsolve paths, before
  `jm_model_remember_basis` and the return, so nothing writes `sol_col` after
  it. And `jaos_check_solution` computes its `primal_objective` from the very
  `sol_col` the caller receives: **81 of 94 now agree with it to the last bit.**
  An incomplete `sol_col` would make those two disagree, not agree exactly.
- **Was guarding the two fault-build tests right?** Yes, and the reason is
  worth keeping: the fault is injected on purpose, so a positive test must be
  skipped under it, which is the convention `tests/test_presolve.c` already
  follows at thirty sites. **What the failure showed is a gain, not a loss.**
  Until this change the objective did not read the published values, so a fault
  that corrupted a postsolved value left it untouched and tests designed to be
  broken by that fault passed. The objective is a detector for a corrupted
  postsolve now, which is why they started failing.
- **Is `settled_objective` an inconsistency that can decide something?** Yes,
  and **the first version of this answer was wrong.** It said the sum decides a
  trajectory and not a published number. `take_best_if_better` restores the
  saved best status and basis, and its own comment says *"Publish the best one
  instead (D89)"* — so `settled_objective` selects **which point gets
  published**. The model for it therefore does not need to show a different
  pivot; it needs two rounds that tie under a naive sum and separate under a
  compensated one, which is cheaper to build. Found by `numerics-reviewer`,
  confirmed by reading `take_best_if_better` and its four callers.

### The four things the review added

**1. `test_a_maximised_empty_column_is_not_unbounded_downwards` was guarded too
widely.** The guard skipped D19's own `JAOS_SOLVE_OPTIMAL` assertion as well as
the objective, and neither fault can flip that verdict: `ps_restore_index` and
the `JAOS_PRESOLVE_FAULT_WRONGDUAL` branch are both in postsolve, while
UNBOUNDED is decided during the scan in `ps_empty_col_value`. Only the
objective comparison is guarded now, so the verdict is asserted in all five
configurations again.

**2. The sense-and-offset control never ran a postsolve path.** Presolve
reported NONE on `max 2x0 + 3x1 + 100 s.t. x0 + x1 <= 4` — measured, `outcome`
0 and no reduction — so the control exercised `publish()` alone and neither of
the two sites this change touched most. It carries a third column fixed at 1
with a cost of 5 now: presolve reduces to 2 columns and 1 row with
`obj_offset = 105`, the column is replayed through `jm_postsolve_expand`, and
the answer is 114.

**3. The helper's precondition is enforced.** `assert(m->num_col == 0 ||
m->sol_col != nullptr)`. Without it a caller arriving before the solution
arrays exist gets the bare `obj_offset` published beside an OPTIMAL status.

**4. Neither new test can pass for a wrong reason, and the review checked
why.** Test 1's 258 columns are all fixed, so the shipping build reduces to
zero columns and takes `jm_postsolve_solved`, where the parent published
`p->reduced.obj_offset` accumulated in the same column order — also 0. The
reference build takes `publish()` — also 0. 256.0 is reachable only by fixing
the summation.

**What is left open.** The two-product above, and `settled_objective` with the
condition just stated. presolve's `obj_offset` is still accumulated naively;
nothing reads it for the answer any more.

## D170 — The published reduced costs contradict the published basis on five netlib instances, and every one of them is §2 rather than a new defect

**The question.** D168 and D169 closed two published numbers that were wrong,
both of them naive sums. `price_entry` computes `y' a_j` as a naive dot
product, so the reduced costs were the obvious third. This asked whether they
are wrong, and the answer is that they are right and the statuses beside them
are not.

**Nothing in this repository was asking.** `jaos_check_solution` recomputes `d`
from `y` and never reads `col_dual`. `bench/run.c`'s digest covers x and y
only, and `basis=` is a hash of the statuses compared against a previous hash
and against nothing else. A `col_dual` that contradicts a `jaos_basis` status
is invisible to the gate, to the checker and to every measurement in
`bench/measurements/`. The detector is three public calls per instance and
needs no instrumented build (`bench/measurements/02-80/`).

**What fires.** Five netlib instances above the 1e-7 dual tolerance — `nesm`,
`finnis`, `perold`, `bandm`, `pilot-ja` — worst breach **15018.5** on `nesm`.
Kennington is clean, 16 of 16, worst 1.02e-10. 27 columns in total.

**On every one of the 27 the published reduced cost equals `c_j - a_j' y`
recomputed in `long double` to the last bit.** The reduced cost is not what is
wrong.

**It is §2, and the split is total.** Cross-tabulated against the count
promise, `REDCOST ONLY = 0`: every instance that fires here also publishes the
wrong number of basic variables. 23 of netlib's 94 fail the count, 5 of those
23 fire here, 71 are clean on both, Kennington clean on both.

| where the firing column's value rests | columns | what is wrong |
|---|---|---|
| exactly on its own **lower** bound | **25** | the STATUS — all 25 have `d >= 0` and would be dual feasible as `AT_LOWER` |
| on its own upper bound | 0 | — |
| strictly **inside** its own box | 2 (`finnis` 564, 565, `d = -54.17`) | the replay published BASIC for a column recovered inside its box |

Both shapes are ones `TODO.md`'s §2 already names, the second word for word.

**A figure is cross-checked as a side effect.** `TODO.md` records 46 wrong of
netlib's 188 optimal solves; the gate solves each instance twice for its
determinism check, so 188 is 94 × 2 and 46 is 23 × 2. Two probes that share no
code agree. D167 is the entry that says why that is worth writing down.

**What this changes about the item.** Its stated cost was too small. `TODO.md`
says the cost is *"a lost warm start, not a wrong answer"*, which is true of
the count and is not the whole cost: a caller reading `col_dual` beside
`jaos_basis` gets two statements that cannot both be true, on five instances,
and on `nesm` the number is 15018.5. **What it does not change is the
measure** — the count of solves publishing a wrong basis is still 46, and the 5
are a strict subset of the 23.

**What is refused.** `price_entry`'s naive dot product. It was the hypothesis
that started this. A sweep of all 94 puts the worst disagreement between
`col_dual` and a `long double` `c - a'y` at 3.37e-09 on `dfl001`, over columns
whose own traffic is about 1e7 — one rounding of a dot product at that scale,
which is exactly the contract D23 states. No repair, and the reopen condition
is a published reduced cost that breaches its own sign condition on a column
whose status is NOT in the 46.

**The cost.** No source change. `TODO.md`'s §2 gains the second symptom and the
corrected cost sentence; `SPECS.md`'s basis row gains the same.

## D171 — The refinement residual is compensated too, and the argument that refused it was sound about the terms and wrong about the consequence

**The question.** D168 compensated `compute_primal`'s right-hand side and left
two sums of the same shape alone. `numerics-reviewer` refused them from the
terms, and this entry exists because that refusal was written into the record
and then measured.

**The argument, stated fairly.** `subtract_basis_times` sums products of
`x_B`, which is an FTRAN output already carrying the factorization's error, so
**an accumulator cannot reach an error that is already inside a term**. That is
true. What it does not follow from is that compensating buys nothing: the
residual is what the refinement correction is computed FROM, so a term lost
there leaves a correction that is short, and the published point is the one the
correction lands on. D168's own reopen condition was *"a model where the
refinement residual loses a term that changes a published value"*. The sets
have three.

**The control comes first, and the first version of this measurement did not
have one.** Built with the same flags from the parent tree, its 94 netlib
instance lines are byte-identical to the committed record; only the two-line
baseline footer differs, because the probe runs without `-b`. That also settles
a question nobody had asked in writing: `-O2` and the Makefile's
`-O3 -flto -march=native` produce the same bits.

**netlib says the change is systematically more accurate and nothing more.**
88 of 94 moved, work geometric mean **1.0000x**, and **the worst value of every
figure over the set is unchanged**: `row` 8.44e-07, `rowrel` 6.36e-12, `gap`
2.21e-10, `rsub` 6.91e-05. Direction counts: `rowrel` 76 better / 7 worse, `N`
71/15, `row` 59/20, `rsub` 53/14, `Q` 52/15, `col` 17/8, `sub` 31/30, `gap`
31/51, `dual` 0/0. **76 against 7 is not a coin**, so the bias is real; on this
set alone it would not be enough to pay for 88 digest changes, and reading only
netlib nearly refused this.

**Kennington is where the worst case moves.** 11 of 16, work **1.0000x**, and
`col` is a breach of a column bound in the model's own units:

| | `col` before | after | `rowrel` before | after |
|---|---|---|---|---|
| `pds-06` | 4.26e-14 | **1.58e-30** | 4.26e-14 | **1.58e-30** |
| `pds-10` | 2.84e-14 | **2.52e-29** | 2.84e-14 | **5.89e-17** |
| `pds-20` | **8.81e-13** | **5.05e-28** | **8.81e-13** | **8.42e-17** |

Over the set, `rowrel`'s worst goes 8.81e-13 → 8.88e-17 and `col`'s 8.81e-13 →
2.07e-14, with 9 better and 0 worse on `rowrel` and 7 and 0 on `col`. A
published value that sat 8.81e-13 outside its own declared bound now sits
5.05e-28 outside it.

**The cost.** `gate: PASS` on all three sets with `0 regressed, 0 improved,
0 new`; 139 of 139 `checker=ok` and 29 of 29 correctly refused. Work geometric
mean 1.0000x on netlib and on Kennington, worst instance `scagr7` 1.0003x.
netlib-infeas moves 14 instances by a handful of work units with **0 digest
changes**.

**Nine netlib instances publish a different optimal basis**, and this entry's
first version did not say so (`numerics-reviewer`): `bandm`, `bnl2`, `cycle`,
`czprob`, `dfl001`, `etamacro`, `fit1p`, `ganges` and `scrs8`. Kennington: none.
The mechanism is the one this change is for — `refine = true` runs at the
refresh that verifies optimality, a more accurate `x_B` moves a marginal
feasibility test, and the solve takes a different last pivot. `bench/run.c`
says the basis is part of the answer, so it belongs in the entry that owns the
verdict rather than inside a digest count.

**The symmetric change is refused.** D29 says a point read off an accurate
`x_B` and an inaccurate `y` is not more consistent than one read off neither,
so compensating `compute_duals`' refinement dot product as well is the obvious
next step. Built and measured in the same run: **it changes nothing at all**,
on any figure, on either set. That sum runs over one column's nonzeros while
this one runs over every basic column touching a row, so there is nothing there
to recover.

**The `gap` column goes the wrong way on 51 netlib instances, and here is what
settles it.** This entry's first version argued that if the 51 were D29's
primal-dual inconsistency the symmetric change would have moved them, and it
did not. **That argument has no power and is withdrawn** (`numerics-reviewer`):
the symmetric change moved nothing at all, and a null intervention cannot
discriminate between hypotheses. The evidence is direct instead, read off the
two committed records: **`dual`, `cert`, `drop` and `rays` are unchanged on all
94 netlib and all 16 Kennington instances.** A primal that had gone inconsistent
with its dual would show in `dual`, and `dual` did not move anywhere. What is
left is that `gap` is a difference of two halves much larger than itself, so at
these magnitudes it is what cancellation leaves rather than a number: the 82
netlib movers span **1.49e-19 to 4.12e-13** — the first version of this entry
said 1e-19 to 1e-16 and was wrong by three orders at the top — against a set
worst of 2.21e-10 that does not move.

**What it cost, on the one figure the gate cannot see.** Nine netlib basis
hashes moved, so 02-48's probe was run at three trees rather than assumed —
D167 is the entry that says why. `4747f29` reads `exact=142 WRONG=46 worst +18
SUM=+248`; **D171 reads `exact=140 WRONG=48 worst +21 SUM=+272`**; D172 is
identical to D171, which is what a diff writing only `m->objective` predicts.
Kennington is `WRONG=0` at all three. **So this cost two netlib solves their
valid published basis, and by that item's own stated measure it is a
regression** — while the gate reported `0 regressed`, because `basis=` is a
hash that detects a change and never reports a count.

**It is recorded rather than traded away quietly, and the change stands.** What
it bought is three published column values that sat outside their own declared
bounds and no longer do, worst 8.81e-13. What it cost is two more solves on an
item already wrong on 46, whose stated price is a lost warm start and not a
wrong answer. The two are not the same kind of thing and this entry says both.

**The seconds, which D171's first version did not have** (`numerics-reviewer`;
`CLAUDE.md` asks for a ratio where the units are blind, and `jm_work_add` is
unchanged while the arithmetic per nonzero roughly quadrupled). The noise floor
is unusually good: the whole `ken` family and `pds-02` come back bit-identical,
so their ratio is the arithmetic alone, and `ken-13` is 747 million work units
of it. The four span **0.9549x to 1.0364x** for a geometric mean of 1.0067x —
this host's own 6.27% (D93). Not measurable, which is what a routine running
only on the handful of `refine = true` refreshes predicts.

**What is left open.** There is no constructed model, and the entry says so
rather than dressing the campaign up as one: D162's shape does not carry over,
because on that model every product is exact and here the terms are products of
an FTRAN output. A model would turn this into a pinned test and is worth a
session. `apply_flips` is the third sum of this shape and is untouched — it
loses terms mid-solve only, since the final `refine = true` refresh rebuilds
`x_B` from scratch, and it would need a third `[nrow]` array because `s->rhsc`
and `s->resc` are both live inside the call it runs in.

## D172 — The published objective recovers what each product lost, and 109 of 110 now agree with the checker exactly

**The question.** D169 made the objective a compensated sum and said plainly
what was left: each `c_j * x_j` is rounded once before it is added, and no
accumulator reaches an error that is already inside a term. On `finnis` that
residue was 2.65e-05. This removes it.

**The minimum model needs two columns and no instance.** `c0 = 2^27 + 1` with
`x0` fixed at `2^27 + 1`, and `c1 = -1` with `x1` fixed at `2^54 + 2^28`. One
ulp at 2^54 is 4, so the first product rounds down by exactly 1 and **every
accumulator over the rounded products sums to 0**, however carefully it adds,
while the true objective is 1. The parent publishes 0 on the shipping build and
on `-DJAOS_NO_PRESOLVE`; `jaos_check_solution`, which multiplies in
`long double`, reads 1 on both. D172 publishes 1.

**Dekker's split, and neither the flag people reach for nor `fma` is what
matters here.** Measured across six flag sets, contraction ON and OFF give
identical bits, because every product inside the split is exact — so
`-ffp-contract=off` is not what protects it, `-fno-associative-math` is, and
only `-ffast-math` and `-Ofast` would break it (`numerics-reviewer`). That flag
stops the COMPILER contracting `a*b+c` on its own and says nothing about an
explicit `fma()` call, which IEEE-754 requires to be correctly
rounded and which would be deterministic across machines in a way `log` and
`exp` are not (`numerics-reviewer`, D169). The split is preferred because it
needs no claim about libm at all. Beyond a factor magnitude of `2^996` the
split would overflow, so the function reports a zero residue there and the sum
falls back to the plain product; `SPLIT * 2^996` is `2^1023`, one binade under
`DBL_MAX`.

**The measurement is `jaos.h`'s promise, measured** — the distance between
`jaos_objective` and the checker's `long double` objective of the same point:

| | parent | D172 |
|---|---|---|
| **netlib, exact agreement** | 83 of 94 | **93 of 94** |
| netlib: closer / further / unchanged | — | 11 / **0** / 83 |
| **Kennington, exact agreement** | 15 of 16 | **16 of 16** |
| Kennington: closer / further / unchanged | — | 1 / **0** / 15 |

**Nothing moves the wrong way on either set**, where D169 had four netlib
instances going further at the last bit. That is what a compensated sum does
when a naive one got lucky, and recovering the products leaves nothing for luck
to do.

**The one that is left is `finnis` at 2.2992e-08, and it is below the oracle's
own floor.** `(long double) c * x` is **not** an exact product: a binary64
product needs 106 bits and that mantissa holds 64, so each term carries up to
`2^-64 |t|` — 1.73e-07 over `finnis`'s `sum|t| = 3.2e12`, before `src/check.c`
sums anything. The observed value is 7.5 times below that, so the comparison is
exhausted and neither number is more right (`numerics-reviewer`).

**The better oracle is Koch's exact-rational optimum, and it says the remaining
`finnis` gap is the POINT rather than the sum.** This change moved 0 digests,
so the published `x` is bit-identical and every `obj` move was pure summation.
Against the reference the eleven netlib movers read **8 closer, 3 further, and
exact matches 3 → 6** — `25fv47`, `80bau3b`, `afiro`, `maros`, `ship04l` and
`truss` match to the last bit now, while `bandm`, `scagr25` and `tuff` each
went from exact to about one ulp, which is not a cost. `finnis` goes
1.027e-04 → 7.624e-05: better by 25% and still **2.6 million ulps**, where
`ulp(172791.06) = 2.9e-11`. A compensated sum of exact products cannot leave
that behind. It is `TODO.md`'s now, and its oracle is exact rational arithmetic
over the published `x` and `c` rather than the checker, which carries the same
per-term rounding this entry just removed one layer out.

**The cost.** `gate: PASS` on all three sets with `0 regressed, 0 improved,
0 new` and **0 digest changes anywhere**: 11 netlib instances and 1 Kennington
one moved, every one of them on `obj` and on nothing else. The diff writes
`m->objective` and nothing else, so the campaign confirms the reasoning rather
than discovering anything.

**What is left open.** `settled_objective` is still a naive sum and is not
reached by this: it selects which point gets published, and this repairs the
objective only after the point is chosen. presolve's `obj_offset` is still
accumulated naively and nothing has read it for the answer since D169.

## D173 — The published objective is the correctly rounded exact one on all 110, so `finnis` is refused and `pilot` is the instance with a wrong point

**The question.** D172 left `finnis` 7.62e-05 from Koch's optimum and said
what would settle it: exact rational arithmetic over the published `x` and
`c`, because `jaos_check_solution` cannot be the oracle. The checker
multiplies in `long double`, whose 64-bit mantissa cannot hold a binary64
product's 106 bits, so it carries the same per-term rounding D172 removed one
layer out. The expectation was that the published point would turn out to be a
slightly wrong vertex.

**The oracle** (`bench/measurements/02-83/`). Every `c_j * x_j` is a dyadic
rational of at most 106 bits, so a 5632-bit binary fixed-point accumulator
holds the whole sum with no rounding at all. It is validated twice before any
reading is taken: against D172's model, D169's model, the exact 55-digit
expansion of the double nearest 0.1 and both ends of the exponent range; and
against Python's `fractions`, which shares no code with it and agrees to all
20 printed places on `finnis`, `pilot`, `pilot87` and `afiro`. **The first
run of the written-down cases failed**, on four digits this session had
transcribed wrong, which is the case the second oracle exists for.

**`jaos_objective` is the correctly rounded exact objective on 110 of 110** —
94 netlib, worst 0.493 ulp (`ship08l`), and 16 Kennington, worst 0.476
(`cre-a`). The checker manages 109, worst **790.3 ulps** on `finnis`. D172's
"neither number can be called more right than the other" is settled: the
2.2992e-08 it measured between them is the checker's error and not a shared
floor. **`refeps` below means nothing on Kennington** — that manifest's
references are eight significant digits from netlib rather than Koch's exact
rationals, so all 16 read 1e6 to 1e8 and every figure is the reference's own
decimal.

**`finnis` is REFUSED, and the reason is the model rather than the solve.**
It carries `sum |c_j x_j| = 3.198e+12` against an objective of 1.7e+05, and
the 7.62397e-05 gap to Koch is **0.107 of `eps` times that traffic**.

**The threshold is right and this entry's first justification for it was
false** (`numerics-reviewer`). It said no double objective could be placed
nearer than half that unit, which is wrong by seven orders — a double near
172791 is placeable to 1.46e-11. The argument is about the POINT: the
optimal vertex's coordinates cannot be held more finely than a double, and
rounding them moves `c'x` by up to `sum |c_j| ulp(x_j)/2`, which is at most
`2^-53 sum|c_j x_j|` — exactly half a unit. So `|refeps| <= 0.5` means the
gap is no larger than writing the answer down costs. Measured against the
exact `sum |c_j| ulp(x_j)/2`, the unit is 16% loose on `finnis` and 30% on
`pilot`, and neither verdict moves.

**Said without the unit at all, which is the stronger form**: `finnis`'s
traffic is concentrated — the largest single `|c_j x_j|` is 20.2% of it and
the top five are 82.3% — so one half-ulp on the largest term alone is worth
6.167e-05 of objective. The gap is **1.24 of those**. `pilot`'s is 9.2e+09 of
its own. Ten orders separate them and nothing in that sentence depends on
`refeps`.

Its worst exact row residual, 8.439e-07 on rows 23 and 43, sits against a
traffic of 2e+10 on each: 4.2e-17 and 3.0e-17 relative. A backward-stable
solve on that row gives about `u * traffic = 4.4e-06`, so the point sits five
times under its own backward error and **the feasibility half is settled**.

**The optimality half is not, and this entry first claimed both.** Nothing in
02-83 separates "Koch's vertex rounded to doubles" from "a neighbouring
vertex": `refeps` says the gap is consistent with rounding, not that it is
rounding. And the record carries one number that argues the other way, which
the first version did not mention. **`finnis` has `gap_negative = 2.888e-05`,
the largest of all 110 and 356 times the next** (`pds-20`, 8.118e-08). That
is the total of wrong-sign terms in the checker's own suboptimality
certificate, whose whole hypothesis is that they do not exist; on `finnis`
they are 27.5% of `gap_positive` and 37.9% of the disputed gap. The refusal
stands on the arithmetic — the gap cannot be resolved in binary64 — and not
on a claim that the point is optimal.

**That number is also an accidental second oracle.** `gap_negative` and this
record's `priced` agree to all four printed digits on `finnis`, and they share
no code: the checker accumulates in `long double` over the model, the
accumulator sums exact row activities. Only two of 110 instances show that
equality and the other is `afiro`, where both sit at 1.6e-14. It is the one
independent check the row path has, on the one instance that needs it.

**What the same reading found instead.** `refeps` is the gap to Koch divided
by `eps * sum |c_j x_j|`. Over the 93 instances with a Koch optimum (`e226`
excluded — its 7.113 is the objective constant `docs/format-support.md` owns):
68 are within 0.5, twenty within 10, one within 1e4, and **four above 1e4**.

| | refeps | gap to Koch | `gap_positive` | `gap_certified` | row residual |
|---|---|---|---|---|---|
| **`pilot`** | **1.87e+08** | **2.31e-05** | **0.0386** | **yes** | 3.55e-13 |
| `pilot87` | 1.53e+06 | 1.04e-07 | 7.68e-04 | no | 1.51e-12 |
| `scsd6` | 9.97e+04 | 1.12e-09 | 9.73e-15 | no | 9.59e-17 |
| `etamacro` | 2.74e+04 | 1.31e-08 | 1.34e-07 | yes | 1.71e-13 |

Every row residual is at the arithmetic floor, so these four points are
feasible and suboptimal rather than infeasible, and `pilot`'s published
objective is higher than Koch's on a minimization. **`pilot` is the only
netlib instance where every other solver in `bench/compare` disagrees with
JAOS**: HiGHS, SoPlex and Clp all publish Koch's `-5.5748972927e+02` against
JAOS's `-557.489706168`.

**The library already certifies `pilot`, and what the gate does with that is
narrower than this entry first said.** `jaos_check_solution` reports
`gap_positive = 0.0386` there with `gap_certified = yes`, a proof that
`P - P* <= 0.0386`. Among the 53 instances whose certificate is complete that
is four orders above the next (`grow22`, 1.797e-06), and it belongs to the one
instance every other solver beats.

**Correction, made 2026-08-21 while auditing this entry.** The first version
said `bench/run.c` never reads the field. It does read two: `gap_positive`
goes into every record line as `Q=`, and `relative_suboptimality` is a
**regression predicate** in the baseline comparison — an instance whose `rsub`
exceeds `RSUB_FLOOR = 1e-9` and grows by `RSUB_REGRESSION_FACTOR = 2.0`
against its baseline is reported REGRESSED, in its own message.

What the gate does not do is judge an instance's suboptimality in ABSOLUTE
terms against the reference. It watches for a suboptimality that gets worse,
and `pilot`'s was already there when the baseline was written. That is the
precise reason the answer is invisible, and it is a different defect from
"nothing reads it": the instrument exists and its zero point is the wrong one.
The objective test alongside it is `|got - ref| <= 1e-6 * max(|ref|, 1)`, a
window of 5.57e-04 on `pilot`, cleared with 24 times to spare.

**What was refuted.** Three explanations for the `finnis` gap, in the order
they were tried. The summation: refused, the published number is within a
third of an ulp of exact. The reference being imprecise: refused, the
manifest's decimal stops at 1e-11 and the gap is seven orders above that. The
point being infeasible: refused for `finnis` at any scale that matters, since
its residual is under one eps of the rows' own traffic — and the same test is
what shows the four instances above are feasible, so their gap has to be
suboptimality.

**What is left open**, handed to `TODO.md`: `pilot` and the other three
publishing a point that is not the optimum, and whether the gate should read
`gap_positive` when `gap_certified` says it is a proof. One instance
separating cleanly on one set is not a threshold.

**The cost.** Nothing in `src/` was touched and no campaign was run, because
there is nothing for a campaign to measure.

## D174 — `pilot`'s wrong answer is `DUAL_TOL` and nothing else, and the repair that fixes it turns the gate red on six instances

**The question.** D173 measured four netlib instances publishing a point that
is not the optimum and handed the cause to `TODO.md`. `pilot` is the worst at
2.31e-05, which is 1.87e+08 times the floor arithmetic sets for that model,
and it is the only netlib instance HiGHS, SoPlex and Clp all beat. The
expectation was a tolerance, and the two a caller owns are the place to look
first because neither needs a rebuild (D64).

**The mechanism.** `DUAL_TOL` is what the solve calls zero for a reduced cost
— `dual_breach`, `published_breach`, `settled_dual_violation` and
`points_outwards` all read it — and `docs/tolerances.md` described only its
role in the Harris window. A reduced cost is a rate. What a column is still
worth is that rate times the distance it would travel, and the tolerance
bounds the rate alone; on `pilot` a rate under 1e-7 in scaled space is worth
2.31e-05 of objective.

**The measurement** (`bench/measurements/02-84/`). Presolve is not involved:
`-DJAOS_NO_PRESOLVE` publishes the same 2.312e-05 and the same 0 at
`dual_tol = 1e-11`. `PRIMAL_TOL` is not involved: eight settings from 1e-13 to
1e-5 publish the identical number. The dual tolerance alone moves it, and it
moves it monotonically until it runs out.

| `dual_tol` | `pilot` gap to Koch |
|---|---|
| 1e-6 | **`numerical error`** |
| **1e-7, the default** | **2.312e-05** |
| 1e-8 | 2.312e-05 |
| 1e-9 | -5.266e-09 |
| 1e-10 | **0** |

At 1e-9, all four of D173's instances improve and nothing else on the set
moves materially: `pilot` 2.312e-05 → -5.266e-09 at **0.9134x** work,
`pilot87` 1.044e-07 → **exactly 0** at 0.9202x, `scsd6` 1.118e-09 → **exactly
0** at 1.0807x, `etamacro` 1.315e-08 → -1.137e-13 at 0.9934x. **Three of the
four cost less work.** `pilot87` is D92's backlog row, "suboptimality bound,
not understood": it publishes Koch's optimum exactly at every setting from
1e-8 to 1e-13, each for less work than today.

**The control, which is why the sweep means anything.** `DUAL_TOL` is
caller-owned, so `jaos_set_dual_tolerance` reaches it at run time and one
binary serves every setting. Passing 1e-7 explicitly names the built-in
default, and over all 94 netlib instances the two agree on status, objective,
iterations, work and digest with **0 differing**. Without that row the sweep
could be one binary measured seven times, which is how three of the five
build configurations were broken for a session (D154).

**The cost, over all three sets.** At 1e-9: netlib has no failures, 35 digests
move and the work geometric mean is 1.0339x; `netlib-infeas` still refuses all
29 at 1.0070x; Kennington has no failures, 3 digests move and the geometric
mean is 1.0976x, of which the whole is `pds-20` at 4.815x. **Six instances
across the three sets cross the gate's 2.0x per-instance work bar, so the gate
would report `6 regressed`.**

**What was refuted.** That the cheaper setting is a smaller version of the
same repair: **1e-8 does not fix `pilot` at all**, leaving 2.312e-05
unchanged, so 1e-9 is the first setting that reaches this defect. And that the
2.0x crossings measure the price of the accuracy: they are not monotone in the
tolerance. `grow22` reads 2.14x, 3.00x, 1.49x, 0.22x, 0.22x across 1e-6 to
1e-11 and `greenbea` swings by a factor of three between adjacent settings,
while `grow22` and `d2q06c` both cross 2.0x when the tolerance is **loosened**
to 1e-6, where the answers get worse. The bar is detecting a changed pivot
sequence, and this change moves 38 of them. Only `agg3`, `d2q06c`, `perold`
and `pilot-ja` stay high once the tolerance is tight.

**No source change, and the default stays at 1e-7.** Lowering it changes the
contract every caller already has, it turns the gate red on six instances, and
1e-9 is one step from `dfl001` failing at 1e-10 and `wood1p` joining it at
1e-11. The sweep is what the decision needs, not the decision.

**A defect in the probe, found before anything read it.**
`jaos_solve_status_str` returns `"numerical error"`, with a space in it, so
every failing row carried one extra field and shifted every column after it.
Both affected runs were discarded and re-taken with a single-token status.

**What is left open**, handed to `TODO.md`: whether to lower the default, and
the narrower candidate that would avoid the trajectory churn — a tighter
tolerance for the termination test only, leaving the Harris window at 1e-7.
Nothing has been built for it and no model says it would cost less.

## D175 — The sum that ranks two rounds decides which point is published, so it is compensated too — and no solve on the three sets can reach the case

**The question.** D169's review corrected this item's severity and `TODO.md`
carried the corrected form: `settled_objective` is a naive sum, and it does
not decide a trajectory. `better_point` ranks two rounds by it and
`take_best_if_better` restores and publishes the winner, so it decides **which
point the caller receives**. The repair is the one D169 and D172 already
applied to `jm_model_publish_objective`. What was missing was evidence that it
changes an answer.

**The failure is a tie rather than a small error**
(`bench/measurements/02-85/two-points.txt`). A column of cost `1e16` held at
1, then 256 columns of cost 1, then a column of cost `-1e16` held at 1. One
ulp at `1e16` is 2, so every unit term is lost as it arrives and the `-1e16`
brings the total to exactly 0.0 — **for both of two points whose true
objectives are 0 and 256**. `better_point` reads `0 < 0`, answers no, and the
loop publishes the worse point with no number anywhere recording it. The order
is the mechanism: put the `-1e16` column second and the cancellation happens
first, the small terms land on zero and the naive sum is right, which is what
the first version of that file measured.

**No solve on the three sets reaches it.** A throwaway diagnostic build
recorded both objectives both ways at every comparison the settling loop
makes, measured on the parent because the question is what the naive sum did:
**304 comparisons over 94 + 29 + 16 instances, 0 verdict flips.** 80 are
settled by dual feasibility with the objective playing no part; 220 tie
exactly and **every one of those is a point compared with itself**; the
informative population is **4**, all netlib. The infeasible set contributes
nothing, because an infeasible model never reaches the settling loop's
optimum path, and `take_best_if_better` — the site that publishes — was never
exercised on two distinct points in 220 tries.

Two margins on those 4, and they answer different questions: the **spread**,
each side's error against the gap being decided, worst **0.571**; and the
**flip margin**, `|errc - errb|` against it, worst **1.53e-06**. A term common
to both sides cancels inside `a < b`, so the flip margin governs — and where
the spread is 0.571 the two errors are bit-identical and cancel exactly, which
is a property of those instances rather than of the method.

**The cost.** `gate: PASS` on all three sets with `0 regressed, 0 improved,
0 new`, and `bench/results/*.txt` **byte-identical to the committed records**:
no digest, work unit, iteration count, basis hash or objective figure moved.
Confirmed a second way, by a different program on a different code path —
02-83's exact-objective records are bit-identical on all 110 published
objectives. `make configs` exits 0.

**What was refuted.** Three defects in the measurement itself, each of which
produced output that looked like a clean result. The probe passed `-m` without
`-d`, so `bench/run` read the standard instance directory three times and
**Kennington recorded nothing while the output printed its name**. The probe
then copied `src/` from the working tree, where the repair already was, and
compared the compensated sum against itself: every error column read exactly
0. And one figure was printed under one name with two definitions, disagreeing
by five orders. All three were found after the measurement looked finished,
two of them by `numerics-reviewer`.

**Three findings from the review, folded in rather than deferred.**
`jm_obj_add`'s requirement that `sum` and `comp` are distinct objects was a
file-local promise and is now tree-wide, so it is asserted rather than
described — not `restrict`, which promises what nothing can check and is the
shape D75 and D76 refused. `static_assert(FLT_EVAL_METHOD == 0)` moved from
`src/model.c` to `src/jaos_internal.h`, so it is checked in every translation
unit that can call the split rather than only where it is defined. And the
`isfinite` guard was checked rather than copied: `comp` can go non-finite only
after `sum` already has, and `better_point` answers "not better" for a NaN or
a shared infinity either way, so no verdict moves.

**There is no test that fails at the parent, and no test was written.** The
only state that separates the two versions is a settling loop holding two
distinct points that tie under a naive sum; no model built here steers a solve
there, and `tests/` reaches the library through its public interface, so a
static function cannot be called directly. The evidence is the constructed
case plus the 304 comparisons. A test that passed either way would be worse
than none — this repository has shipped one of those before, and the review
that caught it is the reason the rule exists.

**What is left open**, handed to `TODO.md`: `apply_flips` stays refused for
D171's reason, and presolve's `obj_offset` is still accumulated naively with
nothing reading it for the answer since D169.

## D176 — REFUSED: presolve's objective offset is compensated for nothing, because poisoning it with NaN moves not one byte on any of the 139

**The question.** D168's class had four accumulations of one shape. Three are
closed — the simplex's right-hand side (D168), the published objective (D169,
D172) and the sum that ranks two rounds (D175). The fourth is presolve's
`obj_offset`, four sites in `src/presolve.c`, and `TODO.md` said nothing has
read it for the answer since D169. The expectation was a small coherence
change to finish the class.

**Reading the code agrees with the claim, and reading is not measuring.** The
four sites accumulate onto `p->reduced.obj_offset`; `jm_presolve_run` folds it
in as `m->obj_offset + accumulated_offset`; the simplex publishes
`reduced.objective` from it; and both postsolve paths then call
`jm_model_publish_objective(orig)`, which recomputes from the caller's own
model, so `reduced.objective` is overwritten before any caller can reach it.
`src/check.c` reads the caller's offset and never the reduced one, and the
progress callback carries no objective.

**The measurement** (`bench/measurements/02-86/`). Three builds from `HEAD`,
each in its own copy of the tree, each over all three sets: a control, the
reduced offset replaced with `1e300`, and the reduced offset replaced with
`NaN`. Two poisons because they fail differently — the finite one survives
every `isfinite` guard and would surface in a consumer as a wrong number, the
NaN surfaces as a NaN and exercises the guards.

**Every one of the nine runs is `gate: PASS`, the control reproduces all three
committed records bit-identically, and both poisons are bit-identical to the
control on all three sets.** Replacing the whole reduced offset with `NaN`
changes not one byte of any record on any of the 139 instances.

**What was refuted, and it is the control rather than the poison.** The first
version of the harness omitted `-e infeasible`, the flag the real target
passes so the gate expects an infeasible verdict. Under it the infeasible
set's records differed from the committed ones on all 29 instances — under
both poisons **and under the control alike**. Read without the control that is
"the value is read on the infeasible set and dead on the other two", which is
specific, plausible and wrong. It is also why the poisons are compared against
the control rather than against the committed record: that comparison removes
everything the harness does differently and leaves only the poison.

**So the sum is refused.** Compensating a value nothing reads adds arithmetic
to every presolve round and buys nothing measurable, and D166 is the precedent
— 196 lines came out when the case they existed for stopped happening.

**What is left open.** Removing the four accumulation sites is a different
question and is not taken here: the value is a legitimate quantity, the
objective of the reduced model, and deleting it would leave a future reader
with nothing rather than with a number that is naive. Neither direction has a
measurement. The reopen condition is in `TODO.md`'s refusals table and the
probe in 02-86 is the test for it.

---

## D177 — The gate's suboptimality predicate watched 4 solves of 110, and the floor that excluded the other 106 is refuted by D171's own numbers

**2026-08-24.** `bench/run.c`: `RSUB_FLOOR` from `1e-9` to `1e-16`. No change
to `src/`. Evidence in `bench/measurements/02-89/`.

### The question

`TODO.md` carried "the gate cannot see a suboptimal answer" and named what it
needed: an absolute threshold, with the note that one instance separating
cleanly on one set is not one. The item said the instrument exists and its
zero point is the baseline.

The audit that started this went looking for the threshold and found a
different defect first, one nobody had counted: **the predicate that does
exist was watching 4 solves out of 110.**

### The measurement — how little the predicate reached

`relative_suboptimality` regresses when it passes `RSUB_FLOOR` and grows by
`RSUB_REGRESSION_FACTOR = 2.0` against the baseline. At `1e-9`:

| set | watched | of | the instances |
|---|---|---|---|
| netlib standard | **4** | 94 | `forplan`, `pilot`, `pilot87`, `wood1p` |
| Kennington | **0** | 16 | its worst value is `4.18e-14`, five decades under |

So the predicate was dead across the whole Kennington set, and an instance at
`1e-15` could degrade by six orders of magnitude while the gate reported
`0 regressed`.

### What was refuted — the floor's stated reason

The comment said ratios mean nothing below the floor. `rsub` is deterministic,
so the only thing that moves it is a real change to the solve. **D171 is that
change and its numbers are already committed**: it moved 88 of 94 digests, and
`02-81/gate-diff.txt` kept the before and after for the 73 instances whose
`rsub` moved at all.

| | |
|---|---|
| worst move up | **1.688x**, `scsd1`, `4.3e-17 -> 7.26e-17` |
| worst move down | 0.594x, `sctap1` |
| moved by 2.0x or more, either way | **0 of 73** |
| false regressions at any floor, including none at all | **0** |

### Why 1e-16, measured on that same change

The floor keeps the factor of 2 away from values too small for a ratio between
two of them to mean anything. So the question is the headroom the factor keeps
at each candidate:

| floor | watched of 73 | worst legitimate move | headroom under 2.0x |
|---|---|---|---|
| 1e-15 | 32 | 1.078x (`scagr7`) | 1.86x |
| **1e-16** | **55** | **1.078x** (`scagr7`) | **1.86x** |
| 1e-17 | 71 | **1.688x** (`scsd1`) | **1.18x** |

**One instance puts the knee there.** `scsd1` ends at `7.26e-17`, so 1e-16
excludes it and 1e-17 admits it. A second argument agrees and owes the data
nothing: `rsub` divides by `1 + |primal_obj|`, so below about eps the numerator
is the rounding of the number underneath it. Coverage goes from 4 solves to
**84** — 75 of 94 standard, 9 of 16 Kennington.

The floor still has a job. Three baselines read exactly 0, and against a zero
baseline every positive value is an infinite ratio.

### The case it has to catch, built and confirmed

`adlittle` publishes `rsub = 1.52e-15`. Halving its baseline value to `7e-16`
makes the ratio 2.2x, which the old floor cannot see however large it gets.
Three runs, one variable each, both runners from the same `gcc` line:

| | |
|---|---|
| parent (1e-9), doctored baseline | `0 regressed` — does not see it |
| this change (1e-16), same baseline | `1 regressed`, `7e-16 -> 1.52e-15 (2.2x)` |
| this change, committed baseline | `0 regressed` — no false positive |

`run-predicate-validation.sh` aborts when the two sources carry the same
`RSUB_FLOOR`, because an experiment with no variable in it is one binary
measured twice (D82).

### The cost

`make test` and `make sanitize` exit 0. All three sets `gate: PASS` with
`0 regressed, 0 improved, 0 new`. **110 solution digests and 29 infeasibility
verdicts unmoved**, and `bench/results/*.txt` came out byte-identical to the
committed records. A constant in the runner's comparison logic changes what is
compared, never what is solved.

### What was refuted about the absolute bar itself

Two candidates were measured against 02-83's `refeps` as ground truth.
`pilot` and `pilot87` are the two wrong answers the checker has any chance of
seeing.

| candidate | top clean instance | margin |
|---|---|---|
| **`rsub`, what the gate already records** | 7.4e-09 (`wood1p`) | **343x** |
| `gap_positive` on its own | 0.002185 (`ken-18`) | **0.35x** |
| `gap_positive / (eps * sum|c_j x_j|)` | 5.646e+07 (`wood1p`) | 199x |

**`gap_positive` on its own does not even order correctly.** `ken-18` is a
clean answer carrying a larger absolute bound than `pilot87`'s, so an absolute
bar on it is refused on that alone.

**Normalising by the objective's own traffic is worse than what already
exists.** That denominator was this session's first idea, and it loses to
`rsub` by 343x against 199x. The denominator was never the problem.

### What is left open, handed to `TODO.md`

**The zero point is still the baseline.** This widens the predicate's reach by
five decades and gives it no absolute bar. `pilot`'s suboptimality was there
when the baseline was written and stays invisible.

Two things block the bar, and both are written into `TODO.md` item 5.
`wood1p` publishes the exactly correctly rounded optimum — `refeps = 0` in
02-83 — and carries the loosest certificate of any clean instance on either
set, so a bar placed to catch `pilot87` sits 343x above a perfect answer. And
an absolute bar turns the gate red on whatever is already over it, which is
`pilot` and `pilot87` today: the same judgement item 1 carries.

**A third route was measured here and is the strongest of the three, and it is
also item 1's judgement.** `objective_accepted` is already an absolute window,
`|got - ref| <= 1e-6 * max(|ref|, 1)`. Tightened to `1e-9` it catches `pilot`
with **zero false positives on all 94** standard instances. The population is
bimodal: 88 instances sit at or under `6.84e-16`, which is the reference's own
last digit, and the next value up is `modszk1` at `2.8e-13`. `finnis` at
`4.41e-10` is the nearest clean instance to `pilot`'s `4.15e-08`, and D173
already refused `finnis` with its own measurement. Any window in the empty
band gives the same answer, so this one is not fitted. It turns the gate red
on `pilot` today, which is why it is not taken here.

---

## D178 — REFUSED: `scsd1` and `degen2` do not lose the same way, so §3 asks for a predictor of something that happens once in twenty

**2026-08-24.** No behavioural change: one comment in `src/simplex.c`, proved
object-identical by `comment_only.sh`, and the two warm records refreshed
because they were 24 `src/` commits stale. Evidence in
`bench/measurements/02-90/`.

### The question, as it was actually asked

`TODO.md` §3 carried `scsd1` and `degen2` as two instances losing real work
behind D151's cap, **described as losing the same way** — "each with warm
iterations exactly equal to cold", which is D148's guard throwing the repaired
trajectory away and charging the attempt plus the whole cold solve. The item
asked for a predictor of a doomed trajectory and said plainly that it needed a
hypothesis about the mechanism rather than another sweep, because the
shortfall was already refused: both are short by 1, the same as the sixteen
that win (D151).

The hypothesis taken was the most direct one available. `build_warm_basis`
closes a shortfall with two loops — one promoting the logical of an UNCOVERED
row, which is structurally forced, and one walking in index order, which is a
free choice. If the losers use the free branch and the winners the forced one,
the repair knows which it used before it pays for anything.

### The measurement — the premise is false

A diagnostic build reported, per instance, what the repair saw and what the
guard then did. Over the twenty instances whose mapped basis arrives short by
4 or fewer:

| | `degen2` | `scsd1` |
|---|---|---|
| warm iterations / cold | 565 / 565 | **314 / 89** |
| settled dual violation, warm | **12.91** | **0** |
| guard fires | **yes** | **no** |
| work against cold | 3.6751x | 3.7165x |

**Only `degen2` is D148's guard.** `scsd1` reaches a dual feasible point, the
guard accepts it, and the warm solve genuinely runs 314 iterations where the
cold one runs 89. `degen2`'s two counts are equal because the cold restart
resets the counter, which is the guard's documented behaviour.

So §3 asks for a predictor of a phenomenon that occurs **once** in twenty.
`lotfi` is a third instance costing more warm than cold, 1.2753x, and its
guard does not fire either.

### What was refuted

**The branch hypothesis, outright.** 18 of the 20 promote entirely by index
order, including all three losers and 15 winners. The only two that use the
uncovered-row loop are `pilot-we` and `ship08l`, and both win — at 0.0939x and
0.0332x, two of the three best ratios in the set.

**Ten more quantities, all known before the solve**, and none separates the
three losers from the seventeen winners: the shortfall, the count of rows the
wanted basis leaves uncovered, the promotions each loop made, `nrow`, `ncol`,
wanted-basic columns, wanted-basic logicals, `S/nrow`, wanted logicals over
`nrow`, and `ncol/nrow`. The narrowest is `ncol` with three winners inside a
range whose ends are 2.6x apart, in a variable spanning 35 to 3148 over twenty
points. An interval that is merely sparse is not a predictor and D46 is why.

**And two figures the source comment quoted are stale**, which is how the false
premise survived. D151's per-instance table is 2026-08-19's tree. Now: netlib
work geometric mean **0.1910** against the sweep's predicted 0.1916, worst
**3.7165x** rather than 4.65x, `degen2` 3.6751x rather than 4.09x, `25fv47`
**wins** at 0.9854 where it was 1.0349, three instances costing more warm than
cold rather than four, Kennington 0.0071 against 0.0070. **The mean is exactly
where the sweep put it and the two worst cases are not**, so a comment quoting
a worst case ages faster than one quoting a mean.

### What is left open, handed to `TODO.md`

**§3 is now one instance and it is `degen2`.** Why a repaired warm basis on
that model settles 12.91 outside dual feasibility is not answered, and one
instance cannot supply a threshold. What would change that is a second
instance of the same mechanism, which means a wider model population — §4's
fourth instance set, whose reopen condition this becomes.

**`scsd1` is a different question and it is new.** Its warm start is accepted,
correct, and 3.5x longer than starting cold. Nothing here says why, and it is
not a guard problem, so nothing in D148 or D151 bears on it.

---

## D179 — A rule wider than the firing row has a supply on 19 of 24 instances and none at all on two, so it improves the residue and cannot close it

**2026-08-24.** No source change. A public-API probe, three calls per
instance. Evidence in `bench/measurements/02-91/`.

### The question, as it was actually asked

`TODO.md` has carried "a rank argument WIDER than the firing row" as what the
invalid-basis item needs, ever since D141 closed every local repair with a
count: of the firings that publish a basis one member too long, **66 of 80 and
86 of 152 have no other basic column of that row resting on its own bound**.

Before designing a wider rule, the thing to establish is whether a wider rule
has anything to work with. This counts the supply, and it does not attempt the
rank argument.

A candidate is a basic variable — column or logical — whose published value
rests **exactly** on one of its own declared bounds. Demoting it to `AT_LOWER`
or `AT_UPPER` is status-consistent on its own. Fixed variables are excluded:
they are nonbasic-eligible on both sides, so demoting one says nothing.

### The count reconciles with 02-48 from an instrument sharing no code with it

**24 netlib instances publish a basis one or more members too long; 0
Kennington instances do.** 02-48 counts solves and the gate solves each
instance twice, so 24 x 2 = **48**, the figure D171 left. Two independent
routes to the same number, and the second one is a probe over the public API
rather than a walk over presolve's records.

### The measurement

| tier | instances whose over-count the model-wide supply covers |
|---|---|
| **exact equality with a bound** | **19 of 24** |
| within one ulp of a bound | 19 of 24 |
| within 1e-9 relative of a bound | 21 of 24 |

The five not covered at exact equality:

| instance | over | supply exact | ulp | 1e-9 rel |
|---|---|---|---|---|
| `bandm` | 18 | 2 | 2 | 8 |
| `capri` | 6 | 3 | 4 | 6 — covered |
| **`fit1p`** | 21 | **0** | **0** | **0** |
| `nesm` | 18 | 0 | 4 | 18 — covered |
| **`share1b`** | 2 | **0** | **0** | **0** |

**`fit1p` and `share1b` have no candidate at any tier**: not one basic variable
in either model rests within 1e-9 of its own bound. No demotion rule of this
shape closes them at any tolerance, so loosening a window is not a route out.
`bandm` is the third no tier reaches, at 8 against 18.

### What was refuted

**"A wider rule closes the item" is refuted.** It improves it a great deal —
the within-row rule had nothing on 66 of 80 firings and this has a supply on 19
of 24 instances — and 3 instances stay wrong at every tolerance measured. Any
rule of this family leaves a residue.

**And loosening the window is refuted as the way to reach the rest**, which is
the same answer D141 gave one level in. Two of the three uncovered instances
have a supply of exactly zero at 1e-9 relative, which is nine orders of margin
above where a rounding argument could live.

### What is left open, handed to `TODO.md`

**The rank argument is still the whole of the work and nothing here touches
it.** A candidate is necessary and not sufficient: demoting a variable whose
column is the only one covering some row makes B singular. Postsolve has no
factorization, and adding one is what such a design would have to justify —
against a bar D137 already recorded from Galabova 2023, which is a valid
starting basis rather than the optimal one.

**This puts a number under the published state of the art's other half.** HiGHS
attempts an assignment and falls back rather than deriving one. On this
population the fall-back is at least 3 of 24 instances, and 5 of 24 without a
tolerance. Accepting the residue was already the alternative `TODO.md` named;
it now has a measured floor rather than a shrug.

---

## D180 — REFUSED: the refactorization interval stays at 64 although 32 is 8.6% cheaper, and the sweep that says so also reaches `pilot`'s optimum without touching a tolerance

**2026-08-24.** No behavioural change: the sweep goes beside the constant in
`src/simplex.c` and into `docs/tolerances.md`, proved object-identical by
`comment_only.sh`. Evidence in `bench/measurements/02-92/`.

### The question, as it was actually asked

`TODO.md` carried this as a standing debt in its own words: the
`REFACTOR_EVERY` 16..256 trajectory sweep is manual, **three of M1's four
defect closures came from it**, and no target automates it. D119 was the
fourth.

The interval decides how many Forrest-Tomlin updates accumulate before the
basis is factorized again. It changes the numerical trajectory and it must not
change whether an answer is correct, so an instance right at the shipping 64
and wrong at another interval is a defect the gate cannot see — the gate builds
one binary. **The constant had no sweep in the source and none in
`docs/tolerances.md`**, which no other interval in that file can say.

### The two controls, without which none of the rest counts

`record-netlib-64.txt` is **identical to the committed `bench/results/netlib.txt`
on all 94 instance lines**; only the footer differs, because the sweep runs
without `-b`. So the harness at the shipping setting reproduces the gate
exactly and every difference elsewhere is the setting.

Six settings produced six distinct binary md5s, and **0 of 94 instances report
identical work at every setting**. A sweep where that is all of them has
measured one binary N times (D82), and the script aborts on either failure.

### The measurement

**No answer changes verdict at any interval.** 94 netlib and 29 infeasible
instances at six intervals: `objective=ok`, `checker=ok` and `det=ok`
throughout. The interval hides no defect at HEAD.

| interval | 8 | 16 | 32 | **64** | 128 | 256 |
|---|---|---|---|---|---|---|
| work, geometric mean against 64 | 1.0318 | 0.9484 | **0.9143** | 1.0000 | 1.1873 | 1.5663 |
| worst single instance | 2.267 | 4.430 | 2.819 | — | 5.881 | 9.125 |
| worst instance | `grow22` | `d2q06c` | `grow15` | — | `d2q06c` | `nesm` |

A geometric mean of per-instance ratios, never a sum: two instances are 74% of
this set's total (D46).

### Why 64 stays although it is not the minimum

**32 reads 8.6% better on the mean and 16 reads 5.2% better.** Both cost a
worst case — `grow15` 2.819x at 32, `d2q06c` 4.430x at 16 — and the mean is
flat across 16, 32 and 64 while the worst case is not. That is the shape D151
chose the warm-repair cap by, and it points the same way.

**And 32 costs accuracy that no verdict reports.** `pilot87` goes from
1.044e-07 to 5.329e-05 against Koch, three orders worse, and still passes: the
gate's window is `1e-6 * 301.7 = 3.017e-04`, so it clears with 5.7x to spare.
That is D177's open half seen from the other side — the gate watches
suboptimality against its own baseline and cannot see an answer getting worse
in absolute terms.

So the value is refused a change, chosen for the worst case and for `pilot87`,
and it has its measurement on both sides now.

### What the sweep found instead

`pilot`'s distance from Koch, per interval:

| interval | 8 | 16 | 32 | **64** | 128 | 256 |
|---|---|---|---|---|---|---|
| gap to Koch | **0** | 2.312e-05 | **0** | **2.312e-05** | **0** | 5.266e-09 |
| work on that instance | 6.83e9 | 5.33e9 | **3.80e9** | 4.72e9 | 4.95e9 | 4.37e9 |

**It is not monotone.** Three of six intervals publish Koch's optimum exactly,
at the shipping tolerance, and the shipping interval is one of the two that do
not. 32 reaches it for **0.805x** of 64's work on that instance.

**And 5.266e-09 is D174's own number.** That entry's `dual_tol` sweep reads
`pilot` at -5.266e-09 for `dual_tol = 1e-9`. Two independent knobs land on the
same value, so both select among one small set of neighbouring vertices. Across
everything measured so far the published point takes three distinct values:
Koch exactly, 5.266e-09 away, and 2.312e-05 away.

### What this refines, and what it does not refute

**D174's mechanism stands.** `DUAL_TOL` is what lets the solve stop while a
column is still improving, and that is unchanged. What is new is that the
tolerance does not decide *where* it stops: the trajectory does, and the
interval perturbs the trajectory enough to reach the optimum without touching
any tolerance. D174's heading says "and nothing else", which is right about the
cause of stopping early and was never tested against this knob.

### What is left open, handed to `TODO.md`

**Which vertex `pilot` should publish, and at what price, is item 1 and it is
the maintainer's call.** This adds a second route to it that nobody had, and
the route is not shippable as it stands: 32 fixes `pilot` and costs `pilot87`
three orders and `grow15` 2.819x.

**The standing debt is closed.** The sweep is
`bench/measurements/02-92/run-refactor-sweep.sh` and takes the settings as
arguments.

---

## D181 — The fourth set does not reopen §3 — the mapped basis arrives short by 5.6% of rows and the repair never runs — and it prices §2 at three of four warm starts

**2026-08-24.** No source change. Two public-API probes and one throwaway
diagnostic build, all reverted. Evidence in `bench/measurements/02-93/`.

### The question, as it was actually asked

D178 left `degen2` as the only instance in twenty where D148's guard throws a
repaired warm trajectory away, and one instance cannot supply a threshold.
§3's reopen condition is a second instance; §4 says a fourth instance set is
the executable form of that condition.

**The set was already in this repository and the warm campaign had never been
run on it.** `plato-fome` and `plato-pds` came in with D115, from Mittelmann's
LPopt. `plato-pds` is 6.4 hours of wall clock and was not attempted.
`plato-fome` is four instances, and `fome11 -> fome12 -> fome13` doubles
exactly in both dimensions — the one family here that can say whether a cost
grows linearly or worse with nothing else about the model changing.

### §3 is not reopened

Four instances, **0 repairs fired**. The block §3 is about never executes, so
the set says nothing about a doomed trajectory. Only `fome21` starts warm at
all and its guard does not fire.

### Why, and the probe could not say until it was told to

`build_warm_basis` refuses a short count past the cap and a long count at the
same line, and **neither printed anything**. From outside the two read the same
and they are different questions: the cap is D151's and refused a change, a
long map is refused because none had been measured. Every exit names itself
now.

| instance | `nrow` | mapped basis short by | past the cap of 4 by | fraction of `nrow` |
|---|---|---|---|---|
| `fome11` | 12142 | **681** | 677 | **5.609%** |
| `fome12` | 24284 | **1357** | 1353 | **5.588%** |
| `fome13` | 48568 | **2720** | 2716 | **5.600%** |
| `fome21` | 64574 | **0** | — | — |

**The shortfall is a constant 5.6% of rows and doubles exactly when the model
does**: 1357/681 = 1.993, 2720/1357 = 2.004. netlib's worst is 596 (`dfl001`).

### What was refuted

**Both cap shapes, on this set, and the refusal is now measured rather than
assumed.** The absolute cap would have to go from 4 to 2720, and D149 measured
the blanket repair at `dfl001` 172x work for a 596-short repair the guard then
threw away. A relative cap is no better: D151 swept `S <= r*nrow` to a best
mean at r = 0.0036, and 5.6% is **15 times** that.

**And this entry's own first inference was wrong.** It read the published
over-count and concluded the map arrives long, because `build_warm_basis`
refuses a long count outright and that fitted. The published basis and the
mapped basis are different objects: the map arrives short, by 681 where the
published count is over by 8.

### What the set does say, and it is §2's price

| instance | published count over by | warm work against cold |
|---|---|---|
| `fome11` | **8** | **1.0000** |
| `fome12` | **21** | **1.0000** |
| `fome13` | **53** | **1.0000** |
| `fome21` | **0** | **0.5258** |

**Three against three, one against one.** The three publishing a wrong count
are exactly the three whose warm re-solve does bit-identical work to the cold
one, and the one publishing an exact count saves **47%**. That is D129's and
D130's attribution on netlib, reproduced on a set netlib's conclusions were
never taken on, and `fome13`'s 53 is larger than netlib's worst — `fit1p` at 21
(D179).

**The two counts grow at different rates on the same family.** The mapped
shortfall doubles exactly; the published over-count goes 8 -> 21 -> 53, 2.63x
then 2.52x. Different objects with different mechanisms — the mapping drops
stored-basic members presolve removes again, `SINGLETON_COL` adds one per
firing — and this family is the only place here where the two rates can be read
apart.

### What is left open, handed to `TODO.md`

**§3 needs a second instance and `plato-fome` is not it.** `plato-pds` has not
been tried and is 6.4 hours; `plato-nug` is three instances and is unmeasured
rather than unsolvable.

**§4's argument is stronger than it was.** Its case is that the population
decides the verdict, and here it does: on netlib the invalid basis costs 24 of
94 instances their warm start, and on this set it costs 3 of 4 — with the one
that escapes saving 47%.

---

## D182 — `plato-nug` solves one of three, and presolve reaches a median of zero rows on every plato set against nine per cent on netlib

**2026-08-25.** No source change. Evidence in `bench/measurements/02-94/`.

### The question, as it was actually asked

`TODO.md` §4 has carried `plato-nug` as "unmeasured rather than unsolvable"
since D115 and nobody had checked the sentence. It is the one shape this tree
does not have: every model JAOS reads today is economic, transport or
stochastic, and a QAP relaxation is none of those. A set that cannot say
whether it solves cannot be part of an argument about model population.

### The measurement

| instance | rows x cols | result |
|---|---|---|
| **`nug08-3rd`** | 19728 x 20448 | **solves**, 34424 iterations, **294654930775 work units** |
| `nug20` | 15240 x 72600 | **did not finish in 3600 s** |
| `nug30` | 52260 x 379350 | **did not finish in 1800 s** |

The seconds are a stopping rule and not a cost: they make "did not finish in T"
checkable. Work units are the cost and they are in the record.

**The family is not ordered by rows.** `nug20` has 4488 fewer rows than
`nug08-3rd` and 3.5x the columns, and does not finish in five times the time
`nug08-3rd` needs.

`nug08-3rd`'s answer is clean — `checker=ok`, `det=ok`, `cert=yes`,
`Q=8.89e-10`, `rsub=4.14e-12` — and it publishes 214.00000000040001 with no
reference optimum to score against.

**So `plato-nug` is not a practical fourth set and one instance of it is
usable.** That is a different answer from either of the two §4 allowed for.

### What the one that solves says

**Presolve removes nothing at all on it**: `19728/20448/139008` unchanged on
both sides of the arrow. Not a row, not a column, not a nonzero.

That is worth the table, because §4's whole argument is that the population
decides the verdict. Taken from the committed records with no run at all:

| set | n | median rows removed | median nonzeros removed | removes nothing at all |
|---|---|---|---|---|
| netlib | 94 | **9.04%** | 6.35% | 8 of 94 |
| Kennington | 16 | 12.57% | **21.57%** | 0 of 16 |
| `plato-pds` | 8 | **2.93%** | 1.40% | 0 of 8 |
| `plato-fome` | 4 | **0.00%** | 2.06% | 0 of 4 |
| `plato-nug` | 1 | **0.00%** | **0.00%** | **1 of 1** |

**JAOS's presolve reaches a median of 0 to 3% of rows on the plato sets against
9% on netlib.**

### What is NOT claimed, and the care matters

§4 quotes Galabova 2023 for the opposite direction: HiGHS's presolve
geometric-mean speed-up is **1.10 on netlib against 1.67** on a modern set, so
its presolve is worth *more* there while JAOS's *reaches* less. **These are not
a matched comparison** — that set is Mittelmann's benchmarks plus four
industrial models, not `plato-pds`/`fome`/`nug` — and reach is not the same
quantity as speed-up. What is established here is the reach, on JAOS, on these
sets. Reading it as "JAOS's presolve is 1.67x behind" would be inventing a
number.

### What was refuted about the instrument

**The first version of `run-nug.sh` produced output that read as a finished
measurement and carried no evidence.** It filtered the runner's console with
`grep -vE` on a leading bracket, to drop the per-instance timing prefix — and
the runner prefixes the RECORD line with that same bracket, so the only line
carrying work units, the digest and the checker numbers was the one thrown
away. The summary line and `gate: PASS` survived. The record comes from `-o`
now and never from the console.

### What is left open, handed to `TODO.md`

**Whether `plato-nug` enters the record as a set of one.** `nug08-3rd` is the
only QAP-shaped model this tree can measure, and a set of one instance is a
statement about one instance (D46). Adopting it is a decision this entry does
not take.

**`nug20` and `nug30` have a cap and not a verdict.** Neither is known to be
unsolvable; both are known not to finish in the time they were given.

---

## D183 — `pilot87`'s suboptimality bound moves because its dual solution is not unique, and its priced primal answer does not move at all

**2026-08-25.** No source change. Evidence in `bench/measurements/02-95/`.

### The question, as it was actually asked

`TODO.md` has carried this standing debt since D92: "`pilot87`'s suboptimality
bound is not understood — `gap_positive` moves 0.0068 to 26.7 across D92's
variants while every answer is inside tolerance."

D180 handed it a case the record cannot resolve. At `REFACTOR_EVERY` 8 and 256,
`pilot87` publishes the **identical objective**, `301.71035883543192`, and two
different digests — and `bench/run.c` hashes `x` and `y` into one, so its record
cannot say which half moved.

### The measurement

| | `REFACTOR_EVERY = 8` | `REFACTOR_EVERY = 256` |
|---|---|---|
| objective | 301.71035883543192 | **identical** |
| digest of `x` | `334a6e189adb4b45` | `875faca3a7332c93` |
| digest of `y` | `2e204845a11c08e6` | `a6d5e3366594ebae` |
| `gap_positive` | 0.00139018 | **0.00140689** |
| `unquantified_rays` | **10** | **14** |
| basic members | 2031 of 2030 rows | 2031 of 2030 rows |

**The priced primal answer does not move.** 987 of 4883 columns move and **738
of them cost exactly zero**, so they cannot touch the objective. The 249 that
are priced move by at most **4.44e-15**, carrying 1.073e-13 of objective traffic
between them, of which 5.16e-15 survives.

**The duals are genuinely different.** 1817 of 2030 moved, **166 by more than
1e-9 relative**, which no rounding-level change reaches. The largest relative
move is **55.7%**, the largest absolute **1.79e-04** on row 1079 (-3.34e-04 to
-1.55e-04), and **none changes sign**, so both dual solutions are feasible.

**So `pilot87` has a non-unique dual solution.** `gap_positive` is built from
the duals and follows them; `unquantified_rays` counts columns whose multiplier
the checker calls zero and follows them too, 10 against 14. **The bound moving
is a property of the model rather than a defect in the bound.**

### What was refuted, including this entry's own first reading

**"Two genuinely different vertices with the same objective" is wrong**, and it
was this session's first reading, taken from the digests alone. 987 columns
moving while `c'x` holds to the last bit is a claim that needs the cost beside
it: with the cost in the dump, most of those columns price at nothing and the
rest move at the arithmetic floor. The primal answer is the same answer.

### What this does NOT establish

D92's variants moved `gap_positive` from **0.0068 to 26.7**, a factor of 3900.
The two settings here move it by **1.2%**. The mechanism is demonstrated and
the magnitude is not reproduced; D92's variants are not in this tree. Reading
this as "the 3900x is explained" would be claiming a span nobody measured.

### What is left open, handed to `TODO.md`

**The debt is answered in mechanism and not in magnitude**, and it stays with
that stated. What would close it is D92's variants rebuilt against this tree,
which is a separate piece of work with no measurement either way.

**`pilot87` publishes 2031 basic members against 2030 rows at both settings**,
which is §2's defect and puts it among D179's 24. Nothing new, recorded because
the probe saw it.

---

## D184 — `DUAL_TOL` is 1e-9 and all four wrong points are gone, at 1.0339x work on netlib and 1.0976x on Kennington, which D174 had not measured

**2026-08-25.** `src/simplex.c`: `DUAL_TOL` from `1e-7` to `1e-9`. Evidence in
`bench/measurements/02-96/`, on D174's sweep in `02-84/`.

### The question, as it was actually asked

D174 measured the repair and refused to take the call, because it costs work
and `TODO.md` item 1 has carried it as a judgement since. **The maintainer took
it on 2026-08-25**, choosing this over the alternative D180 had produced —
`REFACTOR_EVERY` at 32, which reaches `pilot`'s optimum for 8.6% LESS work and
costs `pilot87` three orders of accuracy that no verdict reports.

### The measurement

| | before | after |
|---|---|---|
| `pilot` | 2.312e-05 | **5.266e-09** |
| `pilot87` | 1.044e-07 | **exactly Koch** |
| `scsd6` | 1.118e-09 | **exactly Koch** |
| `etamacro` | 1.315e-08 | **1.137e-13** |

| set | work, geometric mean | worst instance |
|---|---|---|
| netlib | **1.0339x** | `d2q06c` **5.319x** |
| **Kennington** | **1.0976x** | **`pds-20` 4.815x** |
| infeasible | — | 29 refusals unmoved, 0 regressed |

Past the 2.0x per-instance bar: `agg3` 2.28x, `d2q06c` 5.32x, `nesm` 2.17x,
`perold` 2.80x, `pilot-ja` 2.21x, and `pds-20` 4.81x.

**`gate: PASS` on all three sets** — every instance solves with `shape=ok`,
`objective=ok`, `checker=ok`, `det=ok`. The regressions are cost against the
baseline and no answer got worse.

### What D174 did not have, and it was put in front of the maintainer

**netlib's 1.0339x is D174's own prediction to four figures. Kennington's
1.0976x is not in D174 at all** — that sweep ran on netlib, and `CLAUDE.md` is
explicit that Kennington is where a change that scales badly shows. `pds-20`
goes from 6.15e9 to 2.96e10 work units. The decision was re-put with that
number before the baselines were rewritten, and reaffirmed.

### D177 is why two of the regressions are visible at all

`bnl1` reports its suboptimality bound going 7.09e-15 -> 1.57e-14 and `scsd1`
4.3e-17 -> 1.96e-16. **Both sit far under the `RSUB_FLOOR = 1e-9` this project
shipped until the day before**, so neither would have been reported. That
predicate watched 4 solves of 110 and watches 84 now.

### What was found in review, and then refuted by measuring it

**`DUAL_TOL` is read in two different units.** Everywhere it bounds a rate,
`s->d[v] < -s->dual_tol`. At one site, `can_move`, it bounds a PRODUCT:
`wrong_way * |other - nonbasic_value| > dual_tol`, which is an objective
quantity. That function's comment argues carefully that the product is the
right thing to test because it has no space, and **never says what it is
compared against**. Tightening by two decades makes that site 100x more eager,
which nobody asked for.

**It changes nothing, and that is measured.** A variant holding `can_move` at
1e-7 while every other reader goes to 1e-9 publishes **94 of 94 identical
digests** at a work geometric mean of **1.0000x**, best and worst both 1.000x,
all four target instances gap for gap. Two distinct binaries, and the harness
aborts if the patch does not rewrite the line.

**The likely reason is structural and is not measured here**: `can_move` feeds
`anything_to_move`, its own comment says such columns need a primal pivot, and
`SPECS.md` has the primal simplex as missing. A test whose consequence does not
exist decides nothing.

Giving that site its own constant is refused for now — it would be fitting a
number with no sweep behind it.

### What was NOT done

**No independent verdict was taken.** `CLAUDE.md` asks for `jaos-measurer` to
judge a finished candidate in a context that did not produce the numbers, and
this session was instructed not to spawn subagents. The per-instance evidence
is in 02-96 so the judgement can still be made.

### What is left open, handed to `TODO.md`

**`can_move`'s threshold has the wrong units and no measurement can see it
today.** It becomes live the moment a primal pivot exists, which is `SPECS.md`'s
missing primal simplex, and that is its reopen condition.

**Kennington's 1.0976x is accepted rather than explained.** Nothing here says
why `pds-20` needs 4.8x the work at a tighter tolerance.

---

## D185 — The gate has an absolute bar on suboptimality at 1e-6, placed on 123 solves across five sets, and it rejects the answer this project shipped two days ago

**2026-08-25.** `bench/run.c`: `RSUB_CEILING = 1e-6`, a per-instance predicate
that reads no baseline. No change to `src/`. Evidence in
`bench/measurements/02-97/`.

### The question, as it was actually asked

`TODO.md` item 5: "the gate cannot see a suboptimal answer … the instrument
exists and its zero point is wrong."

`RSUB_FLOOR` and `RSUB_REGRESSION_FACTOR` ask whether the bound **got worse**.
A bound already bad when the baseline was written reads as permanently fine,
which is how `pilot` published a point 2.31e-05 above the optimum with nothing
in this gate saying a word.

### Why it could not be placed until now, and what freed it

**Until D184 it would have failed `pilot` and `pilot87`**, and a bar that
rejects what is already wrong is a decision about those answers rather than
about the predicate. D184 fixed both. **Item 5 was blocked by item 1 and
nothing else**, which was not obvious until item 1 closed.

### The measurement — where the bar goes

`TODO.md` said what this needed: a threshold, and one instance separating
cleanly on one set is not one.

| set | instances | worst `rsub` |
|---|---|---|
| netlib | 94 | **1.4e-07** (`pilot`) |
| Kennington | 16 | 4.18e-14 |
| `plato-pds` | 8 | 9.91e-15 |
| `plato-fome` | 4 | 1.15e-13 |
| `plato-nug` | 1 | 4.14e-12 |

**1e-6 clears the worst by 7.1x** and every set but netlib sits below 1.2e-13.
It is not fitted: the band between 1.4e-07 and the 6.91e-05 it has to catch is
494x wide.

Today it fires on nothing. `gate: PASS`, `0 regressed, 0 improved, 0 new` on
all three sets, `bench/results/*.txt` **byte-identical**.

### The case it must reject, built and confirmed

A bar nothing reaches is indistinguishable from a bar that is never evaluated.
The new runner was built against the solver as it stood at `bc398a5`, two days
earlier, with a canary that aborts if the two trees agree on `DUAL_TOL`.

| instance | `rsub` at `bc398a5` | the bar |
|---|---|---|
| `pilot` | 6.91e-05 | **OVER-CEILING** |
| `pilot87` | 2.54e-06 | **OVER-CEILING** |
| `wood1p` | 7.4e-09 | quiet |
| `adlittle` | 1.52e-15 | quiet |
| | | **`gate: NOT MET`** |

**`wood1p` is the control that matters.** It publishes the exactly correctly
rounded optimum — `refeps = 0` in 02-83 — and carries the loosest certificate
of any clean instance on either set. It stays quiet, so the bar is not
rejecting a perfect answer. And the verdict flips rather than the message
alone.

### What was refuted about the instrument

**The first version did not compile, and `make test` reported
`4 Tests 0 Failures OK`.** A `
` inside the new `emit` became a real line
break, and `make test` builds the library and the unit suite — **it does not
compile `bench/run.c`**. A change to the runner is green under `make test`
whether or not it builds at all. `make bench` is what compiles it.

### What is left open, handed to `TODO.md`

**The bar is placed above every value this project can currently produce, so
it catches a degradation and not a standing error.** If a future population
sits higher, the number is re-derived from that population rather than kept.

**It reads only `relative_suboptimality`**, which is `gap_positive` over
`1 + |objective|` and is only as strong as `gap_certified` says. D183 showed
that quantity follows a dual solution which is not unique on `pilot87`, so the
bar has a moving quantity under it. On the current sets the movement is 1.2%
against 7.1x of headroom.

---

## D186 — REFUSED: no mapped basis arrives long in 101 calls, so a demotion rule in `build_warm_basis` has nothing to act on and 35 of 90 netlib warm starts fall back to cold

**2026-08-25.** No source change. Evidence in `bench/measurements/02-98/`.

### The question, as it was actually asked

`build_warm_basis` refuses a long count with a premise written into the source:
"A LONG count is still refused: **no long map has been measured**, and a
demotion rule for an unmeasured case would be a constant fitted to nothing."

Nobody had counted. A refusal's premise has expired unnoticed three times here
(D24, D94, D101), so a premise of the form "no X has been measured" is worth
measuring.

**The hypothesis behind it.** §2's rank argument is needed at POSTSOLVE, which
has no factorization, and that is what has made the item look expensive.
`build_warm_basis` runs inside the solver, and its own comment says rank stays
where it already lives — `repair_singular_basis`, downstream of it. So a
demotion THERE would need no new rank machinery, and D179 had already measured
the supply: 19 of 24 instances covered.

### The measurement

| set | calls | exact | short | **long** |
|---|---|---|---|---|
| netlib | 90 | 35 | 55 | **0** |
| Kennington | 11 | 6 | 5 | **0** |
| **both** | **101** | 41 | 60 | **0** |

### What was refuted

**The premise holds and the hypothesis is dead.** No mapped basis arrives long
anywhere in 101 calls, so a demotion rule in `build_warm_basis` has no
population to act on. The cheap route to §2's cost is closed, and it is closed
by a count rather than by an argument.

**The published basis and the mapped basis move in opposite directions.** D179
counted 24 netlib instances publishing a basis one or more members too long;
this counts 0 mapped bases too long and 55 too short. `fit1p` publishes **over
by 21** and maps **short by 241**. `SINGLETON_COL` adds a BASIC at postsolve
without freeing a slot; presolve's mapping drops every stored-basic member
presolve removes again. Nothing that repairs one touches the other.

### What the census gives that nobody had

**35 of 90 netlib warm starts fall back to a cold solve** because their
shortfall is past `WARM_REPAIR_MAX_SHORT = 4`. Kennington loses none: all five
of its short maps are within the cap, which is D151's own finding that
Kennington does not vote on the value. That is D151's cap priced per instance
rather than as a geometric mean.

The worst shortfalls are `sctap3` 596, `sctap2` 432, `dfl001` 343, `seba` 331,
`fit1p` 241, `fit2p` 237.

**One figure disagrees with D149 and is recorded rather than resolved.** That
entry refused the blanket repair on `dfl001` "paying 172x for a 596-short
repair". Today `dfl001` is 343 short and `sctap3` is 596. D149 is 2026-08-19
and many `src/` commits have landed since, so the tree may have moved under it.
Re-running at that tree is what would tell, and it was not done here.

### What is left open, handed to `TODO.md`

**§2 still needs the rank argument at postsolve**, and this closes the only
cheaper location anyone had proposed for it.

**The `dfl001`/`sctap3` figure.** Either D149 misattributed the instance or the
shortfalls moved. It changes no verdict — the blanket repair is refused on cost
either way — and it is the kind of number this project has found stale before.

## D187 — The primal clean-up priced its row the expensive way, and the saving is 3.2% on `wood1p` against 1.0000017x lost on `pilot87`

Both simplex methods need the same two vectors out of a basis change: row `r`
of `B^-1`, and row `r` of `B^-1 M`. The dual picks its entering column out of
the pricing row; the primal has already chosen one and needs the row to step
every other reduced cost by. `price_and_select` built them, and
`primal_cleanup` repeated the work with a comment saying why it was not
factored out — "pulling them into a helper would put the dual method's
preamble in a function the dual method does not call".

**The premise expired when `TODO.md` §0 gave the primal a second caller with
the same claim.** Re-reading the repeat while extracting it showed it was
never equivalent in the first place.

### What the repeat actually did

`primal_cleanup` ran a dense `jm_lu_btran` and then built `alpha` column by
column through `price_entry`. That is one dot product per column, so it costs
the entire matrix however sparse `rho` is. `price_all` walks the row-wise CSR
mirror instead, where one zero of `rho` skips a whole row of the matrix — the
saving D35 measured, and the one the column view structurally cannot express.
It also set `anpat = -1` and `nrpat = -1`, so the two consumers inside
`pivot()` that read a pattern fell back to full scans as well.

### The measurement

`make netlib netlib-infeas netlib-kennington J=12`, read per instance with
`record_diff.py` (`bench/measurements/02-100/`):

| instance | before | after | ratio |
|---|---|---|---|
| `wood1p` | 55637071 | 53867372 | **0.9682x** |
| `etamacro` | 3309456 | 3308076 | 0.9996x |
| `pilot87` | 17961079189 | 17961110514 | **1.0000017x** |

91 of netlib's 94 bit-identical; `netlib-infeas` and `netlib-kennington`
bit-identical throughout.

**`0 digest change(s)`, and the iteration counts unmoved as well** —
`etamacro` 571, `pilot87` 38000, `wood1p` 694. The trajectory is identical and
only the billed work differs, which is what an equivalent implementation doing
less of the same arithmetic looks like. That is the whole correctness argument
and it is a stronger one than any tolerance.

### What was refuted

**That this was free.** `pilot87` costs 31325 units more, on 1.8e+10. The
sparse route bills the pattern ordering `jm_pattern_order` performs —
`nr + words + nrpat` — which a dense BTRAN never charged for, and on a `rho`
dense enough that the ordering buys nothing that bookkeeping is a small net
loss. It is 1.7 parts per million and it is in the record rather than rounded
away, because the same mechanism on a model with denser rows would be a real
one.

**That the gate would show any of it.** All three sets read `0 regressed,
0 improved, 0 new`. No predicate flipped and nothing passed the 2.0x work bar,
so the summary line was silent while three instances moved. This is the case
the per-instance diff exists for.

### What is left open

Nothing here. The three instances that moved are the three whose solves reach
`primal_cleanup` far enough to price a row from it, which is why 91 of 94 are
bit-identical rather than merely close. `TODO.md` §0 stage 1 is the next
caller of `build_pricing_row` and is unaffected by this entry.

## D188 — The primal simplex's first version, and both defects it shipped with were invisible to a green suite

`TODO.md` §0 stage 1: a phase-2 primal method, priced by Dantzig's rule,
sharing `pivot()` and `build_pricing_row` with the dual. Devex is stage 5 and
is blocked on a paywalled paper; Dantzig needs none, the primal was never a
speed argument here (D81), so correctness first.

**Both of its defects produced a passing test suite and a correct answer.**
Neither was found by reading the code.

### The tests were satisfied by the dual

`run_primal` doctored to declare optimality immediately, without one pivot:
**all four primal tests still passed**. `reenter_after_settling` calls `run()`,
the dual repaired the point, and the objective, the independent checker and
`jaos_iterations() > 0` were all satisfied by the wrong algorithm.

A test asserting a correct answer cannot say which method produced it. The
repair is `n_primal_iters`, a count only `run_primal` raises, carried on the
closing summary line so a caller can see it too; the tests assert on that.
Doctored, both positive tests now fail by name
(`bench/measurements/02-101/run-negative-control.sh`).

### The warm start shifted away the primal's work

`build_warm_basis` arms `shift_pending`, and the next `refresh` pushes every
breached reduced cost onto the feasible side by shifting the cost behind it.
**The dual needs that**: it requires dual feasibility to start and a warm basis
carries no such guarantee. For the primal it removes the work — dual
infeasibility is what the method consumes, so a sweep zeroing every breach
hands the loop an optimal point on arrival.

Measured by enumerating **all 24 bases** a two-row model admits and forcing the
primal on each: **every accepted one gave `0 primal iterations`**, ten of them
from points that were primal feasible and plainly suboptimal, including `(0,5)`
at an objective of 15 against a true 2. After clearing `shift_pending` in
`run_primal`, those ten take real pivots and every one reaches the optimum.

### What was refuted

**That a small model would do.** `load_warm_model`, the fixture the warm-start
tests use, has a singleton row; presolve folds it and takes the model from
three rows to one, and on the one row that survives every basis is already dual
feasible. Two tests were written against it and said something they did not
mean. The primal tests use a model with no singleton of either kind.

**That "iterations > 0" says the primal ran.** See above. It says *a* method
ran.

### A third defect, found on the way and older than this entry

`jm_set_err(s->m, ...)` writes to the model the simplex ran on, which is
`p.reduced` whenever presolve reduced one, and the caller holds `m`. **So every
message the simplex produced was invisible to callers on reduced models.**
Exactly **8 of 94** standard instances carried one through, and they are
precisely the eight whose `presolve=` column is unchanged: `degen2`, `degen3`,
`fit1d`, `fit2d`, `scsd1`, `scsd6`, `scsd8`, `truss`. What it lost includes the
iteration guard's "this is a JAOS defect" and `classify_optimum`'s refusal. The
driver copies the message out on the error path now; all 94 carry it.

### What it cost

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, and every record **byte-identical** — an
unread switch and a method nothing reaches must cost nothing.

### What is left open

**The primal's reach from a cold start is zero, on all 94 standard instances**,
and the harness says so rather than erroring: `unreached 94`. A cold basis is
dual feasible by construction and not primal feasible, so this is §0 stage 4's
number to move, not a defect.

`reenter_after_settling` still calls `run()`, so a forced-primal solve can
finish with dual iterations. That is stage 1's shape; making the re-entry
follow the method is a later question. The unboundedness verdict is refused
rather than declared, per D19, and is stage 7.

## D189 — The primal published a value outside a declared bound as OPTIMAL, and stage 1's own pricing rule is what made it reachable

`primal_ratio_test` scans basic variables and asks which a step would push past
a bound. **No basic variable can express the entering column's own opposite
bound**, so a ratio test built only from rows walks past it.

### The wrong answer

`min -x - 0.5y` over `x + y <= 10` and `x + 2y <= 12`, `x` in `[0, 1]`, `y` in
`[0, 10]`. From the origin the primal prices `x` first — `|d|` of 1 against 0.5
— and moves it up; nothing basic stops it before 10 and `x`'s bound is 1.

| | objective | x | checker |
|---|---|---|---|
| dual | -3.75 | 1 | primal ok |
| primal, before | **-10** | **10** | **primal REFUSED** |
| primal, after | -3.75 | 1 | primal ok |

**Published as `OPTIMAL`, with no signal anywhere in the solve.** Only the
independent checker refused the point (`bench/measurements/02-102/`).

### Why it could not happen before, and why it can now

`primal_cleanup` enters a column only through `wants_a_pivot`, which admits no
column with a declared bound in the improving direction. So the entering
column's other bound was always infinite on that path and the case was
unreachable. D188's pricing rule chooses freely among eligible columns.

`TODO.md` §0 had written, before either landed, that the gap "goes live the
moment a pricing rule chooses entering columns". It went live in the same
session, and was found by asking rather than by a campaign — the gate cannot
reach a primal path at all.

### The repair, and the trap inside it

A bound flip: q crosses its own box, no basis changes.

**The limit is read from `real_upper`/`real_lower` and never from `up`/`lo`.**
Those strip the bounds dual phase 1 invented. Sizing the flip off the raw
arrays would park a variable on a bound the model never declared — the case
`repair_dual_infeasibility` refuses in as many words, and the evidence
`classify_optimum` reads immediately afterwards. A column whose other side is
only an invented bound therefore has no flip available and falls through to the
refusal, which is the honest answer rather than a ray (D19).

It costs no solve: `primal_ratio_test` leaves `B^-1 M_q` in `s->col`, and
moving q by `delta` moves the basics by `-delta * col`.

**It terminates, and the argument is short.** No basis change means the duals
do not move, so `d[q]` does not move; q was eligible because its reduced cost
pointed off the bound it rested on, and at the opposite bound that same sign is
feasible. Each flip strictly reduces the number of dual infeasible columns, and
the objective falls by `d_q * delta`.

### What was refuted

**That the dual's bound-flipping ratio test has a phase-2 primal twin.** It
does not, and looking for one wastes time: in the dual ratio test one row is
scanned across many nonbasics so there are many breakpoints to walk past, while
in the primal only the entering variable moves. The primal long step exists in
phase 1 alone, where the objective is a sum of infeasibilities and therefore
piecewise linear (`docs/research/primal-simplex.md` §2). What this entry adds
is the single flip, which is part of the plain bounded ratio test.

### What it cost

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, every record byte-identical — the gate
reaches no primal path, so the evidence is the case above and the test that
pins it.

### What is left open

The Harris two-pass ratio test and the snap it forces are §0 stage 2 and
untouched by this. Phase 1 is stage 4 and is still what holds the primal's
reach at zero of 94.

## D190 — The primal phase 1 lands and takes the reach from 0 of 94 to 64, and a loan of exactly 1.0 is what found the defect it shipped with

`TODO.md` §0 stage 4: a composite phase 1 in short-step form (Maros 1986). The
phase-1 objective is the sum of the basics' bound violations, `-1` below a
declared bound and `+1` above. **No artificial variables and no second model**
— it works on whatever basis it is given, which is the property crossover needs
and the textbook artificial-variable method cannot offer.

The duals come from `compute_duals` with `s->cost` pointed at the phase-1 cost
vector for one call. The reuse is exact rather than convenient: `compute_duals`
reads `cost`, writes `y` and `d`, and does nothing else.

### Two hypotheses refuted before the right one

`sc50a` stopped after two pivots at -57.2 against a true -64.6, refused by the
D146 guard on a model the dual solves in 47 iterations.

- **Tiny pivots degrading the factorization.** The primal ratio test's minimum
  pivot was swept over `1e-9`, `1e-7`, `1e-5`, `1e-3`: **identical output at
  every setting**, 40 iterations and 183481 units. The floor never binds
  because the method stops after two pivots.
- **Cost shifting at the tail of `pivot()`.** Guarding that one call changed
  nothing — same iterations, same work.

### What found it

An instrument printing what `primal_price` sees when it declares optimality:

```
PRICE-STOP iters=2 maxscaled=0 maxpub=0 borrowed=1 total=1
```

Every reduced cost feasible in both spaces, and one variable carrying a loan of
**exactly 1.0**. A loan of exactly one is not a repair — `shift_to_feasible`
lends the minimum a sign condition needs. It is the size of a phase-1 cost.

**The mechanism.** Phase 1 puts *its own* reduced costs in `d`, gradients of a
sum of violations and so of magnitude one, and `pivot()` lends `cost[v]`
against them — on the **model's** cost vector. Phase 1 was corrupting the
objective phase 2 would then optimise.

**`update_dual` is the site, not `pivot()`'s own call**, which is exactly why
guarding one and not the other changed nothing: `update_dual` lends against
every variable the pricing row touches, once per iteration. `in_primal` guards
both now.

### What it cost, and what it did not fix

`sc50a` moves from 40 iterations and 183481 units to 51 and 58062 — a third of
the work on a different trajectory — **and still ends in `NUMERICAL_ERROR`**.
The loan was real and it was not the only thing wrong.

Over the standard set, bounded at 10x the dual's work per instance
(`bench/measurements/02-103/`):

| | count |
|---|---|
| agree with the dual, checker accepting both | **64** |
| overrun the 10x budget | 16 |
| disagree — the primal returns `NUMERICAL_ERROR` | 12 |
| error | 2 |

Work units primal/dual, geometric mean **3.2352**; iterations 1.7710. That is
what Dantzig pricing costs and is why Devex is stage 5.

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, every record byte-identical — nothing in the
gate can enter a primal path.

### What is left open

**Fourteen instances where the dual reaches an optimum and the primal does
not**, and they are not fourteen questions: `sc50a`, `sc50b`, `sc105` and
`sc205` are one family from one generator failing the same way. That is the
thread to pull first.

The long-step ratio test Maros describes — walking several breakpoints with a
running slope, so several basics become feasible per iteration — is not here.
This is the short-step form, which is correct and slower.

## D191 — The primal's "64 of 94" was 54, and the difference was a guard that was documented, believed, and never applied

`/code-review max` on the branch. Fifteen findings; this entry closes the one
that falsified a published number and records the rest as open.

### The defect

D190 added `in_primal` to stop the primal lending the model's cost vector
against phase-1 gradients, and named **two** sites: `update_dual` and the tail
of `pivot()`. **Only `update_dual` was ever guarded.** The `perl` substitution
for `pivot()` did not apply, it left the comment mangled, and the check that
should have caught it counted occurrences of `in_primal` rather than reading
which lines they were on. Four occurrences was the expected number for five
sites and it looked right.

The struct's own comment claimed both were guarded. So did
`bench/measurements/02-103/README.md`. Both were wrong.

### What it cost

The reviewer measured the loans outstanding when `run_primal` declares
optimality, on a build with the missing line added: `share2b` 43 loans totalling
2773.0, worst single loan **1026.11 on a variable whose true cost is zero**;
`adlittle` 27 loans; `blend` 20; `afiro` 4. **Every loan was raised in phase 1
and phase 2 added none**, which is the mechanism D190 described and failed to
stop.

So the primal was reaching `OPTIMAL` on those models only because the loan had
perturbed the objective, with `settle_shifts` repaying it and the dual
re-entry recovering the answer afterwards.

### The corrected number, and one correction to the correction

Applying the guard at both sites drops the standard set from **64 agreeing to
20**. That is over-correction: the argument only condemns **phase 1**, where
`d` holds a different objective's gradients. In phase 2 `d` is the model's own
reduced cost and the shift is the same legitimate repair the dual makes. Scoped
to phase 1 alone — the flag is `in_phase1` now, set around one call — the
honest figure is:

| | D190 claimed | actual |
|---|---|---|
| agree with the dual | 64 | **54** |
| disagree | 12 | **31** |
| overrun the 10x budget | 16 | 8 |
| error | 2 | 1 |
| work geomean primal/dual | 3.2352 | **3.8332** |

**D190's table is superseded by this one.** Its narrative stands; its counts do
not.

### A second defect the same review found in the harness

`bench/primal.c` classified a designed refusal by matching `"no primal phase
1"` — a string that left `src/simplex.c` in the same commit that added the
phase 1. So `PRIMAL_UNREACHED` became unreachable and `make primal` exited 1 on
the outcome the file's own comment says a runner must not fail on. It matches
the `D19` citation now, and a primal ending in `NUMERICAL_ERROR` has its
message read at all, which it previously did not: that status returns
`JAOS_OK`, so the branch reading `jaos_model_error` never ran for the dominant
failure class.

### What is left open, and it is most of the review

Thirteen findings are recorded in `TODO.md` §0 rather than fixed here. The ones
that would change an answer: **no Bland's rule in phase 1 at all**, and only
half of one in phase 2 (the leaving tie breaks on row position, not variable
index, so there is no finiteness argument); **`refresh`'s repair sweep is a
third unguarded lending path**; **`primal_bound_flip` can move a skipped row by
1e8 times `primal_tol`** when the origin is an invented bound; and **nothing in
the project loop compiles `bench/primal.c` or `bench/warm.c` at all**, which is
why the dead string above survived a full cycle.

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, every record byte-identical.

## D192 — Bland's rule now reaches the primal's leaving variable, and it arms nowhere on netlib

D191's first answer-changing finding, closed. The other three are still open in
`TODO.md` §0.

### The question

Bland's rule is a promise about *both* choices a simplex iteration makes: the
lowest-indexed eligible entering variable, and — among the candidates attaining
the exact minimum ratio — the leaving variable of lowest index. The dual
chooses a row and then a column, so its rule falls on the entering variable and
`jm_bland_pick` is the whole of it. The primal chooses a column and then a row,
so its rule falls on the leaving one, and that half did not exist.

Phase 2 had the entering half in `primal_price` and broke equal ratios on the
first **row position** scanned, which is a choice the basis order makes and not
one the variable index makes. Phase 1 had neither half. So neither phase had a
finiteness argument, and Hall & McKinnon (2004) exhibit LPs that cycle under
the most negative reduced cost, which is exactly the rule both phases price
with.

Expected: no answer changes. A cycle needs a degenerate vertex revisited in a
particular order, and no instance anyone has run reaches one.

### The measurement

`bench/measurements/02-104/`.

**The gate saw nothing.** All three sets `gate: PASS`, `0 regressed, 0
improved, 0 new`, and every file in `bench/results/` byte-identical to the
committed record — `git status bench/results/` empty. The records were stale by
seven `src/` commits, so that covers the whole span from D188 to here and not
this change alone. 110 solution digests and 29 refusal verdicts unmoved.

**`make primal J=12` reproduced D191's figures to four figures**: measured 54,
overrun 8, disagreed 31, errors 1; work geometric mean 3.8332, best `lotfi`
0.9817, worst `sctap2` 14.8415. A no-op on that campaign as well.

**Identical figures are also what a dead branch looks like, so the branch was
counted.** A probe solving with `cfg.force_primal` at `JAOS_LOG_DETAIL` over
all 94: **0 phase-1 arms**, and **5 other arms on 3 instances** — `grow15` 1,
`grow22` 3, `grow7` 1, all still reaching `optimal`. `other` is phase 2 and the
dual's settling re-entry together, because the two print the same sentence.

**The 0 is validated rather than trusted.** Rebuilt in a worktree with
`STALL_FACTOR` forced from 10 to 0: `afiro` 5, `adlittle` 6, `share2b` 15,
`stocfor1` 44, `sc50a` 0. Four of five move off zero, so the probe can see a
phase-1 arm.

### What was refuted

The premise that a stall detector reaching 5 firings on the dual's own set
would reach anything comparable in the primal's phase 1. It reaches nothing.
Phase 1 either improves its total violation every iteration or ends before
`STALL_FACTOR * (nrow + ncol + 1)` iterations pass, on all 94.

`sc50a` arming zero times even at `STALL_FACTOR = 0` refutes a stall as its
mechanism. It improves on every phase-1 iteration and still ends
`NUMERICAL_ERROR`.

### What is left open

The rule was never reached on netlib, so **no instance exercises the
leaving-variable half** and the campaign is not evidence that it is correct.
The seven unit tests are, and that is the same standing `jm_bland_pick` has had
since D26.

D191's other three answer-changing findings are untouched and stay in `TODO.md`
§0: `refresh`'s repair sweep as a third unguarded lending path,
`primal_bound_flip` moving a row the ratio test skipped, and the two phases
sharing one iteration cap.

## D193 — `refresh` is the third place a cost is lent, it fires 30 times inside the primal phase 1, and guarding it trades `pilot4` for `pilot-ja` and `pilotnov`

D191's second answer-changing finding, closed. Two are left open in `TODO.md`
§0.

### The question

`shift_to_feasible` moves `cost[v]` to put a reduced cost back on the feasible
side. The dual requires that. The primal phase 1 does not: it holds gradients
of a sum of bound violations in `d`, so a shift taken against them moves the
model's objective by a number that belongs to another problem. D190 named two
sites and D191 found only one of them guarded. **Neither named the third.**
`refresh` sweeps `shift_to_feasible` over every variable when
`repair_singular_basis` fired or a warm start armed `shift_pending`, and it
read no flag at all.

Asked in two parts, because they need different entries: does the path execute,
and if it does, does guarding it pay?

### The measurement

`bench/measurements/02-105/`.

**It executes, and not rarely.** A worktree with one counting log line inside
the sweep, over all 94 with `cfg.force_primal`: **30 sweeps inside phase 1 on
11 instances**, and 8 outside it on 2. The largest are `pilot` at 3 sweeps
shifting 3990 costs, `dfl001` 2/3560, `pilot87` 2/1924, `pilot-ja` 3/1450,
`greenbeb` 1/1177. **Three instances sweep and shift nothing** — `perold`,
`stair`, `wood1p` — which is the census's own control.

**The gate saw nothing.** All three sets `gate: PASS`, `0 regressed, 0 improved,
0 new`, every file in `bench/results/` byte-identical. `in_phase1` is set only
inside `run_primal_phase1`, so the dual cannot reach the changed line.

**The primal campaign was compared per instance against the parent**, because
its geometric mean is taken over its measured set and that set grew from 54 to
55. A mean over 55 is not comparable to a mean over 54.

- **53 `ok` on both sides, 52 of them bit-identical in primal work units.** The
  only one that moved is `pilot`, at 0.9673.
- **30 `DISAGREE` on both sides, 4 moved**: `greenbeb` 1.0596, `tuff` 0.9994,
  `d2q06c` and `greenbea` at 1.0000 to four figures.
- **Three changed category.** `pilot-ja` overrun → ok at 18536 iterations.
  `pilotnov` `NUMERICAL_ERROR` → ok, and cheaper: 18014 iterations and
  809015777 units become 12640 and 598949184. `pilot4` ok → `NUMERICAL_ERROR`,
  4148 iterations and 112003171 units becoming 5920 and 160747384.

**Net 54 agreeing → 55, and 8 overrun → 7.** Every instance that moved was on
the census list and nothing off it moved, including the three that swept and
shifted zero.

### What was refuted

The reading that this is a rare repair path worth guarding on principle alone.
It is the busiest of the three sites on the instances that have trouble: 30
firings against `update_dual`'s once per iteration is not the comparison, but
1450 costs moved on `pilot-ja` in three calls is a perturbation of the
objective on the scale of the model.

The hope that a correct guard only gains. `pilot4` runs 43% longer and then
ends `JAOS_SOLVE_NUMERICAL_ERROR` with no message, which is the D146 guard's
status rather than a raised error. This is the failure shape `jaos-measure`
names — a repair that fixes what is in front of it and breaks something else —
and it was caught only because the parent was run beside the candidate.

### What is left open

**`pilot4` is a real regression and is not diagnosed.** It goes to `TODO.md` §0
beside the other 30 message-less refusals, which D191 already names as the
dominant failure class.

Phase 2 stays unguarded at all three sites, for D191's reason: there `d` holds
the model's own reduced costs and the shift is the repair the dual makes.
`scsd1` and `scsd6` are the only instances that sweep outside phase 1 and both
already disagree, so that is where a phase-2 argument would have to be tested
if anyone makes one.

D191's other two answer-changing findings are untouched: `primal_bound_flip`
moving a row the ratio test skipped, and the two phases sharing one iteration
cap.

## D194 — 60.5% of the primal campaign's iterations are the dual's, the primal's phase 2 runs exactly one iteration on 80 of 94, and `pilot4` is not a primal regression

> **CORRECTED IN PART BY D195.** This entry's phase-1 counts came from a log
> line printed only when phase 1 succeeds, so eight instances whose phase 1 ran
> and did not finish were counted as having no phase 1 at all — and then read as
> pure phase-2 runs. The 60.5% dual share stands. **"The 8 that run a real phase
> 2 are exactly the 8 whose phase 1 is zero iterations" is false**: 0 of 94 skip
> phase 1, and those 8 never leave it. The corrected split is phase 1 336660
> (39.5%), phase 2 **97 (0.0%)**, dual 515522 (60.5%), with no instance running
> more than 10 phase-2 iterations.

Opened to diagnose the instance D193 broke. It ended somewhere else, and the
number `SPECS.md` publishes for the primal is the casualty.

### The question, as it was asked

D193 took `pilot4` from `optimal` to `NUMERICAL_ERROR` with no error message.
Expected: a phase-1 trajectory changed by the loans D193 stopped, diagnosable
from the loan census that found the defect.

### The measurement

`bench/measurements/02-106/`.

**The message-less refusal is one line.** `src/simplex.c:5496` sets
`JAOS_SOLVE_NUMERICAL_ERROR` on a cold start whose settled point is dual
infeasible, and calls no `jm_set_err`. Every other site that produces that
status writes a message first. That accounts for all 31 disagreeing instances,
and D191 named the class without naming the line.

**`pilot4` was refuted as a primal regression, in three steps.** Its phase 1 is
bit-identical across D193 — same infeasibility at iterations 0, 1000 and 2000,
and `phase 1 reached a feasible point in 2596 iterations` on both sides,
because phase 1 prices on `c1` and never reads `cost`. There were **0 loans
outstanding** at the guard on either side. What differs is **one phase-2 primal
iteration**: 2598 primal of 4148 before (2596 of them phase 1), 2597 of 5920
after. The dual's re-entry then diverges — best infeasibility 251 → 397 →
91084 against 801 → 281 — and leaves a published breach of 6.72712 at variable
668, a column whose own cost is zero and which carries no shift. **Both the old
success and the new failure are the dual re-entry's.**

**That one-or-two-iteration phase 2 is the whole set, not `pilot4`.** Reading
the solver's own summary over all 94 with `cfg.force_primal`:

| | |
|---|---|
| phase-2 primal iterations exactly 1 | **80 of 94** |
| 2 to 10 | 6 |
| more than 10 | 8 |
| zero dual iterations | 8 |
| **dual share of every iteration run** | **60.5%**, 515522 of 852279 |

**The 8 that run a real phase 2 are exactly the 8 whose phase 1 is zero
iterations.** They arrive primal feasible, so nothing hands over. Seven hit the
work limit; the survivor is `pilot87`.

**The mechanism was confirmed by forcing it off.** `update_dual` and the tail
of `pivot()` run `shift_to_feasible` once per iteration on every variable the
pricing row touches, guarded only while `in_phase1`, and it sets `d[v] = 0.0`
on every breached nonbasic — which is exactly what `primal_price` reads.
Guarding phase 2 as well, in a worktree, with the dual's own re-entry still
lending:

| | shipping | phase 2 guarded |
|---|---|---|
| optimal | 56 | **17** |
| numerical error | 31 | 58 |
| work limit | 7 | 19 |
| phase-2 iterations exactly 1 | 80 | **0** |
| phase-2 iterations over 10 | 8 | **91** |
| dual share of all iterations | 60.5% | **0.0%** |

`truss` goes from 2802 phase-2 iterations to 422576. 44 instances lose
`optimal` and 5 gain it: `80bau3b`, `cycle`, `fit1p`, `ship08l`, `ship12l`.

### What was refuted

**D191's reading that guarding both phases "over-corrects".** 54 agreeing
becoming 20 is not an over-correction. It is the removal of the dual's 60.5%.
D191's own conclusion — that in phase 2 `d` holds the model's own reduced costs
and the shift is the repair the dual makes — is true and is beside the point:
the repair is applied to the numbers the primal's pricing rule reads, so the
primal stops.

**The expectation that `pilot4` had a diagnosable phase-1 cause.** It has no
loans, an identical phase 1, and a failure that belongs to another method.

**D188's reading of the re-entry as a harness detail.** One line said a
forced-primal solve can still finish with dual iterations. It is the dominant
term.

### What is left open, and it is a decision

`SPECS.md`'s primal row reads **55 of 94 agreeing**. The primal's own reach,
on the instances where it runs the method, is **17 of 94 optimal**. Both
numbers are true of different things and only one of them is what the row is
read as saying.

The choice — guard phase 2 and publish 17, or leave it and relabel the 55 —
is the maintainer's and is in `TODO.md` §0. **No source changed here.** So is
whether `bench/primal.c` should report the split, which would have made this
visible from the first campaign.

## D195 — The bound flip's 1e10 delta fires on nothing, and chasing it found that D194 counted phase 1 from a log line printed only on success

Two results from one investigation. `TODO.md` §0's third answer-changing
finding is refused with a reopen condition, and D194's phase-1 accounting is
corrected. D194's 60.5% stands; what it said about the other 39.5% does not.

### The question

`primal_bound_flip` takes its destination from `real_upper`/`real_lower` and
its origin from `nonbasic_value`, which reads the raw `lo`/`up` and so may read
a bound dual phase 1 invented. `delta` can then be the size of an artificial
bound, and the ratio test skips every row with `|col[i]| < PIVOT_MIN = 1e-9`
while those rows still move by `delta * col[i]`. D191 put the worst case at
about 9, which is 1e8 times `primal_tol`.

Expected: rare but real, and worth a guard.

### The measurement

`bench/measurements/02-107/`. Both flip sites instrumented in a worktree, all
94 instances, `cfg.force_primal`:

| | phase 2 | phase 1 |
|---|---|---|
| flips | 8 | 10604 |
| origin was an invented bound | 0 | **3974** |
| the phase's own measure grew | **0** | **0** |
| largest `\|delta\|` | 200 | **1e+10** |

**`delta` reaches 1e10 from an invented origin 3974 times and moves neither
phase's own measure.**

### What was refuted, and the first refutation was my own predicate

**The first version asked whether `primal_worst_violation` grew, in both
phases, and reported 113 firings.** Every one was inside phase 1 and every one
was innocent: phase 1 minimises the SUM of violations and `primal_phase1_ratio`
deliberately skips a row already under its bound and moving further under. The
worst growing inside phase 1 is the method working. Phase 2's measure is the
worst; phase 1's is the total; both read 0.

**Phase 1's predicate is validated and phase 2's is not.** Forcing a 1e6 push
after every flip makes 260750 of 393041 flips report the total growing. Two
attempts to reach a phase-2 flip failed: raising `PIVOT_MIN` to 1e-3 reported
nothing but also took the flip count from 10604 to 221, so it moved the
trajectory rather than holding it still; and forcing the damage in phase 2 only
produced no phase-2 flips at all. The phase-2 row is 8 flips with no instrument
test behind it and is written down as unproven.

### The correction to D194

`02-106/split.c` read phase 1's count from `phase 1 reached a feasible point in
N iterations`, **printed only on success**. A phase 1 that ran and did not
finish left the counter at 0, so `phase2 = primal - 0 = primal` and the
instance read as a pure phase-2 run. D194 published that as *the 8 that run a
real phase 2 are exactly the 8 whose phase 1 is zero iterations*. Both halves
are false, and 02-107's canary is what caught it: those 8 produced **2634
phase-1 flips**, which a phase 1 that never ran cannot do.

Re-measured with the count logged after every exit
(`bench/measurements/02-108/`): **0 of 94 skip phase 1**, 86 finish it, and the
same 8 never leave it — reclassified from *never entered* to *never left*.
`wood1p` is 3820 phase-1 iterations and 0 phase-2, where D194 recorded 0 and
3820.

**The headline gets stronger, not weaker.** Phase-2 primal iterations: 8
instances run 0, 80 run exactly 1, 6 run 2 to 10, and **none runs more than
10**. Over all 94 solves: phase 1 **336660 (39.5%)**, **phase 2 97 (0.0%)**,
dual **515522 (60.5%)**. **Ninety-seven phase-2 iterations in the whole
campaign.**

**A second flaw in that probe, found and bounded.** It read
`jaos_status_of` while ignoring `jaos_solve`'s return value, so an instance
whose solve raises a hard error shows the previous solve's status. Exactly one
is affected — `pilot87` prints `optimal` and in fact raises `column 478 prices
at 0 in row 790 of the primal phase 1`. The iteration figures come from log
lines and are untouched.

### The verdict on the flip

**No repair.** The hazard is real by inspection and reaches nothing here, and
fitting a guard to zero observations is what this project's rules forbid.

**Reopen conditions**: a phase-2 flip whose worst violation grows past
`primal_tol`; any change to `PIVOT_MIN`; or a starting basis that is not the
slack basis, because every one of the 3974 invented origins comes from dual
phase 1's loans and a crossover supplies different ones.

### What is left open

D191's fourth answer-changing finding — the two phases sharing one iteration
cap — is untouched.

**And a new one.** `pilot87` reaches `run_primal_phase1`'s refusal after 17165
iterations without ever entering phase 2, and it is the only instance of the 8
unfinished ones that is not a budget. The other seven are all `work limit
reached` inside phase 1. **Phase 1, not phase 2, is where this method spends
its budget**, and that is where stage 5's pricing question actually applies.

## D196 — The iteration cap really is shared across both primal phases and the dual, and phase 1 spends at most 1.68% of it, so only the guard's message needed fixing

D191's last answer-changing finding, closed. All four are now disposed of:
D192 fixed one, D193 fixed one, D195 refused one, and this refuses the fourth
while repairing what it revealed.

### The question

`run_primal_phase1`, `run_primal` and `run` each compute
`ITER_SANITY_FACTOR * (nrow + ncol + 1)` and each test the **cumulative**
`s->iters` against it. So phase 2's allowance is the cap minus phase 1's spend,
and the dual's settling re-entry is third in the queue. D191 put it as: a
phase 1 that uses most of the cap makes phase 2 trip the guard and report phase
1's iterations as its own.

D195 is why it was worth measuring rather than assuming — phase 1 is 39.5% of
every iteration the campaign runs, so the queue is not hypothetical.

### The measurement

`bench/measurements/02-109/`. One log line at each of the three sites, carrying
that method's reading at its own start, over all 94 with `cfg.force_primal`.

| | |
|---|---|
| start phase 2 with the cap already partly spent | **86 of 94** |
| **largest share spent before phase 2** | **1.68%**, on `pilot-ja` |
| next largest | `25fv47` at 1.2% |
| typical | 0.1% to 0.8% |

`ITER_SANITY_FACTOR` is 200 times the model's size. **Phase 1 uses at most
three of those 200.**

### What was refuted

The half of D191's finding that predicts a wrong verdict. Phase 2 cannot be
squeezed into tripping the guard by phase 1 on this set, and it is not close:
the worst case leaves 98.3% of the cap.

### What was NOT refuted, and was fixed

**The message.** Phase 2's read `after N primal iterations` off the cumulative
count, which after a phase 1 is phase 1's number under phase 2's label. Both
primal messages now carry the phase's own count, the position in the solve, and
the cap. `phase2_entered` is the only new state and it is read in one error
string.

A message that nothing on this set can print was worth changing because **two
sessions running lost time to a count that meant something other than what it
was called** — D194's phase-1 figure came from a log line printed only on
success, and this guard's count came from the wrong scope. The cure for both is
the same: name the scope in the label.

### The cost

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, and every
file in `bench/results/` byte-identical to the committed record. `make configs`
exits 0 on all five configurations. 110 solution digests and 29 refusal
verdicts unmoved.

### What is left open

**The cap sharing itself stays, and its reopen conditions are in the source
beside it**: `ITER_SANITY_FACTOR` dropping below about 60, or a phase 1 given a
harder job than reaching feasibility from the slack basis — a crossover's
basis, which is the motivating case for this entire feature.

**`bench/primal.c` still does not report the split.** Three decisions in a row
would have been cheaper if it did, and D194 would not have been wrong. Doing it
honestly needs the solver to log phase 1's count on **every** exit and not only
on success, which is the same defect D195 found in a probe. That is the next
item in `TODO.md` §0 and it is independent of the 55-versus-17 decision.

## D197 — The primal campaign reports which method did the work, and two instruments now agree that phase 2 is 0.0% of it

`TODO.md` §0's "either way `bench/primal.c` should report the split", closed.
It was written as independent of the 55-versus-17 decision and it is; that
decision is still open.

### The question

D194, D195 and D196 were each spent working out from outside what a "primal"
campaign's iterations actually are. The runner reported `primal=iters/work`
for solves the dual finished, because `reenter_after_settling` calls `run()`.
Nothing in the record said so.

### What landed

**`sx` gains `n_phase1_iters`, assigned after the phase-1 call in `run_primal`
so it is written on every exit.** That placement is the substance: the count
was previously readable only from `phase 1 reached a feasible point in N
iterations`, which is printed on **success**, so a phase 1 that ran and did not
finish read as one that never ran — the error D195 corrected in D194.

Both closing summaries carry it. `bench/primal.c` prints
`split=p1:N/p2:N/dual:N` on every record line, **including the overrun
branch**, and leads its summary with the campaign's totals by method.

### The measurement

`bench/measurements/02-110/`. The runner's own summary over all 94:

| | iterations | share |
|---|---|---|
| phase 1 | 336660 | 39.5% |
| **phase 2** | **97** | **0.0%** |
| dual re-entry | 515522 | 60.5% |

**Identical to `bench/measurements/02-108/`**, which produced the same three
figures through a patched worktree and a separate probe. Two instruments, two
code paths, the same numbers — and per instance on five named cases including
`wood1p` at p1:3820 p2:0 dual:0, the one D194 recorded backwards.

Everything else the campaign reports is unchanged: measured 55, overrun 7,
disagreed 31, errors 1, work geometric mean 3.9023.

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical. `make configs` exits 0 on all five.

### The test is validated, and its first control was worthless

`test_the_summary_separates_phase_1_from_phase_2` pins the property that
failed. Injecting the defect — `n_phase1_iters` forced to 0 in a worktree —
gives exactly one FAIL and it is this test.

**The first control broke the build instead**, because zeroing the assignment
left `phase1_entered` unused and `-Werror` refused it. A control that cannot
compile proves nothing, and "make test came back non-zero" is not the claim
"this test caught it". The script prints every FAIL line now rather than an
exit code.

The non-success path is not reproduced in the suite and the test says why: it
needs a phase 1 that takes many iterations and then runs out of budget, which
no two-row model reaches. `wood1p` is the named case in the campaign.

### Two defects this shipped with, both caught before the record

- **`make configs` caught the test helper**, used only inside a block the two
  fault builds compile out, so `-Werror=unused-function` refused it there while
  a plain `make test` passed. The same shape as D154 and D-10, and the third
  time this trap has been paid for.
- **`write_result` never learned the three new fields.** `read_result` expected
  17 and the `fprintf` still wrote 14, so every worker file failed to parse and
  `make primal J=12` reported all 94 as `worker died`. **Invisible at `-j 1`**,
  which runs in process and never crosses that boundary — and `-j 1` is exactly
  where the columns had been cross-checked. The `perl` substitution that should
  have applied it reported success because `perl` exits 0 whether or not `s///`
  matched. Every other edit in this change verified its match count; this one
  did not, and that is the whole reason it survived.

### What is left open

The 55-versus-17 decision, unchanged and still the maintainer's. What this
adds is that the campaign now states the case for it in its own output rather
than needing three decisions to reconstruct.

## D198 — Phase 1 was under-billed by `nvar` on every iteration, and charging it honestly costs two instances and exposes an `O(nvar)` clear that should be `O(nrow)`

`TODO.md` §0's remainder list, first item. No answer changes anywhere; two
campaign verdicts do, and the reason is a budget rather than the solver.

### The question

`primal_phase1_costs` clears `nvar` doubles and then reads `nrow` basics, and
billed `nrow`. `docs/work-units.md`'s rule is one unit per variable looked at,
and a write is a look. Against 336660 phase-1 iterations across the standard
set (D197) the omission is not a rounding error.

Expected: a uniform few per cent, and no verdict moving.

### The measurement

`bench/measurements/02-111/`.

**Every instance paid, and none was identical.** Over the 53 that are `ok` on
both sides: **0 of 53 bit-identical**, work geometric mean **1.0625**, worst
`standata` **1.1759**, best `grow22` 1.0007. The 29 that disagree on both sides
moved too, worst `stocfor3` at 1.1586.

**Nothing on the gate moved.** All three sets `gate: PASS`, `0 regressed, 0
improved, 0 new`, every file in `bench/results/` byte-identical.
`primal_phase1_costs` is reachable only through `run_primal`, and only
`cfg.force_primal` reaches that.

**Four instances changed category and the expectation was wrong about that.**
`bnl2` and `tuff` go DISAGREE → overrun; `pilot-ja` and `standmps` go ok →
DISAGREE. Campaign totals: measured **55 → 53**, overrun **7 → 9**, work
geometric mean against the dual **3.9023 → 4.0039**.

**None of the four is a solver regression.** The arithmetic is identical; the
budget is `10x` the dual's work units, so billing honestly exhausts it sooner
and phase 1 or the dual's re-entry stops where it previously ran on. `tuff`'s
phase 1 goes from 843 iterations to 805, `wood1p`'s from 3820 to 3764. The work
was always being done. `pilot-ja` and `standmps` end `NUMERICAL_ERROR` rather
than `WORK_LIMIT` because the budget runs out inside the settling re-entry and
leaves the point for the D146 guard to refuse — the message-less refusal D194
localised to `src/simplex.c:5496`.

Iterations by method: phase 1 336660 → 325776 (39.5% → 38.8%), phase 2 97 → 95
(0.0%), dual re-entry 515522 → 513203 (60.5% → 61.2%).

### What it exposes, and it is worth more than the fix

**The clear is `O(nvar)` per iteration to zero at most `nrow` entries.** Only
basics that violate a bound are ever set, so at most `nrow` of `nvar` positions
are non-zero, and `nvar` is `ncol + nrow`. Clearing only what the previous call
set makes it `O(nrow)` and would recover most of what this entry just charged.

**That was invisible while the sweep was free**, which is the general argument
for billing honestly and is now an instance of it rather than a principle. It
is `TODO.md` §0's next item.

### Landed with it: four records describing a solver that no longer exists

`run_primal`'s header opened `The primal simplex, phase 2 only` and said
`There is no primal phase 1 yet` and `a cold start never gets here`. All three
have been false since phase 1 landed, and D195 measured the exact opposite of
the last: 0 of 94 skip phase 1. In `bench/primal.c`, `PRIMAL_UNREACHED`'s
comment, the `all_ok` comment and **the message the runner prints** all said
the primal declines because there is no phase 1.

Landed together because they mislead a reader immediately, and this session has
twice paid for a claim that outlived its code — D194's own error was of exactly
this kind.

### What is left open

`SPECS.md`'s primal row now reads 53 of 94, not 55. The 55-versus-17 decision
is unchanged and still the maintainer's; what moved is only the honest cost of
the left-hand column.

## D199 — The phase-1 clear stops sweeping every variable to undo the basis, and the campaign returns to its pre-D198 verdicts at 0.42% more work

D198's own next item, closed. The pair is the same rule applied twice, and the
second half was only reachable because the first made it visible.

### The question

`primal_phase1_costs` cleared all `nvar` doubles of `c1` on every phase-1
iteration. At most `nrow` of those positions are ever set — only a basic that
violates a declared bound gets a `±1`, and a basis holds distinct variables —
and `nvar` is `ncol + nrow`. D198 measured what the sweep costs by billing it;
this removes it.

### The measurement

`bench/measurements/02-112/`. The clear now visits exactly the positions the
previous call recorded, in a `[nrow]` array, and bills `cleared + nrow`.

**D198 → D199**, over the 53 instances `ok` on both sides:

| | |
|---|---|
| work geometric mean | **0.9452** |
| cheapest | `standata` **0.8511** |
| dearest | `grow22` 0.9993 |
| **primal iteration counts that moved** | **0** |
| **primal objectives that moved** | **0** |

Four verdicts recover: `pilot-ja` and `standmps` DISAGREE → ok, `bnl2` and
`tuff` overrun → DISAGREE. Campaign totals return to **measured 55, overrun
7**, from D198's 53 and 9.

**pre-D198 → D199**: **0 category changes**, work geometric mean **1.0042**,
dearest `ganges` 1.0169, and again 0 iterations and 0 objectives moved.

Campaign work geometric mean against the dual across the three trees: **3.9023
→ 4.0039 → 3.9186**. Iterations by method: phase 1 336660 → 325776 → 336064,
phase 2 97 → 95 → 97, dual re-entry 515522 → 513203 → 515435.

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical. `make configs` exits 0 on all five
configurations, `sanitize` included — which is the build that would catch a
bad write to the new `[nrow]` array.

### What was checked rather than asserted

**That no digit of any answer moves.** The comparison reads the objective
column and the iteration column as well as the work column, because work alone
cannot support that claim. Both come back with zero instances moved on both
comparisons. The array ends value for value in the state a full clear would
leave it in; only the cost changes.

### What was refuted

The expectation that removing the sweep returns the campaign to its pre-D198
numbers exactly. It returns to its pre-D198 **verdicts** at **1.0042** of its
work. That 0.42% is the clear billed for the positions it visits, where before
D198 it was billed for none of them. **It is not recoverable by writing the
code differently — the work is real, and an honest counter charges for it.**

### What is left open

`SPECS.md`'s primal row goes back to 53 → **55 of 94**, at **3.9186x**. The
55-versus-17 decision is unchanged and still the maintainer's.

The general point is worth keeping where a reader will meet it: **an unbilled
sweep is invisible to every campaign this project runs.** That is the argument
behind `docs/work-units.md`'s rule, and this pair is the first time it has cost
and then repaid something measurable.

## D200 — Two of phase 1's four refusals now refresh before refusing, neither is reached on the standard set, and phase 1 can finally be watched and stopped

`TODO.md` §0's remainder list, two items. No answer changes; a class of wrong
refusal is removed and a capability that was missing for a milestone is added.

### The question

`pivot()` steps `d` in place every iteration and the factorization is patched
rather than rebuilt, so a verdict read off those numbers is exactly what D20
refuses. Phase 2's optimality test has gated on `!s->verified` since it landed.
Phase 1 has four refusals and **two of them are verdicts**: `q < 0` ("nothing
improves the phase-1 objective") and `r < 0` ("no declared bound stops this
column"). Neither gated.

The other two are not verdicts: the iteration guard is a defect guard, and the
tiny-pivot refusal already rebuilds and retries.

Expected: some of the 31 disagreeing instances would be rescued.

### The measurement

`bench/measurements/02-113/`.

**The primal campaign came back byte-identical** — `diff` over the whole
record, not the summary line. That rules out "the gate fired and the fresh
reading agreed", which would have cost an extra refresh and moved the units.

Counting over all 94 confirms the branch is not reached:

| refusal | reached |
|---|---|
| `q-retry` (the new gate) | **0** |
| `q-refuse` | **0** |
| `r-retry` (the new gate) | **0** |
| `r-refuse` | **0** |
| `tinypivot` | 1 |
| `tinypivot-retry` | 13 |

**The positive control is inside the table.** `tinypivot` is ungated and
`pilot87` ends there — the campaign's one ERROR — so a blind probe would show
zero in that row too.

**A count nobody had**: phase 1's tiny-pivot retry fires **13 times across 5
instances** — `dfl001` 7, `d6cube` 2, `greenbeb` 2, `pilot87` 1, `tuff` 1.

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical. `make configs` exits 0 on all five.

### What was refuted

**The expectation that gating would rescue instances.** It rescues none,
because neither branch is reached: the 31 disagreeing instances end at the D146
guard in `run()` (D194 localised it to `src/simplex.c:5496`), not inside phase
1 at all. Phase 1 either reaches feasibility or runs out of budget.

### Landed with it: phase 1 can be watched and stopped

Phase 2 and the dual both offer `progress_cb`; phase 1 did not, so a caller
could neither see nor stop the part of a forced-primal solve that spends 39.5%
of its iterations (D197). A budget could end it and a person could not.

`infeas_best` now carries phase 1's own total. `run_primal` set it to 0.0 at
entry with a comment saying the primal holds the point feasible from end to
end — true of phase 2 and false of every cold start, which runs phase 1 first
(D195). It becomes 0.0 at the hand-over, where it is true.

**The test is validated and the record says which half caught the fault.**
`test_a_watcher_can_stop_the_primal_phase_1` requires `INTERRUPTED` without the
line phase 1 logs on finishing. With phase 1's callback guarded off, exactly one
test fails and it is this one — **on the status assertion, not the log one**
(`Expected 7 Was 1`): without the callback the two-row model reaches OPTIMAL,
because phase 2 takes one iteration and the dual finishes before another
callback beat. The log assertion covers the model where that would not hold.

### What is left open

**The gate stays, unfired.** It costs nothing when it does not fire and removes
a class of wrong refusal no campaign here can currently produce — the standing
`jm_bland_pick` has had since D26. **Reopen conditions**: any instance reaching
`q-refuse` or `r-refuse`, and a starting basis that is not the slack basis,
because a crossover's reaches phase 1 with drift a cold start never has.

Two items remain on §0's remainder list: the hand-over check's zero margin
against two computations of `xb`, and `s->col`'s contract being a comment with
five writers.

## D201 — The hand-over's zero margin is 55000x in practice, and `s->col`'s contract stops being a comment with five other writers

`TODO.md` §0's remainder list, closed. One refusal with a number behind it, and
one invariant moved out of prose.

### The hand-over margin

Phase 1 stops when `primal_phase1_costs` returns exactly 0.0 — every basic
inside `primal_tol` of the `xb` it carried through the pivots. `run_primal`
then refreshes, recomputing `xb` from a fresh factorization, and re-checks the
worst violation against the same `primal_tol` exactly, with nothing between
them, reporting a JAOS defect if it fails.

**Measured** (`bench/measurements/02-114/`), over all 94 with
`cfg.force_primal`:

| | |
|---|---|
| reach the hand-over | 86 of 94 |
| refreshed violation exactly 0.0 | **62 of 86** |
| largest | `ganges` **1.81899e-12** |
| next three | `greenbeb` 6.43e-13, `sierra` 4.55e-13, `greenbea` 2.29e-13 |
| `primal_tol` | 1e-07 |
| **worst as a fraction of the bar** | **0.000018** |

The eight that never reach it are the eight that never leave phase 1 (D195).

**No repair.** The margin is 55000x and widening the check would add a second
constant with no measurement on either side of it, which is the mistake this
project's first rule exists to prevent. **Reopen conditions**: any instance
whose ratio passes about 0.01, a change to `primal_tol`, or a starting basis
that is not the slack basis, because a crossover's arrives with drift a cold
start never has.

### `s->col`'s contract

`primal_bound_flip` reads `B^-1 M_q` out of `s->col` where the ratio test left
it, and **`s->col` has five other writers**, two of which alias it as `rhs`. The
contract was a comment. `jaos-testing`'s rule is that an invariant another piece
of code depends on is an assert or a test, and this project has a documented
case of a correct, prominent warning comment being violated by new code and
costing weeks.

It recomputes the column into **its own buffer** — never the shared scratch,
which would corrupt what it observes — and compares **bit for bit**, because
the claim is that nothing wrote it rather than that something wrote something
close. `s->work` is saved and restored so a debug build bills what the release
build bills. One FTRAN per flip, compiled out by `-DNDEBUG`.

**Validated, and it answered two questions at once.** `s->col[0]` perturbed
immediately before the check aborts the suite at
`test_simplex.c:3560`. So the suite **does** reach the flip — which a green
assert alone would never have told anyone — and the assert catches the
violation.

### The cost

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical: the shipping build compiles the assert out.
`make configs` exits 0 on all five, `sanitize` included, which is where it runs.

### What is left open

**§0's remainder list is empty.** What remains in §0 is the 55-versus-17
decision, which is the maintainer's, and then stage 2 (Harris in primal form)
or stage 5 (Devex, blocked on a paywalled source) — and D195 has already said
stage 5's pricing question belongs to phase 1 rather than phase 2.

## D202 — An abandoned solve published the previous solve's iteration total, and the column added to expose the split is what exposed it

**The question.** `bench/primal.c` needed three numbers per solve: total
iterations, the primal's share, and phase 1's share of that. It read all three
by parsing the solver's closing SUMMARY sentence, and `tests/test_simplex.c`
carried a second copy of that parser which required a different substring.
Moving the two new counts onto `jaos_model` beside `solve_iters` was expected
to delete both parsers and be strictly safer than reading numbers out of prose.

**The measurement.** `bench/measurements/02-115/`. `solve_iters` has exactly
one writer, inside `publish()`, and `publish()` runs only when the solve
returned `JAOS_OK`. So an abandoned solve left the field holding whatever the
model was solved with last time. `bench/primal.c` solves each model with the
dual first and then forces the primal on the same model, so what it subtracted
was a difference between two different solves.

`pilot87` is the whole population: the one instance of the standard 94 that
takes the hard refusal path. Two trees, one machine, one session.

| tree | pilot87's split column |
|---|---|
| parent `dc2beee` | `p1:17165 / p2:0 / dual:20835` |
| the fix | `p1:17165 / p2:0 / dual:0` |

38000 minus 17165 is 20835: the dual reference solve's total minus the primal's
own count, printed as a dual re-entry that never ran. The campaign headline
moved from 536270 (61.5%) to 515435 (60.5%) once it was removed.

The defect was reachable only because two of the three counts moved onto the
struct and the third stayed behind `publish()`'s gate. The parser this
milestone deleted read the total from `"abandoned after %lld iterations"`,
which is printed on both branches, so it had never been wrong about this.

**What was refuted.**

- *That "written on every exit from `jm_dual_simplex`" was true.* Three returns
  run before an `sx` exists: a presolve error, and `sx_init` failing on either
  the first build or the cold restart. `jm_dual_simplex` zeroes all three
  counts on entry now, which is what makes the sentence true.
- *That writing the total before `publish()` was enough.* On the
  presolve-reduced path `publish` writes the REDUCED model and can fail before
  `jm_postsolve_expand` copies up, leaving the caller's total at the entry zero
  while the two counts below were written anyway. The same negative column,
  through an out-of-memory door. The write belongs below the call.
- *That `solve_iters == solve_primal_iters` is the invariant of an abandoned
  solve.* It holds only when the refusal came from inside `run_primal`.
  `run_primal` can reach OPTIMAL and `reenter_after_settling` fail afterwards,
  and on that path `primal_cleanup`'s pivots and the re-entry's own `run()`
  have correctly raised `s->iters` without touching `n_primal_iters`. A test
  asserting the equality unconditionally would be asserting the defect, and
  `bench/primal.c` reaches that path whenever its work limit bites during the
  re-entry.
- *That a two-row model could separate a carried total from an honest one.*
  Measured under `-DJAOS_NO_PRESOLVE`: the dual solve costs 1 iteration and the
  refused primal solve costs 1 as well, so the two are the same integer there.
  The test says so rather than implying otherwise, and the evidence for the
  defect lives in the measurement directory instead.

**Open.** Nothing.

## D203 — D199's scatter clear buys no seconds, costs none, and needs no density fallback

**The question.** D199 replaced `primal_phase1_costs`'s `memset` over all
`nvar` doubles with a scatter over the positions the last call set. It was
accepted on a work geometric mean of **0.9452** with byte-identical digests,
and no time ratio was taken anywhere in the entry. A review objected with a
specific argument: a `memset` moves `8*nvar` bytes at 32-64 B/cycle while the
replacement is `cleared` scattered 8-byte stores into an array of 0.5-2 MB, so
break-even sits near `cleared` about `nvar/12` -- and the sampled density on
this set is 11-13% of `nvar`. That is the one case where work units and seconds
move in opposite directions, which is why D45 judges a change on three things
rather than one.

The gate cannot answer it. A cold start is dual feasible, so `make netlib`
never enters phase 1 at all. `bench/primal` is the only campaign that does.

**The measurement.** `bench/measurements/02-116/`. Two worktrees on one machine
in one session, `4d1ca2d` (the memset) against `f135e8b` (the scatter). `-j 1`,
alternated, minimum over 5 rounds, geometric mean of per-instance ratios. A
ratio below 1.0 means the scatter is faster.

| movers, phase 1 is 37-94% of the solve | ganges | fit2d | fit2p | pilot | **geomean** |
|---|---|---|---|---|---|
| ratio | 0.9800 | 1.0000 | 1.0122 | 1.0109 | **1.0007** |

| controls, the change cannot reach these | grow15 | grow22 | **geomean** |
|---|---|---|---|
| ratio | 0.9824 | 0.9883 | **0.9853** |

**No measurable effect in either direction.** The controls are the reading that
decides it: they moved 1.5% towards "the scatter is faster" on solves the
scatter never touches, so 1.5% is what noise looks like in this reading. The
movers moved 0.07%. Both sit far inside this host's 6.27% repeatability (D93).

**What was refuted.**

- *The review's own hypothesis.* At the observed densities the scatter is
  neither faster nor slower than the `memset` by anything this host can
  measure. The two largest movers sit at +1.1% and +1.2%, which is smaller than
  the controls' own excursion.
- *That `c1_at` should copy `apat`/`anpat`'s `< 0` memset fallback.* That
  fallback guards a CAPACITY overflow: the pricing pattern can be larger than
  the array kept for it. `n_c1_at` cannot overflow, because at most `nrow`
  positions are ever set -- only a basic variable can be infeasible and a basis
  holds distinct variables -- and `c1_at` is `nrow` long. The density case is
  what was measured here and it costs nothing. A threshold would be a constant
  with no measurement behind it, which is what this project loses weeks to.

**Limits, stated rather than left to be found.** `pilot-ja` produced no pair.
`bench/primal` prints seconds only for instances that reach `ok`, and its
budget is 10x the dual's work, so an instance near that edge can finish under
one tree and overrun under the other -- which is exactly what a 5.5% work
change does to whatever sits closest to the line. Two controls is thin; they
agree with each other to 0.6%, which is why 1.5% is quoted as a floor rather
than used as a correction.

**Open.** Nothing.

## D204 — Phase 1 is 39.5% of the campaign is a statement about two instances, and the median instance is 57.3%

**The question.** `bench/primal`'s "iterations by method" line is a sum over
the set. CLAUDE.md and D46 ban exactly that, because two instances are 74% of
the standard set's total work and a total then becomes a statement about those
two. The line shipped with a comment arguing D46 did not apply here, on the
grounds that the answer is a property of the population rather than a ratio
between two trees.

**The measurement.** The campaign record refutes the comment.

| | share of the campaign's 851596 iterations |
|---|---|
| `d2q06c` | 26.2% |
| `dfl001` | 15.9% |
| the two together | **42.1%** |

`dfl001` never runs phase 2 or the re-entry at all, and `d2q06c` is 214244 of
the 515435 dual re-entry iterations on its own. So "phase 1 39.5%, dual
re-entry 60.5%" was substantially a statement about two instances.

The gap is not small. Against the sum's **39.5%**, the **median per-instance
phase-1 share is 57.3%** over 94 instances. The typical instance spends most of
its forced-primal solve inside phase 1. The total says it spends rather less,
because the two carriers are re-entry-heavy.

**What was refuted.** *That a total is safe when it is a fraction of a
population rather than a ratio between trees.* D46's objection is about which
members carry the number, and that is independent of what the number divides.
The fix is not to delete the total, which answers a real question, but to print
the two largest carriers by name and the median beside it -- so a reader sees
in the same line whether it describes the population or two members of it.

**Open.** Nothing.

## D205 — The most common primal failure published a verdict with no sentence, on 31 of 94

**The question.** `bench/primal.c`'s record kept only "different verdicts" on
every DISAGREE line, and its own comment says why that is not enough: it cannot
tell a wrong answer from a refusal the method was designed to make. A block was
added to recover `jaos_model_error` for those lines. It did not fix anything,
and the reason was not on the bench side.

**The measurement.** 31 of 94 instances end their forced-primal solve at
`NUMERICAL_ERROR` because the settled point is not dual feasible. That site
wrote no message at all. Every other `NUMERICAL_ERROR` in `src/simplex.c`
explains itself -- eleven `jm_set_err` sites, each paired with the refusal it
describes -- so `jaos_model_error` returned the empty string on exactly the
instances a reader would open it for.

| | DISAGREE lines carrying a solver message |
|---|---|
| before | **0 of 31** |
| after | **31 of 31** |

Measured before and after the bench-side recovery block landed, and it read 0
of 31 both times, which is what says the bench was never the defect. The
verdict now names the breach and the start it came from.

**What was refuted.** *That the empty note was a bench defect.* Widening
`jm_dual_simplex`'s message-copy condition to every non-OPTIMAL outcome was
tried first, and it was the wrong repair twice over. It delivered nothing,
because there was no message to deliver. And it opened a new exposure:
`reenter_after_settling` recovers from a `refresh` failure that has already
written "the basis went singular", nothing cleared the buffer, so every
non-OPTIMAL verdict became a candidate for a stale sentence -- the 29
correctly-refused instances among them. All eleven `jm_set_err` sites were then
checked against the outcome they pair with, and not one pairs with INFEASIBLE,
UNBOUNDED, WORK_LIMIT, TIME_LIMIT or INTERRUPTED. So the condition reads
`== NUMERICAL_ERROR`, which loses no message and cannot attach one to a
designed verdict, and the buffer is cleared at both points where a message
outlives the failure it describes.

**Open.** Nothing. What those 31 instances mean for the primal is `TODO.md`
section 0's open decision, not this entry's.

## D206 — A refusal had expired unnoticed, the record was checked by nothing, and the instrument that could not see 0.5% now can

**The question, as the maintainer asked it.** The record has 21175 lines and
the source is 42% comments. Both were true when written. What checks that
either is still true, and what re-runs a refusal whose premise may have
expired? Three worries, in the maintainer's words: documentation stating
old things, refusals made "inside the noise" that were never measurements,
and decisions from 150 commits ago that the tree has moved out from under.
The proposed cure was a branch that deletes every comment, `DECISIONS.md` and
`CHANGELOG.md`, and audits the code from scratch.

**The measurement, part one: how much drift, and where.** A mechanical
sweep over 1729 identifier citations and 87 constants found essentially
none: every mismatch was a sweep setting or a historical figure, correctly
stated. A sweep over 178 present-tense claims of absence ("there is no X",
"not yet", "does not exist") found seven false ones, all the same shape: true
when written, silently false the day the thing was built (347c4fb). One was
not a comment. `TODO.md` said `can_move`'s units stay unmeasured "until the
primal lands", which is D184's stated reopen condition. It landed on
2026-08-25 and nothing checked.

So the drift is real, confined to one shape, and lived in the two documents
and the comments that describe the present. The two history files cannot go
stale by aging; an entry describes the day it closed. The branch-and-delete
cure would have thrown away 27 measured refusals and 140 re-runnable
experiments to fix a class of sentence that can be enumerated.

**The measurement, part two: the expired refusal, re-run.**
`bench/measurements/02-118/run-can-move-units.sh` applies D184's own
one-line variant (rate against rate instead of rate-times-distance against
rate) and runs the standard set and the forced-primal campaign on both trees.
The standard set is byte-identical, as D184 found. **The primal campaign is
not: `scsd1` and `scsd6` move.** The refusal has expired. What the right
units are is section 0 stage 6 in `TODO.md`, open.

**The measurement, part three: an instrument for what seconds cannot see.**
This host repeats to 6.27% (D93) and work units cannot see a layout, branch or
cache change (D45), so a refusal made because a change sat "inside the noise"
was never a measurement of the change. `valgrind --tool=callgrind
--toggle-collect='jm_dual_simplex*'` counts instructions inside the solver
and only there (`jaos_solve` is LTO-inlined and counts zero; the whole
process differs by about a hundred instructions per run from the driver's
clock). Validated: `adlittle` retires 7755048 instructions, twice, and the same
with ASLR off. `tools/icount.sh -r <ref> <instances>` is the tool, and it
carries the D82 canary: every instance counting identically on both trees is
a STOP, because that is what one binary measured twice looks like.

First readings. D199's memset-to-scatter clear, which D203 could only call
"inside the noise": **1.00017** over `afiro`, `adlittle`, `share2b`, an exact
number, and the scatter costs a few hundred instructions more on a tiny model.
D76's `restrict`, refused with the words "a pinned, quiet measurement host
could resolve it": re-tested on the LU kernel signatures in
`bench/measurements/02-119/`, four LU-dominated instances retire **exactly the
same instructions on both trees** (`maros-r7` 13408694332, `dfl001`
180804924692, `25fv47` 6936809399, `fit2p` 102165435926; geometric mean
1.00000). The tool's canary fired, and here that is the finding: the qualifier
changes nothing in the generated code, which is what D76 argued and could only
bound to ±1%. The refusal holds, with a number.

**What was refuted.**

- *That the record should be deleted and rebuilt from the code.* Code shows
  what exists; it cannot show what was tried and refused, and "55 of 94" is a
  campaign result, not a property of the source. The rebuild would have
  produced `DECISIONS.md` again, without the measurements.
- *That eleven refusals lacked a reopen condition.* A first count read the
  entries; the conditions live in `TODO.md`'s refusals table, where step 7 of
  the loop puts them. Truly missing were three: D36, D76, D156. They have
  rows now, and so do D61 and D184.
- *That "identical binaries" is a usable canary.* `-g` puts line numbers in
  the object, so a comment-only edit changes the bytes. The canary is
  identical counts, not identical files.
- *That the 85 `D-NN` citations were dangling.* They name planning-era
  decisions deleted with `.planning/` (D98), and the same number names two
  different decisions in two phases. The appendix at the end of this file
  carries both lists so a citation resolves; nothing new cites them.

**What now checks the record.** `make test` runs `make record-check`
(`tools/record-check.py`): every cited decision exists, every constant in
`docs/tolerances.md` matches the source, every `SPECS.md` label is present
tense and every `partial` row says what is missing, every measurement
directory cited exists, every evidence script's anchor still matches or is
pinned to its commit, and `docs/claims.txt` lists what the record says does
not exist, so the line fails when one of them lands. It found 147 things on
its first run and passes now. `bench/refusals.txt` lists every refusal with
what would reopen it; `make refusals` re-runs the ones with a script. And
`CLAUDE.md` has three process tiers, because this session spent the solver's
full loop on a bench tool's record format.

**Open.** The right units for `can_move` (section 0 stage 6). The assert debt
the comment purge left, one line per contract, in `TODO.md`. `simplex.c` and
`presolve.c` are thinned but not yet landed.

---

## D207 — The primal ratio tests' pivot floor was absolute, and one ulp of the column's own largest entry is the value

**The question.** `TODO.md` §0 stage 8. Both primal ratio tests accepted a
blocking row when `|col[i]| >= PIVOT_MIN`, an absolute 1e-9.
`bench/measurements/02-120/` showed what that costs: on `pilot87` the FTRAN
returns -1.59e-07 for an entry of `B^-1 A_q` that is structurally exactly zero
— row 790 of `B^-1` is a singleton and none of column 478's twelve nonzeros
sits on it — the ratio test takes that row, the BTRAN pricing row reports the
true zero, and on a freshly built factorization the solve refuses in its own
words: *"this is a JAOS defect"*. It is the one `ERROR` of the standard 94.

An absolute floor on a quantity computed from terms of wildly differing scale
is simultaneously too strict and too lax, and which one it is depends on
nothing but the column's magnitudes. `pilot87`'s column reaches 2.1e+14.

**The constant, swept.** `bench/measurements/02-122/`. A `JAOS_DIAG` census
recorded, for every ratio-test call that chose a row,
`r = |col[best]| / (DBL_EPSILON * max_i |col[i]|)`. A floor of
`C * DBL_EPSILON * max|col|` moves a solve **if and only if** that solve's
minimum `r` is below `C`, so one census sweeps every `C` at once. It predicted
the affected set exactly: **15 instances of 15**.

| `PIVOT_MARGIN` | ok | DISAGREE | overrun | ERROR |
|---|---|---|---|---|
| 0 | 55 | 31 | 7 | 1 |
| 3e-1 | 56 | 30 | 8 | 0 |
| **1** | **56** | **30** | **8** | **0** |

`C = 0` reproduces `bench/results/primal.txt` **byte for byte**, work units
included, which is the control. The tally is flat from 0.3 to 1, so the value
is not a spike. On the other side the census is decisive without a campaign:
below `pilot87`'s 3.3457e-06 the floor decides nothing at all.

**1.0 is not fitted to an instance.** It is one ulp of the column's largest
entry: an entry below that cannot be told from zero by a computation that
carried that largest entry. It is also stricter than what this codebase
already calls structurally absent — `DROP_REL` is 1e-14 of the basis matrix's
largest magnitude, about 45 ulps.

**What it costs.** Work geometric mean of per-instance ratios, `C=0` → `C=1`:
**1.000000x on the dual solve**, byte-identical on all 94, and **0.995321x on
the forced primal**, worst `perold` at 1.271581x. The dual figure is the
census's prediction measured rather than argued: the lowest `r` anywhere on
the dual path is `wood1p`'s 5.4855, so at `C = 1` the floor never rejects a
row there and the second pass never runs. On `netlib-infeas` and
`netlib-kennington` `primal_ratio_test` is not reached at all.

**What it does not do.** It does not solve `pilot87`. The refusal becomes
`overrun` — 387235 phase-1 iterations against 17165 — which is the other seven
phase-1 instances' fate and not a self-declared defect. The mechanism behind
the refusal is untouched: the two computations of `(B^-1 A_q)_r` can still
disagree on a fresh factorization, and the pricing-row side of the test is
still absolute. That is carried in `TODO.md` §0.

**What the review changed, and what it was worth.** `numerics-reviewer` found
two serious defects in the first implementation, both about what removing a
row does to the callers. First, an emptied candidate set returned -1, which
both callers read as *"no declared bound stops this column"* and refuse on —
the repair for one false refusal could manufacture another. Second, `*step`
came from the floored pass, and the callers use it to decide a bound flip,
which then moves every row including the one just called meaningless. The
candidate keeps pass 0's answer for both: `*step` is read off every row, so
flip behaviour is exactly the shipping one, and pass 0's winner stands when
the floor leaves nothing. **Both fixes produce a byte-identical campaign
record**, so neither state is reached by these 94 instances. They are
insurance, and the record says so rather than claiming they were needed.

**The implementation, and the trap it avoids.** The column's largest entry
comes out of the loop that already runs, and a second pass runs only when the
row that loop chose falls below the floor. A first version computed the
maximum in a separate scan and billed it; `bench/primal` caps the primal solve
at 10x the dual's **work**, so that charge shortened every primal solve and
moved 18 instances with four verdict changes — `bnl2` at 10.0066x and `tuff`
at 10.0186x sat on the bar. D203 had already written that limit down. The
control caught it.

Skipping the second pass is exact: the winner of a scan is still the winner
over any subset containing it, because `jm_primal_row_wins` orders on
`(step, basis)`. That proof is now two unit tests rather than a comment
(`tests/test_simplex.c`), one of which confirms the other is not vacuous.

**The verdict, and what it caught.** `jaos-measurer` judged `9ef21ef` in a
context that did not produce these numbers: **ACCEPT**. It ran the parent
itself rather than trusting the committed record — the preflight warned that
`primal.txt` predated four `src/` commits — and reproduced it byte for byte,
which is what makes the before/after comparison here sound. 139 of 139 gate
instances bit-identical by md5 taken before and after the run; 79 of 94
forced-primal instances bit-identical; **no instance ends in a worse verdict**,
checked per instance. It also reproduced `cand-1.txt` byte for byte from the
shipping binary, so the environment-variable sweep build and the `constexpr`
build are the same number.

Three corrections came with it, none in the code:

- **Fifteen instances move at `C = 1`, not twelve.** The three the first
  reading missed — `scsd8`, `d6cube`, `dfl001` — are `overrun` on both sides,
  and an `overrun` record line carries no `primal=` field at all, so a key of
  verdict plus primal iterations reads them as unchanged. `stair` at
  `C = 3e-1` was missed the same way, by a key that saw everything but the
  work figure. Counted from a full line diff the census is exact in both
  directions at both settings: 11 instances have `min r < 0.3` and 11 move,
  15 have `min r < 1` and 15 move. `read-sweep.sh` counts from the diff now.
- **The census histogram's legend was off by one decade.** `b0` collects
  everything below 1, not `[0.1, 1)`. Read the wrong way, `wood1p`'s gate line
  predicts that `C = 1` moves it, and the gate came back byte-identical. No
  conclusion here used the histogram — they all use `min_r`, computed
  separately — and the legend is corrected in place.
- **`make refusals` overwrites the evidence it re-tests.** Each script `tee`s
  into its own measurement directory, so the target rewrote
  `02-118/run-can-move-units.txt` and `02-119/run-restrict-icount.txt` as a
  side effect. `bench/refusals.txt` says so now.

`make refusals` at this tree: D76 holds, D199 holds, **D184 flips** — and the
flip is not this change. It flips at the parent too, with the same verdict
text, and `TODO.md` §0 item 6 and `bench/refusals.txt` already carry it.

**Open.** Whether the floor's column-dependent candidate set weakens Bland's
finiteness argument; `improves_without_limit` and the three `alpha[q]` tests
are still absolute. All three are in `TODO.md` §0.

---

## D208 — The pivot floor does not weaken Bland's rule, and the reason `pilot87` stalls is that its phase 1 has already diverged

**The question.** `TODO.md` §0 stage 8b. D207's floor is
`PIVOT_MARGIN * DBL_EPSILON * max_i |col[i]|`, so the ratio test's candidate
set now depends on the entering column's own norm. Bland's rule needs the
lowest-index basic among those attaining the minimum ratio over a **fixed**
set; a set that changes per column is not that. Determinism is not at risk —
the choice is a function of the data — but termination is. The evidence was
circumstantial: `pilot87`'s phase 1 goes 17165 → 387235 iterations at `C = 1`.

**The measurement.** `bench/measurements/02-123/`. `n_bland` counts how often
a solve gives up on Dantzig and arms Bland's rule after a stall. The solver
already prints it as `stalls`; `bench/primal` installs no log callback, so it
was silent. Both settings, over the fifteen instances D207 moves plus three
controls it cannot reach.

**Thirteen instances' phase-1 counts move and twelve of them arm Bland's rule
zero times**, as do all three controls. `pilot87` arms it **once, at iteration
343682** — 89% of the way through the run, after 66031 iterations without
progress.

**The premise does not hold, and the stall is a symptom.** `pilot87`'s
phase-1 objective is a sum of bound violations, so it must never rise. It
falls to 1.24365e+12 at iteration 341000, **turns at 342000**, reaches
1.88282e+24 by 351000 and ends alternating between 3.24653e+20 and
3.23341e+20. **The rise begins before Bland arms.** The anti-cycling rule is
not what went wrong; it fired because the numbers had already gone wrong.
Late iterations cost about 27x more work each, across 6246 refactorizations,
50419 weight restarts and 3139 stability rebuilds.

**The control, and the first one that could not have been one.** `pilot87`
cannot be compared against itself: at `C = 0` it refuses at 17165 and never
reaches 341000. The first control — `d6cube`, `scsd8`, `scrs8` — came back
perfectly clean at both settings and **proves nothing**, because their budgets
end phase 1 at 1000 to 3000 iterations, three hundred thousand short of the
effect. It is kept in the directory because it was nearly written up as
reassurance. The control that works is `dfl001`, at 136695 phase-1 iterations
the longest clean run in the set: its objective starts at 8209, **peaks at
8209**, and ends at 6565.03 with the floor off and 6488.85 with it on. It
never rises, at either setting.

So the divergence is specific to `pilot87` and not a property of long phase-1
runs.

**What is refused, and what is not.** Stage 8b is **closed**: the floor does
not weaken Bland's finiteness argument on this population. What is **not**
settled is whether the floor causes `pilot87`'s divergence or merely uncovers
it — without the floor that solve stops at 17165, so the diverging regime is
unreachable and the two states cannot both be produced. No amount of re-running
changes that.

**And what it says about D207's own gain.** `pilot87` went `ERROR` → `overrun`,
which reads as an improvement and did remove a self-declared defect. Both are
failures. The new one costs 387235 iterations and 179.6e9 work units and ends
1e25 outside its bounds; the old one cost 17165 and said so. `SPECS.md`'s
count is honest and this is what stands behind it.

**Open.** The divergence itself, which is a phase-1 defect and not a floor
defect: what happens between iterations 341000 and 352000, and whether phase 1
should stop when its own objective rises rather than grind to a work limit —
a monotonicity it can check for the cost of one comparison. `TODO.md` §0.

---

## D209 — `PIVOT_MIN` on the pricing row is a stability floor, not a noise floor, and the noise floor it was mistaken for was missing

**The question.** `TODO.md` §0 stage 8a. D207 gave the **column** side of the
pivot test a floor relative to its own scale. The **pricing row** side stayed
absolute: three sites tested `fabs(s->alpha[q]) < PIVOT_MIN` against 1e-9, in
`primal_cleanup`, in phase 1 and in phase 2. D207's constant could not be
carried over: `alpha[q]` is `rho' M_q`, so the terms behind it are
`rho_i * a_iq` and its traffic is `sum_i |rho_i * a_iq|`. `max|col|` is a
different quantity and says nothing about this one.

**The census.** `bench/measurements/02-124/`. At each site, before the test,
`r = |alpha[q]| / (DBL_EPSILON * sum_i |rho_i * a_iq|)`, kept per solve and
separately for the calls where the absolute test fired. 94 instances, both
solves, 106 records.

**What fires today is the best-determined number in the set.** Thirteen calls
fire, and every one has `|alpha[q]|` equal to its own traffic to all
seventeen digits printed — `dfl001` -3.3951065374647217e-10 against
3.3951065374647217e-10, `scsd6` 1.7233192650678575e-15 against itself.
`|sum| = sum|terms|` is a dot product with one term and no cancellation, and
`r ≈ 1/eps ≈ 4.5e+15` is the largest this ratio can be. These are exact.

**So the constant was doing a job its name does not describe.** `PIVOT_MIN`
here is a **stability** floor — `pivot` and `theta_dual` both divide by this
number, and 1e-10 is as dangerous to divide by when it is exact as when it is
not. Rejecting those thirteen is right. `docs/tolerances.md` said it was about
telling a pivot from zero, which is the other question entirely.

**And the other question was going unasked.** The smallest `r` on the set is
`scsd1`'s **0.352457** at the cleanup site: `alpha[q]` standing at a third of
one ulp of its own terms, which is cancellation and not a number. The absolute
test passes it and the solve pivots on it. Nothing else on the set is below 1;
the next values up are `wood1p` at 20740.5 in phase 1 and 32874.7 on the dual
path, and `forplan` at 2.37e+14 in phase 2.

**The repair** adds the relative floor rather than replacing the absolute one,
because the two answer different questions:
`max(PIVOT_MIN, PIVOT_MARGIN * DBL_EPSILON * alpha_traffic(s, q))`. The
constant is D207's own 1.0 — one ulp of the quantity's own terms — and the
census puts it in the middle of a window five orders wide, `(0.352, 20740)`,
and 32874 times below anything the gate reaches.

**The cost, separated from the effect.** The sweep varies one thing: at
`C = 0` the relative half cannot fire while the traffic walk still runs and
still bills, so `C = 0` against the committed record is the walk alone and
`C = 1` against `C = 0` is the floor alone.

| | work geomean | worst |
|---|---|---|
| the walk, dual solve | **1.000001x** | 1.000084x (`wood1p`) |
| the walk, forced primal | **1.000496x** | 1.003609x (`beaconfd`) |

The stability test runs first, so the walk is skipped on every call already
rejected. That is what keeps it at 0.05%: D207's first implementation cost
2.8% and lost `bnl2` and `tuff` to the 10x work bar (D203's trap), and this
one changes **no verdict at all** — 56 / 30 / 8 / 0 at both settings and in the
committed record.

**The floor's own effect is one line in 94.** `scsd1`'s primal work,
3595889 → 3598204. Same verdict, same iterations, same objective, same split.
The census predicted one instance and one moved.

**The gate.** `PASS` on all three sets, `0 regressed, 0 improved, 0 new`.
Unlike D207 the records do **not** come back byte-identical, and that is the
change rather than a regression: this floor sits at a site the dual reaches.
Exactly three lines move, and they are the three instances the census counted
— `etamacro` (1 call) 3308076 → 3308078, `pilot87` (1) 17961110514 →
17961110549, `wood1p` (169) 53867372 → 53871917, which is 27 work units per
call, one column's nonzeros. **Every digest, iteration count and objective is
identical**, on all 139 instances.

**Open.** Nothing in 8a. `docs/tolerances.md`'s `PIVOT_MIN` row now says which
job it does, which is the part of this that outlives the constant.

---

## D210 — The last absolute pivot floor stays absolute: it decides nothing on 139 instances, and the only way to move it is the unsafe one

**The question.** `TODO.md` §0 stage 8c. After D207 and D209, one site still
judges an FTRAN entry against an absolute `PIVOT_MIN`:
`improves_without_limit`. It is also the only one whose test decides a
**published status**, `JAOS_SOLVE_UNBOUNDED`.

**The direction is the opposite of the other sites', and that is most of the
answer.** The loop skips a row below the floor, and a skipped row is one that
does not block. So a smaller floor counts **more** rows as blocking: the
absolute 1e-9 **under**-declares unbounded and prefers `NUMERICAL_ERROR` to a
wrong ray. A relative floor would skip more rows and declare a ray on the
strength of ignoring them — and D19 already says unboundedness needs a proof
against a ray rather than the absence of a blocker. On the other sites the
relative floor trades a wrong pivot for a rejected one; here the same shape
trades a safe refusal for a possibly wrong published verdict.

**The measurement.** `bench/measurements/02-125/`. Under a `JAOS_DIAG` build,
over all three gate sets: calls to the function, times it answered
"unlimited", and rows the absolute floor skipped that had a finite limit —
the only way the test as it stands can be wrong.

| set | records written | calls |
|---|---|---|
| `netlib` | 97 | **0** |
| `netlib-infeas` | 32 | **0** |
| `netlib-kennington` | 19 | **0** |

**Not one of the 139 gate instances reaches the function.** The dump is
written only when the count is non-zero and all three control lines say the
campaigns ran, so this is zero calls rather than zero output.

**Refused.** A constant swept on a population that never exercises it is a
number fitted to nothing, which `CLAUDE.md` names as how this project loses
weeks. The floor stays absolute.

**Not claimed.** The census cannot separate "`classify_optimum` is never
reached" from "it is reached and no column is held by a lent bound". Both give
zero calls. It does not change the refusal — either way the floor decides
nothing — but it matters to stage 7, and one counter in `classify_optimum`
tells them apart.

**Reopens when** any instance reaches `improves_without_limit`. The census
script is its own re-test and returns the exit codes `bench/refusals.txt`
reads: 0 while the refusal holds, 1 when it does not, 2 when it could not run.
Stage 7 — lifting the loan and re-solving — is what would make it live.

---

## D211 — `pilot87`'s phase 1 diverges because the primal ratio test has no rule against a tiny pivot, and a stop on the objective rising is refused because a solve that finishes rises 25x

**The question.** `TODO.md` §0 stage 8d, opened by D208. `pilot87`'s
phase-1 objective is a sum of bound violations and must never rise; it falls
to 1.24365e+12 at iteration 341000, turns at 342000 and reaches 1.88282e+24
by 351000. Two questions in order: what happens in that window, and whether
phase 1 should stop when its own objective rises.

**A hypothesis refuted first.** `pivot` divides by the BTRAN value and moves
the basics by the FTRAN value; the two are checked to `LU_AGREE_TOL` except
on a freshly rebuilt factorization, where the pivot is taken unchecked so a
refusal cannot loop (D86). The diverging regime rebuilds every 62 iterations.
Counted over the run: 20 such unguarded disagreeing pivots on `pilot87`, the
first at 306731, none on `dfl001`. The first sits 35000 iterations before
the turn with the objective still falling, the worst is off by 0.3%, and at
every pivot behind a large rise the two values agree to between 1e-10 and
1e-16. Not the cause.

**What happens.** `bench/measurements/02-126/`, every pivot in
[340000, 352000] against the objective at the top of the next iteration.
The turn is one pivot: iteration **341234**, pivot element **3.26e-09**,
step 1.4e+06, predicted change -1.7e+09, actual **+3.4e+12**. The update
refused the new diagonal (`LU_UPDATE_TOL`), the factorization was rebuilt
(`n_updates` 28 → 0, `n_refactor` 3092 → 3093) and `xb` recomputed from the
basis that now contains that pivot came back 3.4e+12 larger. The same three
lines repeat at 341656 (2.45e-08) and 344067 (1.76e-09, with a step of
3.3e-05 that cannot have moved anything, and a jump of +2.1e+11). By 341657
the next pivot reads `alpha = -1.42e+08`: `B⁻¹` carries entries in the
hundreds of millions from then on. Every pivot below 1e-8 in the window
broke the prediction; the breaks at ordinary pivot sizes all sit after
344350 on a basis already ruined.

**Why here and not earlier.** The primal ratio test takes the minimum ratio,
ties on the basis index, and admits any element down to `PIVOT_MIN`, with no
preference for a larger pivot among near-ties. Over the run `pilot87` took
**582** pivots on elements below 1e-4, 74 of them in [1e-9, 1e-8), the first
at iteration 18341 with the objective at 2.2e+13 and 320000 falling
iterations still to come. `dfl001` took 3. So no single tiny pivot is the
cause; they are the wear, and 341234 is where it broke. The dual side has
the rule the primal lacks: `jm_harris_pick`'s second pass returns the
largest pivot whose true quotient still fits the step. **Stage 2, the Harris
two-pass in primal form, is that rule for this side, blocked on nothing, and
this is the measurement that makes it next.**

**The stop rule, refused.** For all 94 forced-primal solves, the largest
relative rise of the phase-1 objective above its running minimum:

| instance | largest rise | ended |
|---|---|---|
| `pilot87` | 8.43e+13 | overrun |
| **`pilot-ja`** | **25.0**, at iteration 2091 | **`ok`** |
| `scsd8` | 8.94e-04 | overrun |
| `perold` | 1.26e-05 | `ok` |
| 90 others | below 1e-05, 82 of them below 1e-12 | |

`pilot-ja` rose twenty-five times above its best value, spent 316 iterations
more than double it, came back, reached feasibility at 6252 and agreed with
the dual. A nearly singular basis is not a one-way street, and the objective
rising is not a signal that the solve is lost. A threshold would sit between
25 and 8e+13 with one instance on each side, which is a constant fitted to
one instance twice, and it would buy `pilot87` a stop near 350000 instead of
a work limit at 387235 on a solve lost either way.

**Reopens when** no solve that ends `ok` rises above 1e-3 any more.
`relrise.sh` is the re-test.

**Open.** Stage 2. And whether `pilot87` still diverges once it lands, which
is the one measurement that says whether the wear was the whole story.

---

## D212 — Harris's two-pass ratio test in primal form: 60 of 94 agree against 56, `wood1p` publishes a different vertex for 22% less work, and `pilot87` is untouched

**The change.** `TODO.md` §0 stage 2. Both primal ratio tests took the exact
minimum ratio, ties on the basis index, and admitted any pivot down to
`PIVOT_MIN`. D211 measured what that costs: `pilot87` took 582 pivots on
elements below 1e-4 against a control's 3, and its phase-1 objective rose
from 1e+12 to 1e+24. They now build a candidate list and select with
`jm_harris_pick`, the same generic routine the dual side has used since D26:
pass one widens every distance by `primal_tol` and takes the smallest
quotient, pass two returns the **largest pivot** whose exact quotient still
fits. Under Bland's rule the exact minimum with the lowest-index tie stays,
because the finiteness argument needs a fixed rule and not a widened one.

D207's relative floor moves with it. It used to skip a row in the scan while
still reading the travel distance off every row; it now **compacts the
candidate list**, so a floored row neither pivots nor blocks. That is exact
where the skipped second pass was not: Harris's first pass is a minimum over
the candidates, so a removal can change the winner and every removal has to
be applied. A floor that would empty the list leaves it alone, which keeps
D207's rule that `-1` means no declared bound blocks.

**The published form.** `docs/research/harris-primal.md`. `literature-scout`
verified the shape against Hall and McKinnon (2004), read in full, and the
GMSW 1989 scan was then read directly for section 3.3, which settles two
things the design note had marked unknown: Harris "sets α = 0 but retains the
same blocking variable" when the chosen row already stands past its bound,
and the snap this leaves is an error of order δ in `Ax = b`, "eliminated each
time the basis is refactorized". Harris (1973) itself stays unread and no
constant is carried from it.

**The forced-primal campaign**, `bench/results/primal.txt`:

| | before | after |
|---|---|---|
| agree with the dual | 56 | **60** |
| disagree | 30 | 30 |
| overrun a 10x budget | 8 | **4** |
| iterations, primal/dual geomean | 2.2135 | **2.0411** |
| work, primal/dual geomean | 3.9470 | **3.8224** |

Seven instances gained agreement — `bandm`, `fit1d`, `sc50a`, `scrs8`,
`scsd1`, `sctap3`, `tuff` — and **three lost it**: `israel`, `pilot-ja` and
`pilotnov` go `ok` → DISAGREE, each on the settled point failing dual
feasibility. Net +4. `grow15` is the best work ratio at 0.3843 against a
previous best of 0.9861.

**The gate passes all three sets** and three records move, the three whose
dual path reaches `primal_cleanup`. `etamacro` and `pilot87` move by work
alone. **`wood1p` publishes a different optimal vertex**: 694 iterations and
53871917 work units become 560 and 42078864, a work ratio of **0.781**, with
the objective identical to the last bit, the checker green, and the solution
digest `ce88fc7e25bc72be` → `514493ffbde8a088`. A different vertex of the same
optimal face is a correct answer and the gate's `0 regressed` covers it; it is
recorded here because a summary line does not show it.

**What it does not do.** `pilot87` still overruns, at 386392 phase-1
iterations against 387235. D211 named that as the number that would say
whether the tiny pivots were the whole story, and on this evidence they were
not — or the relaxation gives back what the pivot preference buys. The width
sweep separates those two: at `0` the test is pass two alone, the largest
pivot among exact ties, with no relaxation at all.

**Open.** The three regressions, and the width. Both closed by D213, in the
same sweep, `bench/measurements/02-127/`.

---

## D213 — The Harris width is half `primal_tol`, and the measurement did not choose it: one flat plateau on the campaign and a gate that does not move at all

**The change.** `TODO.md` §0 stage 2a. `constexpr double PRIMAL_HARRIS_DELTA =
0.5`, and pass one of the two primal ratio tests widens by
`PRIMAL_HARRIS_DELTA * s->primal_tol` instead of `s->primal_tol`. D212 shipped
the width equal to the tolerance. One constant and one call site; nothing else
in the solver moves.

**Seven settings, and the campaign is flat across four of them.**
`sweep-delta.sh` reads the width from the environment so one binary serves
every setting (D154). Agreement with the dual over the standard 94:

| width | 0 | 0.01 | 0.1 | 0.3 | 0.5 | 1 (D212) | 10 |
|---|---|---|---|---|---|---|---|
| agree | 59 | **61** | **61** | **61** | **61** | 60 | 58 |

**From 0.01 to 0.5 it is the same 61 instances, name for name.** The width buys
nothing inside that band. What the band buys over D212's `1` is one instance,
`wood1p`, and that is the only verdict separating `0.5` from `1`. `0.1` and
`0.5` differ by one instance too, `pilot87`, which fails at both — an `ERROR`
at `0.1`, an overrun at `0.5`.

**The gate does not move anywhere in 0 to 10.** All three sets are
byte-identical to the committed baselines at 0, 0.1, 0.5, 1 and 10, every set
reading `0 regressed, 0 improved, 0 new` (`gate-delta.sh`, `gate-log.txt`).
**At 1e9 it breaks**, and that is the reading that makes the five zeroes worth
having: five identical clean results are also what a probe that never reached
the code prints. At 1e9 the widening is 100 in the units of `xb`, pass one
admits everything, pass two takes the globally largest pivot, and `pilot87`'s
work and objective move and its checker rejects the answer — `gate: NOT MET`.
The instrument reaches the code.

**D211's counter chose nothing, and the earlier reading of it was wrong.**
`pilot87`'s worst relative rise of the phase-1 objective reads 7.2e+11,
8.3e+11, **1.4e+03**, 3.3e+16, 8.1e+11, 8.3e+13, 4.6e+11 across the seven
widths. With only 0, 0.1, 1 and 10 measured, the 1351 at `0.1` looked like the
width bringing the divergence under control, and this file's own measurement
record said so. With `0.01` and `0.3` either side of it at 8.3e+11 and 3.3e+16,
it is one point with no order around it. **The width does not control the
divergence**, and no value here can be justified by `pilot87`.

**So the value is argued, not fitted.** Three things point at `0.5` and none of
them is a campaign number:

1. **The bound.** `docs/research/harris-primal.md` bounds the width above by
   `primal_tol`: a relaxed basic ends at most `delta` outside its bound and
   that is feasible only while `delta <= primal_tol`. D212's `1` met the bound
   with equality. `0.5` keeps a factor of two. An `assert` in `primal_pick`
   pins it, on the widened value itself, because nothing else did: before it,
   `1e9` passed `make test`, and no test reaches that call with a width that
   matters. It does not pass now. `record-check` catches the table first, and
   with the table edited to agree, `test_simplex` aborts on the assert. A
   `static_assert` cannot carry the bound: a comparison of floating
   constants is not an integer constant expression, and `-Wpedantic -Werror`
   rejects one. The runtime check is the better place anyway, because it reads
   the per-model `s->primal_tol` rather than the constant alone.
2. **The published ratio.** MINOS and SNOPT start EXPAND's tolerance at
   `delta_f / 2`. JAOS holds its width fixed and carries no `tau`, no `K` and
   no reset, so this is one ratio borrowed from a method that is not
   implemented here. That is weaker than "the published value" and it is
   written that way in the source, because the first draft of this decision
   cited it as GMSW section 3.2 and that is where the two-pass test comes
   from, not the number.
3. **`0.5` is a power of two**, so `PRIMAL_HARRIS_DELTA * s->primal_tol` is
   exact and no contraction can round it differently. `0.1` and `0.3` cannot
   say that. In a solver whose first rule is bit-identical results on every
   machine, that separates two settings the campaign cannot.

**One reading does separate `0.5`, and it is not a verdict count.** The sweep
writes a per-instance histogram of the phase-1 pivot element by decade, and the
first draft of this entry read only `pilot87`'s `max_rel` out of those files.
Aggregated over all 94, the share of phase-1 pivots below 1e-3 reads 0.2603%,
0.1766%, 0.2579%, 0.2503%, **0.1290%**, 19.0463% and 0.3261% across the seven
widths. `0.5` is the lowest of the seven. The obvious objection to narrowing
the window says the opposite — fewer candidates survive pass one, so pass two
can only reach a smaller pivot — and `counters-*.txt` refutes it without a new
campaign. **Width `1` is the outlier and it is one instance**: 110587 of its
110759 tiny pivots are `pilot87`'s, over 308118 phase-1 pivots. That is a
second reason `1` was a bad setting, independent of the verdict count.

**The multiply is on the field, not the default.** `s->primal_tol` is the
per-model override where one is set, so `delta <= primal_tol` holds for every
model rather than only the default one. A hardcoded 5e-8 would have broken the
argument on any model that tightened the tolerance.

**D212's three regressions, from the same records.** `israel` agrees at width 0
and disagrees at every width above it, so the relaxation costs it. `pilot-ja`
and `pilotnov` disagree at width 0 too, where there is no relaxation at all:
what costs them is pass two taking a larger pivot than the exact minimum did.
All three fail on dual feasibility at the settled point.

**Open, two things.**

1. The plateau's upper edge is somewhere in `(0.5, 1]` and is not resolved.
   Nothing is measured between them, and the cost of being wrong is one
   instance of a campaign that is not a gate.
2. **Consecutive relaxed steps are bounded by nothing in the code.** One
   relaxed step puts a basic at most `width` past its bound. A row already past
   its bound has its distance clamped to zero, so pass one still offers it
   `width / den`, and the next iteration can push it a further `width`. Between
   refactorizations the standing overshoot is therefore up to the number of
   consecutive relaxed steps times the width, and `snap_if_past` checks no
   magnitude at all before pulling that distance onto the bound. The reading
   that would settle it is `max_i (xb[i] - up) / width` at entry to
   `primal_ratio_test` over the forced-primal campaign. Halving the width moves
   this in the safe direction, which is why it is open and not blocking. Found
   by `numerics-reviewer` on this diff.

---

## Appendix: planning-era decisions, D-01 to D-16

`D-<nn>` in a source comment or a document names a decision from the two GSD
planning phases that ran before the planning layer was retired (D98). Their
files were deleted with `.planning/`; the definitions survive in git at
`ef14399^:.planning/phases/*/0N-CONTEXT.md`. They are listed here, one line
each, so a citation resolves to something. **The same number names two
different decisions**, one per phase, and only the file citing it says which:
the ratio-test set is cited from `src/simplex.c`, `src/lu.c` and the
measurement skill; the presolve set from `src/presolve.c`, `src/jaos_internal.h`,
`tests/test_presolve.c`, `bench/*.c`, `docs/work-units.md` and the Makefile.
Nothing new cites these; a new decision gets a D-number.

### Phase 01 — candidate admission in the ratio test

### D-01 — The target is the dense scan, not the admission rule
### D-02 — The admission rule itself does not change: same candidates, same order
### D-03 — The nonbasic list is maintained incrementally at the pivot
### D-04 — If the measurement says it does not pay, the phase closes with a refusal
### D-05 — All 139 solution digests must be identical across the three sets
### D-06 — The digests are what authorise rewriting the work baseline
### D-07 — A differential-equivalence test runs the old scan and the new one side by side
### D-08 — The equivalence is also asserted at run time in debug builds (the `dbg_*` cross-check in `src/simplex.c`)
### D-09 — The counter charges what is actually visited, not `nvar`
### D-10 — The work baselines for all three campaigns are rewritten deliberately, after the digests authorise it
### D-11 — A same-instance time ratio at `J=1` gives the verdict; callgrind explains it
### D-12 — Measured over the standard set as a geometric mean of per-instance ratios (D46)
### D-13 — The result is conclusive at 4.2% or better, three times the harness's repeatability (see D93 for why that bar cannot be tested on this host)

### Phase 02 — presolve and postsolve

### D-01 — The first plan is the scaffolding, not a reduction; the reduced model is a separate `jaos_model`
### D-02 — Presolve iterates to a fixed point under a measured round cap (`JM_PRESOLVE_ROUNDS`)
### D-03 — Presolve is switched off by a build-time constant, `-DJAOS_NO_PRESOLVE`
### D-04 — Presolve runs before scaling, on the model as loaded
### D-05 — A new module, `src/presolve.c`, with prototypes in `src/jaos_internal.h`
### D-06 — Presolve builds a reduced problem and never mutates the caller's model
### D-07 — The postsolve record is a tagged arena, replayed strictly LIFO
### D-08 — The postsolve stack is solve-local, built inside `jm_dual_simplex`
### D-09 — The negative control is presolve compiled off; the off build must reproduce the baselines bit for bit (D96)
### D-10 — One round-trip test per reduction family, on a small model
### D-11 — The existing checker is the instrument and is not touched
### D-12 — Criterion 4 (determinism across two solves) is already enforced by `bench/run.c`
### D-13 — Each reduction reports what it removed via a per-family counter; the counters are internal and read only by in-tree tooling (D64)
### D-14 — Presolve bills the same `jm_work` counter every other kernel bills, one accumulator per solve (D16)
### D-15 — The deliverable number is a geometric mean of per-instance ratios (D46)
### D-16 — The phase does not recalibrate the comparison ladder; it records the ratio and D104 recalibrates

## D214 — `can_move`'s units are a rate read in both spaces, and D27's cautionary `pds-20` costs a fifth of the work

`can_move` decides whether a settled nonbasic with a wrong-signed reduced cost
should be flipped to its other bound. Its last line compared

    wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol

a reduced cost times a distance, against a constant that bounds a reduced cost
everywhere else in the file. **The question was which units are right**, and
whether the answer changes a verdict or only a trajectory.

D184 refused the correction in 2026-08-25 on a measurement — the dual set was
94 of 94 identical — with the reopen condition that the primal simplex land.
It landed (D188), 02-118 found the units live on the primal campaign, and
02-128 re-read them after four commits to the ratio tests. **02-128 could not
conclude**: `pds-20` is the instance D27's argument turns on, `pds-20` is a
Kennington instance, and `make netlib-kennington` was never run.

**The base had to be measured, not assumed.** `preflight.sh` reported the
Kennington and infeasible records written before **25 `src/` commits**. A run
of clean HEAD reproduced all three byte for byte, so those commits were
no-ops on both sets and the committed record is a valid parent.

**A numerics review, taken before the campaign, added a second arm**
(`bench/measurements/02-129/review.md`). A pure scaled-rate test leaves a
column breached only in the published space with no repair anywhere:
`can_move` rejects it, `arm_reentry`'s else branch is guarded by the scaled
`dual_breach` so it is not shifted either, and `wants_a_pivot` refuses it
because its other bound is finite — while `settled_dual_violation`, which is
what the checker judges, still counts it.

| arm | last line |
|---|---|
| `rate` | `wrong_way > s->dual_tol`, D184's one-liner, the scaled space alone |
| `union` | `breached(s, v)`, the same question in both spaces |

**The measurement.** All three sets, both arms, `J=12`. `gate: PASS` and
`baseline: 0 regressed, 0 improved, 0 new` everywhere. `netlib` 94 of 94
bit-identical and `netlib-infeas` 29 of 29 bit-identical at both arms. On
Kennington 14 of 16 are bit-identical and two move:

| instance | iterations | work | ratio |
|---|---|---|---|
| `pds-20` | 90938 → 44790 | 29627237041 → 5837911437 | **0.1970x** |
| `pds-06` | 9305 → 8769 | 237193725 → 196806834 | 0.8297x |

Work geometric mean over the 16: **0.8930x**. Both publish the objective they
published before — `pds-20` at 23821658640 — from a different vertex, so
`digest` and `basis` move together. `checker=ok`, `cert=yes`, `dual=0` on
both, and `pds-20`'s `Q` **falls** from 4.22e-05 to 2.75e-05.

**What was refuted, and it is the review's own prediction.** The review
expected that declining to flip D27's column would move `pds-20`'s `dual=` off
0 and raise `Q`. Neither happened. D27 measured that column costing `pds-20`
3.2x its work when the re-entry flipped columns it should not have; on this
tree, refusing the same family of flips is what makes the instance cheap.
**D27's product is not repaid by the instance D27 chose it for.**

**The two arms are byte-identical to each other on all three sets**, so the
gate cannot choose between them, and three of the four reasons for `union` are
arguments. D92 says the two readings of a breach may not replace one another,
and `rate` drops one. `wants_a_pivot` already filters with `breached` over the
complementary case, so the two halves of one partition would otherwise
disagree about what counts as breached. The gap `rate` leaves is reachable
through the public `jaos_set_dual_tolerance`, where D27's own `etamacro`
column — scaled 4.89e-8, column scale 1/32, published 1.56e-6 — reads as fine
to `rate` and as a violation to the checker's `CHECK_TOL`.

**The fourth reason is measured, on the build no reading of this change had
used.** `run-nopresolve.sh` takes `netlib` on `-DJAOS_NO_PRESOLVE` at three
arms, because the product differs from the union in two ways at once:
`product -> rate` isolates the fixed column, whose distance is exactly zero,
and `rate -> union` isolates the published space. `np-rate` comes back
**byte-identical to `np-base`**, which settles the fixed-column question — the
flip a zero distance used to forbid is reached on no instance of that set, on
the build where fixed structural columns exist. `np-union` moves exactly one
instance, `pilotnov`: 3541 iterations to 4182 and 1.2517x the work, for `row`
2.61e-07 to 5.17e-09, `rowrel` 4.12e-11 to 1.28e-12, `Q` 2.22e-08 to 2.13e-10
and the suboptimality bound `rsub` 4.94e-12 to **4.74e-14**. Its objective
lands one ulp off the reference where the product landed on it exactly. The
runner says the same from the other side: against the presolve baseline
`np-base` regresses on the suboptimality bound, 45.5x, and `np-union` regresses
on work instead. **The column the review said would lose its repair exists, it
is on `pilotnov`, and repairing it buys two orders of magnitude of certificate
for 25% more work on one instance of a build JAOS does not ship.**

**What is left open**, handed to `TODO.md`. Nothing in this item; all three of
the review's findings are answered above. Two record repairs came with it.
`docs/tolerances.md`'s `DUAL_TOL` row named a reader called `points_outwards`
that does not exist in `src/` — the function is `held_by_an_invented_bound`,
whose own comment carries the phrase the name was made from — and the row also
said the constant had one site reading it in the wrong units, which is no
longer true. Both are corrected. The same name survives in this file's older
entries, which are history and stay as written. `bench/measurements/02-84/`'s
`DUAL_TOL` sweep was taken against the product; the two versions separate only
as the constant loosens, so that sweep describes the code at 1e-9 and not the
shape of the curve away from it.

`bench/measurements/02-129/`.

## D215 — D211's refusal expired at D212, and the script that proves it measured the main tree from inside three separate worktrees

`make refusals` reported `D211 FLIPPED` on 2026-08-28, the first time it ran
since D212 landed. **The question is which commit expired the refusal**, and
the answer had to be a commit rather than "somewhere in the last seven",
because a reopened item nobody can attribute is a reopened item nobody trusts.

D211 refused a stop rule on the phase-1 objective rising. In exact arithmetic
a sum of bound violations never rises under a correct pivot; in floating point
it does, because `xb` is recomputed from the factorization every 64 updates
and the recomputation differs from the carried values by rounding. `pilot-ja`
rose **25.0449** above its running minimum at iteration 2091 and finished
`ok`, so any threshold worth a constant would have killed a solve that was
going to finish.

**The measurement.** `relrise.sh` at five refs, its own exit code as the
verdict.

| ref | what landed there | largest rise on a solve that ends `ok` | verdict |
|---|---|---|---|
| `e2daf9c` | D211 itself | **25.0449** | HOLDS |
| `da16a20` | **D212, Harris in primal form** | **2.59079e-10** | **REOPEN** |
| `e5bfe3d` | D213, the Harris width | 9.36752e-10 | REOPEN |
| `2ee580f` | a bench record | 9.36752e-10 | REOPEN |
| `3221397` | D214, `can_move`'s units | 9.36752e-10 | REOPEN |

**D212 is the commit.** `pilot-ja`'s rise goes from 25.0449 to 3.3348e-12
there and never moves again, and `wood1p`'s phase 1 goes from 3830 iterations
to 251 in the same step, which is D212's own headline. A two-pass ratio test
prefers a larger pivot, and a larger pivot is what keeps the recomputation
close to the carried values. D213 moves the figure by a factor of four and no
verdict, which agrees with D213's own finding that the width chooses nothing
inside its plateau. D214 moves it not at all.

**What is refuted: the first attribution, which was this decision's own.** The
first pass created three worktrees, ran `relrise.sh` in each, and returned
**9.36752e-10 at all three refs including the one where the refusal holds**.
`relrise.sh:30` was `root=/mnt/c/Users/vall-/Desktop/projectes/jaos`, an
absolute path, and the script `cd`s there before reading `HEAD`. Run from a
worktree it measured the main tree and reported the main tree's ref. Three
trees, one binary, one number — D82's failure, and the output read as a
finished attribution.

Nothing in the run said so. What said so was the arithmetic being too clean:
three genuinely different trees cannot agree to six significant figures.
`02-130/run-attribute.sh` gives each ref its own root by rewriting that line
into the worktree's copy, and **ends with a canary that fails loudly when the
readings are all equal**. Both passes are in `run-attribute.txt`, the wrong one
first, because a corrected number with the wrong one deleted teaches nobody
what to look for.

`relrise.sh` derives `root` from its own location now, which is what the rest
of the directory does and what leaves `make refusals` behaving identically.
**28 other scripts under `bench/measurements/` still carry the absolute
path**, one of them a refusal re-test (`02-125/unbounded-census.sh`). They are
harmless where they are run from the repository root, which is the only way
`make refusals` runs them, and they are a trap for the next attribution. That
is a debt in `TODO.md`, not fixed here: a script whose record nobody is
re-running should not be edited on the way past.

**`pilot87` is not fixed by any of this.** Its rise is 8.07e+11 at `3221397`
against 8.43e+13 at `e2daf9c`, still a divergence. What changed is that the one
instance standing between the rise and a usable threshold no longer stands
there, and the window is about nine orders of magnitude wide.

**What is left open**, handed to `TODO.md`. The stop rule itself. A threshold
is a constant and needs a sweep on both sides; nothing here sweeps one, and
D211's *other* half — that the divergence is a ratio-test problem and stage 2
is the preference it lacked — stays closed and is what D212 acted on.

`bench/measurements/02-130/`.

## D216 — Eight of `lu.c`'s prose contracts are asserts now, and a control proves they catch the defect they exist for

`TODO.md`'s assert debt: the 2026-08-26 comment purge kept a set of sentences
because other code depends on them, and D30's rule with D201's receipt says
such an invariant belongs in an assert rather than a comment. **The question
was whether adding them is worth anything**, because an assert nothing ever
exercises is a comment with punctuation.

Eight went into `lu.c`: `grow_pair`'s capacity covering what was asked
(`jm_svec_push` writes index `n` in both arrays after testing `n < cap`);
`mult_set` all false at the top of every pivot step; `keep <= k` in the
one-walk column update; both renumber maps total; `stamp > 0` after the
increment in `btran_u_pattern`; `top >= 0` at its return; and every
off-diagonal spike entry above the diagonal after `jm_lu_update`'s cyclic
permutation.

**The measurement.** `-DNDEBUG` is set by the release build, so `make configs`
and `make sanitize` run these only over the unit suite's small matrices, and
the gate — the only thing that factors a real basis tens of thousands of times
— never runs them at all. `EXTRA_CFLAGS` is appended last to
`RELEASE_CFLAGS`, so `make netlib EXTRA_CFLAGS=-UNDEBUG` is the shipping
configuration with the asserts live.

| arm | assertion failures | instances |
|---|---|---|
| the eight asserts | **0** | 94 solved, 94 checker ok, `gate: PASS` |
| the same, with the `mult_set` clear loop deleted | **85** | 9 solved, 85 failed |

Every control failure is one line, `src/lu.c:490: jm_lu_factor: Assertion
'!e.mult_set[i]' failed`. Both arms reported the same two numbers on a second
run. Under `-DNDEBUG` the three gate sets are byte-identical and all five
configurations build and pass, which is the other half: the asserts cost the
shipping build nothing.

**What was refuted, twice, and the second one is this entry's own script.**
The `compact_pivot_row` check the debt list suggested — every kept entry
carries a nonzero value — restates the `if (aij == 0.0) continue;` three lines
above it and can only fail if the compiler is wrong. It is not added. `keep <=
k` is locally provable too and *is* added, because it guards a memory hazard
rather than restating a branch: the loop writes `cv->idx[keep]` while reading
`cv->idx[k]` in the same array, so it is a change detector for a second
increment somebody adds later.

The script's first run printed **`STOP: the control did not fire, so the
step-top assert checks nothing`** directly beneath its own `control asserts
fired: 85`. `grep -c` prints `0` and exits 1 when it matches nothing, so
`$(grep -c ... || echo 0)` yielded two zeros on one line and every numeric
test after it fell through to the last branch. The readings were correct
throughout. It is recorded because it is D45's lesson from the other side: the
summary line was wrong about data printed four lines above it, and nothing but
reading the numbers would have caught it.

**What is left open**, handed to `TODO.md`. `lu.c`'s remaining debt is the
four items that need tests rather than asserts — the `grow_pair` fault build,
`find_pivot` on a column whose live count reaches zero, the `piv_n == 0` path
against the general one, and a failed update leaving ftran and btran
non-writing — plus `btran_u_pattern`'s dependency-order check and
`compact_pivot_row`'s duplicate-column half, which needs a stamp array the
function does not own. `model.c`, `check.c`, `jaos_internal.h`, `simplex.c`
and `presolve.c` are untouched.

`bench/measurements/02-131/`.

## D217 — Every measurement script derives the repository root, it was 48 scripts and not 28, and the check that proves it said STOP twice for the right reason

D215 found `02-126/relrise.sh` hardcoding the repository root and `cd`-ing
there before reading `HEAD`, so a script run from a worktree measured the
**main tree** and a three-ref attribution returned one number three times. It
fixed that one script and recorded the rest as a debt. **The question here is
whether the debt can be closed mechanically**, and what a mechanical sweep of
48 scripts has to prove before it is believed.

**D215's count was wrong, and the reason is worth keeping.** It came from one
grep, `^root=`, and the literal is written three ways: `root=<literal>`,
`REPO=`/`MAIN=<literal>`, and a bare `cd <literal> || exit 2`. 28 was the count
of the first form. The real number is **48**. A count from one pattern is a
count of that pattern, not of the defect.

**The change.** One line per script, above any `cd` and after any `set -u`:

    JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

with every occurrence of the literal replaced by `"$JAOS_ROOT"`. The depth is
computed per file rather than assumed; all 48 sit at
`bench/measurements/<id>/`.

**What was checked** (`bench/measurements/02-132/run-root-check.sh`): no `.sh`
still carries the literal (0), every script parses (48 declare `JAOS_ROOT`, 0
syntax errors), and the same script reads the main tree from the main tree and
the worktree from a worktree, at three different working directories. `make
refusals` exits 0 and runs one of the 48 — D210's census — for the same HOLDS;
`make test` and `record-check` pass.

**The failure that would have been silent, and was checked for before anything
ran.** A `<<'PY'` heredoc does not expand variables. A replacement landing
inside one would leave a literal `$JAOS_ROOT` inside a Python program, and the
script would fail later at a path that does not exist rather than at the edit.
0 occurrences.

**What was refuted: the check itself, twice.** Its worktree comes from `HEAD`,
the fix was uncommitted, and every probe read an empty line from a script that
did not declare `JAOS_ROOT` yet — so the verdict said STOP. That is the correct
answer to the question as asked, and it is the second time in two days that a
verdict line has been the thing under test. It now copies the working tree's
scripts into the worktree, because a check meant to run before a commit cannot
only ever read the previous one.

**What is left open**, handed to `TODO.md`. Nothing of this debt. None of the
48 is re-run: the change is mechanical and the only thing it can alter is one
path, which is checked directly. `02-21/excavate.sh` still hardcodes a
scratchpad belonging to a session that ended, which is a dead path and not the
repository root.

`bench/measurements/02-132/`.
