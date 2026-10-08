# 02-367 — a push that does not settle is tried again from the plain active set

Taken on 2026-10-08 on the tree of 395e4d6, for TODO row J20.

## The defect

Four of 400 bounded circulations of 02-366's `netqpb.py` (seeds 163, 182,
231 and 353) ended `numerical_error`. On seed 163 the barrier converges
well (gap 1.4e-13). The push then cycles: round 1 leaves the rows 1114
times their tolerance off and releases 10 pins, the next two rounds pin
them again at step 0, and the cycle repeats until round 40. The polish
that follows leaves free columns with reduced costs of 1e-7 to 2.5e-4,
which only a move of the columns can remove, and the conic walk fails too.

The push's start pins a variable whose distance to a bound is under its
dual slack, or under `QP_PUSH_NEAR` times `1 + norm_b` when the dual slack
is not tiny against the distance. `norm_b` reaches 1e6 here, so the second
clause pins variables that sit well inside their box, and the rows cannot
be met with those pins.

## The change

When the push and the longer walk after it both leave the push unsettled,
`qp_push` runs once more with `push_plain` set: the start pins only the
variables whose distance to a bound is under their dual slack. The polish
still runs after it when this push does not settle either.

## The readings

Against 395e4d6:

| reading | result |
|---|---|
| `netqpb-12-30.txt`, 400 bounded circulations | the 4 `numerical_error` end `optimal` and pass the checker; work 0.992x over the 396 optimal both ways |
| `netqp-12-30.txt`, 02-365's 400 | 2 of 21 `numerical_error` end `optimal` and pass the checker; work 0.996x |
| `netqp-40-100.txt`, 02-365's 120 larger | 1 of 4 `numerical_error` ends `optimal` and passes the checker; work 0.963x |
| QPLIB's convex QPs (`cqp-*.txt`) | the same answers; QPLIB_9002 takes 1.84e9 work units against 1.50e9, because the extra push runs before its polish |
| QPLIB's 17 convex MIQPs (`miqp-*.txt`) | the same statuses; QPLIB_4270, 5527, 5543 and 5577, which end with no answer either way, take 1.01x to 1.32x the work |
| 02-248's 6000 generated QPs (`gen-*.txt`) | the same |
| Maros-Meszaros (`maros-meszaros.txt`) | 0 regressed, 0 improved; `powell20` takes 1.26x the work, because the extra push runs there and does not settle either, and the barrier's point stands as before (`bench/results/maros-meszaros.txt`) |

`tests/data/qp_plain_push.mps` is seed 163.

With this push in place, the polish of 02-365 fires on no generated model
any more: none of 4400 more circulations reaches it (`netqp.py` at 12, 20
and 40 nodes, 600 seeds each; `netqpb.py` seeds 401 to 1200; and 1800 with
flows up to 1e7, 1e8 or 1e9 and floors of -1e8 or -1e10). QPLIB_9002 is the one model that
needs it, and 02-318's `qpread.sh` reads it. `tests/data/qp_polish.mps`,
written for the polish, now settles in this push, and its test asks only
that the answer passes the checker at 1e-7.
