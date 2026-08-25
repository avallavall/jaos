# 02-103 — the primal phase 1, and the loan of exactly 1.0 that gave it away

2026-08-25. `TODO.md` section 0 stage 4.

> **SUPERSEDED IN PART BY D191.** Every count in this file was measured with
> the `pivot()` half of the `in_primal` guard missing, which this file wrongly
> states was applied. The narrative and the diagnosis stand; the counts do not.
> The honest figures are 54 agreeing, 31 disagreeing, 8 overrun, 1 error, at a
> work geomean of 3.8332x.

The primal's reach goes from **0 of 94** to **64 of 94 agreeing with the
dual**, and what is left is measured rather than guessed.

## What landed

A composite phase 1 (Maros 1986 in short-step form): the phase-1 objective is
the sum of the basics' bound violations, `-1` below a declared bound and `+1`
above, and minimising it is phase 1. **No artificial variables and no second
model** — it works on whatever basis it is given, which is the property
crossover needs and the textbook method cannot offer.

The duals come from `compute_duals` with `s->cost` pointed at the phase-1 cost
vector for the length of one call. That reuse is exact: `compute_duals` reads
`cost`, writes `y` and `d`, and does nothing else.

## The defect it shipped with, and the number that found it

`sc50a`: the primal stopped after **two pivots** and published an objective of
-57.2 against a true -64.6, refused by the D146 guard as `NUMERICAL_ERROR` on a
model the dual solves in 47 iterations.

Two hypotheses were tried and **both were refuted by measurement**:

- **Tiny pivots degrading the factorization.** The minimum-pivot floor in the
  primal ratio test was swept over `1e-9`, `1e-7`, `1e-5`, `1e-3`. Every one
  gave **identical** output — 40 iterations, 183481 work units. The floor never
  binds, because the primal only takes two pivots before stopping.
- **Cost shifting in `pivot()`.** Guarding the `shift_to_feasible` call at the
  tail of `pivot()` changed nothing: same 40 iterations, same work.

The instrument that settled it prints what `primal_price` sees when it declares
optimality:

```
PRICE-STOP iters=2 maxscaled=0 maxpub=0 borrowed=1 total=1
```

**Every reduced cost is feasible, in both spaces, and one variable is carrying
a loan of exactly 1.0.** A loan of exactly one is not a repair —
`shift_to_feasible` lends the minimum a sign condition needs. It is the size of
a **phase-1 cost**.

## The mechanism

Phase 1 puts *its own* reduced costs in `d`: gradients of a sum of violations,
so of magnitude one. `pivot()` then lends `cost[v]` whatever those say is
needed, against the **model's** cost vector. Phase 1 was corrupting the
objective phase 2 would go on to optimise.

**`update_dual` is the site that did the visible damage, and `pivot()`'s own
call lends too.** Guarding one and not the other changed nothing measurable
here — but **only `update_dual` was actually guarded when this was written**,
and this file said both were. `/code-review max` caught it the same day: see
D191, which corrects the counts below.

`sc50a` moves from 40 iterations and 183481 units to 51 and 58062 — a third of
the work and a different trajectory — and still ends in `NUMERICAL_ERROR`. So
the loan was real and it was not the only thing wrong.

## Where the primal stands, over the standard set

`build/bench/primal -j 12`, bounded at 10x the dual's work per instance:

```
measured 64, skipped 0, unreached 0, overrun 16, disagreed 12, rejected 0,
errors 2
iterations (primal+1)/(dual+1), geometric mean: 1.7710
work units primal/dual, geometric mean:         3.2352
work ratio, best  grow22 at 0.0406
work ratio, worst israel at 12.5090
took more iterations primal than dual:          58 of 64
```

**CORRECTED BY D191 — the figure below is 64 and the honest one is 54.** What
follows was measured with the `pivot()` guard missing, so the primal was
reaching optimality on some models only because a phase-1 loan had perturbed
the objective.

**64 instances agree with the dual**, objectives within tolerance and both
answers through the independent checker. At 3.24x the dual's work, which is
what Dantzig pricing costs and is why Devex is stage 5.

**16 overrun** the 10x budget. Expected, and a bound rather than a failure.

**12 disagree and 2 error**, all of them the primal returning
`NUMERICAL_ERROR` where the dual reaches an optimum:

```
25fv47  cycle  d2q06c  greenbeb  modszk1  pilot  sc105  sc205
sc50a   sc50b  stocfor3  truss        (+ pilot87, pilotnov as errors)
```

**`sc50a`, `sc50b`, `sc105`, `sc205` are one family** and are the thread to
pull: four instances of the same generator failing the same way is one
mechanism, not four.

## Gate

`make configs`: all 5 configurations build and pass. All three sets
`0 regressed, 0 improved, 0 new`, and `git diff --stat bench/results/` empty.
Nothing in the gate can enter a primal path, so this is what it must say.
