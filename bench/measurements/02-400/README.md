# 02-400 — a push row's tolerance never falls below its rounding

Taken on 2026-10-21 on the tree of f5d802f. `read.sh` takes the readings,
`floor.sh` the constant's sweep.

## Seed 1308

On 02-365's circulation seed 1308 (12 nodes) the barrier converges, and
every push fails. The push's first full step leaves two rows off by
1.4e-6 against a tolerance of 4.1e-7. It frees two pins; the next rounds
pin them again, and that repeats for 40 rounds. Those rows carry flows
near 1e10, so a residual of 1.4e-6 is at the rounding of their sums. A
push row's tolerance is 1e-7 of its traffic, but capped at `QP_PUSH_TOL`
times `1 + max(|b|, |bounds|)`, here 4.1e-7, and that cap does not grow
with the flows.

## The change

In `qp_push` and `bx_polish` in `src/barrier.c`, a row's tolerance is never
below `QP_PUSH_ROUND` (1e-13) times its traffic. `tests/data/qp_far_rows.mps`
is seed 1308. On 02-398's seven circulations the floor at 1e-14, 1e-13 and
1e-12 reads the same: seed 1308 optimal at -2.4705451e13 in 244812 work
units, the other six as before (`floor.txt`). That objective is HiGHS's
point plus the fall `cycle.py` finds along its cycle.

## The readings

`read.sh`: HEAD against the tree.

| set | HEAD | the change |
|---|---|---|
| 02-248's 6000 generated QPs | | the same lines |
| QPLIB's 11 continuous convex QPs | | the same lines |
| QPLIB's 17 convex MIQPs | | the same lines |
| `netqp.py` at 12 nodes, seeds 1 to 400 | 14 `numerical_error` | 13, seed 170 optimal; work 1.0000x over the 386 optimal both ways |
| `netqp.py` at 12 nodes, seeds 1001 to 1400 | 7 | 6, seed 1308 optimal; 0.9932x |
| `netqp.py` at 40 nodes, seeds 1 to 120 | 2 | 2; 0.9997x |
| `netqp.py` at 40 nodes, seeds 1001 to 1200 | 2 | 2; 0.9992x |
| `netqpb.py` at 12 nodes, seeds 1 to 400 and 1001 to 1400 | all optimal | the same; 1.0000x |
| `make maros-meszaros` | 137 of 138 | the same; stadat1 0.989x work, its rows 2.7e-9 off against 9.1e-10, 1.3e-16 of their traffic |

23 circulations still end `numerical_error` (TODO row J24). On seed 1282
the walk stops on `BARRIER_DIVERGE_QP`. With 02-398's switches lifting
that stop and pushing at the stall, the push gets the pins right by round
9 and then stalls: free columns with curvatures near 1e-9 keep reduced
costs while the push's proximal term, `QP_PUSH_REG` (1e-6), is larger
than their curvature (`push1282.txt`, from `push1282.sh`).
