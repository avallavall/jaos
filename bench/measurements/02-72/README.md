# 02-72 — the row bound is a running difference, and the window never counted the terms

D162. Opened by `numerics-reviewer` while reviewing D159, carried by `TODO.md`
§4 since 2026-08-20, closed here.

## The question

`cur_rl[i]` and `cur_ru[i]` start at the caller's row bounds, and every removed
column subtracts its own `a * v` from both. There is no compensation in that
loop. Each subtraction rounds by up to half an ulp of the partial it produces,
so after k removals the error goes with `k * eps * scale`.

Three windows judge one of those numbers against something else, and all three
counted a fixed eight ulps:

| site | what it compares | what it decides |
|---|---|---|
| the emptied-row feasibility test | `cur_rl[i] > etol` | INFEASIBLE, and it is the last word |
| clause 1 of the activity pass | `min_act > ru + itol` | INFEASIBLE |
| the frozen-row test | `min_act > cur_ru[i] + rtol` | INFEASIBLE |

Eight ulps covers a k of about three. `ps_verify_row_activities` already
multiplies by `nnz - 1` for the same quantity and is the shape this copies.

## 1. Three shapes, and two of them are wrong

The count is the easy half. What it multiplies took two attempts to get right,
and **both wrong versions were built and refuted rather than argued away.**

| | the count multiplies | verdict |
|---|---|---|
| **A** | the row's traffic | **short**, by working the case |
| **B** | the magnitude of the END this comparison reads, plus the traffic | **ships** |
| **C** | `ps_bound_scale` — the larger of the two ends — plus the traffic | **refused by D161's own test** |

**A is short and no measurement says so.** The rounding at step j is half an
ulp of the partial `cur_rl` holds at that moment, and a partial is bounded by
`|row_lower[i]| + traffic`, not by the traffic. The argument for A was that
this test only comes near firing when `cur_rl[i]` is small. It is false: at the
activity pass and the frozen-row test the comparison is against
`min_act`/`max_act`, so it comes near firing when `cur_rl[i]` is near the
ACTIVITY, which can be any magnitude. A row of activity 1e9 with 300 removals
totalling 0.9 of traffic carries about 1.8e-5 of error against a traffic-only
window of 6.8e-14.

**C came back with D161's defect and `make configs` caught it.** On
`-1e12 <= x0 + x1 <= 0` with both columns cost-0 bounded singletons, the two
relaxations are two shifts, so `2 * eps * 1e12 = 4.4e-4` of window lands on the
UPPER side against an infeasibility of 2e-4 — and
`test_a_frozen_rows_window_ignores_the_far_bound` went red. The two ends walk
through different partials, so anything scaled by an end has to say which end.
That is `ps_end_scale`, and it is the whole difference between C and B.

**All three vanish at k = 0**, which is what keeps D161 for a row nothing was
ever removed from.

## 2. The three sets — `shifts.txt`, `run-shifts.sh`, `probe-shifts.py`

The probe adds the counter and computes all three candidate windows beside the
shipped one at all three sites. It decides nothing; the shipped window still
runs the solve.

| | netlib (94) | netlib-infeas (29) | Kennington (16) |
|---|---|---|---|
| largest count, emptied-row test | 2 | 4 | 0 |
| largest count, activity clause 1 | **250** | 27 | 4 |
| largest count, frozen-row test | 46 | 2 | **325** |
| rows carrying more than 8 shifts | 690 | 4 | 110 |
| **verdicts A, B or C flips** | **0** | **0** | **0** |

**The count is real and the widening is not.** 804 rows over the three sets
carry more than the eight ulps the window paid for, one of them by a factor of
40, and no verdict moves under any of the three shapes. The twelve genuine
infeasibility firings — eight at clause 1 and four at the frozen-row test, all
in netlib-infeas — fire under every one.

**B and C are the same number on every row of all three sets.** The worst C/B
ratio is exactly 1 at all nine site-set pairs, so **the population cannot
separate the shape that ships from the shape that is wrong**; only D161's
constructed model does. Worth remembering before trusting a green campaign
about a window.

**The absolute window is the figure no ratio answers**, and it barely moves:

| widest ABSOLUTE window | shipped | A | **B (ships)** | C |
|---|---|---|---|---|
| netlib, frozen-row test | 1.776e-08 | 2.025e-08 | **2.247e-08** | 2.247e-08 |
| Kennington, frozen-row test | 6.494e-08 | 6.578e-08 | **6.587e-08** | 6.587e-08 |
| netlib-infeas, emptied-row test | 2.203e-14 | 3.482e-14 | **9.486e-14** | 9.486e-14 |
| netlib, activity clause 1 | 4.038e-07 | 4.038e-07 | **4.038e-07** | 4.038e-07 |
| netlib-infeas, activity clause 1 | 1.908e-06 | 1.908e-06 | **1.908e-06** | 1.908e-06 |

Every one stays under `PRIMAL_TOL` 1e-7 except clause 1's two, which **do not
move at all** — the rows carrying the widest window there are not the rows
carrying the shifts. The 1.908e-06 above `CHECK_TOL` was already what it is
before this change and is not this change's to explain.

**No row on any of the three sets had a positive residue that passed on its
window.** Nothing measured is near a boundary, which is the other half of why
the sets cannot choose a shape.

The probe's windows are one 8-ulp term wider than the shipping emptied-row
window, which has no activity half to spend it on. Conservative in the right
direction on both readings: the shipped window is narrower than what was
probed, so 0 flips still holds and the absolute figures are upper bounds.

## 3. The model that separates them — `shift-count.txt`, `shift-count.c`

```
row R:  x0 + x1 + (k smalls) + w1 + w2  ==  k*2^-25 + 1e-7
row S:  x1 + z                          == -1e9

x0      fixed at +1e9
x1      in [-1e9-1, -1e9+1], NOT fixed at load time
smalls  fixed at 2^-25, a quarter of an ulp of 1e9
w1, w2  in [0, 2e-7], cost 1, so no family relaxes them
z       fixed at 0, and this is what delays x1 by one round
```

Presolve removes x0 and all k smalls in round 1, while x1 is still free. Each
small is a quarter of an ulp of an accumulator of magnitude 1e9 and rounds
away. Round 1 also empties row S of z, which makes it a singleton row; round 2
folds it, fixes x1 at -1e9 and only then subtracts it. `cur_rl` comes back to
`k*2^-25 + 1e-7` where the truth is 1e-7.

At k = 256 that error is 7.63e-6 against a shipped window of
`8 * DBL_EPSILON * 2e9 = 3.55e-6`, and clause 1 reads `max_act < cur_rl - itol`.

| k | the parent | the shifts counted |
|---|---|---|
| 64 | not refused | not refused |
| 128 | not refused | not refused |
| **256** | **INFEASIBLE** | **not refused** |
| 512 | INFEASIBLE | not refused |

**A pin, not one reading.** 128 and 256 are the pair that separates the two
windows, and the control — the same shape 1e-2 away from any feasible point —
is refused on every build at every k.

**The model's feasible point is exactly representable**: `x0 = 1e9`,
`x1 = -1e9`, every small at `2^-25`, `w1 = T - 2^-17` and `w2 = 0` make the
activity exactly `T`. Every value in it is a dyadic rational a double holds
without rounding, and the sum is exact when it is taken in column order.

## 4. What the reference build says, and why it does not decide this

**`-DJAOS_NO_PRESOLVE` refuses the model at every k**, including k = 64 and
k = 128 where the shipped window already accepts. So the oracle is not
disagreeing about the window; it disagrees about the model, on every tree.

The reason is the solver's own row activity: it sums the columns in index
order, so it meets the `+1e9` first and loses the same 256 terms presolve lost.
Its residual is the same 7.63e-6. That is a defect of the solver's feasibility
test and not of this window, and **it is new — it goes to `TODO.md`.**

**So the exact feasible point is the oracle here, and it is checkable by hand.**
That is a weaker footing than a reference-build disagreement and this record
says so rather than dressing it up. What it is not is an argument from the
source: the model exists, it reproduces at HEAD, and it stops reproducing with
the count.

**The published answer is not the true optimum on any build and is not pinned.**
The repaired tree reads `optimal` with `obj = 4e-07` where exact arithmetic
gives 1e-7, because the simplex meets the same unrepresentable row and puts
both `w` columns on their upper bounds. The test asserts presolve's outcome
only; pinning the objective would pin a wrong number.

## 5. The shape as it ships

At clause 1 and the frozen-row test the window covers two different numbers
with two different errors, and they are **added**:

- `min_act` / `max_act` is a compensated sum over the live columns, so eight
  ulps of `rg.traffic` is its budget and the shift count has nothing to do
  with it.
- `cur_rl` / `cur_ru` is the running difference: eight ulps of
  `row_traffic[i]`, plus `k * eps * (ps_end_scale(this end) + row_traffic[i])`.

Adding is never narrower than the shipped window, which matters in one
direction only: a change that could narrow one of these would be a change that
introduces a false INFEASIBLE, and this one cannot. At the emptied-row test
there are no live columns left, so there is no activity half.

**A shift of exactly zero is not counted.** It moves neither end and
`x - 0.0` is exact, so charging it would widen a window for an error that was
never made.

## 6. What this does not close

- **The solver's own row activity loses terms the same way**, which is §4
  above and is now `TODO.md`'s. It is why this record has no reference-build
  disagreement to show.
- **No shape of window can be chosen from the three sets.** 0 rows are near
  any boundary and B and C are identical on all of them. The shape rests on
  the error analysis, on D161's test refusing C, and on the constructed pin.
