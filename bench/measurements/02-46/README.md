# The rule falls out of the dual, and the defect is a status decided on a partial activity

Taken 2026-08-18. It gives `SINGLETON_ROW` a derived rule and a precise
defect, and **it inverted its own conclusion twice** before the numbers were
readable. Closed as D136.

## The rule is not a choice

D134 counted `SINGLETON_ROW`'s four combinations and found two wrong. Reading
the case again, the rule falls out of complementary slackness, and the code
already computes both halves:

```c
y_i = 0.0;                 /* when zero_works || !this_row_owns — column left alone */
y_i = d0 / rec->coef;      /* otherwise — column set BASIC */
```

A basic logical requires a zero dual. So:

- **`y_i == 0`** → the row's logical may be basic, and no column was taken, so
  marking the row `BASIC` balances the row it restored.
- **`y_i != 0`** → the logical must be nonbasic, and the column was taken
  instead, so the row must rest on a bound.

**Measured against what is published, the rule agrees with exactly the two
correct combinations and disagrees with exactly the two wrong ones:**

| | netlib | Kennington |
|---|---|---|
| rule agrees with what is published | 5572 | 48702 |
| rule wants `BASIC`, published on a bound (D134's −1) | 2524 | 3886 |
| rule wants a bound, published `BASIC` (D134's +1) | 526 | 29058 |

5572 = 4200 + 1372 and 48702 = 48586 + 116, which are D134's two balanced
combinations. It is derived, not fitted.

## The `−1` case is free

The rule wants `BASIC` where a bound is published. **A basic variable is
allowed to sit exactly on a bound** — that is degeneracy, not an error — and
the dual is exactly `0.0` on that branch, which is what a basic logical
requires. Nothing blocks it.

## The `+1` case is a status decided too early

The rule wants a bound where `BASIC` is published, which looked impossible:
the case marks the row `BASIC` precisely because its activity matched no
bound. So how far off is it?

| gap to the nearest bound, relative to the row's own traffic | netlib | Kennington |
|---|---|---|
| **exactly 0** | **486** | **29058** |
| 1e-16 | 40 | 0 |

**The rows are on their bounds.** The status test is not.

```c
orig->sol_row[i] = ps_published(rec->coef * xv);
rowc[i] = 0.0;
const double act = orig->sol_row[i];
if (act == orig->row_lower[i]) ...
```

It assigns the row's activity as **its own term alone** and compares that
against the row's bounds immediately. Records replaying later add their
contributions through `ps_row_add`, and those are folded in at the end of
`jm_postsolve_expand`. So the test sees a partial sum.

**Deciding the status after the replay, from the final activity, fixes the
whole `+1` combination** — 29058 of Kennington's firings and 486 of netlib's
526. The remaining 40 sit at 1e-16 of the row's traffic and would still fail
an exact comparison.

## The probe inverted its own answer twice

Both readings looked like findings.

- **Reading the gap inside the replay** gave 498 of 526 at a relative gap of
  order **1.0** — which would have said the published point violates
  complementary slackness, a much larger and false claim. Judged after the
  replay it is 486 at exactly 0.
- **The histogram was one decade short and one decade mislabelled.** Its top
  bucket clamped at 1e-8, so everything above lumped together and printed as
  `10^-7`. Widened to +2 and the label corrected from `q-17` to `q-18`.

**This is the third time in three probes that reading a row activity during
the replay has produced a wrong number** (02-45, and 02-46 twice). It is in
`TODO.md`.

## What is left open

The repair, which now has a shape rather than a question:

1. **Decide the singleton row's status after the replay**, from the final
   activity. Closes the `+1` combination bar 40 netlib firings.
2. **Publish the row `BASIC` when `y_i == 0`.** Closes the `−1` combination.
3. The 40 firings at 1e-16, and `SINGLETON_COL`'s 188 left over by D135.

Nothing is costed and no source file was touched. Any candidate is checked
against the closing sums, +3904 on netlib and +25654 on Kennington, which must
go to zero.

## Reproducing it

`run-slack-gap.sh`, beside this file. `src/` is read and never written.
