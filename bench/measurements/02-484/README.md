# 02-484 — a walk that lifts a column off the bound that closes its rows

Taken on 2026-11-18 on the tree of 1235543, for TODO row J24. `read.sh`
takes the readings, the tree of 1235543 (base) against the change (lift).
`arms.sh` builds 1235543 with `knobs.patch`, which reads each part of the
change and a few more switches from the environment, and solves the 30
circulations under each arm (`arms.txt`).

## Why the walk stops

At 1235543, 30 of 02-365's circulations end `numerical_error`: seeds 18,
25, 64, 110, 127, 195, 199, 201, 287, 288, 1104, 1156, 1163, 1282, 1340,
1357, 1411, 1450, 1458, 1504, 1514, 1535, 1537, 1551, 1596, 1624, 1735 and
1746 at 12 nodes, 95 and 1076 at 40.

On seed 1156 the far cycle (arcs x5, x13, x16, x29) jumps to flows of
1.5e13 at iteration 3, and from there mu, the mean of the products of
slack and dual, is set by those arcs: 2.6e16 at iteration 4, 6.7e14 at
iteration 25. The central path at such a mu gives every column with a
small box a dual of mu over its slack. Rows n0, n3, n9, n10 and n11 carry
only such columns. By iteration 25 their duals are 5e12 to 2.4e13 and
their columns' bound duals as large, so each column's weight in the
normal matrix, slack over dual, is 1e-13 to 1e-11, under the rows'
regularisation `BARRIER_DELTA` (1e-10). On those rows the Newton step
solves `delta dy = r`: it moves their duals by r / delta and their flows
hardly at all. Each of the five rows' dual is 2.2e11 to 2.8e11 times its
residual. The duals pass 1e10 of the data and the growth stop ends the
walk at iteration 25, with the primal residual at 2.8e-4 and falling.

With the stop lifted (02-398's `J24FAR`), the walk takes full steps from
iteration 28, but four of the five rows keep residuals of 0.2 to 5.5
while mu falls: every column of those rows sits at a bound with a slack
of 5e-4 to 3e-3 and a dual near 1e13 by iteration 40. Row n3 needs its
arc x25 at -5.49, and x25 sits at its bound 0.

## The change

Three parts, in `src/barrier.c`, for a QP walk only.

1. **The lift** (`bx_lift`). Every `BARRIER_STALL_ITERS` stalled
   iterations while the primal residual is above the walk's tolerance and
   the duals are past `BARRIER_DIVERGE` (1e6) times the data, and when the
   growth stop is about to fire, the walk looks for rows that only the
   regularisation moves. A column whose weight in the normal matrix,
   times its largest entry squared, is under delta is held at the bound
   its barrier term weighs most. The lift goes on only when a row off by
   more than the tolerance has only held columns. `g` is the rows'
   residual with the held columns on those bounds, and
   `u = (delta (A Θ A' + delta I)^-1)^3 g` is the part of it only delta
   moves: the closed rows' residual, the same three solves the push's
   search for the duals' shift takes, on the walk's last factor. Moving the
   row duals along `u`, with the held columns' bound duals taking up the
   change, keeps the dual residual and raises the dual objective; a ratio
   test on the held bound duals that fall along it picks
   the column to lift. That column moves off its bound by `u'g` over its
   entry of `A'u`, at most half its slack to its other bound, its slack
   and dual keep their product, and the duals move until its dual is
   that. A round lifts one column and refactors; a lift stops when `u` is
   under the tolerance, when the column's slack is already larger than its
   move, when the column would stay held, or when the move would raise the
   largest row dual. At most one round a row.
2. **The growth stop reads the rows.** For a QP, a dual iterate past
   `BARRIER_DIVERGE_QP` is divergence only while the relative primal
   residual is above `BARRIER_DIVERGE_FEAS` (1e-2). A growing primal iterate
   stops the walk as before, and the LP walk is unchanged.
3. **The second leg's last point.** When the push does not settle at
   `BARRIER_TOL`, the walk goes on toward `BARRIER_TOL_QP`. When it stops
   short of that within `BARRIER_TOL`, its last point now stands, and the
   plain push and the polishes after it start there; before, the
   `BARRIER_TOL` point was put back. On these circulations the rows carry
   1e13 and their sums round near 1e-3, so the primal residual stays near
   1e-8 of the data; the last point's mu is far smaller, and on seed 1156
   the checker passes it as it stands. `knobs.patch`'s `J24KEEP` also
   pushes once from that point; on the 30 that push never settles, and
   dropping it reads the same on every circulation set for less work
   (`keep-nopush-*.txt`, the form with the push against the landed one:
   0.9985x, 0.9992x, 0.9972x and 1.0000x, never dearer). QPLIB_9002 takes
   2.09e9 work units with that push and 1.77e9 without.

On seed 1156 the lift opens four columns at iteration 41, and the gap
falls from 4.3e-4 to 1.9e-5; it opens one more at iteration 50, and the
gap falls to 6e-10. The walk reaches 1e-8 at iteration 53, at mu 5.4e5.
The push does not settle; the second leg's primal residual stays near
1e-8 for 50 iterations while mu keeps falling, and its last point,
3.5e-9 off, objective -3.1183875860e17, passes the checker with a gap of
3e-17. HiGHS calls -2.34e15 optimal, and from its point the objective
falls to -1.95e17 along one cycle alone (`02-398/`, `r398-highs.txt`).

## The arms on the 30

`arms.txt`, from `arms.sh`; the lift there is the arms' form, before the
gates of the next section, and the second leg's last point is also pushed
once:

| arm | solved | seeds |
|---|---|---|
| 1235543 | 0 | |
| the last point alone | 5 | 25, 64, 1450, 1596, 40-node 95 |
| the growth rule alone | 3 | 18, 287, 1357 |
| the lift alone | 1 | 1551 |
| last point and growth rule | 11 | the eight of those two, 199, 1340, 1735 |
| last point and lift | 8 | 25, 64, 201, 1282, 1450, 1551, 1596, 95 |
| growth rule and lift | 4 | 18, 287, 1357, 1551 |
| all three | 14 | 18, 25, 64, 199, 201, 287, 1156, 1340, 1357, 1450, 1551, 1596, 1735, 95 |
| all, the rule at 1e-5 | 8 | as last point and lift |
| all, at 1e-4 | 10 | 1357 and 1735 more |
| all, at 1e-3 | 13 | 18, 287 and 1156 more; at 1e-2, 199 and 1340 in place of 1282 |
| all, at 1e-1 | 14 | as at 1e-2 |
| all, the growth stop off for a QP | 15 | and 1163 |
| all, the rule on a growing primal too | 14 | as at 1e-2 |
| all, a column lifted though its slack exceeds its move | 14 | the same |
| all, a lift though it raises the largest row dual | 14 | the same |
| all, a lift tried every stalled iteration | 15 | and 1282 |

The lift adds 201, 1156 and 1551 to the other two parts. Seed 1340 ends
at -8.1064267e12 and 1357 at -6.3469336e14, the values 02-398's switches
found. On 02-365's seeds 1 to 400, a lift tried every stalled iteration
without the largest-dual test broke seed 380, which reached the
iteration cap after a lift at iteration 14 had moved the row duals by
2e13; once every `BARRIER_STALL_ITERS` and with the test, the set reads
clean.

## What the lift costs

The arms' lift (`knobs.patch`) tries every `BARRIER_STALL_ITERS` stalled
iterations and factors the normal matrix each time. Read on QPLIB's convex
MIQPs at 1e10 work units, that form left every status as it was, but five
trees reach fewer nodes in the budget: 3694 4 against 7, 3698 19 against
25, 3792 41 against 48, 3861 7 against 10 and 3871 134 against 153, with
weaker bounds (`cost-miqp-every-stall.txt`). Their node walks stall for
much of 200 iterations and never lift a column: on the fourth form below,
a build that runs the lift and never applies it reads the same node
counts as the lift itself. On Maros-Meszaros the first form costs
1.0024x, liswet12 1.073x at most, with every status as before.

The forms tried to take that cost out (`cost.txt`: the 30 solved, and
nodes and bounds on eight of the MIQPs, for each):

| form | the 30 | 3694 | 3698 | 3792 | 3861 | 3871 |
|---|---|---|---|---|---|---|
| 1235543 | 0 | 7 | 25 | 48 | 10 | 153 |
| every 5 stalled iterations, its own factor (the arms) | 14 | 4 | 19 | 41 | 7 | 134 |
| one try a stall, its own factor | 13, loses 1551 | 7 | 24 | 47 | 9 | 151 |
| every 5, the first round on the walk's last factor | 15, wins 1535 | 6 | 22 | 45 | 8 | 145 |
| that, only when a row off by the tolerance in `g` has only held columns | 14 | 7 | 24 | 47 | 9 | 151 |
| that test on the walk's own residual, before any sweep | 14 | 7 | 24 | 47 | 10 | 152 |
| that, only while the duals are past 1e6 of the data (landed) | 14 | 7 | 25 | 48 | 10 | 153 |

On QPLIB_3792 at 3e9 work units the next-to-last form tries 717 lifts and
lifts nothing, at 0.6% of the work. Every lift on the 30 came with the
duals at 2.4e6 to 3e9 of the data, so the last gate keeps them all and
leaves the MIQP trees' node walks alone. The fourth form's extra win,
seed 1535, needs a lift the row test skips.

## The readings

`read.sh`, 1235543 against the change:

| set | against 1235543 |
|---|---|
| 02-365, 12 nodes, seeds 1 to 400 (`nq-12.txt`) | 18, 25, 64, 199, 201 and 287 end optimal and checked, from `numerical_error`; 4 still `numerical_error`; none lost; work 0.9983x |
| 02-365, 12 nodes, seeds 1001 to 1800 (`nq-12n.txt`) | 1156, 1340, 1357, 1450, 1551, 1596 and 1735 the same; 11 still `numerical_error`; none lost; 0.9977x |
| 02-365, 40 nodes, seeds 1 to 120 (`nq-40.txt`) | 95 the same; none left; 1.0000x |
| 02-365, 40 nodes, seeds 1001 to 1200 (`nq-40n.txt`) | 1076 still `numerical_error`; 1.0000x |
| 02-366, 12 nodes, 1,200 seeds (`nqb-12*.txt`) | all optimal and checked on both; 1.0000x |
| 02-248's 6,000 generated QPs (`gen-*.txt`) | the same lines |
| QPLIB's 11 convex QPs (`cqp-*.txt`) | the same answers; QPLIB_9002 0.964x the work, its rows 3.9e-8 of their traffic against 7.5e-8 |
| Maros-Meszaros (`mm-cmp.txt`, `mm-baseline.txt`) | the same statuses, 0 regressed against `bench/maros-meszaros.baseline`; work 1.0000x, dtoc3 1.0007x the most; powell20's point now has a certified suboptimality of 3.2e-14 against 8.4e-9 |
| QPLIB's 17 convex MIQPs at 1e10 (`miqp-*.txt`) | the same statuses, incumbents, bounds and node counts; eight lines move in the work units past the limit |

The work is the geometric mean over the models optimal on both trees.
On the circulations every model optimal on both moves by under 0.05%
but seven: seed 1586 at 12 nodes takes 1.585x, where a lift at iteration
20 sends the walk through its second leg to that leg's last point, and
1018 1.002x; 1007, 1710, 252, 1027 and 211 take 0.36x to 0.93x, keeping
the second leg's last point.

02-398's `read.sh` on the change (`r398-*.txt`): of its seven seeds,
1156, 1340, 1357 and 1308 end optimal, and 1104, 1163 and 1282 end
`numerical_error`. Of 02-398's switches on top, one refinement step on
the normal equations (`ref1`) finishes 1104 and 1282 as well, and with
the growth stop lifted (`far1e30+ref1`) all seven; before the lift, that
step finished only 1357.

## What is left

16 circulations still end `numerical_error`. Seven stop on the growth
stop: 288, 1411 and 1537 on a primal iterate past 1e10 of the data, 110,
1163, 1514 and 1746 on a dual one while their rows are still off by 1.6e-2
to 2.4. Seeds 127 and 1535 reach the iteration cap. On the other seven
(195, 1104, 1282, 1458, 1504, 1624 and 1076 at 40 nodes) the walk
converges and no push or polish gives a point the checker passes. TODO
row J24 holds them.
