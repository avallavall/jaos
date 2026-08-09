# Plan

Working document per the working agreement: it holds only what is open. Detail
exists only for the active milestone; later stages stay coarse until they open.
Closed items leave for the changelog and the commit that closed them. Constraints
referenced as D<n> live in `DECISIONS.md`.

---

## 1. Destination and staging

The declared scope (D6) staged into milestones. Each has a single gate; a milestone
is closed by its gate, never by the calendar. This refines D6's rough order with
two insertions — deterministic parallelism and interior point — placed where their
prerequisites exist.

| Stage | Delivers | Gate |
|---|---|---|
| **M1 — LP correct** | MPS+LP readers, scaling, revised dual simplex, checker | Netlib set solved to reference values; see §2.9 |
| **M2 — LP fast** | Presolve, hyper-sparsity [9], pricing refinements, and the deferred data-structure work of §2.11. **Open — detail in §3**, where the measured attribution has already reordered this list and ruled a crash basis [12] out for now | Measured competitive gap vs open solvers on the measurement host; **blocked by Q4** |
| **M3 — MILP correct** | Branch & bound, reliability/pseudocost branching [14], warm-started dual simplex | MIPLIB 2017 easy subset: correct optima, correct infeasibility claims |
| **M4 — MILP strong** | Cuts (Gomory [15], MIR [16], covers), MIP presolve [14], primal heuristics (FP [17], diving, RINS [18]) per D9 | MIPLIB benchmark subset coverage targets, fixed when M4 opens |
| **M5 — Parallel deterministic** | `jaos_thread.h` (D13), deterministic parallel B&B per D8, opportunistic opt-in | Bit-identical parallel runs; measured speedup on measurement host |
| **M6 — Interior point** | Barrier for LP, crossover, primal simplex | Large-LP set solved; clean basic solutions from crossover |
| **M7 — Quadratic & conic** | Convex QP/MIQP, then QCQP/SOCP/MISOCP over the barrier machinery | Fixed when M7 opens |
| **M8 — NLP** | Smooth constrained NLP; derivative-strategy decision is this stage's opening gate | CUTEst subset |
| **M9 — MINLP** | Convex (outer approximation), then global non-convex (spatial B&B) | MINLPLib subsets |

Network specializations (min-cost flow, transport, assignment) slot in after M4 as
detection plus dedicated algorithms; they accelerate, they do not gate. SDP remains
unscheduled (D6).

---

## 2. Milestone 1 — LP correctness

**One sentence:** read a real instance from disk, solve it with a revised dual
simplex, and prove the answer right — on the whole Netlib set, deterministically,
with the work counter running from the first kernel.

### 2.1 In / out

**In:** fixed and free MPS reader; LP-format reader (documented dialect subset);
CSC model core with CSR mirror; Curtis–Reid scaling [11]; sparse LU with Markowitz
pivoting [4][6]; Forrest–Tomlin updates [5] over Bartels–Golub LU [20]; dual
steepest-edge pricing [8]; Harris two-pass ratio test with bound flipping
[7][19][1]; dual phase 1 [21]; ranged rows and all bound types; infeasibility and
unboundedness classification; independent solution checker; deterministic work
counter (D16); Unity test suite (D15); Makefile (D14); sanitizer runs; determinism
harness; Netlib acceptance runner. Plus, added deliberately after the gate
demanded it: a **primal ratio test** used only to clean up after the dual
solve has finished (D28).

**Out, explicitly:** presolve (no instance forced one, D31); **the primal simplex** — see below, the ratio test above is not
it; crash basis; hyper-sparsity; parallelism; cuts; callbacks; bindings; file
*writing*; certificate export; public CLI. The Netlib driver is a bench tool,
not a product.

**Where that line now runs, because it moved once and should not move by
drift.** What is in is a ratio test and the basis change `pivot()` already
performs, applied to a column the residue names. What stays out is everything
that makes a primal *method*: pricing to choose an entering column, a phase 1
of its own, its own steepest-edge weights, and any use of it to solve rather
than to finish. A primal simplex chooses what enters; this is told. D28
carries what it bought and what it cost.

### 2.2 Repository layout

```
include/jaos.h        public header, the only one
src/                  library sources, flat until a second engine exists
tests/                unit tests; tests/vendor/unity/ (D15); tests/data/ tiny golden instances
bench/                netlib fetch script + sha256 manifest, acceptance runner, results/
docs/                 format-support.md, scaling.md, tolerances.md, work-units.md
Makefile
```

### 2.3 Toolchain

`CC ?= gcc-14`, `-std=c23` (D1). Dev builds `-Wall -Wextra -Wpedantic -Werror -g`;
release `-O2 -g -DNDEBUG` — aggressive flags (`-O3`, `-march=`) are an M2 matter,
because under D17 they require measurements to justify. `sanitize` target builds
with `-fsanitize=address,undefined`.

### 2.4 Public API conventions

**Closed — D33.** These were held here as a shape awaiting the maintainer's
confirmation. It came, and the conventions, the two places the draft was
wrong, and what is deliberately *not* frozen by any of it now live in
`DECISIONS.md`. Nothing about the API is open.

### 2.5 Components and their literature anchors

1. **Sparse core.** CSC storage plus a CSR mirror of A — the dual simplex prices
   rows, the column view feeds FTRAN. Own locale-independent number parsing in the
   readers: `strtod` under a comma-decimal locale silently corrupts instances, and
   determinism (D8) extends to parsing.
2. **Readers.** Fixed MPS with free-MPS autodetection; RANGES with per-row-type
   semantics, all BOUNDS types, OBJSENSE, objective constants. Unsupported
   constructs (SOS, indicators) are recognized and rejected with a clear message
   and line number — never silently skipped. LP dialect: documented subset,
   CPLEX-style core (objective, constraints, bounds, integrality sections parsed
   and rejected until M3).
3. **Scaling.** Curtis–Reid [11] as default, geometric-mean equilibration as
   option. Solver tolerances live in scaled space; the checker judges in original
   space.
4. **Basis factorization.** LU with Markowitz threshold pivoting [4][6][20],
   sparsity-exploiting triangular solves [4]. Hyper-sparsity techniques [9] are M2.
5. **Basis updates.** Forrest–Tomlin [5]; refactorization on an interval target
   plus stability triggers (FTRAN/BTRAN residual checks). What those checks
   turned out to be worth, and where a residual is worth acting on rather than
   only measuring, is settled in §2.8 by `pilot` (D20, D29).
6. **Pricing.** Dual steepest edge [8] from the start — it is the workhorse that
   makes dual simplex competitive [1] — with a plain max-infeasibility fallback
   behind a flag for debugging.
7. **Ratio test.** Harris two-pass [7] with bound-flipping [19][1], long-step
   variants per Koberstein's treatment [1].
8. **Dual phase 1.** Required for free variables and general starts. Method chosen
   during implementation from the comparison in [21][1] — open question Q1, closed
   by evidence, not preference.
9. **Degeneracy and stability.** Harris tolerances; deterministic bound
   perturbation (seeded, removed and cleaned up at end); DSE weight resets on
   drift.
10. **Classification.** Primal infeasibility proven via dual unboundedness; tiny
    constructed tests for `UNBOUNDED`. Certificate export is M2.
11. **Independent checker.** Reads the *original, unscaled* problem and the claimed
    solution, and verifies status independently of the solver's own bookkeeping:
    activities within row bounds, values within column bounds, dual feasibility
    signs, complementary slackness, primal-dual objective agreement — formulas
    published in `docs/tolerances.md`. Runs after every solve in the test suite.
    This is "check the thing, not the wrapper" as a module.
12. **Work counter (D16).** Counted in the kernels from the day the kernels exist,
    never bolted on. Draft weights, §2.7.
13. **Determinism harness (D8).** Every instance solved twice in-process and once
    across runs; status, objective bits, iteration count, work units and a basis
    hash must match exactly.

### 2.6 Tolerances — draft

**Frozen (D31).** These were drafts on the explicit terms that they would
freeze when the Netlib gate closed; it has, and they freeze at the values
below. Any later change is a changelog entry. Every one of them, where it
acts and what it decides, plus the checker's formulas: `docs/tolerances.md`.

Not one of them was moved to close an instance. Eight instances were refused
along the way and all eight turned out to be defects with a mechanism, which
is the only thing that makes freezing these worth anything.

| Quantity | Draft |
|---|---|
| Primal feasibility (scaled) | 1e-7 |
| Dual feasibility (scaled) | 1e-7 |
| Markowitz threshold u | 0.1 |
| Factor drop tolerance | 1e-14 |
| Harris tolerance window | 1e-7 |
| Steepest-edge weight drift factor | 10 |
| Refactorization interval target | 64–128 iterations, stability-triggered earlier |
| Netlib objective acceptance | \|obj − ref\| ≤ 1e-6 · max(1, \|ref\|) |
| Checker tolerances (original space) | 1e-6 on residuals; the bound-proximity test scales with what the value is made of (D23). Formulas in `docs/tolerances.md` |

### 2.7 Work units — draft weights (D16)

Deterministic integer counting in kernels only. Model loading is excluded from the
solve budget and documented as such. Where each weight is actually charged, and
what else sits outside the budget, is in `docs/work-units.md` — including the
scaling computation, which a solve performs and no unit counts today.

| Event | Draft weight |
|---|---|
| Nonzero touched in a solve, in pricing, or in an update | 1 |
| Nonzero eliminated, in a factorization or in a basis update | 2 |
| Fixed overhead per basis update | 64 |
| Fixed overhead per refactorization | 4096 |

An elimination is charged by what it does, not by which routine runs it: the
same axpy costs the same inside a factorization and inside a basis update.

**The per-iteration overhead had no number and now has none on purpose: it
is zero, and the row is gone (D32).** The attribution it was waiting for was
made — every charge assigned to the phase of the iteration that made it, on
an instrument that leaves all 110 instances bit-identical — and it says the
basis update is 1.8% of an iteration rather than the whole of it. So the
double-billing the note feared was not the reason. The reason is that the
other 98% is already charged, and charged by dimension: the bookkeeping is
exactly `iters * (nvar + 2*nrow)` in 110 of 110 solves. There is no O(1)
residue for a constant to stand for.

Ratios calibrated before 1.0; frozen at 1.0; afterwards changes only at a major
version (D16).

### 2.8 What is left inside M1

Built and closed, recorded in the changelog and its commits: scaffold, model
core with the independent checker, MPS and LP readers, sparse LU with
Forrest-Tomlin updates and the work counter in its kernels, and a dual
simplex that solves bounded LPs on a Curtis-Reid scaled copy, with dual
steepest-edge pricing, a Harris two-pass ratio test with bound flipping,
and a dual phase 1 by artificial bounds. `make test` covers all of it, and
every solved test instance is put through the checker.

**Nothing here is remaining any more — all five items below are met, and the
gate they add up to is met.** They are kept for one more revision because the
shape of what closed them is what §2.9's next milestone will be judged
against, and because two of them record a lesson rather than a result:

1. **Netlib standard set — met.** All 94 instances solve to `OPTIMAL`,
   every objective is within the §2.6 tolerance, the independent checker
   accepts every one, and every one is deterministic across two solves and
   across runs. `make netlib` reports `gate: PASS`.

   The readers were the part that came out clean on the very first run: every
   instance loads with exactly the row and column counts two independent
   canonical sources agree on.

   Eight instances were refused at some point along the way and **not one of
   them was closed by widening a tolerance** — `pilot-ja` (D21), `finnis`
   (D23), `nesm` (D25), `grow15` (D26), `etamacro` (D27), `greenbea` (D28),
   `pilot` (D29) and `pilot87` (D30). Each is a distinct defect with a
   mechanism, and the two that were most readily explained as the limit of
   double precision — `etamacro`, which defeats CPLEX at defaults and SoPlex
   at 1e-6, and `pilot87`, the worst-conditioned model in the set — were both
   defects.

   `DECISIONS.md` carries what each one turned out to be.
   `docs/research/netlib-campaign.md` carries the measurement record: what was
   measured, in what order, and the readings that were wrong on the way,
   because each wrong reading is what pointed at the next.

2. **The infeasible set — met.** §2.9 asks for three instance sets and only
   the standard one existed; the other two were pinned on 2026-08-07 after Q6
   was decided. The first run refused 28 of 29; `gran` is now closed and the
   set reports:

   ```
   29 instances: 29 correctly refused, 29 shape ok, 29 deterministic, 0 failed
   gate: PASS
   ```

   **No false optima.** That was the risk this set exists to measure — a
   model with no feasible point coming back with an answer — and it does not
   happen anywhere in the 29. It is also not a hypothetical risk: the revert
   of 2026-08-07 was forced by its mirror image, `pilot-we` being reported
   INFEASIBLE when it is feasible, and nothing in the suite caught it.

   **`gran`, and what it was.** It gave up after 1728 iterations with a
   numerical error and no message. The cause was not phase 1 and not the
   stall of Q10: its basis went singular, and the solve treated that as the
   end. It is not. `jaos_internal.h` has said since the factorization was
   written that rank deficiency is a fact the caller acts on by replacing
   basis columns — the caller simply never did. The repair pairs the rows
   the factorization could not cover with the basis positions it could not
   use, and puts the logical of one into the other; the result is triangular
   by construction, so it cannot fail for the same reason twice. `gran`
   reaches INFEASIBLE in 2058 iterations, and no instance of the standard set
   moves at all, which is what should happen: the repair only runs where a
   solve previously stopped.

   A singular basis is not something a model can cause. Every basis the dual
   simplex assembles is nonsingular in exact arithmetic — it never pivots on
   an alpha below `PIVOT_MIN` — so one that will not factor is always carried
   error, and 2658 rows is the scale at which that finally accumulated. No
   unit test can produce one, which is why `tests/test_simplex.c` covers the
   family instead: rank-deficient constraint matrices, where the danger is a
   wrong verdict rather than a crash.

   The shape check earned its place immediately, and against the manifest
   rather than the reader: `greenbea` here carries 111 free rows beyond its
   objective row, JAOS loads all of them, and the first count written into
   the manifest had excluded every `N` row wholesale. The gate flagged the
   mismatch on the first run. Note also that this `greenbea` is a different
   model from the standard set's — the original infeasible version, from
   which the feasible one was repaired — which is why the sets are fetched
   into separate directories.

3. **Kennington — met.** 16 instances, much larger than anything JAOS had
   loaded, and all sixteen pass every condition on the first run:

   ```
   16 instances: 16 solved, 16 shape ok, 16 objective ok, 16 checker ok,
                 16 deterministic, 0 failed        gate: PASS
   ```

   `ken-18` is 105127×154699 and takes 113652 iterations; `osa-60` carries
   232966 columns; `pds-20` costs 3.1e10 work units. An order of magnitude
   past the standard set in every direction, with the independent checker
   green throughout. That answered something while seven instances of 1a were
   still open: whatever ailed them, it was not that the readers or the
   factorization stop working at scale.

   Reference optima come from that directory's readme, computed with
   Vanderbei's ALPO and carrying eight significant digits — enough against a
   1e-6 relative tolerance, but not Koch's exact rationals, and marked
   `netlib` in the manifest for that reason. An instance missing by less
   than 1e-8 relative could not be judged against them.

4. **Reader robustness — met.** §2.9.4 asks that truncated and corrupted
   input produce errors rather than crashes. `tests/data/` covers malformed
   *content*, one file per rejection class, and every one of those files is
   well-formed enough to reach the check that rejects it: nothing was cut
   mid-record, no byte was flipped inside a number, nothing was empty and
   nothing was random. That was a fuzz-shaped gap, and `tests/test_fuzz.c`
   is the fuzzer. Every prefix of every corpus file, small seeded edits,
   uniform noise, random sequences of real keywords, and named shapes at the
   sizes where a buffer decision changes — each offered to both readers,
   since an LP file handed to the MPS reader is corrupted input by any
   definition.

   11543 cases run in the suite and 1623443 at `JAOS_FUZZ_SCALE=200`, all
   clean under ASan+UBSan, which is where the claim actually lives: without
   the sanitizer this file proves only that the readers do not segfault.

   A fuzzer that finds nothing is not evidence until it is shown able to
   find something, so the instrument was checked the way §2.8's own lesson
   demands. `split()` in the MPS reader had `n == MAXTOK` changed to
   `n > MAXTOK`, a one-token stack overflow, and the fuzzer caught it at
   `src/mps.c:46` — in the random-edit class rather than in any case written
   by hand, which is the part that says the classes reach past the first
   rejection.

5. **`pilot87`'s reference — fixed, from the report's PostScript.** The
   manifest took it and `maros-r7` from the netlib readme because Koch was
   believed not to cover them; he does. The blocker was that his exact
   rationals were published at `zib.de/koch/perplex`, which no longer
   resolves, and that reading them off the report's PDF reproduced only 23 of
   the 92 values already known to be his — not a source to set ground truth
   from.

   The same report's **PostScript** is a different matter. It is dvips
   output and carries the whole table as literal strings, so the values come
   out with nothing but the typesetting undone: `Fc(\000)` is the minus sign,
   `Fa(:)` the decimal point, `Fq(n)` the exponent, and kerning splits both
   names and mantissas across strings. `bench/koch-refs.py` does that and
   `bench/koch-verify.py` checks the result against every reference pinned —
   **82 reproduced exactly, double for double, and none in disagreement**, of
   which eighty were pinned from Koch before any of this ran. That the same
   pass reproduces eighty independently transcribed values bit for bit is
   what makes the two it adds worth taking.

   ```
   pilot87    301.71072827  ->  301.7103473331105     relative 1.26e-6
   maros-r7   1497185.1665  ->  1497185.166479644     relative 1.36e-11
   ```

   The gate's tolerance is 1e-6 relative, so `pilot87` really was being
   judged against a reference outside tolerance of the exact optimum, and
   `maros-r7` never was. Every line of the manifest is now sourced `koch`.
   No verdict moves — 0 regressed, 0 improved on the standard set —
   which is the expected outcome and not a disappointment: `pilot87` misses
   its objective by fifteen tolerances against either number. What changes is
   that the gate is now honest about what it is measuring against.

Degeneracy handling is done as far as it can be done without evidence:
steepest-edge weights repair and restart themselves, and what the ratio test
spends in dual feasibility is now lent and called back rather than left
lying. What is left of it is an anti-stall perturbation, which addresses a
problem no instance has shown yet, and Q10 holds it until one does.

Of §2.5.5's stability triggers, the one that ends a solve exists: a
declaration of optimality is re-priced from a fresh factorization before it
is accepted, because the values it was read off are carried and drift (D20),
and that refresh now refines its two solves as well (D29).

**The other one — a residual watched during the solve that refactorizes
early — is answered rather than built, and the answer is that it was aimed
at the wrong cause.** `pilot` is the instance that asked for it, and the
residual it was rejected on is measured against a factorization that is
already fresh, so no rule about *when* to rebuild could have reached it. What
reaches it is refining the solve. And refining every solve rather than the
last one was measured: `pilot-ja`, a model with a known finite optimum, comes
back INFEASIBLE, and `pilot87` pays 4.5x the work. Mid-solve those two
vectors choose a pivot; at the end they are the answer, and only the second
is worth spending accuracy on.

§2.5.6's debugging fallback to max-infeasibility pricing is not built. There
is nowhere to put the flag — the library has no options API and inventing one
for a debugging aid is the wrong order — and the rule itself is one line: it
is what steepest edge becomes when every weight is pinned at one. It waits for
the first option the library needs for a reason of its own.

### 2.9 Acceptance gate for M1

All of the following, no exceptions. The right-hand column is where each one
actually stands as of 2026-08-07, because a gate whose status is only known
in aggregate is what §2.8 has just finished being a lesson about.

| # | Condition | Status |
|---|---|---|
| 1a | Netlib **standard** set: `OPTIMAL`, objective within §2.6 tolerance, checker green | **met** — 94/94 solved, 94 objective, 94 checker, `gate: PASS` |
| 1b | **Kennington** subset, for correctness with no performance expectation | **met** — 16/16, every condition, `ken-18` at 105127x154699 included |
| 1c | **Infeasible** subset: classified `INFEASIBLE`, no false optima | **met** — 29/29 refused, no false optima; `gran` closed by the basis repair (§2.8.2) |
| 2 | Determinism harness green on every instance (D8) | **met** — 94/94 on the standard set, 16/16 Kennington, 29/29 infeasible. Same-machine by construction; the one cross-machine mechanism anyone identified is bounded by measurement (D34) |
| 3 | Full suite clean under ASan+UBSan | **met** |
| 4 | Reader robustness: truncated/corrupted input errors, never crashes | **met** — 1.6M fuzz cases clean under ASan+UBSan, on an instrument checked against an injected fault (§2.8.4) |
| 5 | Results recorded under `bench/results/` as data, no wall-clock (D17) | **met**, and all three sets now diffed per instance against a baseline (D21) |

Two of these seven were being read as one. "The gate" meant condition 1a in
every conversation until late, and 1b and 1c had no infrastructure behind
them at all — which is worth recording, because the distance to M1 was never
the distance to closing seven instances.

**All seven now hold, and `make netlib` reports `gate: PASS`.** Every
instance of the standard 94 solves, is deterministic, is within objective
tolerance and is accepted by the independent checker.

The last two closed within a day of each other and neither was a tolerance:

- **`pilot` (D29) was a residual of the basis solve.** It had been rejected
  on one row lying `1.73e-6` outside its bound and on nothing else, with its
  objective inside tolerance, its dual violation exactly `0` and its gap
  `6.6e-14`. That looked like the *primal* residue D24 is about, and
  measuring it said it was not: no basic variable is outside its bound in the
  solver's own arithmetic, and the `1.73e-6` is the disagreement between the
  solver's carried row activity and the checker's recomputation of it. The
  residual of `x_B = -B^-1 (N x_N)` at the accepted point is `7.06e-6` in the
  space the checker reads; one step of iterative refinement leaves `9.09e-13`,
  and the row goes to `6.73e-13`.
- **`pilot87` (D30) was a clean-up loop that could take one pivot per call.**
  Its objective was `2.28e-3` out against a tolerance of `3.02e-4` and its
  dual violation `1.87e-5`, and it was read for a long time as the
  worst-conditioned model in the set having simply run out of precision.
  It had not. `primal_cleanup` asks `wants_a_pivot`, which reads the duals
  out of `rho`; its own first pivot overwrites `rho` with a pricing row, so
  from the second candidate onwards the question was being asked of the wrong
  vector — 12 candidates on entry, one pivot, zero on exit, every round.
  Underneath that, `pivot()` runs `shift_to_feasible` over every variable, so
  the first pivot *lends away* the other candidates' sign conditions rather
  than repairing them. Judging each candidate before any of them moves, and
  calling in its own loan first, takes the objective to `1.33e-7` relative
  and the dual violation to `0` — for less work than before.

§2.8.1 records what each of the closures was, measured rather than grouped by
the size of the number reported — because the number the checker reports is
the magnitude of a multiplier and says nothing on its own about how far
anything is from where it should be. That mistake cost `finnis` months in the
wrong group; it was the most accurate answer of the seven and was closed by
D23.

Of the seven instances the checker once rejected, **all seven have closed and
not one of them by moving a tolerance**: `pilot-ja` was a contribution the
checker was dropping (D21), `finnis` a bound-proximity test judged absolutely
on a row that cancels ten orders of magnitude (D23), `nesm` a settled basis
the method had never been handed back (D25), `etamacro` a repair test reading
the wrong quantity in the wrong space (D27), `greenbea` a column with nowhere
to rest, which needed a basis change rather than a move (D28), `pilot` a
residual of the basis solve (D29) and `pilot87` a clean-up loop taking one
pivot where twelve were waiting (D30). `grow15`, which was a different kind
of failure, was a cycle read as a stall (D26).

That is the strongest thing §2.6 has going for it: **every failure anyone was
tempted to blame on a number turned out to be something else.** Not one of
the eight was closed by widening a tolerance, and the two that were most
readily explained as the limit of double precision — `etamacro`, which
defeats CPLEX at defaults and SoPlex at 1e-6, and `pilot87`, the
worst-conditioned model in the set — were both defects with a mechanism.

### What happens next

**The gate is met, and the bookkeeping that closing it triggered is done.**
All seven conditions of §2.9 hold on all three instance sets. The four items
that remained were none of them a solver change, and all four have closed:

1. **The §2.6 tolerances are frozen (D31).** They were drafts throughout, on
   the terms that they would freeze when the Netlib gate closed. Any change
   to one of them is now a changelog entry.
2. **§2.7's per-iteration work weight is settled at zero, and its row is
   gone (D32).** The attribution it was waiting for was made: the basis
   update turns out to be 1.8% of an iteration rather than the whole of it,
   and the rest is already charged by dimension, so there is no O(1) residue
   for a constant to represent.
3. **The questions the campaign was going to decide are closed (D31).** Q1
   (dual phase 1 by artificial bounds survived it), Q3 (no instance forced a
   presolve into M1), Q9 (no instance was refused for reaching the lent
   bound), Q10's perturbation half (no instance needs it — `grow15` was a
   cycle, and D26 cures it without perturbing anything). Each closed on the
   campaign's evidence or on the absence of a model that demanded it, and
   the distinction matters: an unused device is not a validated one.
4. **§2.4's API shape is a decision record (D33).** Confirmed by the
   maintainer, and two places where the draft was wrong were corrected
   rather than recorded: the no-internal-pointers rule was narrowed to what
   is actually true of it, and the basis statuses §2.4 promised were built
   as `jaos_basis` instead of deferred.

**Then M2 opens, and its first item is not code.** Everything M2 delivers —
presolve, hyper-sparsity, a crash basis, pricing refinements, and the
deferred data-structure work of §2.11 — is a change that only measurement can
justify, and the deterministic work counter does not see the largest part of
what those changes buy: optimisation level, memory layout and cache behaviour
do not move it at all. So **Q4, the measurement host, blocks the milestone**
rather than accompanying it.

### 2.10 Instance acquisition and reference values

**Acquisition.** Instances are fetched by a `bench/` script and pinned by a sha256
manifest committed to the repository; the instance files themselves never enter
the repo. The canonical Netlib source (`netlib.org/lp/data`) distributes the
standard set compressed in its historical "emps" form; plain-MPS mirrors exist but
are unofficial. Two routes, chosen when the manifest is first built (Q6): expand
the canonical files with netlib's own emps tool — a dev-time tool that enters only
with maintainer approval per D11/D15 — or pin an unofficial plain-MPS mirror by
checksum after a one-time comparison against canonical expansions.

**Set sizes.** Standard feasible set circa 90–95 instances, Kennington subset 16,
infeasible subset circa 29. Sources disagree on exact totals, so the built
manifest is the authoritative count; no exact number is claimed before it exists
(D17).

**Reference values.** Optimal objectives from Koch [22], which reports exact
rational optima for the Netlib problems (ZIB-Report 03-05, free PDF at
`opus4.kobv.de/opus4-zib/files/727/ZR-03-05.pdf`). The acceptance table in
`bench/` carries each instance's reference value with its source noted; any
instance not covered by [22] takes its reference from the Netlib readme and is
marked as such.

These references are load-bearing for a reason beyond convenience: they are
the only part of the gate that can catch a model the readers built wrongly.
The checker verifies a solution against the matrix JAOS stored, so a
reader bug leaves checker and solver agreeing about the wrong problem (D18).
An externally published optimum for a named instance is the one thing in the
milestone that does not come from JAOS.

Koch verified those values with exact rational arithmetic and, in doing so,
found previously published reference values that were wrong — which is also
the reason the gate cites him rather than the readme where they differ.

**Tolerance context.** The §2.6 drafts sit at HiGHS's documented defaults (1e-7
primal and dual feasibility); SoPlex defaults to 1e-6. Both verified against
their documentation and source respectively — ours match the stricter of the two.

**MILP, for the later gates (M3/M4).** MIPLIB 2017: a 240-instance benchmark set
within a 1065-instance collection (`miplib.zib.de`,
doi:10.1007/s12532-020-00194-3). Instances carry per-instance licences (CC BY-SA
4.0 as default per the MIPLIB 2017 paper) — fetched by script, never committed.

### 2.11 Deferred by measurement, not by oversight

Known costs, correct today, to be revisited in M2 with numbers rather than
opinions (D17). Recording them here keeps "we chose this" distinct from "we
missed this".

- **Slot detachment in a basis update is quadratic.** `jm_svec_erase` scans,
  and an update erases once per entry of the outgoing slot's row and column,
  so a slot carrying `f` nonzeros costs O(f²) to unhook — on the hottest
  path there is, with `f` growing until the next refactorization. A position
  map removes the inner scan.
- **Elimination storage is one growable array per column**, rather than a
  single arena with compaction. Simpler to get right; more allocator traffic
  and worse locality.
- **U is stored twice**, by row and by column, because updates need both
  orientations. Memory for time, deliberately.
- **The solves are dense in the working vector**, and after D35 they are
  where the work is: 45.8% of the standard set's units, 28.6% in `pivot`'s
  two FTRANs and 17.2% in the BTRAN. FTRAN already skips a zero; BTRAN
  cannot, because it resolves `U'` by dot products.

  **The cheap version of the fix was tried and rejected (D36).** Scattering
  over `urow` would skip 76.2% of the `U'` pass — 96.7% of its slots resolve
  to exactly zero — but it buys that by reordering the sum, and on vectors
  that cancel the two orders are not comparable. Two feasible models came
  back INFEASIBLE and total work rose by half.

  **The route that remains was the real one, and it is built (D38).** The
  BTRAN's U' pass now searches the factor's dependency graph for the slots
  that can produce a nonzero and solves only those — bit-identical, because
  the ones it skips are exactly zero. 1.04x to 1.10x less work across the
  three sets, no digest and no iteration count moved.

  Still open on the same path: the **L' pass** is untouched (only 4.1% of its
  entries sit under a zero, and L has no row-wise copy to search), the
  **FTRANs** skip zeros but still walk all `nrow` slots to find them, and the
  search itself scans all of `y` for its roots because callers know the
  support and do not pass it.
- ~~**Row-wise pricing reads the entries of basic columns.**~~ **Measured, and
  the filter is refused.** It is what makes the fourteen dense-`rho`
  instances of D35 up to 5% more expensive, so the question was whether to
  skip them. 36.1% of the 6.23e9 entries the pricing sweep reads over the
  standard set are in basic columns — a real ceiling — but the filter costs a
  `status[v]` read on *every* entry, from a second array. Per entry that is
  `0.64 x 5 + 0.36 x 2 = 3.92` memory accesses against 4 today: a wash, and
  with worse locality, since it pulls another array into the scatter's
  working set. Not worth a status test on the hottest loop there is.
- **Neither pricing form bills its own O(nvar) sweep.** The column-wise loop
  charged per matrix entry and per logical; the row-wise one charges the same
  way, so the two are comparable, but the clear of `alpha` and the reset of
  the basic entries are real work no unit counts. Worth fixing when the
  counter is next revised, and worth knowing before reading a pricing
  measurement as exact.
- **`col_max_abs` is recomputed per pivot search.** A column's largest
  magnitude only changes when the elimination rewrites that column, so it
  could be cached and refreshed at that one point. Left alone because a
  cache that is wrong is worse than a scan that is slow, and there is no
  measurement yet to say how often the same untouched column is
  re-examined.
- **A basis update clears its dense row buffer over the whole dimension**
  when triangularity says only positions at or after the outgoing slot can
  be read. The narrower clear is correct but leans on that invariant, and
  buys a constant factor on a path that has no benchmark behind it yet.
- **Finding the pivot row's entry inside a column is a scan.** Same missing
  row-to-position lookup as the erase above, on the factorization path.
- **A column's live count can overcount after cancellations.** The
  factorization's row patterns are append-only, and only the compacted
  pivot row hands its columns a decrement — a column whose entry in the
  pivot row cancelled earlier keeps the stale count. Markowitz then works
  from a pessimistic estimate: pivot choice quality, never correctness,
  and deterministic either way.
- ~~**Scaling's determinism across machines leans on libm.**~~ **Tested and
  closed (D34).** It was the one part of D8 resting on an argument rather
  than a measurement. Perturbing `log2` — which covers every library within
  the perturbation, where comparing two machines would only compare two
  libraries — moves not one scale factor of the 139 instances until the
  offset reaches somewhere between 2e-6 and 5e-6 in log2 units, roughly
  4x10^8 ulps. The closest any of 1,590,682 rounded exponents came to a tie
  was 4.29e-7. The probe was shown to fire at 1e-4 before its silence was
  taken for evidence.
- **The ratio test's long step re-scans for each breakpoint.** Bound
  flipping walks the candidates in ascending ratio order and finds each
  one with a linear scan of those still standing, rather than sorting the
  set once. That is the right trade when few candidates are passed and the
  wrong one when many are, and which of those a real model does is exactly
  what has not been measured.
- **Two sparse-accumulator idioms coexist** in the elimination — a
  touched-list scatter and a stamp-based dedup — because they arose for
  different jobs. When the simplex needs its own scatter for pricing, all
  three should become one shared mechanism rather than a third variant.

---

## 3. Milestone 2 — LP fast

**Open, and opened by work rather than by decision.** Seven entries have
landed against it (D35, D37, D38, D39, D40, D41, D42). Measured against the
commit where M1's gate first passed on all three sets, total work over the
139 reference instances has gone 293,987,935,333 -> 136,567,215,198, which
is **2.153x** — and **3.043x on Kennington**, where 73% of it was — without
a single verdict, objective, iteration count or solution digest changing
anywhere. The standard set is 1.090x over the same span, and that gap is
§3.2 in one number. What follows is the detail the working agreement asks
for once a milestone is active.

### 3.1 The gate, and what blocks it

M2's gate is a measured competitive gap against open solvers on the
measurement host. **Q4 blocks it and nothing else will unblock it.** Every
figure below is deterministic work units; the counter is blind to
optimisation level, memory layout and cache behaviour, which is where the
rest of the speed lives. No wall-clock claim belongs anywhere until that
host exists (D17).

What *can* be done without it is everything the counter sees: fewer nonzeros
touched, fewer eliminations, fewer refactorizations, fewer iterations.

### 3.2 Where the work actually is — and it is two different answers

Attributed with a `JAOS_DIAG` build that assigns every charge to the phase
that spent it, on the tree as of D38. **Read both columns before choosing a
target: they disagree almost completely, and Kennington carries 73% of all
work measured.**

The shares below predate D40, which removed most of the ratio test's row
from the Kennington column. The totals moved and the ranking did not, so it
is left as measured rather than rescaled by arithmetic; the next attribution
run replaces it.

| | standard 94 | **Kennington 16** |
|---|---|---|
| dual update + DSE weights | 6.41% | **44.96%** |
| ratio test | 8.79% | **31.23%** |
| the `rho'M` product | 11.22% | 11.99% |
| row scan for the infeasibility | 1.28% | 9.19% |
| the two FTRANs | 29.76% | 0.61% |
| refactorization + refreshes | 20.84% | 1.08% |
| BTRAN of the pricing row | 13.73% | 0.64% |
| basis update | 7.82% | 0.17% |
| **triangular solves together** | **43.48%** | **1.25%** |
| total units | 62,701,726,771 | 168,372,717,242 |

On mid-sized models the cost is the triangular solves and the
factorization. On the large ones it is two dense sweeps over every variable
per iteration, and the solves barely register. **Measuring on the standard
set and generalising is how this milestone's plan got its order wrong
twice.**

### 3.3 Making the consumers of `alpha` sparse — half done

**The target.** D35 made the `rho'M` product walk rows so a zero of `rho`
skips a matrix row, and everything downstream still walked all `nvar`
entries: `dual_ratio_test` scanning every variable to build its candidate
set, `pivot` scanning every variable for the dual update, the steepest-edge
update sweeping the rows. Together, 76% of the work on the large models —
exactly what [9] warns about, a sparse result feeding dense loops.

**The ratio test's half is built (D40).** `price_all` records where it wrote
for the price of a comparison against a value the `+=` already loaded,
`jm_pattern_order` makes that ascending and distinct through a bitmap, and
the ratio test walks it — as does the clear that starts the next iteration,
which removes a `memset` of `nvar` doubles per iteration that no unit
counted. 1.306x less work on Kennington, 1.014x on the standard set, 1.012x
on the infeasible one, every digest and every iteration count unmoved.

The obstacle this had to clear is worth keeping written down, because the
remaining halves have the same one. `price_all` fills `alpha` by walking
rows, so the pattern comes out unordered — and `bfrt_walk`,
`jm_harris_pick` and `apply_flips` all break ties, or order a
floating-point sum, by position in the candidate array. Ascending order is
what makes the change bit-identical rather than merely defensible.

**`pivot`'s dual update is built too (D41)**, and it is the larger half:
another 1.451x on Kennington, which takes that set to 1.895x cheaper than
when M2's pricing work began. What it needed beyond D40's machinery was an
invariant with a name — the loop repairs reduced costs as well as moving
them, so skipping a variable is safe only where the repair would have done
nothing — and `duals_dirty` is that name, armed by the two places that
write a reduced cost outside a pivot.

**Half of `pivot`'s `2 * nrow` charge is gone too (D42)**, and it was the
half that came free: the exact steepest-edge weight is the norm of `rho`,
and `price_all` already walks the whole of `rho` in ascending order, so
recording the pattern is a store per nonzero on a loop that was running
anyway. 1.208x on Kennington. The other half is item 1 below and needs real
machinery.

**What is left, in the order the measurements rank it.**

1. **The steepest-edge weight update, and with it the FTRAN.** It sweeps
   every row to touch **0.03%** of them on `ken-18` and 33% on the standard
   set. Its input is an FTRAN result, so unlike D42's half there is nothing
   already walking it: this needs the solve to hand over a pattern. That is
   hyper-sparsity proper [9] — the Gilbert–Peierls reachability search D38
   built for BTRAN's `U'` pass, applied to the forward direction, where it
   is structurally *easier* because both L and U are stored by column, which
   is the orientation FTRAN scatters along.

   The trap to plan for: FTRAN's passes are scatters, not the dot products
   D38 could reorder freely. `y[i]` accumulates from many sources and the
   order decides the bits, which is how D36 failed. Visiting the reachable
   set in the same order the dense loop visits it — increasing slot for L,
   decreasing position for U — keeps it exact, and that is an ordering
   problem `jm_pattern_order` already solves.

   It also removes the `O(nrow)` walks the FTRAN currently does to find its
   own zeros, and those are what D42's saving is still leaning on.
2. **The row scan that picks the infeasibility** charges one per row every
   iteration, and was 9.19% of Kennington's work before any of M2; as a
   share of what is left it is much larger now. It is a different problem
   from item 1 — a scan over `nrow` with no sparsity to exploit, which is
   what partial and multiple pricing exist for [1] — and both of those
   change the search path, so neither can be judged on digests. It is the
   first item of M2 that will need the full gate rather than a comparison.

Smaller items on the same path, all measured and all modest: the FTRAN and
BTRAN eta passes apply 45.1% and 10.6% of their etas to a zero and are
charged for all of them (1.69% of the standard set together), and BTRAN's
L' pass has 4.1% under a zero with no row-wise copy of L to search.

### 3.4 Settled during M2, so it is not re-derived

- **The refactorization interval stays at 64.** Swept over 16..256: it is
  one of only two values that come out completely clean, and the ones that
  looked cheaper looked that way because `pilot87` — 38% of the standard
  set's work on its own — had dropped out of the total by failing (D39).
- **A crash basis [12] is not the cheap win the staging table implies.**
  `build_initial_basis` starts from the slack basis for a stated reason: with
  `B = -I` every steepest-edge weight starts at its exact value. A crash
  basis destroys that, and exact weights for an arbitrary basis cost one
  solve per row. Devex or a weight reset would pay for the crash in pricing
  quality, at the start of the solve where it costs most iterations.
- **Filtering basic columns out of the pricing sweep is refused**, measured:
  36.1% of entries are in basic columns, but the filter needs a `status[v]`
  read on every entry and comes to 3.92 memory accesses against 4 (D35).
- **Reading `alpha` through its pattern always was worse than never doing
  it** when one consumer read the pattern — 1.8% more work on the standard
  set, 10% on the infeasible one — because ordering a pattern that covers
  most of the vector costs more than the scan it replaces (D40). With two
  consumers reading it that reverses, because the ordering is paid once and
  amortised, and the crossover will move again with each further consumer
  (D41). `SPARSE_ALPHA_DEN` stays at 4 regardless: the counter cannot see
  the indirection, every cost it cannot see pushes the true crossover
  towards dense, and the readings that would argue for moving it are
  fractions of a percent. Locating it exactly needs Q4.

### 3.5 Method worth keeping: sweep the trajectory, not just the instances

All 139 instances pass and always have — at one refactorization interval, so
along one trajectory. The eight defects M1 closed were closed against that
trajectory. Varying a parameter that must not change any verdict, and
requiring the gate to hold across the range, measures something the instance
sets at one setting cannot: **not whether the gate passes, but with how much
margin.** It costs minutes with the parallel runner and it found D39.

Q12 carries what it found and D39 did not close.

### 3.6 Tooling that is not in the repository yet

Measurement was the bottleneck on the work above until it was fixed outside
the tree, and the fix is worth bringing in. Instances are independent and the
figures — work units, digests, iteration counts — are integers that depend
on neither the optimisation level nor on what else is running. So the bench
runner can be built `-O3 -march=native -flto` and the instances solved
concurrently, which takes the standard set from about eight minutes to
ninety seconds and Kennington from thirty minutes to fifteen.

Verified three times now, on different trees, by producing records identical
byte for byte to the sequential `-O2` runner — which also settles, for Q11,
that those flags change no result over the 94. D40's threshold sweep is what
it bought most recently: seven settings across two instance sets, which is
hours sequentially and minutes this way.

---

## 4. Open questions

Q1, Q3, Q9 and Q10 closed with the Netlib campaign and are recorded in D31:
dual phase 1 by artificial bounds survived it, no instance forced a presolve
into M1, the refusal Q9 guarded against never fired on a real model, and no
instance asked for the anti-stall perturbation Q10 held in reserve. The last
two closed because nothing demanded them, which is weaker than the first two
and reopens the moment a model lands on either.


- **Q2** — LP and MPS dialect edge semantics (e.g., RANGES on E rows with a
  negative range value, a sub-case the public docs leave ambiguous): fixed as
  encountered, recorded in `docs/format-support.md`.

  One was encountered and closed, and the answer was that nothing needed
  changing. An `RHS` entry on the objective row sets a constant; JAOS negates
  it, which is what CPLEX documents. What the campaign actually found is that
  the published Netlib optima omit that constant — both sets, so it is not
  one source's slip — and `e226` is the only instance of the standard set
  where that is visible. The gate carries the constant in its manifest rather
  than the reader dropping it (`docs/format-support.md`). Recorded here
  because the next such case will look the same: an instance disagreeing with
  a reference is not evidence about which of them is wrong.
- **Q4** — Measurement host (D17): set up when M2 opens.
- **Q12** — **Two failure modes the trajectory sweep found and D39 did not
  close.** Varying `REFACTOR_EVERY` across 16..256 walks trajectories the
  gate never walks, and two things break that are not the verdict asymmetry
  D39 fixed. `pilot` and `pilot87` fall outside objective tolerance or get
  checker-rejected at several intervals — `pilot87` closed at 1.33e-7
  relative against a 1e-6 tolerance, so seven times of margin, and another
  path spends it. And at 128 and above `pilot87` trips the iteration guard,
  which the solver's own message calls a JAOS defect. Neither is a tolerance
  to widen; both need the instance in hand and the trajectory that produces
  them, which the sweep now makes reproducible.

  Recorded with the method, because the method is the transferable part:
  varying a parameter that must not change any verdict, and requiring the
  gate to hold across the range, measures how much margin the gate passes
  with. 139 instances at one setting cannot measure that.
- **Q11** — **Build targets for shipping: `release` and `native`.** Raised by
  the maintainer, deferred deliberately, and recorded so the reasoning is not
  re-derived. Today there is one build, `-O2 -g -DNDEBUG`, and it leaves
  everything on the table that the work counter cannot see.

  The split would be `release` — portable, reproducible across machines, and
  the one the gate runs on — and `native`, which a user gets by building on
  their own machine. Candidates for `native`, none of which touch the
  arithmetic while `-ffp-contract=off` stands: `-O3`, `-flto`,
  `-march=native -mtune=native`, `-fno-math-errno`, `--gc-sections`, and PGO
  with `make netlib` as the profiling load — 139 real models of every size
  are already the representative production workload, so no synthetic one has
  to be invented. A separate and probably larger win is `restrict` on the
  kernel pointers in `lu.c` and `simplex.c`, which is a code change rather
  than a flag and is only safe if the non-aliasing claim is actually true.

  Two things settled while it was raised. **`-g` costs nothing at run time**,
  measured: the debug sections are not `ALLOC`, so with and without `-g` the
  binary maps exactly 261,327 bytes on this machine while the file on disk
  goes 311,816 -> 94,936. Stripping is worth doing for artefact size, and the
  symbols belong in a separate file (`objcopy --only-keep-debug`) rather than
  discarded, or a user's crash becomes undiagnosable. And **the hardening
  flags Ubuntu enables by default must not be dropped from the readers**:
  `mps.c` and `lpfmt.c` parse untrusted input, which is exactly where a
  stack canary earns its cost. `lu.c` and `simplex.c` never see an
  unvalidated byte.

  What makes this tractable without Q4: that none of these change an answer
  is verifiable today, by comparing solution digests over the 139 instances.
  What they are worth in time is not, and that still needs the host.
- **Q5** — NLP derivative strategy (AD, finite differences, user-supplied): gate
  decision when M8 opens; shapes the public API of that engine.
- **Q6** — Netlib acquisition route: netlib's emps expander (a dev-time tool,
  needs D11 approval) versus a checksum-pinned unofficial mirror; decided when
  the manifest is first built.

  **Closed, 2026-08-07, both halves.** The standard set took the mirror
  route: Koch publishes plain MPS, `bench/fetch.sh` pins each file by sha256,
  no expander needed. That answered the question for 94 instances and for
  nothing else — Koch mirrors only what his paper verified, and netlib serves
  the Kennington and infeasible sets in packed emps form only.

  The maintainer decided to use netlib's `emps`. It is fetched by
  `bench/fetch.sh`, pinned at
  `fee41f544f6873a5e12bc598947828dc9964ef0676162e4df55e915760e2be22`, built
  into a temporary directory, and **not stored in this repository** — the
  same rule the instances follow (2.10). That detail is not incidental:
  `emps.c` carries no licence, no copyright notice and no public-domain
  declaration, so redistributing it is not something an Apache-2.0 project
  can do cleanly, while using it as a dev-time tool is. Nothing about the
  decision changes if that ever needs revisiting; only `fetch.sh` does.

  D12 is untouched by this. The rule forbids writing JAOS code from other
  people's source; running a format converter is not that. No part of the
  expander's logic has been read into anything here — which is also why
  option 2, writing an expander from a format `lp/data/readme` does not
  document, was the expensive one.
- **Q8** — How exact verification gets done, decided when M2 opens with
  certificate export. GMP is the obvious tool and D11 excludes it (LGPL),
  including for test-only use, since D15 exempts test dependencies from D2
  but not from D11. The alternatives to weigh then: iterative refinement
  (Gleixner et al. — machine arithmetic for almost everything, exactness only
  at the end), interval arithmetic in plain `double` (rigorous bounds, no
  dependency, hardware does the work), or hand-rolled rationals used only to
  verify a final basis. Nothing in M1 depends on this: tolerances plus Koch's
  reference values close the Netlib gate.

---

## 5. Bibliography

Verified citations — each checked against its publisher or archive before entering
this list. Implementation works from these and their kin only (D12).

1. A. Koberstein, *The Dual Simplex Method: Techniques for a Fast and Stable
   Implementation*, PhD thesis, Universität Paderborn, 2005.
2. I. Maros, *Computational Techniques of the Simplex Method*, Kluwer/Springer,
   International Series in OR&MS vol. 61, 2003.
3. V. Chvátal, *Linear Programming*, W.H. Freeman, 1983.
4. U.H. Suhl, L.M. Suhl, "Computing Sparse LU Factorizations for Large-Scale
   Linear Programming Bases", ORSA Journal on Computing 2(4):325–335, 1990.
5. J.J.H. Forrest, J.A. Tomlin, "Updated Triangular Factors of the Basis to
   Maintain Sparsity in the Product Form Simplex Method", Mathematical
   Programming 2(1):263–278, 1972.
6. H.M. Markowitz, "The Elimination Form of the Inverse and Its Application to
   Linear Programming", Management Science 3(3):255–269, 1957.
7. P.M.J. Harris, "Pivot Selection Methods of the Devex LP Code", Mathematical
   Programming 5:1–28, 1973.
8. J.J. Forrest, D. Goldfarb, "Steepest-Edge Simplex Algorithms for Linear
   Programming", Mathematical Programming 57:341–374, 1992.
9. J.A.J. Hall, K.I.M. McKinnon, "Hyper-Sparsity in the Revised Simplex Method
   and How to Exploit It", Computational Optimization and Applications
   32(3):259–283, 2005.
10. Q. Huangfu, J.A.J. Hall, "Parallelizing the Dual Revised Simplex Method",
    Mathematical Programming Computation 10(1):119–142, 2018.
11. A.R. Curtis, J.K. Reid, "On the Automatic Scaling of Matrices for Gaussian
    Elimination", IMA Journal of Applied Mathematics 10(1):118–124, 1972.
12. R.E. Bixby, "Implementing the Simplex Method: The Initial Basis", ORSA
    Journal on Computing 4(3):267–284, 1992.
13. R. Wunderling, *Paralleler und objektorientierter Simplex-Algorithmus*, PhD
    thesis, TU Berlin / ZIB TR-96-09, 1996.
14. T. Achterberg, *Constraint Integer Programming*, PhD thesis, TU Berlin, 2007.
15. R.E. Gomory, "Outline of an Algorithm for Integer Solutions to Linear
    Programs", Bulletin of the AMS 64(5):275–278, 1958.
16. H. Marchand, L.A. Wolsey, "Aggregation and Mixed Integer Rounding to Solve
    MIPs", Operations Research 49(3):363–371, 2001.
17. M. Fischetti, F. Glover, A. Lodi, "The Feasibility Pump", Mathematical
    Programming 104(1):91–104, 2005.
18. E. Danna, E. Rothberg, C. Le Pape, "Exploring Relaxation Induced
    Neighborhoods to Improve MIP Solutions", Mathematical Programming
    102(1):71–90, 2005.
19. R. Fourer, "Notes on the Dual Simplex Method", draft report, Northwestern
    University, 1994.
20. R.H. Bartels, G.H. Golub, "The Simplex Method of Linear Programming Using LU
    Decomposition", Communications of the ACM 12(5):266–268, 1969.
21. A. Koberstein, U.H. Suhl, "Progress in the Dual Simplex Method for Large
    Scale LP Problems: Practical Dual Phase 1 Algorithms", Computational
    Optimization and Applications 37(1):49–65, 2007.
22. T. Koch, "The Final NETLIB-LP Results", ZIB-Report 03-05, Zuse Institute
    Berlin, 2003; also Operations Research Letters 32(2):138–142, 2004.
