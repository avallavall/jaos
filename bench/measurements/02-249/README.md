# 02-249 — the five convex QPs the barrier could not settle

02-248 left five of its 6000 generated convex QPs ending `NUMERICAL_ERROR`,
before the push and after it, since the push only runs on `OPTIMAL`. They
are the barrier's own, and this is the reading behind the two rules that
solve them. The harness is 02-248's; the five models are
`fail-s1-m453`, `fail-s3-m761`, `fail-s4-m930`, `fail-s5-m460` and
`fail-s6-m951` of its six seeds, and two of them are in `tests/data/` as
`g_qp_stall.lp` and `g_qp_pivots.lp`.

## Two shapes

**Two walks stall.** On m453 and m930 the primal residual is 1e-13 from
the fourth iteration on, and then mu goes 0.32, 0.72, 0.32, 0.72 for the
remaining 190 iterations, the step lengths 0.54 and 0.60 in turn, sigma
0.32 and 0.075. The dual residual on a quadratic model after two step
lengths is `(1 - ad) rd + (ap - ad) Q dz`, and nothing drives the second
term down; `bench/refusals.txt` (barrier-qp-equal-steps) measured one
step length for both sides on 2026-09-10, found it cures the node it was
built for and costs 8 MIQP nodes and 3 QPs elsewhere, and named the
reopen condition: the same step taken only where the walk has stalled.
That alone solves m453 but not m930, whose cycle is one of centring: with
equal steps the walk still alternates between mu 0.32 and 0.72, so from
the stall on sigma is floored too.

**Three walks lose pivots.** On m761, m460 and m951 the walk converges
normally to a dual residual of 1e-5 or better, then the quasi-definite
LDL of the augmented system replaces a pivot at `BARRIER_AUG_FLOOR`
(1e-30) and the next direction is wrong by 1e30: the dual iterate jumps
from 7 times the data to 5.6e5 on m761 and to NaN on m460, and the walk
is handed to the dual simplex as diverged. The primal regularisation at
that point is `BARRIER_REG` times the worst residual, 1e-14, so the top
block is as good as singular where `Q` is. Now a replaced pivot refactors
the system with a floor of `BARRIER_REG_RETRY` under the regularisation,
kept for the rest of the walk and multiplied by `BARRIER_REG_GROWTH` on a
further replacement, up to `BARRIER_REG_MAX`.

## The reading

| model | before | after |
|---|---|---|
| m453 | 200 iterations, `NUMERICAL_ERROR` | 37 iterations, optimal |
| m761 | diverged at 11 | 12 iterations, optimal, one refactoring |
| m930 | 200 iterations, `NUMERICAL_ERROR` | 47 iterations, optimal |
| m460 | NaN at 11 | 11 iterations, optimal |
| m951 | diverged at 11 | 11 iterations, optimal |

Over the six seeds of 02-248's harness, 6000 of 6000 models now end
`OPTIMAL`, the checker takes every answer on both sides, and the work is
355.15M units against 354.10M with the five left unsolved, 0.3% more, all
of it on the five and on walks the stall rule catches late.

## The constants

Seeds 1 to 3, everything else at its value. Every setting below solves
every model; the column that moves is the work.

| constant | value | work against 5 / 0.5 / 1e-4 |
|---|---|---|
| `BARRIER_STALL_ITERS` | 3 | 1.012x |
| | **5** | **1.000x** |
| | 10 | 0.999x |
| `BARRIER_STALL_SIGMA` | 0.3 | 0.997x |
| | **0.5** | **1.000x** |
| | 0.7 | 1.006x |
| `BARRIER_REG_MAX` | 1e-6 | 1.000x, byte-identical |
| | **1e-4** | **1.000x** |
| | 1e-2 | 1.000x, byte-identical |

On the five models alone, `BARRIER_STALL_SIGMA` at 0.2 leaves m453
unsolved and 1.0, pure centring from the stall on, leaves m453 and m930;
0.3 and 0.5 solve all five, and 0.5 stays for the margin over the edge.
`BARRIER_STALL_ITERS` at 3 fires on walks that were going to converge and
costs 1.2%; 10 reads level with 5. `BARRIER_REG_MAX` is never reached on
the set: one growth of the floor, to 1e-6, is what m761 needs.

## The pass is not vacuous

02-248's before table holds the five failures; the same harness against
the library at d83b88e, the push without these rules, reports them
again. `tests/cli.sh` solves both `tests/data` models and checks their
objectives, and `test_quadratic.c` holds m930 as
`test_a_stalled_barrier_recentres_and_finishes`.

## How to run

```
make all cli
bench/measurements/02-248/push.sh            # six seeds, then the CLI leg
```
