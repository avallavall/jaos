# 02-327 — two settles for QPLIB_2456, refused

Taken on 2026-09-25 on the tree of 06ca756, for TODO row J3.
`settle.diff` adds three switches to `cm_settle` and `newton_polish` in
`src/conic.c`; `settle.sh MODEL ARM...` applies it to a copy of the tree
and solves one model per arm with the detail log.

- `JAOS_BATCH=f`: before the first settle, the rows whose dual is over
  the tolerance and larger than their slack (the rows the first rule
  leaves out and the second takes) are sorted by the size of their dual,
  and the largest fraction `f` of them is held on its nearest side with
  the first rule's rows.
- `JAOS_KEEPINACT`: in the dual refit, every row outside the active set
  keeps its walk dual as a fixed term of stationarity, and in the answer,
  in place of 0.
- `JAOS_FORCE`: after each projection, every row it leaves outside its
  bounds by more than 1e-9 of the bound joins the active set and the
  projection runs again from the walk's point, up to six rounds.

## QPLIB_2456

The first rule's active set holds 4528 constraints, the second's 4646;
118 rows lie between them, with duals over 1e-7 and up to 1.5e-6.

| arm | rows forced | projection | refit | worst after |
|---|---|---|---|---|
| plain (the first rule) | 0 | taken, 1.87e-6 | taken | 5.56e-7 |
| 0.10 | 12 | taken, 1.45e-6 | taken | 4.97e-7 |
| 0.25 | 30 | taken, 9.67e-7 | taken | 3.29e-7 |
| 0.33 | 39 | refused, 4.09e-3 | | 3.69e-3 |
| 0.40 | 48 | taken, 3.67e-3 | taken | 3.56e-3 |
| 0.45 | 54 | refused, 4.01e-3 | | 3.69e-3 |
| 0.50 | 59 | refused, 8.54e-3 | | 3.69e-3 |
| 0.75 | 89 | refused, 8.54e-3 | | 3.69e-3 |
| keep | 0 | taken, 1.87e-6 | refused, residual 8.9e-10 | 1.87e-6 |
| force | 0 | taken, no row violated | taken | 5.56e-7 |

The worst violation after every projection of the first rule is the
checker's dual violation: the walk's duals, kept, against the moved
columns. The refit brings it under 1e-7 only when the rows left out keep
their duals, and then the checker's sign condition counts each of those
duals, over 1e-7 on a row off its side, as a violation of its own size.
So those rows need a dual of 0, and the refit cannot make up for them at
the walk's point: the point is not near enough an optimum for its duals to
name the active rows. The second rule's projection, with all 118 rows,
stays refused at 1.1e-1 in every arm.
