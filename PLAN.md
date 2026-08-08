# Plan

Working document per the working agreement: it holds only what is open. Detail
exists only for the active milestone; later stages stay coarse until they open.
Closed items leave for the changelog and the commit that closed them. Constraints
referenced as D1–D22 live in `DECISIONS.md`.

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
harness; Netlib acceptance runner. Plus, added deliberately after the gate
demanded it: a **primal ratio test** used only to clean up after the dual
solve has finished (D28).

**Out, explicitly:** presolve (only if Netlib evidence forces a minimal one — open
question Q3); **the primal simplex** — see below, the ratio test above is not
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
   | solved to optimal | **94 / 94** |
   | objective within tolerance | **94 / 94** |
   | independent checker green | **94 / 94** |
   | deterministic across two solves | **94 / 94** |

   The readers are the part that came out clean: every instance in the set
   loads with exactly the row and column counts two independent canonical
   sources agree on. Determinism holds everywhere a solve finished.

   The remaining failures were three different problems, and they did not
   share a fix. All but one are closed:

   - **`e226` — closed, and not where it looked.** The reader was right: the
     objective constant follows the documented convention and always did.
     What differs is the reference — neither published Netlib set includes
     the constant, so a correct answer misses both by exactly it. The gate
     records the constant per instance and compares against reference plus
     constant; the reader was left alone on purpose, and `tests/test_mps.c`
     now says why. Details in `docs/format-support.md`.
   - **`grow15` — closed, and it was a cycle rather than a stall.** The
     internal iteration guard tripped at 189201 iterations, correctly
     reported as a JAOS defect rather than a hard model. Q10 read it as the
     stall it had been waiting for; instrumented, it is a **cycle of period
     four** over two rows and four variables, repeating bit for bit from
     iteration ~3000. Half its iterations take a real dual step of ~1.7e-6
     and the four cancel exactly, so "no iteration makes progress" was not
     what was happening.

     Cycles have a cure stalls do not. Bland's rule — the real one: exact
     minimum quotient, no Harris window, smallest index on *both* choices —
     solves it. It cannot be the default, because it costs 25x on `25fv47`
     and takes `grow22` from 2179 iterations to no answer at all, so it is a
     fallback that a detected cycle switches on and progress switches off.
     `grow15` solves in 21653 iterations at sixteen digits of Koch's value,
     and every instance that does not cycle is bit-identical. D26 carries
     the mechanism and the two-sided measurement that sets its one constant.
   - **Dual conditions the checker rejected on seven instances** —
     `etamacro`, `finnis`, `greenbea`, `nesm`, `pilot`, `pilot87` and
     `pilot-ja`. Three are closed (`pilot-ja` by D21, `finnis` by D23,
     `nesm` by D25) and four remain; each closure is recorded below with
     what it turned out to be.

     They were read as two groups by magnitude, then as three. Measured one
     at a time they are **four distinct defects**, and grouping them by the
     size of the reported number was the mistake: what the checker reports
     is `|w|`, the magnitude of the offending multiplier, and a multiplier's
     magnitude says nothing about how far anything is from where it should
     be. `finnis` was reported at 28 and was the most accurate of the seven.

     **`pilot-ja` — closed, and it was the checker.** Its dual violation
     was exactly zero; it was rejected on the gap alone, at 1.87e-6.
     Judging the *same* solution at smaller tolerances gave 1.87e-6 at
     1e-6, 1.99e-7 at 1e-7 and 9.36e-16 at 1e-8: a gap falling in
     proportion to the threshold it is measured against, which is not
     something a wrong answer does. The cause was one line in
     `src/check.c` dropping a multiplier with `|w| <= tol` from the sign
     conditions *and* from the dual objective, as though those were the
     same claim. A multiplier too small to impose a sign condition still
     carries `w · bound`, and that product grows with the bound. Every
     multiplier now contributes; the exemption covers the condition only.
     D21 records why, and `docs/tolerances.md` carries the rule.

     Two repairs that also close this case were tried and are wrong, kept
     here because both look reasonable and one of them passed the whole
     gate. Contributing `w · v` cancels the term — and on a model whose
     multipliers all sit under `tol` the gap is then identically zero for
     any feasible point, so the checker certifies the entire polytope. It
     passed 98 unit tests and all 94 instances with a regression-free diff
     before that was found. Choosing the bound nearest `v`, which is what
     HiGHS does for its own diagnostic, manufactures negative terms that
     offset real residues elsewhere, and evaluates `(-inf + inf) / 2` on a
     free variable.

     What the fix does not change: `etamacro`, `nesm`, `finnis`,
     `greenbea`, `pilot` and `pilot87` are unaffected, and the gate stays
     at NOT MET. Checker-green goes from 86 to 87.

     **`etamacro` at 1.56e-6 and `nesm` at 8.01e-6** are the opposite case:
     their dual violations do not move at all as the checker's tolerance
     drops, so those are real breaches, small but genuine.

     They are also, and this was not known when the group was written, the
     instances the reference work itself singles out. Koch [22] reports
     that "the current development version of SoPlex using 10^-6 as
     tolerance finds true optimal bases to all instances besides `d2q05c`,
     `etamacro`, `nesm`, `dfl001`, and `pilot4`", settled only by moving
     from 64-bit to 128-bit arithmetic; and that CPLEX 8.0 at default
     settings misses `etamacro`, `d2q06c` and `scsd6`, needing tolerances
     at 10^-9, aggressive scaling and preprocessing off.

     Checked against the current run, which is the useful part:

     | instance | SoPlex 1e-6 | CPLEX default | JAOS |
     |---|---|---|---|
     | `dfl001` | misses | — | **checker ok** |
     | `pilot4` | misses | — | **checker ok** |
     | `d2q06c` | — | misses | **checker ok** |
     | `scsd6` | — | misses | **checker ok** |
     | `nesm` | misses | — | REJECTED, dual 8.01e-6 |
     | `etamacro` | misses | misses | REJECTED, dual 1.56e-6 |

     (`d2q05c` is not in the standard 94.) So four of the six that the
     reference solvers needed special settings for come out clean here,
     and the two that do not are `nesm` and `etamacro` — with `etamacro`
     the one instance that defeats CPLEX at defaults, SoPlex at 1e-6, and
     JAOS alike. Failing there in double at a 1e-6 tolerance is documented
     behaviour of the field rather than a JAOS peculiarity. That does not
     make it acceptable; it says what closing it is likely to cost, and
     that the answer is probably precision rather than a bug.

     The hypothesis
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

     **The six, measured.** For each one, the entity the checker rejects,
     the multiplier on it, its distance from the bound that multiplier
     points at, and the traffic through it — for a row, the sum of
     `|a_ij x_j|` over the row; for a column, of `|c_j|` and the `|a_kj y_k|`.
     The last column is the one that separates them:

     | instance | offender | \|w\| | distance | traffic | dist/traffic | gap |
     |---|---|---|---|---|---|---|
     | `finnis` | row 3 | 28 | 1.52e-6 | 4.0e10 | **3.8e-17** | 3.96e-11 |
     | `greenbea` | col 4669 | 2.66 | **infinite** | 2.66 | — | 3.57e-17 |
     | `etamacro` | col 63 | 1.56e-6 | 1.29 | 0.566 | 2.27 | 1.86e-9 |
     | `nesm` | col 2667 | 8.01e-6 | 95.1 | 1.13e-3 | 8.4e4 | 2.71e-11 |
     | `pilot` | row 603 | 1.9e-2 | 7.01e-6 | 558 | 1.26e-8 | 8.29e-6 |
     | `pilot87` | col 4554 | 9.6e-3 | 0.197 | 0.242 | 0.814 | 1.12e-5 |

     **`finnis` — closed, and it was never a solver defect.** Row 3 is a
     `>= 0` row. Its activity comes out at 1.52e-6 from terms whose
     magnitudes sum to 4.0e10, and one ulp at 4.0e10 is 7.6e-6 — the residue
     is a fifth of a single rounding step at the scale the row works at. The
     checker's "is it at its bound" test was `v <= lo + tol` with `tol`
     absolute, so on this row it demanded seventeen correct decimal digits of
     a sum that cancels ten orders of magnitude. No double-precision answer
     could pass it, the duality gap said the answer was right at 3.96e-11,
     and the solve published no violated sign condition in scaled space at
     all.

     The window is now `tol · s`, with `s` the sum of the magnitudes of a
     row's terms and `max(1, |x_j|)` for a column — D23, formulas in
     `docs/tolerances.md`. What keeps it from being the gate made easier is
     an identity rather than a convention: `P − D = Σ w_v (v − bound_v)` with
     every term non-negative on a primal-feasible point, so a row waived at
     distance `d` with multiplier `w` still contributes exactly `w · d` to
     the gap. The waiver can decline to report a discrepancy twice; it cannot
     hide one. `tests/test_check.c` carries the case where the sign condition
     *is* waived and the answer is refused anyway on the gap, checked as
     `0 − (−500)` against `1000 × 0.5`.

     Measured on all three sets: `finnis` goes from REJECTED to checker ok
     with its dual violation at exactly 0 rather than merely smaller, and
     **nothing else moves at all** — 0 regressed, 1 improved, 0 new on the
     standard 94; 0/0/0 on the other two. `pilot`'s row 603 also clears,
     without changing its verdict: it fails on the gap and on the objective,
     which none of this touches.

     **Where each residue comes from, measured rather than reasoned — and
     the first measurement asked the wrong question.** The obvious
     explanation for a published reduced cost with the wrong sign is Q10's:
     the dual simplex shifts costs to hold dual feasibility, and what the
     shifts were hiding reappears when they are called in. Recording
     `shift[v]` before `settle_shifts` zeroes it, beside the residue it was
     supposed to explain, said the shifts accounted for one case of three.

     That reading was wrong, and it was wrong because a reduced cost does
     not only depend on its own column's cost. `d_j = c_j − y' M_j` and
     `y = B^-T c_B`, so a shift resting on a *basic* variable moves every
     nonbasic reduced cost at once, and the violating column need carry no
     shift of its own at all. Measuring `d` on both sides of the settlement
     rather than the shift on one column says so plainly:

     | instance | d before settling | d after | own shift | shifts on the basis |
     |---|---|---|---|---|
     | `greenbea` col 4669 | **+5.67** | −1.33 | 4e-9 | 907 basics, max 7.09e-6 |
     | `greenbea` col 4770 | **+15.0** | −5.28 | 1e-14 | " |
     | `nesm` col 2667 | **+5.12e-5** | −2.00e-6 | 3e-19 | 187 basics, max 1.11e-6 |
     | `etamacro` col 63 | 0 | +4.89e-8 | −4.89e-8 | 20 basics, max 4.35e-8 |
     | `finnis` | *no residue at all* | — | — | — |

     Every one of them is dual feasible before the shifts come off. So all
     three are Q10's residue after all, by two routes: directly, through the
     column's own shift, which is `etamacro`; and through the basis, which is
     `greenbea` and `nesm` and is the one nobody had looked for.

     **What that route costs is the finding.** On `greenbea`, removing
     shifts of at most 7.09e-6 from 907 basic variables moves one reduced
     cost from +5.67 to −1.33 — a swing of 7.0 out of a perturbation of
     7e-6, an amplification of a millionfold. That is `B^-1` on a basis this
     badly conditioned, and it says the size of the residue is not evidence
     about the size of the cause. Q10 said the residue would survive to the
     published reduced costs and that the free repair could not always reach
     it. Both hold. What it did not anticipate is that a perturbation far
     below every tolerance in §2.6 can arrive as a violation of five.

     **`finnis` produces no violated sign condition in scaled space.** Not a
     small one — none. Its rejection exists only in the checker's
     original-space view, which is the cleanest possible confirmation of the
     paragraph above: the solve is dual feasible on its own terms and the
     test it fails is one no double can pass.

     **`etamacro` is the shift residue, and it is only rejected because of
     scaling.** Column 63 rests at its upper bound of 1.2853 with a scaled
     reduced cost of +4.89e-8 where the sign condition wants it non-positive
     — a breach less than half of `DUAL_TOL`, which is to say inside what
     this solver calls zero. Its column scale is 1/32, and `publish` divides
     by it, so 4.89e-8 leaves as 1.56e-6 and lands just past the checker's
     1e-6. The residue is real, the shift explains it, and the reason it is
     visible at all is that a tolerance held in scaled space is being read
     against one applied in the original.

     **Reading the repair threshold in the original space closes it, and
     costs `pilot87` entirely. Measured 2026-08-08, not merged.** The
     re-entry (D25) decides whether a breach is worth repairing by comparing
     `d[v]` against `DUAL_TOL` — a number in the space the solver works in,
     not the one the answer is published in. Judging it where it will be
     read instead (`d[j]/gamma_j` for a column, `d[ncol+i]·rho_i` for a row
     multiplier, which is exactly what `publish` emits) is the only change,
     and it is the tightened-settling idea this section already called worth
     taking:

     | | result |
     |---|---|
     | `etamacro` | REJECTED → **checker ok**, dual 1.56e-6 → 0, 9 extra iterations |
     | `nesm` | REJECTED → checker ok (it already was, by the flip) |
     | 51 instances to `pilot` | nothing else moves, total work −0.0% |
     | **`pilot87`** | **stops solving** — the iteration guard trips at 1382801, against 50616 |

     Two improved, two regressed, and the two regressions are the same
     instance losing its answer altogether. That is the shape both earlier
     repairs of this residue took (Q10) and it is worse than the defect: a
     model with a perfectly good optimum comes back as a JAOS defect. The
     mechanism is not mysterious — a threshold read after dividing by a
     small scale factor admits far more columns as movable, so rounds keep
     finding work and 32 of them are 27 times the iterations the instance
     needs.

     What this settles is that the choice of space is load-bearing rather
     than presentational, and that "settle against a tightened bound" is not
     free: it buys `etamacro` at a price nobody had priced.

     **A second attempt on a quantity that has no space. Measured
     2026-08-08, also reverted, and it got much further.** The point the
     first attempt makes is that *any* rule reading the breach must pick a
     space. There is one that need not: the term the breach contributes to
     `P − D`, which for a nonbasic on a bound with a wrong-signed reduced
     cost is `|d|` times the width of its box. `publish` divides `d` by the
     same `gamma` it multiplies the value by, so the product is identical in
     both spaces — and it is exactly what the checker now reports as `Q`.
     Verified rather than argued: `etamacro`'s three movable breaches
     contribute 2.011e-6, 6.26e-7 and 1.676e-7 in scaled space, summing to
     the 2.805e-6 the checker publishes in the original.

     Judged on the contribution instead of the breach:

     | | |
     |---|---|
     | `etamacro` | REJECTED → **checker ok**, 9 extra iterations |
     | the other 93 of the standard set | **bit-identical**, `pilot87` included |
     | total work over the 94 | +0.0% |
     | infeasible set | PASS, 0 regressed |
     | **`pds-20`** (Kennington) | **work 3.2x, iterations 47785 → 136750** |

     It also fixed a hand-built three-column test whose optimum is readable
     by eye — the solver had been stopping 5e-8 above it with a certificate
     that did not carry, and PLAN 2.8 recorded that as a defect. So the rule
     is not merely tuning: it corrects a wrong answer on a model where no
     reference value is involved.

     **And on its own it is wrong, because of what it does to `pds-20`:
     work 3.2x, 47785 iterations becoming 136750.** Instrumented there, the
     re-entry runs all 32 rounds without converging, and **every column it
     flips has a reduced cost below `DUAL_TOL`** — the smallest run from
     2.2e-11 to 1.7e-10, three to four orders below what this solver calls
     zero. Their contributions clear the threshold only because the boxes
     are 900 to 4955 wide.

     So the contribution answers *is this worth moving* and answers it well;
     it does not answer *is there anything here at all*, and on a wide box it
     multiplies rounding noise up past any threshold. `etamacro` hid that
     because its own reduced costs (3.06e-8, 4.89e-8) sit within half an
     order of `DUAL_TOL` rather than four below it.

     **The third attempt is the merged one, and it is D27.** What was
     missing is a test for "this reduced cost is a number rather than
     rounding" that does not simply exclude `etamacro` — `DUAL_TOL` on the
     breach does exclude it. D23's own shape supplies it: `d_j = c_j −
     y' M_j` is a sum, and a sum is known no more finely than the terms that
     went into it, so a reduced cost means something where it stands above
     `eps` times the traffic through its column. That is a computed
     quantity, not a constant fitted to an instance, and it is not a second
     test bolted on — a product is only as good as its factors, and on a
     wide box the first factor was rounding.

     Measured over both feasible sets, on every column the re-entry would
     consider. Five instances of the 110 have any:

     | instance | columns | `|d| / (eps · traffic)`, smallest |
     |---|---|---|
     | `etamacro` | 3 | **5.055e8** |
     | `pilot87` | 15 | 6.985e10 |
     | `pilot` | 5 | 6.339e11 |
     | `nesm` | 1 | 3.199e13 |
     | **`pds-20`** | 14 | **2.133** |

     Seven orders of daylight, over 110 instances rather than the two the
     previous paragraph was arguing from. `pds-20` keeps exactly one column
     — the one whose traffic *equals* its `|d|`, a single term with nothing
     to cancel, so its reduced cost is exact however small — and it flips
     once and converges: 47786 iterations against a baseline of 47785.

     **`greenbea` was the same residue arriving through the basis, and was
     the largest open item of the set. Closed by D28.** Ten columns rested at
     their lower bounds with scaled reduced costs from −0.019 to −5.28, and
     every one of them was dual feasible until `settle_shifts` ran. Column
     4669 was the worst the checker saw: lower bound 0, no upper bound, zero
     cost, reduced cost −2.665 unscaled, so its multiplier pointed at a bound
     the model never declared. The objective was nonetheless right to 2e-7
     relative and the gap 3.57e-17, because the checker adds no dual term for
     a multiplier aimed at an infinity.

     So the solve ended on a basis that was dual infeasible by a wide margin,
     at a vertex that was primal optimal, and declared OPTIMAL. `nesm` was the
     same thing two orders of magnitude smaller, and D25 closed that one by
     moving its column to the other bound it had.

     `greenbea`'s ten had no other bound, which is what made them the hard
     case and what made every threshold blind to them: the term a repair test
     could weigh is `w · bound`, and there is none for an infinity. What they
     needed was to travel until something stopped them — a primal ratio test,
     which the scope question admitted (D28). Eight pivots. The objective goes
     from −72555233.859378919 to **−72555248.129846007** against Koch's exact
     −72555248.129845992, fifteen significant digits, and the dual violation
     from 2.66 to **0**.

     Where the amplification comes from is not mysterious, but it is not the
     shifts' size either: a shift is repaid at the very end, and a variable
     that was shifted while nonbasic keeps the perturbed cost when it enters
     the basis, so `c_B` carries it and `y` is perturbed for the rest of the
     solve. 907 of greenbea's basics are in that state at the finish. Both
     candidate repairs that moved the repayment *earlier* were measured and
     reverted (Q10): on a basis this ill-conditioned they turn a small final
     violation into a false infeasibility.

     **What worked instead was to move it later, and the set splits on one
     property (D25).** After settling, put the nonbasic set back on the
     feasible side of its sign conditions and run the dual simplex again from
     there — sending a column to its other *real* bound rather than moving a
     cost. Flipping breaks the primal, and primal infeasibility is what the
     method exists to remove. Measured, after settling, over the residual
     sign conditions each instance is left with:

     | instance | residual | with a real opposite bound | outcome |
     |---|---|---|---|
     | `etamacro` | **0** | — | untouched, bit-identical |
     | `greenbea` | 10 | **0** | untouched, bit-identical |
     | `nesm` | 1 | 1 | **closed**: dual 8.01e-6 → 0 |
     | `pilot` | 25 | 5 | dual 1.7e-2 → 8.0e-5, gap 8.3e-6 → 8.6e-13 |
     | `pilot87` | 48 | 15 | dual 9.6e-3 → 3.3e-5, gap 6.0e-5 → 4.0e-8 |

     The two that do not move are outside the mechanism by construction
     rather than by bad luck, and for different reasons. `etamacro` has
     nothing to repair — its breach is inside `DUAL_TOL` in scaled space and
     is a scaling artefact, as recorded above. `greenbea`'s ten all rest at a
     lower bound of 0 with no upper bound at all, so there is nowhere to send
     them; that is the travelling nonbasic, and it is what §2.1 puts outside
     M1.

     **What the rounds do, measured, because two things about them were
     assumed.** Exactly three instances of the 94 re-enter at all, and they
     converge in one round (`nesm`), three (`pilot`) and six (`pilot87`).
     Neither assumption held:

     - *The round cap was deciding an answer.* It was first written as 4,
       which is precisely where `pilot87` still had work to do. Running to
       convergence instead takes its dual violation from 2.28e-4 to 3.33e-5,
       its gap from 2.27e-7 to 4.03e-8 and its objective error from 3.21e-3
       to 2.35e-3, for 182 extra iterations out of 50434 — better on every
       measure for a third of one percent of the work. `SETTLE_ROUNDS` is
       now 32 and is a backstop rather than a limit meant to bind (D25).
     - *The residue does not fall monotonically*, so the loop must not be
       allowed to judge its own progress. On `pilot` the worst breach
       standing at the top of each round runs 4.65e-3, 4.79e-3, **7.85e-2**,
       2.87e-6: round 2 begins seventeen times worse than the solve ended,
       and it is the round that produces the final drop of three orders of
       magnitude. A rule that kept the better of two consecutive points —
       which is the obvious safety measure to reach for — would have stopped
       after round 0 and thrown that away.

     **The other candidate repair, `pilot-analysis.md` §6.1, is closed by
     measurement rather than run.** It proposes capping accumulated
     `|shift[v]|`. Instrumenting the distribution at the moment settling
     repays it, on `greenbea`: 2901 variables carry a nonzero shift, of which
     2407 are below 1e-9, 227 fall in `[1e-8, 1e-7)`, 42 in `[1e-7, 1e-6)`
     and **three** exceed 1e-6, the largest being 7.09e-6. A cap at 1e-6
     therefore touches three variables of 2901 — and not the ones that
     matter: the offending columns' own shifts are 4e-9 and 1e-14, because
     the residue arrives through the basis. On `etamacro`, where the residue
     *is* the column's own shift, every shift in the solve falls below
     `DUAL_TOL`; the cap that would bite there is narrower than the Harris
     window that created it, which is not a cap on shifts but a narrower
     window, a different change with a different cost and not what §6.1
     proposes.

     **`pilot` and `pilot87` are simply less accurate**, and their gaps say
     so on their own — 8.3e-6 and 1.1e-5 against a 1e-6 tolerance, where
     every other instance in the table is at 1e-9 or below. They are the
     worst-conditioned models in the set and they miss the objective as well,
     which the next item takes separately. Whatever closes `finnis` will not
     close these.

     **The primal test has the same shape of problem, and D23 deliberately
     did not touch it.** `interval_violation` is still absolute. Measuring
     the worst row violation against the traffic through that row, as D23
     does for the sign condition:

     | instance | row violation | traffic | relative | ulp(traffic) |
     |---|---|---|---|---|
     | `finnis` | 8.44e-7 | 4.0e10 | **2.1e-17** | 7.6e-6 |
     | `greenbea` | 4.31e-9 | 6.6e8 | 6.6e-18 | 1.2e-7 |
     | `adlittle` | 4.55e-13 | 2589 | 1.8e-16 | — |
     | `25fv47` | 1.30e-12 | 1031 | 1.3e-15 | — |
     | `nesm` | 1.00e-8 | 0.70 | **1.4e-8** | 1.1e-16 |
     | `pilot` | 1.96e-5 | 1129 | **1.7e-8** | 2.3e-13 |

     The absolute rule is **both too strict and too lax**, and which one it is
     depends on nothing but the row's scale. `finnis` passes it by 16% of the
     margin while its residue is a tenth of one ulp of the row — one more
     rounding step in the wrong direction and a correct answer would be
     refused on the primal too, as it already was on the dual. `nesm` passes
     it comfortably at 1e-8 absolute while being a hundred million ulps out
     relative to a row carrying 0.7.

     Between the instances that are clearly fine (1e-15 to 1e-17 relative)
     and the two that are clearly not (1.4e-8, 1.7e-8) there are seven orders
     of magnitude of daylight, which is the measurement §2.6 had been waiting
     for.

     **Closed by D24: the primal test stays absolute.** Not because the
     measurement is wrong — it stands — but because primal feasibility is
     the *hypothesis* of the identity D23 rests on. `P − D = Σ w_v (v −
     bound_v)` has non-negative terms only where `v` is inside its bounds, so
     relaxing that does not extend D23, it removes what D23 stands on; and an
     infeasible entity's term turns negative, offsetting real residues
     elsewhere — the fungibility defect D22 already refused in writing. It
     also buys nothing: exactly one instance of the 94 exceeds 1e-6 on a row
     (`pilot`, already rejected twice over). D24 carries the rest, including
     the one form that would be safe if this is ever revisited — `min(tol,
     tol·s)`, narrowing rather than widening.

     **What the argument turned up, now built.** The gap is `|Q − N|`, with
     `Q` the positive terms and `N` what a within-tolerance primal violation
     contributes negatively; the two cancel and the checker cannot tell a
     small gap from two large halves. Both halves are now accumulated apart
     and published — `gap_positive` and `gap_negative` in
     `jaos_check_report`, alongside `max_row_violation_relative`, which is
     the relative primal residue D24 said it would keep in the report and out
     of the predicate. Three public fields, two `long double` adds, no
     verdict moved, and the record carries all of it per instance.

     Putting it on the 94 turned the constructed case into a measured one,
     and into a common case rather than a curiosity. On **35 of the 93
     instances that reach an optimum, `Q` exceeds `|Q − N|` by more than a
     factor of two** — `pilotnov` by 157, `greenbeb` by 34, `finnis` by 3.
     The gap those instances report is not the bound they are entitled to.

     The size matters before the ratio does: every one of those `Q` values is
     tiny in absolute terms — 7.85e-10 on `pilotnov` against an objective of
     order 4.5e3 — so the certificates were sound throughout and no verdict
     was ever wrong. What changed is that soundness is now something the
     record shows instead of something the identity was assumed to deliver.
     `P − P* ≤ Q` is a bound a reader can check, on an instance that
     *passes*, which is the only kind where this was ever going to be
     visible.
   - **`pilot` and `pilot87` miss the objective as well**, by 2e-4 and 6e-5
     relative. These are the worst-conditioned instances in the set and are
     expected to be last; they are listed apart because an objective error
     is a different claim from a dual residue, even on the same instance.

   None of this is a tolerance to be widened. §2.6 stays where it is until
   there is a measurement on both sides of each number (D17).

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
   green throughout. Whatever ails the seven open instances of 1a, it is not
   that the readers or the factorization stop working at scale.

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
| 2 | Determinism harness green on every instance (D8) | **met** — 94/94 on the standard set, 16/16 Kennington, 29/29 infeasible |
| 3 | Full suite clean under ASan+UBSan | **met** |
| 4 | Reader robustness: truncated/corrupted input errors, never crashes | **met** — 1.6M fuzz cases clean under ASan+UBSan, on an instrument checked against an injected fault (§2.8.4) |
| 5 | Results recorded under `bench/results/` as data, no wall-clock (D17) | **met**, and all three sets now diffed per instance against a baseline (D21) |

Two of these seven were being read as one. "The gate" has meant condition 1a
in every conversation so far, and 1b and 1c had no infrastructure behind them
at all — which is worth stating plainly, because the distance to M1 is not
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

### What happens next, in order

**Steps 1, 2 and 3 are done.** The two in-scope repairs were run, the checker
was instrumented, and the scope question that waited on them has been decided.
The order mattered: without the measurements, step 3 could only have been
argued.

- *§6.3, re-entry from the settled basis* — built, D25. Closed `nesm`. The
  failure both earlier attempts produced, a feasible model returned
  INFEASIBLE, is structurally refused: the settled point is saved and a
  re-entry that ends in anything but a second optimum is discarded.
- *§6.1, a cap on accumulated `|shift[v]|`* — closed by measurement rather
  than by a run. §2.8.1 carries the distribution: on `greenbea` three
  variables of 2901 exceed 1e-6 and the offending columns' own shifts are
  4e-9 and 1e-14, so a cap cannot reach them.
- *`Q` and `N`*, plus the relative primal residue — in `jaos_check_report`
  and in the record (D24). No verdict moved, and they turned out to be what
  D27 needed: the quantity that decides a repair is the one the checker
  publishes as `Q`.
- *`grow15`* — not part of the plan and the largest single change. It was a
  cycle of period four, not the stall Q10 diagnosed, and Bland's rule as a
  detected-cycle fallback closes it (D26).
- *`etamacro`* — closed by D27 after two attempts that were measured and
  reverted, both recorded in §2.8.1 because each is what pointed at the next.
- **The scope question — decided.** A primal ratio test enters M1; the primal
  simplex stays out, and §2.1 now says in writing where that line runs. D28
  carries it. `greenbea` closed for eight pivots, at fifteen significant
  digits of Koch's exact value, and `pilot`'s objective came inside tolerance
  from 390x outside it.

**What is left of 1a is one instance, and it is not the kind of failure the
gate started with.**

1. **`pilot` — closed by D29, and it was a residual of the basis solve.** What
   follows is the record of how it was read before that, because both wrong
   readings were reasonable and the second is the one §2.5.5 had written down.

   **`pilot`, on one row at `1.73e-6`.** Its objective is right, its dual
   violation is exactly zero, its gap is `6.6e-14`. The only thing refusing
   it is `interval_violation`, an absolute test on a row 1.73 times the
   tolerance out. This is D24's question and D28 records that one of D24's
   four arguments — that a relative rule "buys no verdict" — is now false.
   The other three stand, and the first is still sufficient on its own:
   primal feasibility is the hypothesis D23's identity rests on, not a test
   beside it. If it is revisited, the only safe form is the one D24 already
   names: `min(tol, tol·s)`, which narrows and can only turn acceptances
   into rejections.

   **Measured three times. It is neither a tolerance nor a violated bound,
   and it is not the trigger §2.5.5 named either.**

   First, the relative figure D24 put in the report, which needed no new run:
   `pilot`'s row residue is `6.93e-9` of what the row carries, against
   `8.21e-17` for `finnis`, `1.76e-16` for `adlittle` and `6.08e-14` for
   `25fv47` — seven to nine orders above the band a healthy row sits in, about
   3e7 ulps of a row carrying 250. So a relative window of `tol · s` would be
   `2.5e-4` wide there and would wave a real discrepancy through. D24 is
   supported by a measurement now rather than by the absence of one.

   Then the solver's own view of the same point, and this is the finding:
   **no basic variable is outside its bound at all.** The worst violation in
   scaled space is exactly `0`. So the `1.73e-6` is not a bound test failing
   anywhere — it is the difference between two computations of one quantity.
   The solver carries each row's activity in a logical variable obtained from
   `x_B = -B^-1 (N x_N)` on the scaled copy; the checker recomputes
   `sum_j a_ij x_j` from the matrix as loaded and the published `x`. They
   disagree by `1.73e-6`, which is a **residual of the basis solve**, not a
   primal infeasibility.

   That relocates the defect. It is not `interval_violation`, not `PRIMAL_TOL`
   and not a space mismatch: it is how accurately `B^-1` is applied on
   `pilot`'s basis.

   It is also the one place D18's argument for an independent checker pays
   off in the direction nobody was watching: checker and solver agree about
   the model, and disagree about the arithmetic.

   **And the cure §2.5.5 named is not the cure, which is the third
   measurement.** This paragraph used to end by calling for a stability
   trigger watching an FTRAN/BTRAN residual during the solve. But the
   residual is measured against a factorization D20 has just rebuilt, so
   refactorizing earlier cannot reach it — the error is the backward error of
   the triangular solves on this basis, not drift in a patched LU. What
   reaches it is **one step of iterative refinement** on that solve:
   `7.06e-6` becomes `9.09e-13`, and the rejected row `1.73e-6` becomes
   `6.73e-13`.

   Where to apply it had a price on it. Refining every solve was measured and
   is the sixth instance of the failure this milestone keeps producing:
   `pilot-ja`, a model with a known finite optimum, comes back **INFEASIBLE**,
   and `pilot87` pays **4.5x** the work. Refining only the primal is no better
   in kind — it takes `pilot`'s dual violation from `0` to `0.0688`. What
   holds is refining both solves, at the one refresh that verifies an optimum:
   mid-solve the two vectors choose a pivot and a trajectory is not more
   correct for better numbers, while at the end they *are* the answer. **93 of
   the 94 instances take exactly the iteration count they took before**, and
   total work over the set falls 0.029%; Kennington and the infeasible set are
   0/0/0 and the infeasible record is byte for byte identical. D29 carries it.

2. **`pilot87`, on its objective by 7.6x** — `2.28e-3` of error against
   `3.02e-4`. Its dual violation is `1.87e-5` and its gap `2.75e-8`, both
   improved by an order of magnitude, and it is the worst-conditioned model
   in the set. Nothing built so far moves it and no mechanism now in hand
   points at it. It is the whole of what stands between the standard set and
   condition 1a, and it may end up as an exception with a measured mechanism
   and a frozen bound — which was option 2 of the scope question, and it is
   available for one instance without being available for the gate.

One correction that removes an argument from that option, and it has now
half-expired: on `pilot`, JAOS *was* further from Koch than MINOS 5.3, OSL and
CPLEX all are, and all three ran in double
(`docs/research/pilot-analysis.md` §3.2). It no longer is — D28 brought it
within `2.3e-5`. So "the limit of double precision" was never available for
`pilot` and is now visibly not, which is worth remembering when the same
argument is offered for `pilot87`: it needs to be made about `pilot87` and
measured there, not inherited.

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

  One had stalled: `grow15` ran to the internal iteration guard at 189201
  iterations. **It is closed, and the diagnosis above was close but wrong
  (D26).** Instrumented, the solve repeats bit for bit from iteration ~3000
  — same total infeasibility to ten digits, same 43 violated rows, same
  objective, exactly half the steps degenerate — and the pivot log inside
  the repeat shows a **cycle of period four** over two rows and four
  variables, with `xb` and both steepest-edge weights returning to identical
  values every fourth iteration. Half of those iterations do take a real
  step, of about 1.7e-6; the four cancel exactly. So this was never "no
  iteration makes progress", and the cure a cycle has is not the cure a
  stall has.

  `grow15` now solves in 21653 iterations, at an objective matching Koch's
  to sixteen digits, and `grow22`, `grow7` and `truss` come back with
  identical digests. D26 carries the mechanism, the cost of Bland's rule as
  a default (25x on `25fv47`, and `grow22` stops solving), and the
  measurement that sets the trigger.

  **What is left of this question is the perturbation half**, and it is
  narrower than it was: the device is no longer needed for `grow15`, so
  there is again no instance forcing it, and "how much to perturb" is again
  a number with nothing behind it. It waits, as it did before, for a model
  that needs it.

  **Two attempts before that, both measured, both reverted (2026-08-07).**
  Neither is in `main`; both are in the history and worth not repeating.

  *Bland's rule in the ratio test.* Where two pivots inside the Harris window
  are the same size the choice is arbitrary, and an arbitrary choice repeated
  at a degenerate vertex is how a method cycles, so the smallest variable
  index breaks the tie. It does not fix `grow15`, which still runs to the
  guard, and it costs `grow22`, which stops terminating at all. Worth
  recording separately: the first version of this tried the tie-break
  whenever a pivot was *not strictly better* rather than when it was equal,
  which lets a pivot orders of magnitude worse win on index alone and drags
  the standard down for the rest of the pass — Bland's rule silently
  overruling Harris instead of breaking its ties.

  **This attempt was misnamed, and the name is why it read as a dead end.**
  A smallest-index tie-break among equally sized pivots inside the Harris
  window is not Bland's rule and carries none of its guarantee: that needs
  the exact minimum quotient, no widening, and the index rule on the
  *leaving* choice too. Built as the actual rule it solves `grow15` outright
  (D26). What this attempt did establish stands unchanged and is half of why
  D26 is a fallback rather than a default — it costs `grow22` its answer.

  *Cost perturbation, periodic and adaptive.* This one does fix `grow15` — it
  reaches the optimum. It also makes `pilot-we` report a feasible problem as
  infeasible, moves `pilotnov` to a checker rejection, and takes `grow22` from
  2179 iterations to 167865. So the device works; what is missing is any
  principle for how much to perturb and when to stop, which is the part this
  question said had nothing behind it and still does. A perturbation large
  enough to break a cycle is large enough to change what the solver believes
  about other models, and until that trade is understood rather than tuned,
  it stays out.

  In hindsight this is unsurprising rather than mysterious: perturbing costs
  breaks a cycle by destroying the degeneracy that sustains it, and it does
  that on every model whether or not one is cycling. D26 breaks the same
  cycle by removing the freedom instead of the degeneracy, and only on the
  model that is cycling. That is the whole difference between the two, and
  it is why one is merged and the other is not.

  What the two attempts did settle is that this cannot be developed against
  `grow15` alone. Both looked like progress on the instance in front of them
  and were regressions on the set. `bench/netlib.baseline` exists because of
  this, and it is what told D26 that `grow22`, `grow7` and `truss` came back
  bit-identical.

  The repair half of this question is closed: costs are shifted and called
  back, per the changelog. What it cannot close is the residue. Once the
  shifts come off, the basis is primal feasible and may still be dual
  infeasible, and the only repair that costs nothing is swapping a column
  to its other bound. Removing the rest means moving a nonbasic variable
  until something blocks, which is a primal simplex iteration and does not
  exist before M6. Whatever is left shows up in the reported reduced costs,
  where the checker sees it — so the gate will say whether it matters.

  **It exists, the campaign found it, and it is exactly this.** §2.8.1
  carries the measurement: every column the checker rejects on `etamacro`,
  `nesm` and `greenbea` is dual feasible immediately before `settle_shifts`
  runs and infeasible immediately after. Three of the six, and the prediction
  holds in both halves — the residue survives to the published reduced costs,
  and `repair_dual_infeasibility` cannot always reach it (column 4669 of
  greenbea has no other bound at all; `nesm`'s is 95 units away).

  One thing this question got wrong, and it took two measurements to see.
  Asking which shift explains a residue by reading `shift[v]` on the
  violating column answers "none" for two of the three, and that is the wrong
  question: `d_j = c_j − y' M_j` with `y = B^-T c_B`, so a shift resting on a
  *basic* variable moves every nonbasic reduced cost at once and the
  violating column need carry no shift of its own.

  **And that route amplifies.** On `greenbea`, repaying shifts of at most
  7.09e-6 across 907 basic variables takes one reduced cost from +5.67 to
  −1.33. A perturbation four orders below every tolerance in §2.6 arrives as
  a violation of five, because `B^-1` on that basis is what stands between
  them. So the size of a residue is not evidence about the size of its cause,
  and "far too large to be rounding" — the sentence that put `finnis` and
  `greenbea` in one group for months — was never a valid inference.

  How the shift reaches `c_B` is what looked worth attacking. A shift lives
  on a variable's cost, and where that cost sits decides what repaying it
  costs: on a nonbasic column it enters exactly one reduced cost, its own, so
  repaying moves one number; on a basic column it is part of `c_B`, and every
  reduced cost is read off `y = B^-T c_B`, so repaying moves all of them
  through `B^-1`. A variable shifted while nonbasic keeps the perturbed cost
  when it enters the basis, and the repayment waits for the end of the solve.
  So: repay earlier, while the method can still respond.

  **Two attempts, both measured, both reverted (2026-08-07). Neither is in
  `main`.** Both fail the same way, which is the useful part.

  *Repay on entry, inside `pivot()`.* The correction is exact and cheap —
  removing `sigma` from `c_q` moves `y` by `-(sigma/alpha_q) rho_r`, so every
  reduced cost moves by `(sigma/alpha_q) alpha_v`, one more multiple of the
  pricing row folded into the pass already there. It passed all 114 unit
  tests and wrecked the campaign: `greenbea` — a feasible model — came back
  INFEASIBLE, `nesm` went from 8.01e-6 to 1.17e6, `pilot` stopped solving.
  The reason is in the formula. `alpha_q` is allowed down to `PIVOT_MIN`, so
  a pivot of 1e-9 turns a repayment of 1e-6 into a kick of 1e3 through every
  reduced cost in the model, and `shift_to_feasible` answers each kick with
  fresh loans that are larger than the one just repaid.

  *Repay at each refactorization instead*, where `compute_duals` rebuilds
  everything from the factorization and no `sigma/alpha_q` term exists at
  all. This looked like the right fix and is not: `greenbea` again came back
  INFEASIBLE, `nesm` reached 190, `pilot87` reached 10.9 with a gap of 0.998.
  Removing the division does not remove the amplification — `B^-1` is still
  what stands between a 7e-6 perturbation and the reduced costs — and
  applying it every 64 iterations rather than once means applying it two
  hundred times over a solve like greenbea's, with the method chasing its own
  noise between them.

  What the two settle is that the residue cannot be repaid mid-solve on an
  ill-conditioned basis at all: both attempts turned a small final violation
  into a false infeasibility, which is a strictly worse failure than the one
  they set out to fix.

  **A third attempt, post-solve, and this one holds (D25).** Both of the
  above perturb the method while it is running. The alternative is to let it
  finish, settle, and only then put the nonbasic set back on the feasible
  side of its sign conditions and run the method again from there — moving a
  column to its *other real bound* rather than moving a cost. That is not a
  primal pivot and it is inside M1: it is bound flipping, which the ratio
  test already does, applied once at the end instead of mid-iteration. Where
  a column has a real bound to go to, the flip breaks the primal, and primal
  infeasibility is precisely what the dual simplex is for.

  Measured: `nesm` closes outright — dual violation exactly 0, gap from
  2.71e-11 to 1.93e-16, seven extra iterations — and `pilot` and `pilot87`
  improve by two orders of magnitude on the dual without changing verdict.
  Nothing regresses on any of the three sets. What guards the failure the
  first two attempts produced is that the settled point is saved and any
  re-entry not ending in a second optimum is thrown away; a model already
  proved to have an optimum has not become infeasible, so that verdict is
  evidence against the re-entry rather than about the model.

  **So this question's own statement of what was left needed correcting.** It
  said a nonbasic travelling until something blocks was the only remaining
  cure. That is true only of a column with nowhere to rest — and the
  measurement splits the set on exactly that line: `greenbea`'s ten offending
  columns all sit at a lower bound of 0 with no upper bound at all, so
  flipping has nothing to offer them and the travelling nonbasic is still
  the only route; `nesm`'s one had a bound 380 units away and flipping
  reached it. What is genuinely outside M1 is narrower than this question
  claimed: not the repair, but the repair *of a column with no other bound*.

  `finnis` was never in this group: it publishes no violated sign condition
  in scaled space whatsoever, and belongs to the checker's tolerance model.
  The remaining explanation this question originally offered stays ruled out:
  the residue does not track phase 1. `adlittle` needs 22 lent bounds of 97
  columns and comes out clean; `sc50a` needed one of 48 and did not,
  before D20.
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
