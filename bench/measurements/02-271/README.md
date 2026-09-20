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

## Which column the checker refuses

The worst column of sched_100_50_orig is column 4997. Its bounds are 0 and
infinity, its value is 1.04822e-07, and its reduced cost is 84.29. The
checker takes a column as sitting at its bound when it is within
`tol * max(1, |x|)`, which is 1e-7 here, so the column misses that window
by 4.8e-10 and its whole reduced cost counts as a violation. The
complementarity product is 8.8e-6. The same shape holds for the columns
above it, up to 1818 at 1.4e-9 from the bound.

So the walk is not wrong about the dual. It leaves the columns the
distance an interior point leaves them, and the Newton finish does not
bring this set of them home.

## Two repairs, both refused

Both were built, measured on the three, and taken out.

1. **A free column alone in its row fixes that row's dual.** 4742 of the
   4744 free columns of sched_100_50_orig hold one row entry each, so
   `y_i = (c_j - z_j) / a_ij` is exact for them. 4740 rows take their dual
   that way, and the checker's dual violation does not move at all (92.4
   before and after), because the columns it refuses are not those.
2. **A column within 1e-6 of a bound its reduced cost pushes it to goes
   to the bound.** 4274, 9300 and 19345 columns move. The column
   violation falls to 0 and the rows rise to 5.85e-05, 1.03e-04 and
   4.82e-05 of their traffic, which the checker refuses, and the dual
   violation moves to 166, 209 and 56.1. Moving the columns without
   solving the rows again is not an active set.

`bench/refusals.txt`, line `conic-dual-singleton-snap`.

## A third repair, also refused

`TODO.md` row 5 named an active-set update after the finish as the missing
piece. The finish was run again on the point and the duals it had just
produced, up to three rounds, with the accept rule relaxed so a point the
checker still refuses is taken when its worst violation is smaller. The
second round reads the same active set as the first, 9362 constraints on
sched_100_50_orig and 37521 on sched_200_100_orig, reaches the same KKT
residual and ends at the same violation, 9.459 and 0.5188. The columns the
checker refuses are already in the set, because `cm_active` takes a column
whose slack is under its own dual, and the finish puts them on their bound
exactly. So the set has nothing to update, and what refuses the point is
elsewhere.

## What is left to try

The finish solves for the multipliers of a fixed set. What the three need
is a different set, not another pass over the same one: the rows the walk
leaves inactive and the columns it leaves free have to be chosen by what
the checker will measure, not by the walk's own slack against its own
dual.
