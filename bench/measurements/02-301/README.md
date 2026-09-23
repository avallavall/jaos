# 02-301 — the aggregator's row and fill limits, read again

Taken on 2026-09-23 for TODO row H1, on the tree of 6ea6aef. The limits
were set on 2026-09-21 (`bench/measurements/02-285/`), when rows of 4 left
`dfl001` at `NUMERICAL_ERROR`. Since 1b0265a a failed aggregated solve is
solved once more without the aggregator, so that obstacle is gone, and
HiGHS's presolve removes far more of `stocfor3` than JAOS's (8259 rows left
against 13305).

`sweep.sh ROW/FILL ...` copies the tree to `~/jaos-rf-aROW-FILL` and runs
netlib, the infeasible set and Kennington with `JAOS_AGG_ROW_MAX_VALUE` and
`JAOS_AGG_FILL_MAX_VALUE` set; `summarise.py ROW-FILL ...` sets each copy's
results against the committed ones at 3/8. Work units are exact.

| rows / fill | work vs 3/8 (139 instances) | worst | gate regressions |
|---|---|---|---|
| 4 / 8 | 1.030 | d2q06c 3.21 | 6 |
| 5 / 8 | 1.013 | fffff800 1.63 | 4 |
| 8 / 8 | 1.025 | cycle 3.80 | 8 |
| 3 / 16 | 1.007 | cycle 2.51 | 7 |
| 4 / 16 | 0.998 | pilot 7.56 | 10 |
| 3 / 32 | 1.021 | pilotnov 4.22 | 5 |

Every verdict agrees at every setting. None reads under 3/8 without an
instance past 2x, so 3/8 stays. At 8/8 `stocfor3` keeps 12921 rows (0.87x
work) and `d2q06c` 0.73x, so longer rows reach some of what HiGHS removes,
but nowhere near its 8259 rows on `stocfor3`: HiGHS gets there through
doubleton equations whose removed column is not implied free, which moves
that column's bounds onto the other one (D97).
