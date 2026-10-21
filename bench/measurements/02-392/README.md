# 02-392 — the third push drops the duals' free shift

Taken on 2026-10-21 on the tree of 2d208d7. `read.sh` takes the
readings.

## Seeds 1018 and 1374

A circulation's rows sum to zero, so its duals can all move by the same
amount (times the row scale) and the reduced costs stay the same. The
barrier's rows are regularised, so nothing holds that shift, and it
drifts: on seed 1018 every dual ends near 1.68e9, on seed 1374 near
1.26e9. Each reduced cost is then a difference of two such numbers, at
about 2e-7 precision, and on seed 1018 the polished point holds x23 at its
upper bound 0 with a reduced cost of +0.513, the wrong sign. Taking the
shift out before the push lets it settle on both.

## The change

`bx_drop_dual_shift` in `src/barrier.c` factors the rows' normal matrix
with every free column at weight 1 and regularisation `QP_PUSH_DELTA`,
and applies `delta (E E' + delta I)^-1` three times to the duals. What
survives is their part that the rows' transpose sends to zero; it is
subtracted only when `E'` maps it under `QP_PUSH_TOL` times its size.

02-391 ran the third push right after the first two. On QPLIB_9002 it
then found a shift, ran 8 rounds and failed, and the first polish passed
as before: 6.4% more work for the same answer. Every model the third push
rescues has a point the checker refuses and a first polish that fails, so
the third push now runs there, between the two polishes. That also gives
QPLIB_5527 and the other two MIQPs that end `numerical_error` their work
from before 02-391. The third push runs when it drops a shift or frees a
row. `tests/data/qp_dual_shift.mps` is seed 1374.

## The readings

`read.sh`: HEAD (02-391's code) against the tree.

| set | HEAD | the change |
|---|---|---|
| 02-248's 6000 generated QPs | | the same lines |
| QPLIB's 11 continuous convex QPs | | the same answers; QPLIB_9002's work back to before 02-391 |
| QPLIB's 17 convex MIQPs at 1e10 | | the same answers; QPLIB_4270, 5527 and 5543 back to their work before 02-391 |
| `netqp.py` at 12 nodes, seeds 1 to 400 | 15 `numerical_error` | 14; work 1.0001x over the 385 optimal both ways |
| `netqp.py` at 12 nodes, seeds 1001 to 1400 | 9 | 7; 1.0001x |
| `netqp.py` at 40 nodes, seeds 1 to 120 | 3 | 2; 1.000x |
| `netqp.py` at 40 nodes, seeds 1001 to 1200 | 2 | 2; 1.0001x |
| `netqpb.py` at 12 nodes, seeds 1 to 400 and 1001 to 1400 | all optimal | the same; 1.0001x and 1.000x |
| `make maros-meszaros` | 137 of 138 | the same; powell20's work back to before 02-391 |

25 circulations still end `numerical_error` (TODO row J24). Of the eight
left from 02-391's sweep, six stop in the barrier itself after 35 to 211
iterations, before any push; seed 1308 and 40-node seed 1076 reach the
push and it fails. The six stop on `BARRIER_DIVERGE_QP` (1e10): on seed
1156 the iterate reaches 2.96e7 and 3.88e10 times the data at iteration
25, with the objective at -2.77e17 and still falling. With the limit at
1e13 or 1e16 the walk goes on, all six still end `numerical_error`, and
five of them spend about twice the work. The message of that stop printed
the LP's limit, 1e6, for a QP; it now prints the limit it applies.
