# 02-393 — generated MIQPs, and propagation rounds with the prune window

Taken on 2026-10-21 on the tree of 9707654. `read.sh` takes the
readings.

## The generator

`miqp.c` builds convex MIQPs with brute force as the judge: 3 to 8
columns, 1 to 3 integer columns boxed around a planted point, 0 to 5
linear rows, and an objective `Q = D + B'B` with full cross terms, often
singular, so some models are unbounded. Every fifth model has no integer
point. Until now only one model in 27 of 02-255's set was a plain MIQP,
and its `Q` was diagonal.

On seeds 1 to 6 (`new/miqp.txt`) every model passes: 6000 models, none
unsure, none failed.

## Rounded equalities

The first version of `miqp.c` rounded each equality's side to 2^-20, so an
equality over integer columns alone can miss the planted point by up to
about 2e-7. On seeds 1 to 3 (`head/miqpr.txt`) 11 models failed, all with
the tree answering `infeasible` where brute force found an optimum or a
ray. `read.sh` keeps that version as `miqp-round.c`.

Model 500 (seed 1): R1 holds only the integers C1 and C3, and C1 = C3 = 1
misses it by 4.1e-8, inside even the absolute tolerance 1e-7. Propagation
bounded C3 by 0.99999998 and rounded that down to 0 with a window of
`MIP_PROP_SLACK` (1e-9) times (1 + |bound|), and the node was infeasible.
The prune test beside it keeps any row within `MIP_PROP_INFEAS` (1e-7)
times (1 + |side| + |activity|), so rounding threw away a value the prune
test keeps.

## The change

In `implied_box` and in the node propagation of `src/mip.c`, an integer
column's propagated bound now keeps a window of at least
`MIP_PROP_INFEAS` times (1 + |side| + |the rest of the row|) over |a_ij|
before it is rounded. `tests/data/mip_prop_window.mps` is model 500.

## The readings

`read.sh`: HEAD against the tree.

| set | HEAD | the change |
|---|---|---|
| `miqp.c`, seeds 1 to 6 | all pass | the same lines and digests |
| `miqp-round.c`, seeds 1 to 3 | 11 failed | 4 failed; work +28%, +14%, +26% |
| 02-328's fcnet, 02-235's certs, 02-326's discrete, 02-255's misocp, seeds 1 to 3 | | the same lines and digests |
| `make miplib J=2` | | 24 of 24, the same results file |

On the rounded set, values that propagation used to cut off now reach a
node's solve, which costs the extra work. A window of `MIP_PROP_INFEAS`
over |a_ij| alone, closer to what the node solves accept, left 5 failed
there at +13%, +5% and +17%. The wider one is kept, since it matches the
prune test. Of the 4 left, model 560 (seed 1) is an equality on one
integer column that the column's integer value misses by 1.84e-7; the
node's solve refuses that with C1 fixed, and brute force's barrier takes
it.
