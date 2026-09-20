# 02-271 — why CBLIB's three `sched_*_orig` end `NUMERICAL_ERROR`

2026-09-20. The reading names the cause. One remedy was built, measured and
refused; the message the solve prints now says which side is off.

## What the three do

`sched_100_50_orig`, `sched_100_100_orig` and `sched_200_100_orig` stop
after 38 to 40 conic iterations with "no progress for 3 iterations", and
their last point within 1e-6 fails the checker. The message said only that.
It now carries the checker's own numbers:

| instance | columns | rows | rows, relative | cones | duals |
|---|---|---|---|---|---|
| sched_100_50_orig | 2.23e-09 | 9.31e-08 | 9.31e-08 | 1.59e-10 | 92.4 |
| sched_100_100_orig | 2.6e-08 | 2.08e-07 | 2.08e-07 | 2.16e-10 | 411 |
| sched_200_100_orig | 4.18e-09 | 2.31e-08 | 2.31e-08 | 1.33e-11 | 9.54 |

The primal side is at working precision. The duals are not, by 9.5 to 411.
The violation is a column's reduced cost, not a cone's dual, and it is not
a scaling artefact: on sched_100_50_orig the worst column carries a reduced
cost of 2274 against a traffic of 12316, so a test relative to the traffic
would refuse it too. `TODO.md` row 5 named the primal residual as the
cause; this reading says the dual side is.

The walk's own stop reads `dres / (1 + |q| + |x| + |z|)`, a relative
measure, and it is under 1e-6 while the checker's per-column test is not.

## The remedy that was refused

The walk keeps the last iterate that came within 1e-6 on all three
measures, not the best. Keeping the best by the walk's own merit,
`max(prel, drel, gap)`, is one line. It changes nothing on the three, and
`make cblib` then regresses the suboptimality bound on six other instances:
chainsing-10000-1 1.09e-15 to 2.37e-15, nql60 2.13e-09 to 4.81e-09, nql90
6.98e-10 to 3.3e-09, qssp60 1.65e-11 to 1.65e-10, qssp90 1.25e-11 to
2.02e-10, qssp180 1.84e-09 to 1.31e-08. A later iterate has a smaller `mu`
and a better complementarity even when the merit ticks up, and the
suboptimality bound reads the complementarity. 02-253's 3000 models and
02-255's 3000 models pass either way, with 0 failed and the same
certificate counts.

`bench/refusals.txt`, line `conic-best-rough-point`.

## What is left to try

A dual repair for the three: the columns whose reduced cost has the wrong
size are free columns of the CBF cone rewrite, so their multiplier comes
from an equality row, and a least-squares fit of the row duals to the
active set would move them. `conic-dual-refit` refused such a fit in
2026-09-19 for a model whose Newton finish runs, and here the finish runs
and is refused too, so the fit would have to replace the finish rather than
follow it.
