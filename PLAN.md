# Plan

Working document per the working agreement: it holds only what is open. Detail
exists only for the active milestone; later stages stay coarse until they open.
Closed items leave for the changelog and the commit that closed them. Constraints
referenced as D1–D21 live in `DECISIONS.md`.

---

## 1. Destination and staging

The declared scope (D6) staged into milestones. Each has a single gate; a milestone
is closed by its gate, never by the calendar. This refines D6's rough order with
two insertions — deterministic parallelism and interior point — placed where their
prerequisites exist.

| Stage | Delivers | Gate |
|---|---|---|
| **M1 — LP correct** | MPS+LP readers, scaling, revised dual simplex, checker | Netlib set solved to reference values; see §2.9 |
| **M2 — LP fast** | Presolve, hyper-sparsity [9], crash basis [12], pricing refinements, benchmark harness, and the deferred data-structure work of §2.11 | Measured competitive gap vs open solvers on the measurement host |
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
harness; Netlib acceptance runner.

**Out, explicitly:** presolve (only if Netlib evidence forces a minimal one — open
question Q3); primal simplex; crash basis; hyper-sparsity; parallelism; cuts;
callbacks; bindings; file *writing*; certificate export; public CLI. The Netlib
driver is a bench tool, not a product.

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

Shape, not final signatures. Once the maintainer confirms, these become a decision
record entry.

- Single public header `include/jaos.h`; prefix `jaos_` / `JAOS_`; opaque
  `jaos_model`.
- `int64_t` for every index and count in the public API; `double` for values.
  Internal storage may pack tighter; the ABI never does.
- Every fallible function returns a `jaos_status`; results come out through
  parameters. No global state; no errno games. Two models are fully independent.
- Queries copy into caller-provided buffers. The library never hands out pointers
  into its internals, so no hidden lifetimes exist.
- The library owns its memory; `jaos_model_free` releases everything.
- Two budgets, separate, per D8: work limit in work units (deterministic), time
  limit in seconds (reporting and cutoff only at deterministic checkpoints — the
  clock never picks a pivot).
- Status values at minimum: `OPTIMAL`, `INFEASIBLE`, `UNBOUNDED`, `WORK_LIMIT`,
  `TIME_LIMIT`, `NUMERICAL_ERROR`, `OUT_OF_MEMORY`, `INVALID_INPUT`.
- Queryable after solve: objective, column values, row activities, duals, reduced
  costs, basis statuses, iteration count, work units spent.
- SemVer; version macros plus `jaos_version()`.

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
   plus stability triggers (FTRAN/BTRAN residual checks).
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

Draft values; frozen when the Netlib gate closes, and any later change is a
changelog entry. Every one of them, where it acts and what it decides, plus
the checker's formulas: `docs/tolerances.md`.

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
| Checker tolerances (original space) | 1e-6 absolute on residuals, formulas to be published |

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
| Fixed overhead per simplex iteration | *see note* |
| Fixed overhead per refactorization | 4096 |

An elimination is charged by what it does, not by which routine runs it: the
same axpy costs the same inside a factorization and inside a basis update.

The per-iteration overhead has no number yet, deliberately. A dual simplex
iteration performs exactly one basis update, so a separate per-iteration
constant charged alongside the per-update one would bill a single event twice
under two names. It is settled when the iteration exists and its non-update
overhead — pricing, ratio test, bookkeeping — can be attributed on its own.

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

Remaining:

1. **Netlib campaign.** `bench/` exists and the gate has been run once, end
   to end, on all 94 instances of the standard set. It is not met. The record
   is `bench/results/netlib.txt`; what it says, and what each line of it is
   asking for, is below.

   | | |
   |---|---|
   | shape correct | **94 / 94** |
   | solved to optimal | 93 / 94 |
   | objective within tolerance | 91 / 94 |
   | independent checker green | 86 / 94 |
   | deterministic across two solves | 93 / 94 |

   The readers are the part that came out clean: every instance in the set
   loads with exactly the row and column counts two independent canonical
   sources agree on. Determinism holds everywhere a solve finished.

   The eight failures are four different problems, and they do not share a
   fix:

   - **`e226` — closed, and not where it looked.** The reader was right: the
     objective constant follows the documented convention and always did.
     What differs is the reference — neither published Netlib set includes
     the constant, so a correct answer misses both by exactly it. The gate
     records the constant per instance and compares against reference plus
     constant; the reader was left alone on purpose, and `tests/test_mps.c`
     now says why. Details in `docs/format-support.md`.
   - **`grow15` — the solve does not terminate.** The internal iteration
     guard trips at 189201 iterations, which the code correctly reports as a
     JAOS defect rather than a hard model. This is the stall Q10 has been
     waiting for an instance to produce, and now one has.
   - **Dual conditions the checker rejects on seven instances** —
     `etamacro`, `finnis`, `greenbea`, `nesm`, `pilot`, `pilot-ja`,
     `pilot87`. They were read as two groups by magnitude. They are three,
     and the third one is not the solver's fault.

     **`pilot-ja` — the checker's gap test, not a wrong answer.** Its dual
     violation is exactly zero; it is rejected on the gap alone, at
     1.87e-6. Judging the *same* solution at smaller tolerances (solve
     once, check repeatedly) gives:

     | tol | dual violation | gap |
     |---|---|---|
     | 1e-6 | 0 | 1.87e-6 |
     | 1e-7 | 0 | 1.99e-7 |
     | 1e-8 | 0 | 9.36e-16 |

     The gap falls with the tolerance, roughly in proportion, and then
     collapses. That is not what a wrong answer does — a real gap does not
     care what threshold it is measured against. The cause is in
     `src/check.c`: a multiplier with `|w| <= tol` is dropped from the sign
     conditions *and* from the dual objective, on one line, as though those
     were the same claim. They are not. A multiplier too small to impose a
     sign condition still contributes `w · bound`, and that product grows
     with the bound — so on a model with wide bounds the discarded mass is
     proportional to the tolerance, which is exactly the shape in the table.
     At 1e-8 the dual objective settles on -6113.1364655810767 against a
     primal of -6113.1364655810712: they agree to twelve figures, and
     strong duality holds.

     So `pilot-ja` is a defect in the oracle rather than in the solver, and
     it is left open here rather than fixed on the spot. The checker is what
     every solve is judged against (D18) and changing it so that a failing
     instance passes is the one repair that must never be made casually. The
     open question is what a negligible multiplier should contribute: not
     `w · bound`, since its sign is noise at that magnitude, and not zero,
     which is what produces this. `w · v`, the value the variable actually
     holds, is the candidate — complementary slackness makes the two agree
     wherever the multiplier is real. **`docs/tolerances.md` describes the
     current rule and would have to change with it.**

     **`etamacro` at 1.56e-6 and `nesm` at 8.01e-6** are the opposite case:
     their dual violations do not move at all as the checker's tolerance
     drops, so those are real breaches, small but genuine. The hypothesis
     for them, worth writing down before it is lost, is that the checker
     accumulates in `long double` where the solver works in `double`, so a
     residual the solver reads as just inside `CHECK_TOL` comes back from
     the checker just outside it. The fix would be for the solver to settle
     against a tightened bound — half of `CHECK_TOL`, say — and leave the
     rest as margin. This was prototyped on an unmerged branch and is not
     merged: that version carried debug instrumentation, duplicated the
     tolerance into a third file, and assigned `DSE_MIN` — the floor on a
     steepest-edge weight, which is a squared norm — to a reduced cost. The
     idea is worth taking; that implementation is not. Whatever replaces it
     is judged per instance against `bench/netlib.baseline`, since a
     tolerance change touches every instance at once and `etamacro` alone
     cannot say what it cost.

     **`finnis` at 28, `greenbea` at 2.66**, and the dual violations on
     `pilot` and `pilot87`, are structural and none of the above explains
     them. They are the real work left in this group.
   - **`pilot` and `pilot87` miss the objective as well**, by 2e-4 and 6e-5
     relative. These are the worst-conditioned instances in the set and are
     expected to be last; they are listed apart because an objective error
     is a different claim from a dual residue, even on the same instance.

   None of this is a tolerance to be widened. §2.6 stays where it is until
   there is a measurement on both sides of each number (D17).

Degeneracy handling is done as far as it can be done without evidence:
steepest-edge weights repair and restart themselves, and what the ratio test
spends in dual feasibility is now lent and called back rather than left
lying. What is left of it is an anti-stall perturbation, which addresses a
problem no instance has shown yet, and Q10 holds it until one does.

Of §2.5.5's stability triggers, the one that ends a solve now exists: a
declaration of optimality is re-priced from a fresh factorization before it
is accepted, because the values it was read off are carried and drift (D20).
What is still missing is a trigger that watches during the solve — an
FTRAN/BTRAN residual check that refactorizes early. The first instances say
the end-of-solve check is enough on its own; whether that survives larger
models is for the campaign, and it is the campaign that would say what
residual is worth acting on.

§2.5.6's debugging fallback to max-infeasibility pricing is not built. There
is nowhere to put the flag — the library has no options API and inventing one
for a debugging aid is the wrong order — and the rule itself is one line: it
is what steepest edge becomes when every weight is pinned at one. It waits for
the first option the library needs for a reason of its own.

### 2.9 Acceptance gate for M1

All of the following, no exceptions:

1. Every instance of the Netlib standard set: `OPTIMAL`, objective within tolerance
   of the reference value (§2.6), checker green. Kennington subset included for
   correctness with no performance expectation. Infeasible subset: classified
   `INFEASIBLE`, no false optima.
2. Determinism harness green on every instance (D8).
3. Full suite clean under ASan+UBSan.
4. Reader robustness smoke: truncated/corrupted inputs produce errors, never
   crashes.
5. Results recorded under `bench/results/` as data (status, objective, iterations,
   work units) — no wall-clock claims anywhere (D17).

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
- **The solves are dense in the working vector.** Hyper-sparsity [9] is
  already scheduled for M2 and is where this is addressed.
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
- **Scaling's determinism across machines leans on libm.** The exponents
  come from `log2`, whose last-ulp rounding IEEE does not pin down across
  C libraries. They are rounded to whole powers of two, so only a result
  landing within an ulp of an exact half-integer could differ — but "could
  differ" is a claim the cross-machine harness has to test, not assume,
  when the Netlib campaign runs it.
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

## 3. Open questions

- **Q1** — Dual phase-1 method. Artificial bounds are what is built; whether
  they survive the Netlib campaign, or a subproblem or cost-shifting method
  from [21][1] has to replace them, is decided there on evidence. Q9 carries
  the known way the present one gets an answer wrong.
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
- **Q3** — Whether any Netlib instance forces a minimal presolve into M1: decided
  by evidence during the campaign; if yes, the smallest presolve that closes the
  gate, and no more.
- **Q4** — Measurement host (D17): set up when M2 opens.
- **Q5** — NLP derivative strategy (AD, finite differences, user-supplied): gate
  decision when M8 opens; shapes the public API of that engine.
- **Q6** — Netlib acquisition route: netlib's emps expander (a dev-time tool,
  needs D11 approval) versus a checksum-pinned unofficial mirror; decided when
  the manifest is first built.
- **Q9** — Reaching an optimum that lies past the bound dual phase 1 lends.
  The verdict half of this is closed (D19): unboundedness is proven against
  a ray now, so the size of the loan no longer decides whether an answer is
  true, and a model the loan cuts off is refused rather than answered.

  What is left is that it *is* refused. The repair is to lift the loan and
  re-solve, which keeps dual feasibility — the basis and the costs do not
  move, only a nonbasic bound value — so the dual simplex can carry straight
  on. The case that does not close that way is degenerate: a basic variable
  already pressed against a real bound in the ray's direction blocks at zero
  distance, so widening the loan buys no progress, and the move that does is
  a primal pivot. That is M6 machinery.

  How often the refusal fires is known only for generated models — 46 of
  3000 — and that number says nothing about real ones, because the generator
  was written to put phase 1 to work rather than to resemble anything. So
  this waits on the campaign, which is the first set of instances that will
  mean something. If none lands here, the refusal costs nothing and the loan
  is a performance parameter. If one does, it arrives with the model that
  forced it, which is the only way to size the fix rather than guess it.
- **Q10** — What breaks a stall in *this* method, and what repairs what the
  ratio test spends. §2.5.9 calls for deterministic bound perturbation, and
  that is the primal simplex's device: the primal stalls on a zero-length
  primal step, and bounds are what a zero-length primal step is made of. The
  dual simplex stalls somewhere else. Its progress per iteration is the dual
  step times the violation being repaired, and the dual step is
  `d_q / alpha_q`, so a zero-progress iteration is one whose entering
  candidate already had a zero reduced cost. Bounds do not appear in
  `d = c - y'A`, so no perturbation of them can move that off zero;
  perturbing costs can. Bound flipping, already built, removes part of the
  short-step behaviour but not this.

  One has now stalled: `grow15` runs to the internal iteration guard at
  189201 iterations and is reported as the defect it is. So the device is no
  longer held for lack of evidence — there is an instance to develop it
  against, and one that will say whether the diagnosis above (a zero reduced
  cost on the entering candidate, repairable by perturbing costs and not
  bounds) is what is actually happening. How much to perturb is still a
  number with nothing behind it, and `grow15` is now the thing that can put
  something there.

  **Two attempts, both measured, both reverted (2026-08-07).** Neither is in
  `main`; both are in the history and worth not repeating.

  *Bland's rule in the ratio test.* Where two pivots inside the Harris window
  are the same size the choice is arbitrary, and an arbitrary choice repeated
  at a degenerate vertex is how a method cycles, so the smallest variable
  index breaks the tie. It does not fix `grow15`, which still runs to the
  guard, and it costs `grow22`, which stops terminating at all. The reason
  the diagnosis above survives this is that Bland's rule addresses *which*
  degenerate pivot is taken, and the stall here is that the pivot makes no
  progress whichever one it is. Worth recording separately: the first version
  of this tried the tie-break whenever a pivot was *not strictly better*
  rather than when it was equal, which lets a pivot orders of magnitude worse
  win on index alone and drags the standard down for the rest of the pass —
  Bland's rule silently overruling Harris instead of breaking its ties.

  *Cost perturbation, periodic and adaptive.* This one does fix `grow15` — it
  reaches the optimum. It also makes `pilot-we` report a feasible problem as
  infeasible, moves `pilotnov` to a checker rejection, and takes `grow22` from
  2179 iterations to 167865. So the device works and the diagnosis holds; what
  is missing is any principle for how much to perturb and when to stop, which
  is the part this question said had nothing behind it and still does. A
  perturbation large enough to break the stall is large enough to change what
  the solver believes about other models, and until that trade is understood
  rather than tuned, `grow15` stays open.

  What the two attempts did settle is that this cannot be developed against
  `grow15` alone. Both looked like progress on the instance in front of them
  and were regressions on the set. `bench/netlib.baseline` exists because of
  this: the third attempt gets told what it broke.

  The repair half of this question is closed: costs are shifted and called
  back, per the changelog. What it cannot close is the residue. Once the
  shifts come off, the basis is primal feasible and may still be dual
  infeasible, and the only repair that costs nothing is swapping a column
  to its other bound. Removing the rest means moving a nonbasic variable
  until something blocks, which is a primal simplex iteration and does not
  exist before M6. Whatever is left shows up in the reported reduced costs,
  where the checker sees it — so the gate will say whether it matters.

  It exists, and the campaign found it. Repairing D20 cleared it from all
  1269 generated models that reach an optimum, but seven real instances
  still publish duals the checker rejects: `finnis` at 28 and `greenbea` at
  2.66 are far too large to be rounding, while `etamacro` at 1.6e-6 and
  `nesm` at 8e-6 sit close enough to the 1e-6 tolerance that they may be
  §2.6 mis-set rather than anything wrong.

  Separating those two groups is the first job, because they have opposite
  fixes and one of them is not a fix at all. What is already ruled out is
  the explanation this question originally offered: the residue does not
  track phase 1. `adlittle` needs 22 lent bounds of 97 columns and comes out
  clean; `sc50a` needs one of 48 and did not, before D20.
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

## 4. Bibliography

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
