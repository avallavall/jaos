# 02-391 — a row its pinned columns break frees one of them

Taken on 2026-10-21 on the tree of 9a2aee4. `sweep.sh` takes the sweep,
`read.sh` the readings.

## The sweep

The brute-force generators of 02-235 to 02-248 on seeds 7 to 12, 38000
models (`sweep/a.txt`, `b.txt`, `c.txt`): every check passes. 02-366's
bounded circulations on seeds 1001 to 1400 and 02-371's QCQPs on seeds
1001 to 1150 at three sizes (`sweep/nqb.txt`, `qc.txt`): all optimal and
taken by the checker. 02-365's circulations (`sweep/nq.txt`): 12 of 400
at 12 nodes and 2 of 100 at 40 nodes end `numerical_error`. `fcnet.sh`,
taken later on de89dcc: 02-328's fixed-charge networks on seeds 2 to 7,
6000 models, none unsure and none failed (`sweep/fcnet.txt`).

## Seed 1019

The barrier converges at iteration 15: primal 2.2e-16, dual 5e-16, gap
8.4e-11, objective -2795998294, which HiGHS 1.15.1 also reaches. The
checker refuses the point's duals: x12 sits 1.3e-7 below its bound 0 with
a reduced cost of -3705, so it is not on the bound and the reduced cost
counts. The push and the polish should move such columns onto their
bounds, and both fail.

Row n11 holds only x10 and x11, so x10 = x11. x11 sits on its lower bound
0.582041; x10's lower bound is 0.568218, so x10 lies 0.0138 above its
own. The barrier's dual slack on that bound is 0.0358, larger than 0.0138,
so the push and the polish both read x10 at its bound and move it there.
Row n11 then has every column held and is broken by 0.0138. The polish
leaves a row with no free column out of its system, so the other rows
take the error and end 2958 times their tolerance. The push's first full
step leaves every row off by up to 1.38e-2, it releases ten pins, the next
step pins them again, and that repeats for 40 rounds.

## The change

`bx_free_broken_rows` in `src/barrier.c` finds each row whose columns
are all held, and that its held values break by more than 1e-7 of its
traffic. It frees the held column that moved the most from the barrier's
value, and repeats until no such row is left. A first version ran it in
every push and polish; it broke `tests/data/qp_push_checked.mps`, where
the barrier reads x18 at its bound 3.59 away and the first push settles
from there. So it runs only where the old flow has failed:

- a third push, after the plain push and the push from the variables the
  barrier reads at a bound, of at most `QP_PUSH_FREE_ROUNDS` (8) rounds;
- a second polish of a refused point.

Each is skipped when no row is freed. On the four circulations it
rescues, the third push settles in 1 to 6 rounds. With 40 rounds it added
17% to QPLIB_5527's work, where it fails; with 8 it adds 5.2%.
`tests/data/qp_held_row.mps` is seed 1019.

## The readings

`read.sh`: HEAD against the tree.

| set | HEAD | the change |
|---|---|---|
| 02-248's 6000 generated QPs | | the same lines |
| QPLIB's 11 continuous convex QPs | | the same answers; QPLIB_9002 +0.002% work |
| QPLIB's 17 convex MIQPs at 1e10 | | the same answers; QPLIB_5527, `numerical_error` both ways, +5.2% work, QPLIB_4270 and 5543 under +0.03% |
| `netqp.py` at 12 nodes, seeds 1 to 400 | 19 `numerical_error` | 15; work 0.997x over the 381 optimal both ways |
| `netqp.py` at 12 nodes, seeds 1001 to 1400 | 12 | 9; 0.999x |
| `netqp.py` at 40 nodes, seeds 1 to 120 | 3 | 3; 1.000x |
| `netqp.py` at 40 nodes, seeds 1001 to 1200 | 3 | 2; 0.998x |
| `netqpb.py` at 12 nodes, seeds 1 to 400 and 1001 to 1400 | all optimal | the same; 0.997x and 0.999x |
| `make maros-meszaros` | 137 of 138 | the same; powell20 +0.07% work |

The 29 circulations left are TODO row J24: their optimum lies far out,
and the push cannot bring the reduced costs of the free columns with tiny
curvature to zero. HiGHS solves seed 1018, at |x| up to 2.5e8, and also
fails on seed 1053 at 40 nodes, which this change solves.
