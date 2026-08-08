# Design decisions

Closed decisions only, with the reasoning that closed them. Open questions do not
live here — they live in the working session until they close.

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
