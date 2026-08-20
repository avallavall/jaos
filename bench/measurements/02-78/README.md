# 02-78 — the simplex loses a row's small terms behind a large one

D168. One source change, in `src/simplex.c`: the right-hand side
`compute_primal` builds is accumulated with Neumaier compensation.

## The defect

`x_B = -B^-1 (N x_N)`. The vector `-N x_N` is built by walking every nonbasic
variable in column order and adding its entries into the rows it touches, so a
row is a slot that many columns write into and the order is the column order.
A row that meets a large term before many small ones loses the small ones
outright: each is below half an ulp of the running total, so each addition
returns the total unchanged and the whole tail is dropped.

It is the same defect D165 removed from presolve's `cur_rl` / `cur_ru`, one
layer out. D162 named it (`bench/measurements/02-72/`, section 6) and could not
close it: that record's own oracle refused its model, and the reason was this.

## The reading — `lost-terms.txt`

D162's model, unchanged, at four removal counts. Its feasible point is exactly
representable: `x0 = 1e9`, `x1 = -1e9`, every small at `2^-25`, `w1 = 1e-7`,
`w2 = 0` make the activity exactly what the row asks for. Every value in it is
a dyadic rational a double holds without rounding.

| | k = 64 | 128 | 256 | 512 |
|---|---|---|---|---|
| reference build, parent `f3a7798` | **infeasible** | **infeasible** | **infeasible** | **infeasible** |
| reference build, compensated | optimal | optimal | optimal | optimal |
| shipping build, compensated | optimal | optimal | optimal | optimal |

**B, the control, reads `infeasible` on every build at every count.** It is the
same shape moved 1e-2 away from any feasible point, four orders of magnitude
outside anything the accumulation could account for. A repair that accepted it
would be a wider window rather than a more accurate sum.

**`-DJAOS_NO_PRESOLVE` is where the defect is visible and that is not an
accident of the model.** Presolve removes the fixed columns before the simplex
sees them, and since D165 it subtracts them with the residue kept, so the row
the shipping build hands over is the row the model has. The reference build
hands the whole model to the simplex. The oracle every presolve entry in this
directory is judged against was the build answering wrong.

**The published objective on this model is still not pinned, for D162's
reason.** The reference build reads `1.19209e-07` where exact arithmetic gives
`1e-07`: `x1` sits near `-1e9`, where one ulp is `2^-23 = 1.19e-07`, so the
last step of the ratio test cannot be taken on the grid the model lives on.
The test asserts OPTIMAL and not the objective.

## Where it stops being feasible — `controls.txt`

The same model swept over the slack, on both builds at both trees. It is what
settled the two claims the review made about the tests, and one of them was
wrong.

| slack | shipping, parent | shipping, D168 | reference, parent | reference, D168 |
|---|---|---|---|---|
| 0 | 1.0000000000000074e-07 | 1.0000000000000074e-07 | **infeasible** | 1.1920928955078125e-07 |
| 1e-8 | 1.1000000000000064e-07 | same | **infeasible** | 1.1920928955078125e-07 |
| 1e-7 | 2.0000000000000147e-07 | same | **infeasible** | 1.9999999999999999e-07 |
| **5e-6** | infeasible | infeasible | infeasible | infeasible |
| 1e-5 | infeasible | infeasible | infeasible | infeasible |
| 1e-2 | infeasible | infeasible | infeasible | infeasible |

**The objective is not a single number at slack 0.** The review said every
feasible point has `obj = 1e-7` exactly; `x1` is a variable, so the two builds
land one ulp of `x1`'s magnitude apart — `2^-23 = 1.19e-07`. The test's window
is 5e-8 around 1.1e-7 and admits both.

**5e-6 is a real control and 1e-2 is not.** At 5e-6 the model is infeasible by
about 4.7e-6, which `w1 + w2` cannot reach against its cap of 4e-7, and it is
inside a window widened to cover the 7.63e-6 that was being lost. Every build
at every tree refuses it.

## The gate — `gate-diff.txt`, `work-geomean.txt`

`make netlib netlib-infeas netlib-kennington J=12`, all three `gate: PASS`
with `0 regressed, 0 improved, 0 new`, 139 of 139 checker `ok`.

| set | instances | bit-identical | moved | digests |
|---|---|---|---|---|
| netlib | 94 | 69 | 25 | 23 |
| netlib-infeas | 29 | **29** | 0 | 0 |
| netlib-kennington | 16 | **16** | 0 | 0 |

Work, netlib: **geometric mean 0.9996x**, best `pilotnov` 0.9096x, worst
`pilot87` 1.0372x. The ratio of totals is 1.0216x and is not the result (D46).

Five instances changed trajectory rather than only values: `pilot87`
40246 → 41281 iterations, `pilotnov` 2374 → 2390, `pilot-we` 4172 → 4178,
`pilot-ja` 1402 → 1371, `stocfor2` unchanged in iterations with work
16778110 → 16758260.

**The residual figures move both ways and they are not the evidence.** Over
netlib's 94, comparing each figure against the parent record:

| figure | better | worse | worst move |
|---|---|---|---|
| `rsub` | 8 | 2 | `pilot-we` 8.73e-14 → 1.49e-13 |
| `row` | 7 | 8 | `fit1d` 1.32e-14 → 5.86e-14 |
| `rowrel` | 9 | 7 | `grow7` 1.86e-16 → 1.62e-15 |
| `gap` | 12 | 6 | `greenbea` 7.12e-17 → 1.39e-16 |
| `dual` | 0 | 0 | — |

Two of them are worth naming because they are large and in the right
direction: `pilot-ja` 6.03e-12 → 1.62e-14 and `pilotnov` 8.16e-13 → 5.21e-14.
Everything else is a few ulps in one direction or the other, which is what a
changed summation order does to a converged point. **A more accurate sum is
not a smaller residual on every instance and nothing here claims it is.** What
the gate says is that no verdict moved on any of the 139.

## The published basis — `basis-count.txt`

Six netlib instances' `basis=` hash moved (`80bau3b`, `pilot-ja`, `pilot-we`,
`pilot87`, `pilotnov`, `stocfor2`), and the gate reports a hash and never a
count. D167 is what happens when that is assumed away, so 02-48's probe was
re-run on the whole working tree rather than on its `presolve.c` alone.

| tree | exact | **WRONG** | worst over | sum |
|---|---|---|---|---|
| `f3a7798`, D168's parent (D167) | 142 | **46** | +18 | +250 |
| D168 | 142 | **46** | +18 | **+248** |

Kennington reads `exact=32 WRONG=0` on both. **By the measure `TODO.md` insists
on — the count of solves publishing a wrong basis — nothing moved.** The sum
falls by 2 and that is not progress on the item.

## The seconds — `timing.txt`

The Neumaier step is arithmetic `jm_work_add` does not bill, so this is the
case the work counter cannot see and a time ratio is the only evidence there
is. Four of the six instances are bit-identical on the gate, so their ratio is
the arithmetic alone and nothing else.

The protocol was run twice, and running it twice is the finding:

| instance | run 1 | run 2 |
|---|---|---|
| `maros-r7` (bit-identical) | 0.9642x | 1.0302x |
| `truss` (bit-identical) | 0.9501x | 0.9741x |
| `degen3` (bit-identical) | 1.0282x | 1.0097x |
| `dfl001` (bit-identical) | 1.0061x | 0.9938x |
| `pilot87` (+3.72% work) | 1.0353x | 1.0528x |
| `pilotnov` (−9.04% work) | 0.9807x | 1.0334x |
| **geometric mean** | **0.9936x** | **1.0153x** |

`timing.txt` is run 2, the one the script wrote. The four bit-identical
instances span 0.9501x to 1.0302x across the two runs while doing byte for
byte the same work, which is this host's 6.27% repeatability (D93) and not a
cost. **The added arithmetic is not measurable here.** `pilot87` is above the
band in both runs and it moved 3.72% in work, which is the explanation the
counter already gives.

## The test, and what makes it evidence

`test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total` in
`tests/test_simplex.c` is D162's model at k = 256, with no build guard —
OPTIMAL is the right answer in every configuration.

**It pins the objective as well as the status, and the reviewer's figure for
that objective was wrong.** The review said every feasible point has
`obj = 1e-7` exactly. `x1` is a variable, so it does not: the shipping build
publishes 1.0000000000000074e-07 and `-DJAOS_NO_PRESOLVE` publishes
1.1920928955078125e-07, which is 2^-23 — one ulp of `x1`'s own magnitude. The
window is 5e-8 around 1.1e-7, which admits both and rejects 0, 2e-7 and 4e-7.
The concern behind the figure was right: 02-72 records this same model
publishing `obj = 4e-07` on an earlier tree and 0 at three of four counts
before D165, so a repair reporting OPTIMAL with a wrong objective would have
passed a status assertion.

**The pin's rejecting case is measured rather than argued.** The same model at
`slack = 1e-7` publishes 2.0000000000000147e-07 on the shipping build, which
the window refuses — so it discriminates on a real reading of this model.

`test_a_row_activity_still_refuses_a_real_shortfall` carries **two** controls,
and it **runs under both fault builds on purpose**. The convention guards
positive tests off there; this one asserts a refusal, and a fault that made
presolve accept either model would be worth hearing about. Row S becomes a
singleton row on `x1` once `z` is removed, which is the path
`JAOS_PRESOLVE_FAULT_WRONGDUAL` perturbs, so the coverage is real.
1e-2 is D162's inherited one and separates nothing here, being 1300 times the
7.63e-6 recovered — any repair that widened a window by up to a hundredth would
pass it. **5e-6 is the one that separates an accurate sum from a wider
window**: the model is infeasible by 4.7e-6 there, which is 47 times
`PRIMAL_TOL` and INSIDE a window widened to cover the loss, so a widening
repair accepts it and an accurate sum refuses it. Measured at both trees and on
both builds, and INFEASIBLE on all of them.

Validated the way this repository requires, by building the parent's
`src/simplex.c` against the new tests:

| | shipping build | `-DJAOS_NO_PRESOLVE` |
|---|---|---|
| parent `f3a7798` | passes | **FAIL: Expected 1 Was 2** |
| compensated | passes | passes |

The control passes on all four. A test that only ever passes is not evidence,
and this one fails on exactly the tree and the configuration the defect is in.

`make configs` exits 0 — all five configurations build and pass.

## Three readings this record must not be quoted against

**`0 improved` is a predicate count and not an accuracy measure.** The sets say
there is **no harm**; the constructed model says there is a **benefit**; and no
instance of the 139 demonstrates the benefit.

**The work counter cannot see this change, by construction.** The fold pass
bills nothing, while the identical-shape `nrow` loop seventeen lines later
bills `s->nrow * JM_WORK_NONZERO`, and the inner loop went from two operations
per nonzero to about eight at the same billing. So `pilot87`'s 1.0372x and the
0.9996x geometric mean are trajectory, not arithmetic. `timing.txt` is what
measures the arithmetic.

**`refresh()`'s own comment records D29 measuring this lever, and it fired
here.** A more accurate `x_B` mid-solve feeds a different steepest-edge pick.
D29 measured refining every refresh and got `pilot-ja` back INFEASIBLE and
`pilot87` at 4.5x. Here `pilot87` moved 1.0372x with +1035 iterations and
`pilot-ja` did not regress — a different mechanism at two orders of magnitude
less, and the variant of putting the fold behind `if (refine)` is not taken
because the campaign says it is not needed.

## What this does not close

- **`subtract_basis_times` and `apply_flips` are still uncompensated sums, and
  they are refused rather than deferred** (`numerics-reviewer`). Compensation
  buys much less there, and the reason is the terms rather than the schedule:
  on the model above every product is exact — the coefficient is 1.0 and the
  value is a power of two — so the accumulation was the entire error and
  compensation recovered all of it. Those two sum products of `x_B`, an FTRAN
  output already carrying the factorization's error, and no accumulator reaches
  an error that is already inside a term. **Reopen condition: a model where the
  refinement residual loses a term that changes a published value.**
  `subtract_basis_times` is on the `refine` path, so it publishes; `apply_flips`
  loses terms mid-solve only, since the final `refine = true` refresh rebuilds
  `x_B` from scratch. If either lands it gets its own `[nrow]` array and never
  `s->rhsc`.
- **`update_primal` carries `x_B` between refreshes incrementally** and is
  untouched. A refresh resets it, so its error does not accumulate past one
  refactorization interval.
- **The row activity `src/check.c` computes is `long double` and was never in
  question** — D34 confines that type to the checker, which is why presolve and
  now the simplex both use Neumaier instead.
