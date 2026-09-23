# 02-299 — the refactor interval, read again after the LU changes

Taken on 2026-09-23 for TODO row H1, on the tree of e180c91. That commit
keeps the LU's vectors between refactors and lets the update reuse the
entering column's partial FTRAN. It changes no answer and no work unit,
so this reading is about the interval alone.

## What was run

`sweep.sh 32 48 96 128` copies the tree once per value to
`~/jaos-rf-N`, sets `REFACTOR_EVERY` there and runs `make netlib`,
`make netlib-infeas` and `make netlib-kennington`. `summarise.py` sets
each copy's result files against the committed ones, which hold 64.
Work units are exact, so the machine load does not matter here.

## Result

Work as a geometric mean of per-instance ratios against 64, over 139
instances (94 netlib, 29 infeasible, 16 Kennington):

| interval | work vs 64 | worst instance |
|---|---|---|
| 32 | 0.9466 | pilot 1.794 |
| 48 | 0.9604 | greenbeb 1.530 |
| 96 | 1.0857 | pilot 9.549 |
| 128 | 1.1499 | pilot 19.516 |

Every verdict agrees with 64 at every value (139 of 139).

The netlib gate refuses 32 and 48 on accuracy (`regressed-32.txt`,
`regressed-48.txt`). The suboptimality bound of `wood1p` goes from
9.88e-15 to 2.2e-09 at 32 and to 8.69e-10 at 48. That is five orders of
magnitude, far above rounding level. `d2q06c` rises 19x at 32. The
infeasible set and Kennington regress nothing.

So 64 stays. The 2026-08-24 sweep found the same order (32 was 8.6%
cheaper then and lost accuracy on `pilot87`); now 32 saves 5.3%, and
the accuracy it loses is on `wood1p`.
