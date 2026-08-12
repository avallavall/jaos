---
name: sparse-simplex-perf
description: Where the real speed lives in a revised simplex implementation — hyper-sparsity, LU factorization and update strategy, pricing rules, ratio tests, crash bases and presolve. Load when planning or reviewing performance work on an LP solver, before reaching for micro-optimisation. These are factor-of-N algorithmic wins, not percentages, and each carries the published source it comes from.
---

# Making a revised simplex fast

The order below is the order of payoff. Everything in the first two sections
outweighs everything a profiler will point at in a hot loop.

**Not this skill, once the algorithm is settled.** `c-perf` owns the C-level
work — layout, branches, allocation, compiler flags — and the reproducibility
exclusions that constrain all of it. Come back here first, though: a profiler
pointing at a hot loop is usually pointing at a loop that should not be
running.

## Hyper-sparsity is the largest single win

In a revised simplex the dominant cost is the four solves per iteration:
FTRAN of the entering column, BTRAN of the pricing row, and the two the
steepest-edge recurrence needs. On large sparse models the *result* of these
solves is itself extremely sparse — often a fraction of a percent of the
dimension — while a straightforward implementation walks the whole vector
anyway.

Exploiting that is the difference between a toy and a solver, and on the
large Netlib and Kennington-scale models it is worth an order of magnitude,
not a percentage.

The technique (Hall & McKinnon, *Hyper-sparsity in the revised simplex method
and how to exploit it*, COAP 32(3), 2005):

- **Predict the nonzero pattern before computing values.** The pattern of the
  solution of a triangular solve with a sparse right-hand side is the set of
  nodes reachable from the RHS nonzeros in the dependency graph of the
  triangular factor. A depth-first search over that graph gives the pattern
  in time proportional to the *result's* size, not the dimension. This is the
  Gilbert–Peierls idea and it is what makes the rest possible.
- **Then solve only over that pattern**, in a topological order the DFS
  already produced.
- **Switch on the density.** Below a threshold the sparse path wins; above
  it, the dense loop wins because it has no indirection. Real implementations
  carry both and choose per solve. That threshold is a number and therefore
  needs measurements on both sides of it.
- **Keep the result sparse downstream.** A sparse FTRAN result feeding a
  dense update loop throws away what you just bought. The pricing update, the
  basis update and the weight recurrence all need sparse-aware forms.

The bookkeeping — a touched-list scatter, a stamp array, a stack for the DFS
— should be one shared mechanism, not one per caller.

## Factorization and update strategy

- **Markowitz threshold pivoting** (Markowitz 1957; Suhl & Suhl, *Computing
  sparse LU factorizations for large-scale linear programming bases*, ORSA
  JoC 2(4), 1990) trades numerical safety against fill. The threshold `u`
  bounds the pivot against the largest entry in its column: small `u` gives
  sparser factors and worse stability. It is a genuine trade with a real
  optimum, and it moves with the model.
- **Triangularization first.** Before any elimination, repeatedly peel off
  singleton rows and columns. On simplex bases a large fraction of the matrix
  is triangular by inspection — logicals are unit columns — and peeling them
  costs nothing and removes them from the Markowitz search entirely. Skipping
  this is one of the most common reasons a hand-written LU is slow.
- **Forrest–Tomlin updates** (Forrest & Tomlin, *Updated triangular factors of
  the basis to maintain sparsity in the product form simplex method*, Math.
  Prog. 2(1), 1972) keep the factors sparse across iterations, unlike the
  product-form inverse which appends an eta vector per iteration and grows
  without bound.
- **Refactorization interval is a trade, not a constant.** Updates accumulate
  both fill and error, so solves get slower and less accurate as the interval
  runs. Refactorizing costs a fixed lump. The optimum balances the two and
  depends on the model's fill behaviour; a good implementation adapts it from
  the observed cost of a solve rather than fixing it.
- **Stability triggers beat a fixed interval.** Watching the residual of a
  solve and rebuilding when it degrades catches the cases the interval
  misses. Note carefully what such a trigger can and cannot do: it cures
  *drift in a patched factorization*. It does not cure the backward error of
  the triangular solves themselves on an ill-conditioned basis — for that the
  answer is one step of iterative refinement, and refactorizing more often
  will not help at all.

## Pricing

Pricing is the other half of the iteration cost and the main determinant of
the iteration *count*.

- **Dual steepest edge** (Forrest & Goldfarb, *Steepest-edge simplex
  algorithms for linear programming*, Math. Prog. 57, 1992) is what makes the
  dual simplex competitive. The exact recurrence needs a second solve per
  iteration; that cost is why approximations exist.
- **Devex** (Harris, *Pivot selection methods of the Devex LP code*, Math.
  Prog. 5, 1973) approximates the same weights without the extra solve,
  trading iteration count for per-iteration cost. Which wins is model
  dependent and is a measurement.
- **Partial and multiple pricing.** Scanning every candidate every iteration
  is O(n) per iteration with poor locality. Partial pricing scans a rotating
  slice; multiple pricing selects a small candidate list once and reuses it
  for several iterations, re-verifying cheaply. Both cut the dominant scan
  and both change the search path, so both need the full instance sets.
- **Weight drift.** Recurrence-carried weights degrade. Comparing the carried
  weight against an exactly-known one — the pricing row's own norm is
  available for free — detects it, and resetting costs only pricing quality.

## The ratio test

- **Harris two-pass** (same 1973 paper) allows a small bound violation in a
  first pass to admit a larger, safer pivot in the second. Larger pivots are
  the single cheapest stability improvement available.
- **Bound flipping** (the "long step" or BFRT) lets the step pass several
  breakpoints, flipping bounded variables as it goes, and can replace many
  short degenerate iterations with one long one. On models with many boxed
  variables this is a large win.
- **The breakpoint walk is a selection problem.** Walking candidates in
  ascending ratio order by re-scanning the survivors is O(k²) in the number
  of breakpoints passed; sorting once is O(k log k); a heap or a partial
  selection can be better still. Which wins depends on how many breakpoints a
  real model passes, and that distribution is measurable before choosing.
- **Anti-cycling.** A tolerance window makes ties arbitrary, and arbitrary
  ties at a degenerate vertex are how the method cycles. The reliable cure is
  a fallback that a *detected* cycle switches on — exact minimum ratio, no
  window, smallest index on both the entering and the leaving choice — rather
  than a rule that is always on, which is far too slow to be a default.

## Crash bases

Starting from the slack basis wastes iterations. A crash basis (Bixby,
*Implementing the simplex method: the initial basis*, ORSA JoC 4(3), 1992)
picks an initial basis that is nearly triangular and closer to feasible,
typically by preferring columns that are free, then boxed, then sparse, while
keeping the basis triangular by construction so the first factorization is
trivial. Worth a substantial fraction of total iterations on structured
models, and nearly nothing on some others.

## Presolve

Usually the largest single reduction in total work, because it removes
problem rather than processing it faster. The standard reductions:

- empty and singleton rows and columns
- fixed variables and forcing constraints
- redundant and implied bounds tightening
- duplicate rows and columns
- dominated columns

Two properties matter more than the list. **Postsolve must reconstruct a full
primal-dual solution and a valid basis**, or the answer cannot be checked or
warm-started — and building postsolve after the fact is much harder than
building it alongside. And **presolve must not change the answer**: every
reduction needs its inverse, and the independent checker must verify the
solution against the *original* model, never the reduced one.

## Warm starting

For branch and bound the dual simplex exists precisely because a bound change
leaves the basis dual feasible, so a re-solve continues rather than restarts.
Anything that destroys that property — a presolve that changes the basis
shape, a crash that discards it — costs far more in the tree than it saves at
the root.

## What to measure

Iteration count and work per iteration are different questions with different
cures, and a change that improves one usually worsens the other. Report both.
A pricing change that halves iterations while tripling per-iteration cost is
a regression, and only the per-instance numbers will say so.
