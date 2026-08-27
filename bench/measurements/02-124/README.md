# 02-124 — stage 8a: the `alpha[q]` test is doing stability duty, and the noise floor it is mistaken for is missing

## The question

`TODO.md` §0 stage 8a, raised by `numerics-reviewer` on D207's diff.

D207 gave the **column** side of the pivot test a floor relative to its own
scale. The **pricing row** side is still absolute: three sites test
`fabs(s->alpha[q]) < PIVOT_MIN` against 1e-9, in `primal_cleanup`, in phase 1
and in phase 2.

D207's constant cannot carry over. `alpha[q]` is `rho' M_q`, a dot product
over the pricing row, so the terms behind it are `rho_i * a_iq` and its
traffic is `sum_i |rho_i * a_iq|` — the shape `column_traffic` uses for a
reduced cost, with `rho` in place of `y`. `max|col|` is a different quantity
and says nothing about this one.

## The instrument — `alpha-census.sh`, `alpha-census.txt`

At each of the three sites, before the test:

```
r = |alpha[q]| / (DBL_EPSILON * sum_i |rho_i * a_iq|)
```

kept as a per-solve minimum and a log10 histogram, and separately for the
calls where the absolute test actually **fired**. A floor of
`C * DBL_EPSILON * traffic` changes a solve if and only if its minimum `r` is
below `C`, so one census sweeps every `C` — the rule that predicted 15 of 15
in 02-122. 94 instances, dual solve and forced-primal solve, 106 records
written.

## What fires today is the best-determined number in the set

Thirteen calls fire across the 94 instances. Every one of them has
`|alpha[q]|` equal to its own traffic, to all seventeen digits printed:

| instance | site | `alpha[q]` | traffic | `r` |
|---|---|---|---|---|
| `dfl001` | phase 1 | -3.3951065374647217e-10 | 3.3951065374647217e-10 | 4.50e+15 |
| `greenbeb` | phase 1 | 9.4322264868313942e-10 | 9.4325356839437523e-10 | 4.50e+15 |
| `scsd1` | cleanup | -3.5535796527597086e-11 | 3.5535796527597086e-11 | 4.50e+15 |
| `scsd6` | cleanup | 1.7233192650678575e-15 | 1.7233192650678575e-15 | 4.50e+15 |
| `pilot87` | phase 1 | 4.3673374264355619e-10 | 8.7985428256856171e-10 | 2.24e+15 |
| `d6cube` | phase 1 | -8.7283014149619476e-10 | 1.2748285798375067e-09 | 3.08e+15 |

`|sum| = sum|terms|` means a dot product with **one term**, or with no
cancellation at all. `r ≈ 1/eps` is the largest value this ratio can take.
These are not noise; they are exact.

**So `PIVOT_MIN` on this side is not a noise test and never was.** It is a
stability floor: do not pivot on 1e-10, because `theta_dual = d[q]/alpha[q]`
and the basis update both divide by it, and a well-determined tiny number is
just as dangerous to divide by as a badly determined one. Firing on these
thirteen is the right call, for a reason the constant's name and its
`docs/tolerances.md` row do not give.

## What is missing is the noise test, and one call needs it

The smallest `r` reached by any instance, per site:

| site | smallest `r` | which | instances with `min r < 1` |
|---|---|---|---|
| cleanup, **forced-primal** | **0.352457** | `scsd1` | **1** |
| cleanup, **dual — the gate path** | 32874.7 | `wood1p` | 0 |
| phase 1 | 20740.5 | `wood1p` | 0 |
| phase 2 | 2.37007e+14 | `forplan` | 0 |

`scsd1` reaches a call where `alpha[q]` stands at **0.35 ulps of its own
terms** — below the rounding of the dot product that produced it, which is to
say it has no value at all. The absolute test passes it, and the solve pivots
on it.

**The window is five orders wide.** A floor of `C * eps * traffic` with `C`
anywhere in `(0.352, 20740)` rejects that one call and moves nothing else
anywhere on the set. Below 0.352 it decides nothing; above 20740 it reaches
`wood1p`'s phase 1.

**And it cannot touch the gate.** The dual solve reaches only the cleanup
site, and its smallest `r` there is `wood1p`'s 32874.7 — 32874 times the
candidate value.

## The repair this points at

The same shape D207 used, **added to** the absolute test rather than replacing
it, because the two are answering different questions:

```
floor = max(PIVOT_MIN, PIVOT_MARGIN * DBL_EPSILON * alpha_traffic(s, q))
```

`PIVOT_MARGIN` is already 1.0 and already means "one ulp of this quantity's
own terms" (D207). The census puts 1.0 in the middle of a five-order window on
this side too, so the constant carries over even though the traffic does not.

Two things it needs before it lands, and neither is measured here:

- **The traffic walk costs work.** It is O(nnz of column q) per pivot, on a
  path the gate reaches. D203 and D207 both record what happens when a charge
  is added to the primal solve: `bench/primal` caps that solve at 10x the
  dual's **work**, so a new charge shortens every primal solve and moves
  instances across the bar for reasons that are not the change. D207's own
  first implementation lost two instances that way. The same trick may apply —
  the walk is only needed when the absolute test has already passed.
- **`docs/tolerances.md`'s `PIVOT_MIN` row should say which job it is doing.**
  It reads as a noise floor. It is a stability floor, and this directory is
  the evidence.
