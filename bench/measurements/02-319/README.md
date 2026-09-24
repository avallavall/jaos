# 02-319 — a refused conic optimum is settled on its active rows

Taken on 2026-09-25 on the tree of 34817d7, for TODO row J3.

## The refusals

02-316 left 8 of QPLIB's continuous QCQP optima refused by the checker at
1e-7. `qcprobe.c` lists what the checker refuses. On every one it is a
quadratic row `q(x) <= 1`, two squares each, left off its side while its
dual is over 1e-7; no column is refused. QPLIB_2482 has 4 such rows, with
slacks of 1.1e-7 to 2.6e-6 and duals of 2.0e-7 to 8.5e-7. QPLIB_3088 has
54; the 12 the probe prints have slacks of 1.0e-7 to 2.0e-5 and duals of
1.9e-7 to 3.6e-5. The Newton finish reads its active set off
`slack <= |dual|` and diverges there: on QPLIB_2482 its KKT residual goes
from 4.0e-7 to 8.2e-2 in one step, and the checker refuses its point.

## The change

When the checker refuses the finish's point outside a tree node,
`cm_settle` in `src/conic.c` runs two more passes of the finish's own
system from that point:

1. a projection: the identity in place of the Hessian and the duals kept,
   so the columns take the least move that puts every active row on its
   side (Gauss-Newton on the constraints, up to `CONIC_NEWTON_STEPS`);
2. a dual refit: the columns kept, and the active rows' duals take the
   least-squares fit of the columns' stationarity; every other row's dual
   is 0.

The active rows are first those whose dual is larger than their slack,
the finish's own rule. When the checker still refuses, both passes run
again from the finish's point with every row whose dual is over the
checker's tolerance held on its side as well. The first point the checker
takes is published; otherwise the finish's point stands and the solve
ends `numerical_error` as before.

The two rules, each alone, on the 8:

| model | first rule | second rule |
|---|---|---|
| QPLIB_2456 | duals off by 5.6e-7 | the move is refused (1.1e-1) |
| QPLIB_2482 | taken | taken |
| QPLIB_2519 | taken | a row off by 2.7e-7 |
| QPLIB_2784 | duals off by 1.2e-7 | taken |
| QPLIB_2862 | duals off by 1.2e-7 | taken |
| QPLIB_3088 | duals off by 4.1e-7 | taken |
| QPLIB_3105 | duals off by 5.8e-7 | rows off by 7.1e-6 |
| QPLIB_3185 | taken | taken |

Under the second rule the projection alone, without the refit, took
QPLIB_3088 and QPLIB_2784 to duals off by 1.1e-7 and 1.5e-7: the move
changes the columns' stationarity by the rows' curvature times the move. A tighter tolerance
on the walk (`CONIC_TOL` 1e-12 and 1e-14) changes nothing on QPLIB_2456
and QPLIB_3105: their walks stall before it, near a gap of 1e-9.

## Readings

`conread.sh`, HEAD (34817d7) against the working tree:

- QPLIB's 13 continuous QCQPs at 1e11 work units (`qcqp-head.txt`,
  `qcqp-new.txt`): 9 end `optimal` taken by the checker, where 2 did.
  The 6 of the 8 above, and QPLIB_2676, whose walk stops without progress,
  settle; their objectives are within 7.1e-8 of the library's values.
  QPLIB_2456, QPLIB_3105 and QPLIB_2468 still end `numerical_error`, at
  1.20x, 1.11x and 1.24x the work; QPLIB_3312 reaches the work limit as
  before.
- The 3000 generated models of 02-253 (`gen-head.txt`, `gen-new.txt`):
  the same digest and the same work on all three seeds; the settle never
  runs there.
- QPLIB's 17 convex MIQPs at 1e10 (`miqp-head.txt`, `miqp-new.txt`): the
  same lines, except that the roots of QPLIB_4270, 5527 and 5543, which end
  `numerical_error` in both, pay 1.001x, 1.033x and 1.033x the work for the
  settle.
- `make cblib`: 29 of 29 solved and taken by the checker, and
  `bench/results/cblib.txt` reads the same, line for line.

`make test` and `make sanitize` pass.

## What is left

On QPLIB_2456 the second rule holds 118 rows 2e-5 to 4e-5 from their side,
with duals of 1.1e-7 to 1.7e-7, and together they are inconsistent. The
first rule sets those duals to 0, and the refit cannot absorb them. A rule
that adds to the active set only the rows the refit cannot do without, a
batch at a time, is the next thing to try, on QPLIB_2456 and QPLIB_3105.

## Files

- `conread.sh` — the readings, HEAD against the working tree
- `qcprobe.c` — lists the rows and columns the checker refuses
- `gen-*.txt`, `qcqp-*.txt`, `miqp-*.txt` — the records
