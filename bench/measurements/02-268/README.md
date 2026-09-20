# 02-268 — bound propagation at the conic tree's nodes

2026-09-20. Refused. Nothing in `src/` changed.

## What was built

`propagate_bounds` of `src/mip.c` was renamed `jm_propagate_bounds`, declared
in `src/jaos_internal.h` and given a `bool all_cols` parameter: with it set,
a continuous column's bound is tightened too, without the rounding the
integer columns get. `src/conictree.c` then ran it at every node but the
root, `CT_PROP_ROUNDS = 4` rounds, over the node's own bounds, with the
cone rules added by `ct_prop_cones`: a quadratic cone's head is at least the
Euclidean norm of the tail's widest corner, and a tail member is capped by
the head's upper bound. A node whose propagation emptied a box was closed
without a relaxation solve. `CT_PROP_MOVE = 1e-9` was the smallest move
counted. Two counters, `propagated` and `pruned`, went to the summary log.

## The reading

Four CBLIB mixed-integer instances, work limit 3e9, against the committed
runs of `bench/measurements/02-263/`.

| instance | nodes before | work before | nodes after | work after | nodes propagated | nodes closed |
|---|---|---|---|---|---|---|
| sssd-strong-20-4 | 947 (at 1e10) | 3150235092 | 829 (limit) | 3000109529 | 828 | 0 |
| robust_50_1 | 70 | 1776638441 | 70 | 1781178732 | 69 | 0 |
| uflquad-nopsc-10-100 | 498 (at 1e11) | 40542561667 | 33 (limit) | 3002195527 | 32 | 0 |
| turbine07 | 15 | 22104687 | 15 | 23128778 | 14 | 0 |

Propagation moved a bound at almost every node it saw and closed none. The
two instances that finish inside the limit pay 0.26% and 4.6% more work for
the same tree. A first version that tightened integer columns only gave the
same result, 0 closed, turbine07 at 23121806.

## Why it closes nothing

The branching bound the tree adds is on an integer column. A conic
relaxation's other columns are continuous and their bounds are mostly
infinite, so a linear row gives no finite deduction to carry back. The
deductions that do appear come from the cones and are weak: the head's lower
bound moves by a few units where the head's upper bound is thousands. None
of them empties a box, and the walk that follows was going to find the same
infeasibility anyway.

## The refusal

`bench/refusals.txt`, line `conic-node-propagation`.
