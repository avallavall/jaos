# 02-421 — what a ranging interval promises

Taken on 2026-10-22 on the tree of af76198, with 02-420's change. `read.sh`
takes the reading (`rangecheck.txt`). No code changed for it.

## The harness

02-167 read the ranging calls for their form: every current number lies
inside its interval, no end is NaN, and the two bounds of a row or column
never cross. `rangecheck.c` reads what an interval means. Inside the
interval `jaos_cost_ranging`, `jaos_rhs_ranging` or `jaos_bound_ranging`
gives a number, the published basis stays optimal, so the optimal value is
linear in that number. On each optimal model of 02-235's LPs the harness
takes three columns' costs, three rows' two sides and three columns' two
bounds, sets each to two points drawn strictly inside its interval (from
10% to 90% of its width, an open end taken 10 from the current number),
and solves both models afresh. Each solve must end optimal (P1), and the
current value and the two new ones must lie on one line within 1e-7 of
their size (P2).

## The reading

Seeds 1 to 3, 1000 models each: 1639 optimal models, 21089 intervals
probed, no ranging call refused.

P2 holds on every interval. P1 fails once: on seed 3's model 426, the cost
interval of column 10 is [-21.8, 9.77e16], and at a cost of 4.34e16, with
the other costs between -9 and 9, the solve ends `numerical_error`. The
interval's end near 1e17 looks like a ratio over a tableau entry that is
rounding noise, and an end there only narrows the interval.
