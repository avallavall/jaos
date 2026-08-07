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
