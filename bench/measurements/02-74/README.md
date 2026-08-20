# 02-74 — carrying the error a fold puts inside a value is REFUSED, and the reason is what it publishes instead

D164. Opened by `numerics-reviewer` on D162 as F3, confirmed as a wrong answer
by D163, and the repair it named is refused here.

## The question

D162 and D163 put a shift COUNT on the four windows that judge a row's running
difference. A count bounds the roundings THIS row made. It cannot bound an
error that arrived from another row, and one route does exactly that:

1. a singleton row folds, and its implied end `cur_rl[i] / a` carries that
   row's whole accumulated error
2. the fold writes it into the column's box
3. the column is fixed, and every other row it touches subtracts a value that
   was already wrong
4. those rows are charged **one** shift at their own traffic

The repair the review named: carry an error weight instead of a count —
`col_value_err[j]` set at the fold, `row_inherited_err[i] += |a| *
col_value_err[j]` at each subtraction, and every window adds it.

**It was built, it works, and it is refused.**

## 1. The route is real and heavily used — `inherited.txt`

Probe at `b5eebc7`, computing the weight beside the shipped windows and
deciding nothing.

| | netlib (94) | netlib-infeas (29) | Kennington (16) |
|---|---|---|---|
| folds writing a derived end | 8622 | 9750 | 81646 |
| of those, carrying an error | 5408 | 3706 | 75896 |
| subtractions of such a column | 8250 | 1370 | 97780 |
| window reads with inheritance > 0 | 9006 | 2170 | **324826** |
| worst error in a fixed VALUE | 6.92e-09 | 1.439e-10 | 2.08e-11 |
| worst inheritance on one row | 5.703e-10 | 1.117e-11 | 5.698e-11 |
| **worst inheritance / shipped window** | **1.143e+05** | 8.625 | 1.833 |
| **verdicts it would spare** | **0** | **0** | **0** |

**The window is short by up to five decades on real netlib rows**, not only on
a constructed model. And it flips nothing: all 20 genuine infeasibility firings
in `netlib-infeas` — 8 at the fold, 8 at clause 1, 4 at the frozen-row test —
survive it.

**No absolute window moves at all**, on any of the four sites, on any of the
three sets. The fold's widest reads 5.536e-08 → 5.536e-08 on netlib; the other
three are unchanged from 02-72. The row carrying the widest window is never the
row carrying the inheritance.

**One reading in this table was wrong and the shape of the error is why it was
caught.** The fold's shipped window first printed `0 -> 5.536e-08` — a maximum
of zero beside a non-zero sum, which cannot happen. The awk classifier matched
extremes by the suffix `_w$`, which does not match `D_wnow`, so that field was
summed into `v[]` and printed from `m[]`, which is empty. **This is 02-69's
finding, in the same shape, in a script that carries 02-69's own warning as a
comment.** Anchor every extreme by name; a suffix rule silently reclassifies
the next field somebody adds.

## 2. Why it is refused — `chain-answer.c`

02-73's CHAIN model, and the question is the ANSWER rather than the status:

```
row S:  x1 + (256 y_s fixed at 2^-25) == 1e9      x1 in [1e9-1, 1e9+1]
row R:  x1 + w1 + w2 == 1e9 - 63*2^-23            w1, w2 in [0, 2^-23]
```

Feasible exactly at `x1 = 1e9 - 2^-17`, `w1 = 2^-23`, `w2 = 0`.

| tree | status | objective | row S residual | row R residual |
|---|---|---|---|---|
| `b5eebc7`, the parent | **infeasible** | — | — | — |
| **the error weight carried** | **optimal** | **0** | **7.629e-06** | **7.51e-06** |
| reference build, the oracle | optimal | 1.1920928955078125e-07 | 0 | 0 |

**The widening publishes a point that violates both rows by 7.5 times
`CHECK_TOL`**, with an objective of 0 against a true 1.19e-7, and calls it
optimal. The parent's answer is wrong too — a false INFEASIBLE — but it is
loud, and `jaos.h`'s promise is not broken by it. This one is silent.

**A wider window cannot repair a value that is already wrong.** The window
decides whether to REFUSE; it has no way to correct `x1 = 1e9` back to
`1e9 - 2^-17`. Widening it only stops the refusal and lets the wrong value
through to the answer. That is the mirror-image catastrophe `src/presolve.c`
names at the empty-row test, arrived at from the other side.

D163 already refused "a wider window" for this case on an argument. This
refuses it on the measurement, which is a different and better thing.

## 3. What would repair it, neither built nor measured

- **Compensate `cur_rl`/`cur_ru`.** They are the only running sums in this file
  with no compensation, while `ps_row_range` has used a Neumaier accumulator
  for activities since 02-04 and the comment there gives the reason. If the row
  bounds carry no error, the fold's value is right, nothing downstream inherits
  anything, and **D162's and D163's shift counts stop being needed as well** —
  this would subsume the whole class rather than adding to it. `long double` is
  not available (D34); Neumaier is portable and already in the file.
- **Widen the folded BOX instead of the window.** Publish
  `[fold_lo - err, fold_hi + err]` rather than collapsing to a point, so the
  column stays a range, the receiving row keeps a live column, and the simplex
  judges it. It reduces less, so it needs a campaign of its own. It is also
  §11b of `docs/research/dual-postsolve-imposed-bound.md`, the deliberate-slack
  direction, arrived at independently.

## 4. What landed

Nothing in `src/`. `src/presolve.c` is byte-identical to `b5eebc7`, so **no
campaign is owed and none was run** — the last one, at D163, still stands for
these bytes.

What landed is one pinned change-detector,
`test_a_folds_value_carries_its_rows_error_into_the_next`, asserting the wrong
answer JAOS gives today and the right one the reference build gives, in the
same test. When the repair lands, that line is what changes. `make configs`
exits 0 on all five configurations.
