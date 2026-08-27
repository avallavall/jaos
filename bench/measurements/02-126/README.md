# 02-126 — stage 8d: why `pilot87`'s phase 1 diverges, and whether phase 1 should notice

## The question

`TODO.md` §0 stage 8d, opened by D208. `pilot87`'s phase-1 objective is a sum
of bound violations and must never rise. It falls to 1.24365e+12 at iteration
341000, turns at 342000, reaches 1.88282e+24 by 351000 and ends alternating
between two values. `dfl001` runs 136695 phase-1 iterations and its objective
peaks at its starting value, so this is not what long runs do. Two questions,
in this order: what happens between 341000 and 352000, and whether phase 1
should stop when its own objective rises rather than grind to a work limit.

## What the logged number is

`primal_phase1_costs` sums the violations of the **carried** `xb`, the values
`pivot` updates incrementally by `xb[i] -= theta_primal * col[i]`. `refresh`
rebuilds the factorization and recomputes `xb` from it: every 64 updates, and
whenever the Forrest-Tomlin update refuses a pivot. So the carried values can
drift for at most 64 pivots, and a rise that survives repeated recomputation
is a property of the bases being chosen and not of accumulated error.

## The first hypothesis, and its refutation — `unguarded-pivots.sh`

`pivot` takes its step as `theta_primal = (xb[r] - bound) / alpha_q`, the
BTRAN value, and moves every basic by `theta_primal * col[i]`, the FTRAN
value. The two are one number in exact arithmetic and are checked to
`LU_AGREE_TOL`, but only when `n_updates > 0`: on a freshly rebuilt
factorization the pivot is taken unchecked, so a refusal cannot loop (D86).
The diverging regime rebuilds constantly, and the pivot after every rebuild
is the unguarded one. If the two values disagreed there, the step would be
wrong by their ratio.

Counted over the whole run:

| | `dfl001` | `pilot87` |
|---|---|---|
| pivots taken in phase 1 | 136693 | 205845 |
| on a fresh factorization | 2112 | 6246 |
| of those, the two values disagreeing past `LU_AGREE_TOL` | **0** | 20 |
| first such pivot | none | iteration 306731 |
| worst disagreement among them | | 0.3%, step 4.3e+10, at 380054 |

The control has none. But the first one on `pilot87` is 35000 iterations
before the turn, the objective fell through that stretch, and twenty pivots
wrong by 0.3% do not take 1e+12 to 1e+24. **Refuted.** And at every pivot
behind a large rise in the window, the two values agree to between 1e-10 and
1e-16.

## What actually happens — `unguarded.txt`, `analyse.sh`

Every pivot in [340000, 352000], with the objective at the top of the loop.
For a pivot at iteration `k`, the phase-1 objective is piecewise linear and
`d_q * theta` is its predicted change if no basic changes status inside the
step; the actual change is the next top-of-loop total minus this one.

The turn, and the two clearest events after it:

| iteration | pivot element `alpha` | step `theta` | predicted | actual |
|---|---|---|---|---|
| **341234** | **3.26e-09** | 1.4e+06 | -1.7e+09 | **+3.4e+12** |
| 341656 | 2.45e-08 | 2.0e+06 | -2.8e+09 | +6.1e+09 |
| **344067** | **1.76e-09** | **3.3e-05** | ~0 | **+2.1e+11** |

The third line is the tell. A step of 3.3e-05 cannot move the objective by
2e+11 through the incremental update. Around each of these three pivots the
top-of-loop lines read:

| iteration | `n_updates` | `n_refactor` | objective |
|---|---|---|---|
| 341234 | 28 | 3092 | 1.226e+12 |
| 341235 | **0** | **3093** | **4.609e+12** |

The pivot on 3.26e-09 was taken; the Forrest-Tomlin update refused the new
diagonal, below `LU_UPDATE_TOL` times the spike's largest entry, and marked
the factorization unusable; the next loop pass rebuilt it and recomputed `xb`
**from the basis that now contains that pivot**, and the sum of violations
came back 3.4e+12 larger. Same three-line pattern at 341656 and 344067. At
341657 the next pivot on the rebuilt basis reads `alpha = -1.42e+08` with a
reduced cost of -1.98e+11: `B⁻¹` carries entries in the hundreds of
millions from that moment.

So the objective rises because the basis has gone nearly singular, and the
rise is what a recomputation from such a basis looks like. Bucketed by pivot
element over the window, the pivots whose actual change is more than ten
times away from the prediction and larger than 1e+6:

| `alpha` in | pivots | objective rose | broke the prediction |
|---|---|---|---|
| [1e-9, 1e-8) | 2 | 2 | 2 |
| [1e-8, 1e-7) | 2 | 2 | 1 |
| [1e-7, 1e-4) | 14 | 1 | 1 |
| [1e-4, 1e-1) | 675 | 4 | 3 |
| [1e-1, 1e2) | 3970 | 28 | 26 |

Every pivot below 1e-8 broke it. The breaks at ordinary pivot sizes all sit
after 344350, on a basis already ruined.

## Why here and not earlier — `tiny-pivots.sh`, `tiny-pivots.txt`

The primal ratio test takes the minimum ratio, ties on the basis index, and
admits any pivot element down to `PIVOT_MIN`. It has no preference for a
larger pivot among near-ties. The dual side does: `jm_harris_pick`'s second
pass *"returns the candidate with the largest pivot whose true, unwidened
quotient still fits in that step"*, and `TODO.md` §0 stage 2, the Harris
two-pass in primal form, is that same defence for this side, blocked on
nothing.

Was 341234 the first pivot that small? Over the whole run, by decade of the
pivot element:

| | `dfl001` | `pilot87` |
|---|---|---|
| pivots with an element below 1e-4 | **3** | **582** |
| in [1e-9, 1e-8) | 1 | **74** |
| first pivot in [1e-9, 1e-8) | iteration 15536 | iteration **18341**, objective 2.22818e+13 |

**No.** `pilot87` took its first 1e-9 pivot at iteration 18341 and ran
320000 more iterations with the objective falling, and took 73 more of them
on the way. A single tiny pivot is not the cause. They are the wear: 582
pivots on elements below 1e-4 against the control's 3, each one a step
towards a worse-conditioned basis, until at 341234 one more leaves a basis
whose recomputed `xb` no longer resembles the carried one. That is answer one.

## Question two — `relrise.sh`, `relrise.txt`

Should phase 1 stop when its objective rises? In exact arithmetic it never
rises, so the question is how large a rise floating point produces on solves
that are going to finish. For all 94 forced-primal solves, the largest
relative rise the phase-1 objective ever makes above its running minimum:

| instance | largest rise | at iteration | how the solve ended |
|---|---|---|---|
| `pilot87` | **8.43e+13** | 351201 | overrun, diverged |
| `pilot-ja` | **25.0** | 2091 | **`ok`**, agrees with the dual |
| `scsd8` | 8.94e-04 | 2635 | overrun |
| `perold` | 1.26e-05 | 562 | `ok` |
| `greenbeb` | 7.53e-06 | 1601 | disagree |
| 89 others | below 1e-06, and 82 of them below 1e-12 | | |

**`pilot-ja` rose twenty-five times above its best value at iteration 2091,
spent 316 iterations more than double it, came back, reached feasibility at
6252 and solved to the dual's answer.** A nearly singular basis is not a
one-way street. The objective rising is not a signal that the solve is lost.

**Refused.** A stop rule is a relative threshold, and this census puts the
only two candidates for its two sides at 25 and 8e+13, one instance each.
A constant fitted to one instance on each side is what `CLAUDE.md` names as
how this project loses weeks. What it would buy `pilot87` is a stop near
350000 instead of a work limit at 387235, on a solve that is lost either way.
The fix is the one that stops the wear: stage 2.

**Reopens when** no solve that ends `ok` rises above 1e-3 any more.
`relrise.sh` is its own re-test and returns the exit codes
`bench/refusals.txt` reads.

## What is concluded

- **Answer one.** The bases become nearly singular through hundreds of
  pivots on tiny elements, 582 below 1e-4 against the control's 3, which the
  primal ratio test has no rule against. The rise is `xb` recomputed from
  such a basis after the update refuses it. Stage 2, the Harris second pass
  in primal form, is the missing rule, and this directory is the reason it
  is next.
- **Answer two.** No stop rule. A solve that finishes rises 25x on the way.
- **Refuted on the way.** The unguarded pivot after a rebuild: the two pivot
  values agree at every pivot behind a rise, and the control has none.
