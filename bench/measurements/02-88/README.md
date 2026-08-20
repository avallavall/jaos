# 02-88 — the hazard §8d protects against happens 12 times in 98146 chances

No decision and no source change. 02-87 costed D97's §8d refusal; this
measures the thing it protects against, so the two can be put beside each
other.

## The distinction that turns out to be the whole answer

§8d's rank argument breaks when **two columns of one row rest at bounds that
row imposed**. The refusal declines the *first* half of that — a row imposing
on two columns at all — because the second half is not known until the solve
is over.

02-87 counted the proxy. This counts the hazard.

## The count — `rested.txt`

Same tree, `7c7375c`, same caveat: it is the tightening D114 refused, so the
ratio is the finding and not the absolute. Records are read out of the
patched presolve after the solve and compared against the published `x`,
which is in the same original column indices.

| | netlib | Kennington |
|---|---|---|
| imposed bounds | 186269 | 853108 |
| rows imposing on 2 or more columns | 19775 | 78371 |
| **rows where 2 or more actually rest there** | **9** | **3** |
| as a fraction of the rows the refusal declines | **0.05%** | **0.004%** |
| of those, the row is an equality when it imposes | 7 of 9 | 3 of 3 |

**Identical at 1e-7 and at 1e-6**, so the count is not a knife-edge on the
tolerance. It fires on four instances in 110: `agg` (6), `maros` (2),
`pilot-ja` (1), `cre-a` (3).

`rows imposing on 2 or more columns` reads 19775 and 78371 here and in 02-87,
from two probes written separately. The imposed-bound totals differ by 409 on
Kennington because 02-87 counts one record per firing and this counts one per
END that moved, and a firing can move both.

## What it says about the refusal

**§8d's refusal declines 50.2% of netlib's imposed bounds and 82.3% of
Kennington's to prevent something that happens 12 times in 98146
opportunities — 0.012%.**

Even the narrow, equality-restricted form 02-87 recommends — 35.5% and 20.3%
— is three to four orders wider than the hazard.

That reshapes the design rather than tuning it. §8d already records that
Galabova 2023 offers "a fallback for when the slot assignment fails, which is
what a solver does when it has no proof". At 0.012% the shape that fits is to
**let the reduction fire, detect the collision at postsolve where it is
knowable, and take the fallback there** — not to decline half the family up
front against a case that essentially never arrives.

## What this does NOT settle

**The equality prediction is not confirmed and not refuted.** §8d argues the
breaking configuration forces the implying row to be an equality. Ten of the
twelve are equalities and two on `maros` are not — but the flag here is
`cur_rl[i] == cur_ru[i]` **at the moment the bound is imposed**, and presolve
moves row bounds across rounds. A row can be an equality later, or stop being
one. Settling the prediction needs the row's state at the point §8d's argument
applies, which this probe does not capture.

**And it is still the 7c7375c tightening.** A corrected one imposes fewer
bounds and may rest them differently. The conclusion survives that only
because the ratio is 12 to 98146; a change of an order or two does not reach
it.

## Reproducing

```
bench/measurements/02-88/run-rested.sh          # 7c7375c, ~5 min
bench/measurements/02-88/run-rested.sh <ref>    # any tree carrying the pass
```
