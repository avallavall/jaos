# Plan

What is open, in the order it will be done. What JAOS is meant to be is in
`SPECS.md`; what has landed is in `CHANGELOG.md`; why is in `DECISIONS.md`.
Constraints referenced as D*n* live there.

**Where the old section numbers went.** This file used to carry a closed
milestone in a `§2`, and comments in the source still cite it. Nothing was
lost, but the numbers no longer resolve here:

| cited as | now in |
|---|---|
| PLAN 2.1, 2.8, 2.9 — scope, what was built, the gate | `SPECS.md` |
| PLAN 2.4 — the public API's shape | D33 |
| PLAN 2.5.x — components and their literature anchors | `SPECS.md` §3, with the citation numbers |
| PLAN 2.6 — tolerances | `docs/tolerances.md` |
| PLAN 2.7 — work units | `docs/work-units.md` |
| PLAN 2.10 — instances and reference values | `bench/README.md` |
| PLAN 2.11 — deferred by measurement | phase 6 below |
| PLAN 3.x — M2's attribution and method | phase 6 and "Method worth keeping" below |

## The order, and why it is this one

| | phase | why here |
|---|---|---|
| **1** | **Know where we stand** — time every run, and compare against HiGHS, SoPlex and Clp | Every speed decision so far has been taken blind. The only phase that changes what we know rather than what the code does. |
| **2** | **Make it usable** — options, model modification, warm re-solve | Cheap, and it unblocks everything after it. Today a user can load, solve and read; nothing else. |
| **3** | **Presolve** | The largest single algorithmic gap, and phase 1 will have said what it is worth. |
| **4** | **Complete the product** — writing, callbacks, sensitivity, certificates | What turns a solver into a library. |
| **5** | **Bindings** | Needs 2 and 4 to have stopped moving. |
| **6** | **Speed** — the LU, partial pricing, the primal simplex | Deliberately after 1: the attribution below is in work units, and D45 showed those are biased against exactly the part that costs the most time. |
| **7** | **The long ones** — barrier and crossover, deterministic parallelism, MILP | Each is a milestone of its own. |

---

## Phase 1 — Know where we stand

### 1.1 Time on every run

Work units make regressions detectable across machines; seconds say whether
the units bought anything. The two records must never be confused.

- `bench/run.c` prints elapsed seconds per instance and for the set, on every
  run, to the console.
- Seconds go to a side file that names the machine. **They never enter
  `bench/results/*.txt` or any `.baseline`** — those stay deterministic,
  because a baseline that changes every run cannot detect a regression.
- A change is judged on three things from here (D45): solution digests for
  correctness, work units for determinism and cross-machine comparability,
  and a same-instance time ratio to catch the cases where the units lie.

### 1.2 The parallel runner, into the repository

**Done (D57).** `-j N` on the acceptance runner, `J=N` on every netlib make
target. Instances are independent and every figure the record carries is an
integer the solver computed, so solving them concurrently cannot move one:
verified by diffing all 139 lines against the committed records, which came
from sequential runs. Standard set 8 min -> 84 s, infeasible 2 min -> 9 s,
Kennington 30 min -> 8 min 21 s.

**The seconds are the one thing it invalidates**, and the runner says so on
every parallel run. A time ratio (D45) still needs `J=1`.

Not taken: building the runner `-O3 -march=native -flto`. It would have been
a second change measured at the same time as this one, and the flags are
Q11's question, not this one's.

### 1.3 The comparison harness

`bench/compare/README.md` carries the design: a four-rung ladder where each
rung's difference from the one below attributes the gap to one feature; times
judged only when the answer verifies; tolerances equalised explicitly;
competitor versions pinned by checksum; two times per run, the solver's own
and the process's; the minimum of N runs rather than the mean.

**Built, and the first reading is taken (D52).** `solvers.manifest` pins
HiGHS, SoPlex and Clp by checksum with their licences verified at fetch;
`fetch-solvers.sh` builds them, dev-time only, nothing redistributed;
`run-compare.sh` walks a rung and writes `bench/compare/results/`. `make
compare` does the lot.

**T0 is measured against two rivals (D52, D53), and re-measured after D58 and
D59 (D60).** Against HiGHS 1.15.1, JAOS is **3.70x** slower per solve — 1.47x
the iterations and **2.52x** the cost of each. Against SoPlex 8.0.3 it is
**1.31x** slower on **0.70x** the iterations and **1.87x** per iteration, and
faster on 11 of 22. The per-iteration ratio agrees between the two rivals
instance by instance, which is what makes it a quantity rather than a
quotient.

**The harness repeats itself to 1.3%** (D60), which is the floor any claim
about these numbers has to clear. It also used to rebuild its JAOS only when
its own driver changed, so it measured a stale binary and said nothing; that
is repaired and every record now carries the commit it came from.

**Q4 is downgraded from a blocker to a label.** It said the gate needs a
dedicated measurement host, and it does — for a *published* figure. Comparing
on this machine with every line stamped as a development number is
incomparably better than not comparing, which is where two milestones of
speed work had already gone.

Still open here: **Clp**, which needs CoinUtils and Osi — a dependency chain
rather than a repository — and **rungs T1 to T3**, which are what attribute
the gap to presolve and to choosing the algorithm. Also `grow15`, where JAOS
takes 21.7x HiGHS's iterations: that is D26's cycle detection and Bland
fallback, and this is the first measurement of what it costs.

---

## Phase 2 — Make it usable

Nothing here is hard, and most of it has landed. What is left is adding and
deleting rows and columns, and carrying a basis out of a solve that stopped
short of an optimum.

**The line this API is drawn on: it configures the contract, never the
method.** What a caller may set is what depends on their problem and which
the solver cannot know — how much precision their data deserves, how long
they are willing to wait, where their log lines should go. What the solver
decides for itself is everything about *how* it solves: the pricing rule,
when a weight is no longer worth carrying, when to refactorize, whether a
sparse or a dense path is cheaper. A caller cannot know whether their model
wants Devex or steepest edge, and asking them is handing over a problem that
belongs here. That is already the practice — every such constant in this
solver is measured and fixed rather than exposed — and it is now the stated
rule, which is why the adaptive work in phase 6 matters: the intelligence has
to be inside.

- **An options API**, for the contract only: tolerances, limits, logging,
  callbacks. **The shape is decided and the tolerances have landed (D64):**
  setters on the model, because `jaos_set_work_limit` and
  `jaos_set_time_limit` already were, and a second mechanism would give two
  ways to configure one thing. `jaos_set_primal_tolerance` and
  `jaos_set_dual_tolerance` are in, with 0 restoring the default and anything
  that is not a finite non-negative number refused rather than clamped. All
  139 digests unmoved, because a model that sets nothing behaves as before.
  What is left here is logging and callbacks.
- **Logging. Done (D65).** `jaos_set_log_callback` and `jaos_set_log_level`,
  four levels, silent until a callback is installed — a library that writes
  to stdout because nobody forbade it cannot be embedded. Paced by iteration
  count and never by a clock, so two runs of one model produce the same
  lines. The closing summary reports refactorizations, weight restarts and
  stalls, which are the three events four separate diagnoses this milestone
  had to instrument the solver by hand to see.
- **Model modification.** **Bounds and costs are in (D66):**
  `jaos_set_col_cost`, `jaos_set_col_bounds`, `jaos_set_row_bounds`, each
  discarding the answer the model was holding, because an optimum computed
  for the problem as it stood describes a different problem the moment a
  bound moves. Neither touches the matrix, so the scaling and the row-wise
  mirror stay exactly correct and are deliberately left alone.

  **Coefficients are in too (D67):** `jaos_set_coefficient`, three operations
  under one name because the stored matrix holds an invariant — columns
  ascend by row index, no duplicates, no explicit zeros. Zero deletes, a new
  index inserts in sorted position, and both derived copies are discarded
  because both are computed from the matrix.

  What is left: **adding and deleting rows and columns**, which restructures
  every array the model owns.
- **Warm re-solve. Done (D68), and it is one line of design:** the basis is
  stored apart from the answer, so a modification discards one and keeps the
  other. A solve that finds an optimum leaves its basis there and the next
  solve resumes from it with nothing called; `jaos_set_basis` hands one in
  from elsewhere and `jaos_clear_basis` forgets it. Dual feasibility comes
  from cost shifting rather than from artificial bounds — `B = -I` is what
  made a reduced cost a cost, and a warm basis does not have that — and the
  weights restart at one, which is a prior here rather than the fact it is
  cold. All 139 digests unmoved.

  What is left: **a stopping point for a solve that did not reach an
  optimum.** Only `publish`'s optimal path writes a basis, so a run cut off
  by a work or time limit carries nothing forward, and resuming it is exactly
  what a budgeted solve is for.

**Two claims this section used to make, and both were wrong.** It said a
caller cannot vary the checker's tolerance: `jaos_check_solution` has taken
`double tol` as a public parameter all along. And it attributed D47's
diagnosis to that missing option — D47 needed `REFACTOR_EVERY` varied over
16..256, which is a *method* constant and by the rule above is not going in
the API at all. What those diagnoses actually need is a build-time switch or
a private entry point for development, which is a different thing from an
option and belongs nowhere near `jaos.h`. The same goes for the debugging
fallback to max-infeasibility pricing.

**So what the API actually owes a caller is short**, which is the point of
drawing the line: `PRIMAL_TOL` and `DUAL_TOL` — the two every competing
solver exposes and which the comparison harness already equalises explicitly
across solvers — plus logging and callbacks. Everything else in
`docs/tolerances.md` is method: `PIVOT_MIN`, `LU_PIVOT_TOL`,
`LU_UPDATE_TOL`, `DROP_REL`, `DSE_MIN`, `DSE_DRIFT`, `REFACTOR_EVERY`,
`ITER_SANITY_FACTOR`, `ARTIFICIAL_BOUND`. Each of those is measured and fixed
and stays that way.

---

## Phase 3 — Presolve

Q3 closed presolve out of M1 because no instance needed it *for correctness*.
It was never weighed for speed, and phase 1 will have said what the field
gains from it.

Scope when it opens: empty and singleton rows and columns, forcing and
redundant constraints, bound tightening, fixed variables, duplicate rows and
columns, dominated columns. And the postsolve that puts a solution back —
which is where the correctness risk lives, not in the reductions.

---

## Phase 4 — Complete the product

- Write MPS and LP; write a solution file.
- Callbacks.
- Sensitivity and ranging.
- Infeasibility and unboundedness certificates, exportable.
- Exact rational verification of a final basis (Q8).

---

## Phase 5 — Bindings

Python first. Nothing to design until the C API stops moving.

---

## Phase 6 — Speed

**It is two targets, and the set mean was hiding one of them (D63).** The
1.47x iteration ratio against HiGHS is a geometric mean with a fat tail:
`pilot` and `pilot87` take **4.6x** HiGHS's iterations, `25fv47` 3.0x and
`greenbea` 2.9x, while `maros-r7` — the worst instance by time — takes only
2.3x and costs **11.0x** per iteration.

- **Cheaper iterations** is `maros-r7`'s problem and phase 6 item 1's work.
- **Fewer iterations** is what the other four need, and D63 measured the
  cause: their steepest-edge weights are discarded on 80–93% of iterations,
  so they are priced by largest infeasibility with extra steps. Disabling the
  discard takes them to 0.31x–0.54x of their iteration counts and breaks the
  gate; the threshold is bounded on both sides by correctness. The cure is
  weights that survive, not a looser threshold — **Devex [7]** is the
  candidate, since its weights are approximate by construction.

**The seventeen turned out to be two different things (D54), and one of them
is closed.** `fit2p` billed an ordinary 146k units an iteration and took six
times the median of real time per unit; that was two pieces of work nobody
billed — a capacity check across a translation unit (D55) and an elimination
rebuilding columns it had nothing to eliminate from (D56) — and it is down
from 17.5x to **5.1x** per iteration against HiGHS.

**`maros-r7` is the one left, alone at 16.5x**, and it is the opposite kind:
two million billed units an iteration, fifty times the median, at a
*below*-median cost per unit. Its iterations really are that expensive and the
counter sees them. That is the factorization, and fill reduction is the lever
— it carries 4.801 nonzeros in its factors per nonzero of the basis against a
set average of 2.673.

And the figure that governs this whole section: **the real cost of a billed
unit spans 14.7x across the timed set**, 0.795 to 11.686 µs per thousand. A
ranking by work units can be wrong by an order of magnitude depending on which
instance carries the total — and two instances carry 74% of this one.

**Read D45 before any figure below.** The work counter is optimistic by a
factor that is not constant: M2 bought 2.953x in units and **1.866x in
seconds**, and the error is not uniform — the `nvar` sweeps that were removed
were expensive per unit and the `nrow` sweeps nearly free, and both are billed
at 1. Every percentage below is a share of *billed* work, and the LU rows have
the most real work hiding behind them.

### Where the work is

Attributed by source line. **Two different answers, and Kennington is 55% of
the two totals.**

| | standard 94 | Kennington 16 |
|---|---|---|
| inside the LU | **77.80%** | 4.97% |
| inside the simplex | 22.20% | **95.03%** |

Read every total as a statement about three models (D46): `pilot87` is 38.8%
of the standard set and `maros-r7` 35.4% — 74.1% between them; `gosh` alone is
91.9% of the infeasible set. **A change that does not move every instance the
same way is reported as a geometric mean of per-instance ratios**, never as a
sum.

### What is left, ranked

1. **The factorization, and the scatter its factors cost every solve** —
   77.8% of the standard set. **Opened and partly paid (D58).** The profile
   says three quarters of `maros-r7` is `jm_lu_factor` plus the array append
   it calls, and that the one line billing a work unit is 2.9% of it. Asking
   for capacity once per column instead of once per entry bought **1.281x on
   `maros-r7` and 1.169x on `pilot87`** — the two instances that are 74.1% of
   the set's work — with all 139 digests unmoved.

   **The scatter is gone too (D59).** It was 42% of the program against 6.5%
   for the subtraction it carried; the multipliers are scattered once per
   pivot instead and each column is walked once in place. `maros-r7` is
   **1.569x** and `pilot87` **1.214x** against where this session started,
   with every digest, iteration count and work unit unmoved.

   What is left here: the stale live counts that make Markowitz choose on a
   pessimistic estimate, and the fill itself — the factors carry 2.673x the
   nonzeros of the basis, two thirds of every factorization is free
   triangularization and 31.8% is where Markowitz actually chooses (D46).
   Beyond those two the remaining structure is inherent to eliminating
   right-looking, and going further means left-looking, which is a rewrite
   and needs its own decision.

   **Struck off, both by measurement.** The missing row-to-position lookup:
   `compact_pivot_row` scans a column to find one row's entry, which is
   O(r·c) per pivot and looked like the obvious defect — it is under 0.5% on
   `maros-r7`, the instance built to expose it (D58). And the per-column
   arrays against a single arena: allocation is 0.73% and `_int_malloc`
   0.30% on the same profile. The arena's remaining argument is locality,
   which an instruction profile cannot see; that needs a cache simulation
   before it is either costed or dropped.

2. ~~**Whatever `fit2p` spends that nobody bills.**~~ **Closed (D55, D56),
   and it was two things.** A capacity check that could not be inlined
   across a translation unit, and an elimination that rebuilt every column
   of every pivot row even when the pivot column had nothing to eliminate —
   97% of `fit2p`'s pivots. Together, 12.87 s to 2.46 s in the shipping
   build and 17.5x to 5.1x per iteration against HiGHS. Neither billed a
   single work unit, which is why a profiler found them and no internal
   instrument ever had. **The method that worked: profile at the flags being
   timed, and attribute by source line.**
3. **Partial and multiple pricing** — and it is now the top of this list for
   the set figure, not the third item. The instance that decides the gap is
   the ordinary one, and on `truss` the LU is 1.55% while the two dense
   sweeps are 36.5% (D61). Both alternatives to pricing fewer variables have
   been measured and refused: the pattern is not sparse enough to walk, and
   inlining the helpers is slower. **The first change that cannot be judged
   on digests** — it moves the search path, so it needs the full gate and a
   different standard of evidence.
4. **`stocfor3` is a memory-traffic instance, and it is the fourth worst in
   the set.** 6.79x per iteration on 0.97x the iterations, never profiled
   until now. Where it goes: the triangular solves 43.0%, and **`memset` plus
   `memcpy` plus `malloc` 18.8%** — against 11.3% for `dfl001` as a control.
   That is the hyper-sparse case the pricing row's own comment already
   names for `ken-13`: a model that puts a few hundred numbers into tens of
   thousands of slots, where clearing and copying the dense vectors is larger
   than the arithmetic. Measured and left; the repair is to keep the sparse
   results sparse downstream rather than to make the clears faster.

5. **BTRAN's `L'` pass**, 5.15% of the standard set, billed for every slot.
   Only 4.1% of its entries sit under a zero, so a reachability search over a
   row-wise copy of L can recover at most a fifth of a percent. Recorded so
   it is not costed again.
5. **The eta passes** apply 45.1% and 10.6% of their etas to a zero and are
   charged for all of them — 1.69% of the standard set together.
6. **Devex pricing as an alternative to the exact recurrence** — the cure D63
   points at for the iteration-count half of the tail. Its weights are
   approximate by construction and reinitialised by design, so a basis that
   destroys an exact recurrence does not degrade it the same way, and it
   drops the second FTRAN per iteration [7]. Changes the search path: full
   gate, and iteration count and per-iteration cost reported separately,
   because trading one for the other is exactly what it does.

7. **A primal simplex**, which phase 4's crossover needs anyway.

---

## Phase 7 — The long ones

Barrier for LP with crossover; deterministic parallel branch and bound with
`jaos_thread.h` (D13, D8); MILP correctness on the MIPLIB 2017 easy subset,
then cuts, MIP presolve and primal heuristics against the benchmark subset.
Then convex QP and MIQP over the barrier machinery, then conic, then NLP —
whose derivative strategy is that stage's opening decision (Q5) — then MINLP.
Network specializations slot in as detection plus dedicated algorithms; they
accelerate, they do not gate. SDP stays unscheduled.

---

## Known defects, carried

Reproducible, diagnosed, not yet fixed. The first three came out of varying
`REFACTOR_EVERY` over 16..256, which walks trajectories the gate never walks;
the fourth out of a warm start, which reaches a *state* the gate never
reaches. Both are the same lesson about where the defects that 139 instances
at one setting do not find are hiding.

1. **The checker certifies a bound it cannot prove (D47).** Whenever a
   wrong-signed multiplier sits on an unbounded improving direction the dual
   objective is minus infinity and the term is silently dropped, so
   `gap_positive` — documented as satisfying `P - P* <= gap_positive` — can
   read zero on a point that is arbitrarily suboptimal. Two variables and one
   constraint are enough to build it. Live on `etamacro` today at 2.25e-07.
   The cheap repair, judging a reduced cost against its own dot-product
   traffic, is measured and refuted: it separates nothing.
2. **The re-entry loop does not always converge (D49, D50, D51).** Its two
   repairs undo each other — the dual simplex borrows cost shifts to keep
   dual feasibility, `settle_shifts` calls the loans in, and the largest loan
   reappears as the worst breach, to six digits. `SETTLE_ROUNDS = 32` is what
   ends it rather than any condition. The question is whether a cleanup pivot
   needs to borrow at all.
3. **`pilot87` trips the iteration guard at intervals of 128 and above**,
   which the solver's own message calls a JAOS defect. Undiagnosed.
4. **A nonbasic free variable with a negative reduced cost is invisible
   (D68).** `can_move` has nowhere to send a free variable and returns false;
   `wants_a_pivot` computes its wrong-way direction as `status == AT_LOWER ?
   -d : d`, which reads a free nonbasic as sitting at an upper bound, so a
   positive reduced cost is repaired and a negative one is not;
   `primal_ratio_test` takes the same branch and would move it the wrong way
   if it got there. The point is then published as OPTIMAL when it is not.
   Two variables and one row are enough to build it. Both existing producers
   of free nonbasics can reach it — `build_initial_basis` for a zero-cost
   unbounded column, `repair_singular_basis` for an evicted one — and warm
   re-solve declines to become a third rather than fixing it, because the
   repair is a primal step and belongs with phase 6 item 7. Not observed on
   any of the 139 instances; found by construction.

---

## Settled — do not re-derive

Each was measured and closed; the measurement is in `DECISIONS.md`.

| | |
|---|---|
| `REFACTOR_EVERY` = 64 | swept 16..256; one of only two completely clean values (D39) |
| `DSE_DRIFT` = 10 | swept 2..disabled and bounded on both sides: 2 returns `greenbea` INFEASIBLE, 100 costs `grow22` 7.2x, above that `pilot` loses its answer. The interior is one value wide (D63) |
| Restarting the weights to the exact one rather than to 1.0 | refused: it is the right scale and it does buy `pilot` 0.76x, `greenbea` 0.79x and `grow15` 0.09x — and costs `grow22` **13.88x**. Third attempt to keep more weight information, third time `grow22` pays for it (D63) |
| `PIVOT_SEARCH_LIMIT` = 4 | swept 1..32 on two sets; above two the fill moves within 1.2% while totals swing 60% on trajectory alone (D46) |
| `SPARSE_ALPHA_DEN` = 4, `SPARSE_RHO_DEN` = 4 | plateaus bounded on both sides by measurement (D40, D41, D43); confirmed again from the other direction — the pricing row's pattern covers 83% of the variables on `truss` and 85% on `pilot87`, so there is no sparsity the threshold is refusing (D61) |
| Forcing the two dense sweeps' helpers inline | refused: 470M calls removed and it is **slower**, 0.997x, losing on every instance that matters. The instructions are in the work, not in the call (D61) |
| `SPARSE_COL_DEN` = 8 | the first constant whose value contradicts its own work-unit sweep, moved on a clock (D45) |
| A crash basis | refused: it destroys the exact starting steepest-edge weights `B = -I` gives, and exact weights for an arbitrary basis cost one solve per row |
| Filtering basic columns out of the pricing sweep | refused: 3.92 memory accesses against 4, with worse locality (D35) |
| The quadratic slot detachment in a basis update | refused: a billion integer comparisons against 59.5e9 billed units, 0.02% on Kennington |
| Caching `col_max_abs` | refused on its premise: `row_done` retires entries without the column being written, so the cache would be stale (D46) |
| A scatter-form BTRAN | refused: it reorders a cancelling sum, and two feasible models came back INFEASIBLE (D36) |

---

## Method worth keeping

- **Attribute by source line, not by named phase.** One edit to one inline
  function routes every work unit through its `__FILE__` and `__LINE__`. No
  region boundaries to draw and therefore none to draw wrongly.
- **Make the attribution validate itself.** Per-line sums must reconstruct
  each solve's total and the totals must reconstruct the committed baseline,
  both checked before the numbers are read.
- **Re-attribute after every entry that lands.** A ranking three changes stale
  describes a solver that no longer exists.
- **Sweep the trajectory, not just the instances.** Varying a parameter that
  must not change any verdict, and requiring the gate to hold across the
  range, measures how much margin the gate passes with. It costs minutes with
  the parallel runner and it has found three defects that 139 instances at one
  setting did not.
- **Report a geometric mean of per-instance ratios**, not a sum over the set.
- **A green result is not a proof.** When changing a checker or a predicate,
  build the case it must reject and confirm that it does.
- **Measure before repairing.** Every failure in this project that looked like
  a tolerance turned out to be something else.

---

## Open questions

- **Q2 — LP and MPS dialect edge semantics.** Fixed as encountered, recorded
  in `docs/format-support.md`. One is closed: an `RHS` entry on the objective
  row sets a constant, JAOS negates it as CPLEX documents, and the published
  Netlib optima omit it — so the gate carries the constant in its manifest.
  The next such case will look the same: an instance disagreeing with a
  reference is not evidence about which of them is wrong.
- **Q5 — NLP derivative strategy** (AD, finite differences, user-supplied).
  Decided when phase 7 reaches NLP; it shapes that engine's public API.
- **Q8 — how exact verification gets done.** GMP is the obvious tool and D11
  excludes it. The alternatives to weigh: iterative refinement, interval
  arithmetic in plain `double`, or hand-rolled rationals used only to verify a
  final basis.
- **Q11 — build targets for shipping. Closed (D62).** There is one set, not
  two: `make` builds `-O3 -flto -g -DNDEBUG` and `make pgo` rebuilds it from
  a profile. Measured over the whole standard set with every digest unmoved —
  `-O3` 1.0055x, `-flto` 1.0330x, `-march=native` 1.0072x over LTO, **PGO
  1.1122x**. `native` survives as `NATIVE=1` and is documented as
  measured-and-it-did-not-pay; it also makes the archive undistributable.
  Removing the work counter and the clock check was measured at the same time:
  0.987x and 1.004x, both inside the noise, so the public budget API is free.

  **Still open from this question, and it is a code change rather than a
  flag**: `restrict` on the kernel pointers, safe only if the non-aliasing
  claim is true — which is the part to establish first. `--gc-sections` and
  `-fno-math-errno` were never measured and are now unlikely to matter, since
  the flags between them are worth 3%.

---

## Bibliography

Verified citations — each checked against its publisher or archive before
entering this list. Implementation works from these and their kin only (D12).

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
