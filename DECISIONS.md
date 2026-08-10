# Design decisions

Closed decisions only, with the measurement that closed them. What is
still open lives in `PLAN.md`; what a feature is lives in `SPECS.md`.

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
