# 02-67 — The destroyed row width has a bound, and it is one ulp of the row's own activity

2026-08-20. `TODO.md`'s second smaller item: "a row's own width can be
destroyed by the shift that removes a column."

## What was asked

`cur_rl[i]` and `cur_ru[i]` are running differences. Two of the three sites
that shift them subtract the SAME term from both ends, so the width `ru - rl`
is invariant in exact arithmetic — but `1 - 1e17` and `2 - 1e17` are the same
double, so a row the caller wrote as `[1, 2]` can reach the simplex as an
equality. `TODO.md` said the answer is then "wrong by up to that width
whatever presolve does next", and `src/presolve.c` said the same beside the
implied-free family's own guard.

**That overstates it, and the entry is the derivation plus the measurement
that says so.**

## 1. The instrument, before believing anything it says

`width-case.c`, two models one line apart:

```
A:  min x1  s.t.  R: 1e17*x0 + x1 in [1, 2],  x0 in [1,1]
B:  the same with 1e3 in place of 1e17
```

`ulp(1e17)` is 16, so A's two bounds collapse and B's do not.

```
A   shifts=1 lost=1 destroyed=1 rows_hit=1 worst_rel=1 surv_equal=1
B   shifts=1 lost=0 destroyed=0 rows_hit=0 worst_rel=0 surv_equal=0
```

A is detected, B is not. A zero from this probe means something.

## 2. It never happens on the three sets

| | netlib (94) | infeas (29) | Kennington (16) |
|---|---|---|---|
| same-term shifts on a finite width | 320 | 30 | 8592 |
| width changed at all | **0** | **0** | **0** |
| width destroyed to zero | **0** | **0** | **0** |
| surviving rows narrowed | **0** | **0** | **0** |

8942 shift events, no width lost. The subtraction is exact on every one of
them, which is what happens when the bounds and the shift are within a factor
of two of each other.

## 3. And when it does happen, the answer does not move

`width-answer.c`, run on the normal build and on `-DJAOS_NO_PRESOLVE` — the
only oracle for output no predicate of the three sets reads.

| case | shape | presolve vs reference |
|---|---|---|
| A-dies | width destroyed, surviving column at the shift's scale | identical |
| B-control | 1e3, no destruction | identical |
| C-bigcoef | width destroyed, surviving column carries 1e17 too | identical |
| D-deg2 | C with a second surviving column | **x2 differs** |
| E-noWidth | an inequality, no width to lose | identical |
| F-amplify | surviving singleton at `a = 1e-6` | identical |
| G-amplify2 | surviving singleton at `a = 1e-12` | identical |

D-deg2 differs in `x2` alone — `0` against `5` — at the same objective `-1` and
the same `x1`. That is alternate optima on a degenerate model, not an error.
Every other case is bit-identical, including both amplifying shapes.

## The bound, which is what the item was missing

The width dies only when `fl(rl - t)` and `fl(ru - t)` are the same double,
and that needs `ru - rl` below one ulp of `rl - t`.

`rl - t` is the activity the surviving columns have to produce. It cannot be
small: a shift close enough to the bounds' own scale to leave `rl - t` small
subtracts **exactly** (Sterbenz), so nothing is lost. Width loss therefore
requires `|rl|, |ru| << |t|`, hence `|rl - t| ≈ |t|`.

**So the width that dies was already below the resolution of the quantity it
constrains**, and no point satisfying the collapsed row violates the original
by more than the activity's own representation error.

**The division by a surviving coefficient does not break this**, which is the
half worth measuring rather than arguing. A tiny `a` multiplies the row-space
width by `1/|a|` on its way into the column — a width of 1 becomes 1e12 at
`a = 1e-12` — but it multiplies the activity by `1/|a|` as well, so the
relative error is unmoved. F and G measure exactly that and are bit-identical.

## Why §1's collapsed fold is NOT the same shape

This matters, because the two look alike and only one has a bound.

The fold's error is `4 * DBL_EPSILON * row_traffic[i] / |a|`. It is relative
to the row's **traffic**, and cancellation can put the traffic far above the
activity — a row that sums to 1 out of terms of size 1e9 carries a traffic of
1e9. So there the `/|a|` amplifies an error that was never tied to the value
it perturbs, and `TODO.md` §1 records a case reading **0.89**.

Here the error is relative to the **activity**, and the same division scales
both. That is the whole difference, and it is why §1 stays open with no bound
while this one closes with one.

## Verdict

**Refused as a defect to repair.** Nothing is changed in the solve. What
changed is the claim: `src/presolve.c`'s comment and `TODO.md`'s item both
said the answer is wrong by up to the lost width, and both now carry the
bound instead.

The reduction was NOT made to refuse a width-destroying shift. Such a refusal
would have been exact and free on the gate — 0 firings of 8942 — but it would
buy nothing, because the shape it refuses does not produce a wrong answer.

## Reproducing it

```
bash bench/measurements/02-67/run-width.sh
```

It applies the probe to a scratch copy of `src/presolve.c`, runs all three
questions in order, and reverts the probe on exit.
