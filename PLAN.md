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
   | independent checker green | 87 / 94 |
   | deterministic across two solves | 93 / 94 |

   The readers are the part that came out clean: every instance in the set
   loads with exactly the row and column counts two independent canonical
   sources agree on. Determinism holds everywhere a solve finished.

   The seven remaining failures are three different problems, and they do
   not share a fix:

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
     `etamacro`, `finnis`, `greenbea`, `nesm`, `pilot`, `pilot87`, plus
     `pilot-ja` until it was closed. They were read as two groups by
     magnitude, then as three. Measured one at a time they are **four**, and
     grouping them by the size of the reported number was the mistake: what
     the checker reports is `|w|`, the magnitude of the offending
     multiplier, and a multiplier's magnitude says nothing about how far
     anything is from where it should be. `finnis` is reported at 28 and is
     the most accurate of the six.

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

     **`finnis` is not a solver defect and cannot be fixed in the solver.**
     Row 3 is a `>= 0` row. Its activity comes out at 1.52e-6 from terms
     whose magnitudes sum to 4.0e10, and one ulp at 4.0e10 is 7.6e-6 — the
     residue is a fifth of a single rounding step at the scale the row
     works at. The checker's "is it at its bound" test is `v <= lo + tol`
     with `tol` absolute, so on this row it demands seventeen correct
     decimal digits of a sum that cancels ten orders of magnitude. No
     double-precision answer can pass it, and the duality gap agrees the
     answer is right: 3.96e-11.

     So the defect is in the gate, not in the solve: an absolute tolerance
     is being applied where only a relative one has meaning. That is not a
     tolerance to widen — §2.6 stays where it is — it is a change to what
     the checker's tolerance *is*, of the same weight as D21 and D22, and it
     needs a decision record and an adversarial case built against it before
     anything is edited. It is the largest single item left in 1a: it is
     also the reason `pilot`'s row 603 is rejected at a distance of 7e-6 on
     558 of traffic.

     **Where each residue comes from, measured rather than reasoned.** The
     obvious explanation for a published reduced cost with the wrong sign is
     Q10's: the dual simplex shifts costs to hold dual feasibility, and what
     the shifts were hiding reappears when they are called in. That is
     testable — record `shift[v]` before `settle_shifts` zeroes it and print
     it next to the residue it was supposed to explain. Done, and it holds
     for exactly one of the four:

     | instance | scaled residue | shift removed | verdict |
     |---|---|---|---|
     | `etamacro` col 63 | 4.890e-8 | 4.890e-8 | the shift, to the last digit |
     | `nesm` col 2667 | −2.00e-6 | 3e-19 | **not the shift** |
     | `greenbea` col 4669 | −1.33 | 4e-9 | **not the shift** |
     | `finnis` | *no residue at all* | — | not the solver |

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

     **`greenbea` is not the shift residue, and this is the largest open
     finding of the set.** Ten columns rest at their lower bounds with scaled
     reduced costs from −0.019 to −5.28 — a violation five million times
     `DUAL_TOL` on a method whose defining invariant is that reduced costs
     stay feasible — and the shifts removed from all ten sum to less than
     4e-9. Column 4669 is the worst the checker sees: lower bound 0, no upper
     bound, zero cost, reduced cost −2.665 unscaled, so its multiplier points
     at a bound the model never declared. The objective is nonetheless right
     to 2e-7 relative, and the gap is 3.57e-17, because the checker adds no
     dual term for a multiplier aimed at an infinity.

     So the solve ends on a basis that is dual infeasible by a wide margin,
     at a vertex that is primal optimal, and declares OPTIMAL. `nesm` is the
     same thing two orders of magnitude smaller: −2.0e-6 scaled, twenty times
     `DUAL_TOL`, and no shift behind it either.

     What has *not* been established is why. The reduced costs are carried
     between refactorizations and recomputed from scratch at each one, and
     the recomputation is where these appear; `refresh` does not restore dual
     feasibility after `compute_duals`, and the end-of-solve re-pricing (D20)
     re-checks the *primal* side only, so nothing in the solve ever asks
     whether the freshly computed reduced costs are still feasible. That is a
     hypothesis with a measurement behind its premise and none behind its
     conclusion, and it is written here as such. What is certain is that
     `repair_dual_infeasibility` cannot clear it — column 4669 has no other
     bound to flip to, and `nesm`'s is 95 units away — and that the move
     which does is a nonbasic variable travelling until something blocks,
     which §2.1 puts outside M1.

     **`pilot` and `pilot87` are simply less accurate**, and their gaps say
     so on their own — 8.3e-6 and 1.1e-5 against a 1e-6 tolerance, where
     every other instance in the table is at 1e-9 or below. They are the
     worst-conditioned models in the set and they miss the objective as well,
     which the next item takes separately. Whatever closes `finnis` will not
     close these.
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

5. **`pilot87` is judged against a wrong reference.** The manifest takes it
   from the netlib readme because Koch was believed not to cover it; he does.
   His value differs from the pinned one by 3.8e-4 where the tolerance is
   3.0e-4. The instance fails on objective either way, so no verdict moves,
   but it cannot be closed until the reference is right. `bench/README.md`
   records what fixing it needs — the exact rationals, from a source that can
   be verified, since parsing them out of the report PDF reproduced only 23
   of the 92 values already known to be his.

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

All of the following, no exceptions. The right-hand column is where each one
actually stands as of 2026-08-07, because a gate whose status is only known
in aggregate is what §2.8 has just finished being a lesson about.

| # | Condition | Status |
|---|---|---|
| 1a | Netlib **standard** set: `OPTIMAL`, objective within §2.6 tolerance, checker green | **not met** — 93/94 solved, 91 objective, 87 checker |
| 1b | **Kennington** subset, for correctness with no performance expectation | **met** — 16/16, every condition, `ken-18` at 105127x154699 included |
| 1c | **Infeasible** subset: classified `INFEASIBLE`, no false optima | **met** — 29/29 refused, no false optima; `gran` closed by the basis repair (§2.8.2) |
| 2 | Determinism harness green on every instance (D8) | holds on all 93 that finish |
| 3 | Full suite clean under ASan+UBSan | **met** |
| 4 | Reader robustness: truncated/corrupted input errors, never crashes | **met** — 1.6M fuzz cases clean under ASan+UBSan, on an instrument checked against an injected fault (§2.8.4) |
| 5 | Results recorded under `bench/results/` as data, no wall-clock (D17) | **met**, and all three sets now diffed per instance against a baseline (D21) |

Two of these seven were being read as one. "The gate" has meant condition 1a
in every conversation so far, and 1b and 1c had no infrastructure behind them
at all — which is worth stating plainly, because the distance to M1 is not
the distance to closing seven instances.

**Six of the seven now hold.** What is left is condition 1a alone, and it is
seven instances of the standard 94: `grow15`, which does not terminate, and
six the checker rejects. §2.8.1 records what each of those six actually is,
measured rather than grouped by the size of the number reported — because
the number the checker reports is the magnitude of a multiplier and says
nothing on its own about how far anything is from where it should be.

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

  It exists, and the campaign found it — but not where this question put it,
  and this is the paragraph that was wrong. §2.8.1 carries the measurement:
  `shift[v]` was recorded before `settle_shifts` zeroed it and printed
  beside the residue it was meant to explain. **Of the four instances whose
  reduced costs the checker rejects, the shifts account for exactly one.**

  `etamacro` is it, and it is small: 4.890e-8 of scaled residue against
  4.890e-8 of removed shift, agreeing to every digit printed. That is under
  half of `DUAL_TOL`, so the solve considers it zero; it becomes visible only
  because the column's scale is 1/32 and `publish` divides by it, turning
  4.89e-8 into the 1.56e-6 the checker sees. The prediction was right about
  the mechanism and right that the free repair cannot reach it — a flip to
  the other bound would move the column 1.29 units and break the primal.

  `nesm` and `greenbea` are **not** this. Their shifts come off at 3e-19 and
  4e-9 while their residues stand at −2.0e-6 and −1.33 in scaled space, and
  −1.33 is five million times `DUAL_TOL` on a method whose whole invariant is
  that reduced costs stay feasible. Ten of greenbea's columns are in that
  state. Whatever puts them there is not this question's answer, and §2.8.1
  carries what is known about it, which is less than a cause.

  So the cure this question names — a nonbasic travelling until something
  blocks, which is M6 machinery — is still the cure for all three. The
  diagnosis is not. `finnis` was never in this group at all: it publishes no
  violated sign condition in scaled space whatsoever, and belongs to the
  checker's tolerance model. The remaining explanation this question
  originally offered stays ruled out either way: the residue does not track
  phase 1. `adlittle` needs 22 lent bounds of 97 columns and comes out clean;
  `sc50a` needed one of 48 and did not, before D20.
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
