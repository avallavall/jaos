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

Instances are independent and every recorded figure is an integer that does
not depend on the optimisation level, so the bench runner can be built
`-O3 -march=native -flto` and the instances solved concurrently. That takes
the standard set from about eight minutes to ninety seconds and Kennington
from thirty minutes to fifteen. Verified three times on different trees by
producing records identical byte for byte to the sequential `-O2` runner.

It has lived outside the tree since M2 opened. It becomes a make target.

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

**T0 is measured against two rivals (D52, D53), and re-measured after D55 and
D56.** Against HiGHS 1.15.1, JAOS is **3.81x** slower per solve — 1.47x the
iterations and **2.60x** the cost of each. Against SoPlex 8.0.3 it is **1.36x**
slower on **0.70x** the iterations and **1.95x** per iteration, and faster on
10 of 22. The per-iteration ratio agrees between the two rivals instance by
instance, which is what makes it a quantity rather than a quotient.

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

Nothing here is hard. All of it is missing.

- **An options API.** Every tolerance, threshold and interval in the solver
  is a compile-time constant today. The shape is the first decision: an
  opaque options object passed to `jaos_solve`, or setters on the model.
  Either way it must keep D8 — an option that changes an answer has to change
  it identically on every machine.
- **Logging.** The solver is silent. A verbosity level and a callback for the
  line, so a caller can route it.
- **Model modification** — add and delete rows and columns, change a bound, a
  cost, a coefficient.
- **Warm re-solve.** The dual simplex is the right method for it and none of
  the plumbing exists: no way to keep a basis across a modification, no way
  to hand one in.
- **`jaos_set_basis`**, whose read side is already `jaos_basis`.

Two things phase 2 unblocks that are worth naming: the debugging fallback to
max-infeasibility pricing has been waiting for somewhere to put a flag, and a
caller cannot vary the checker's tolerance today, which is what made D47's
diagnosis need a private driver.

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

**The target is a cheaper iteration, not fewer of them (D52, D53).** JAOS
takes 35% *fewer* iterations than SoPlex and 50% more than HiGHS, and is
slower than both — so the pricing rule, the ratio test and the pivot choice
are competitive. What costs **2.2x to 2.8x** is each iteration, and the two
rivals agree on that number instance by instance, which makes it a property
of JAOS rather than of a comparison.

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
   77.8% of the standard set, untouched through all of M2, and now confirmed
   from outside: `maros-r7` bills 50x the median units per iteration and is
   40x slower than HiGHS. The factors carry 2.673x the nonzeros of the basis;
   two thirds of every factorization is free triangularization and 31.8% is
   where Markowitz actually chooses (D46). Live structural items: the missing
   row-to-position lookup on the factorization path, the per-column
   elimination arrays against a single arena, and the stale live counts that
   make Markowitz choose on a pessimistic estimate.

2. ~~**Whatever `fit2p` spends that nobody bills.**~~ **Closed (D55, D56),
   and it was two things.** A capacity check that could not be inlined
   across a translation unit, and an elimination that rebuilt every column
   of every pivot row even when the pivot column had nothing to eliminate —
   97% of `fit2p`'s pivots. Together, 12.87 s to 2.46 s in the shipping
   build and 17.5x to 5.1x per iteration against HiGHS. Neither billed a
   single work unit, which is why a profiler found them and no internal
   instrument ever had. **The method that worked: profile at the flags being
   timed, and attribute by source line.**
3. **Partial and multiple pricing** — the row scan that picks the
   infeasibility is 26.4% of Kennington and has no sparsity to exploit.
   **The first change that cannot be judged on digests**: it moves the search
   path, so it needs the full gate and a different standard of evidence.
4. **BTRAN's `L'` pass**, 5.15% of the standard set, billed for every slot.
   Only 4.1% of its entries sit under a zero, so a reachability search over a
   row-wise copy of L can recover at most a fifth of a percent. Recorded so
   it is not costed again.
5. **The eta passes** apply 45.1% and 10.6% of their etas to a zero and are
   charged for all of them — 1.69% of the standard set together.
6. **A primal simplex**, which phase 4's crossover needs anyway.

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

Reproducible, diagnosed, not yet fixed. All three came out of varying
`REFACTOR_EVERY` over 16..256, which walks trajectories the gate never walks.

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

---

## Settled — do not re-derive

Each was measured and closed; the measurement is in `DECISIONS.md`.

| | |
|---|---|
| `REFACTOR_EVERY` = 64 | swept 16..256; one of only two completely clean values (D39) |
| `PIVOT_SEARCH_LIMIT` = 4 | swept 1..32 on two sets; above two the fill moves within 1.2% while totals swing 60% on trajectory alone (D46) |
| `SPARSE_ALPHA_DEN` = 4, `SPARSE_RHO_DEN` = 4 | plateaus bounded on both sides by measurement (D40, D41, D43) |
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
- **Q11 — build targets for shipping**, `release` and `native`. Candidates for
  `native`: `-O3`, `-flto`, `-march=native`, `-fno-math-errno`,
  `--gc-sections`, and PGO with `make netlib` as the profiling load — 139 real
  models are already the representative workload, so none has to be invented.
  A separate and probably larger win is `restrict` on the kernel pointers,
  which is a code change and only safe if the non-aliasing claim is true.
  Settled while it was raised: `-g` costs nothing at run time, and the
  hardening flags must not be dropped from the readers, which parse untrusted
  input.

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
