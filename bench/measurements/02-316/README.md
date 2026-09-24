# 02-316 — a conic optimum the checker refuses is not published

Taken on 2026-09-24 on the tree of c506afb, for TODO row J3.

## The defect

The conic interior point put its answer to the checker only when its walk
had stopped short (`res.relaxed`). A walk that converged by its own test
published `OPTIMAL` without the checker. On QPLIB's 13 continuous QCQPs
(the `LCD` class, `qcqp.sh` picks them by `jaos stats`: quadratic rows and
no integer column) that published 10 optima of which the checker took 2
(02-256): the other 8 were primal feasible to 2e-13 with duals off by
8.5e-7 to 3.6e-5.

## The change

Outside a tree node, every conic optimum is now put to the checker at the
primal tolerance, and one it refuses ends `NUMERICAL_ERROR` with the
checker's numbers in the message. A node of the conic tree keeps its
rough point as before.

`qcqp.txt`, one line per model:

| outcome | models |
|---|---|
| `optimal`, checker ok | QPLIB_2626, QPLIB_3029 |
| `numerical_error`, published `optimal` before | QPLIB_2456, 2482, 2519, 2784, 2862, 3088, 3105, 3185 |
| `numerical_error` before and after | QPLIB_2468, QPLIB_2676 |
| work limit | QPLIB_3312 |

CBLIB (29 of 29) and Maros-Meszaros write the same lines; `qgrow22`'s
message now comes from the conic solver's own check, which made the check
02-315 had put in `jm_conic_after_barrier` redundant, so it is gone.
`make test` and `make sanitize` pass.

## What is left

A point the checker takes on those 8. The refused duals sit on ball rows
that carry a nonzero multiplier while the row is off its side (3.6e-5 on
QPLIB_3088), and there the Newton finish diverges (`conic-qc-dual-refit`
and `conic-newton-prox` in `bench/refusals.txt`).
