# 02-509 — a QP walk that converges on a point the checker refuses walks again with a refinement step on the normal equations

Taken on 2026-11-25 on 0814fa80, for TODO row J24. 02-484 left 16 of
02-365's circulations at `numerical_error`; on seven of them (195, 1104,
1282, 1458, 1504, 1624 and 40-node 1076) the walk converges and no push
or polish gives a point the checker passes. On 02-398's seeds one
refinement step on the normal equations (`J24REF=1` in 02-398's
`knobs.py`) finished 1104 and 1282 (`02-484/r398-knobs.txt`).

## The step

`refine.py` builds the step into `newton`: after the solve of
`(A Θ A' + δ I) dy = r`, the residual `r - (A Θ A' + δ I) dy` is formed
from the walk's own products with `A` and `Θ` and solved once more on the
same factor, and `dy` takes the correction (`BARRIER_QP_REFINE` steps).
It runs on a QP's walk only, never in the push (`pin`) or the polish
(`dec`), whose matrices are not the walk's.

## Three gates, read on 02-484's sets

`read.sh qp|miqp STEPS GATE` builds a base tree and the same tree with
`refine.py` and reads 02-484's sets (records in `read/`). The readings
were taken with 0814fa80 as the base; `BASE` defaults to b08f2c17, the
change's parent, which `refine.py` also applies to.

| gate | circulations solved / lost | work on the rest | Maros-Meszaros | QPLIB convex QPs | 02-248's 6,000 |
|---|---|---|---|---|---|
| every QP Newton step (`ref1`) | +195, 1104, 1282, 1458 / -1572, 40-node 9 | 1.10x to 1.20x | 4 regressed: `stadat1` 6.7x, `cont-101` 2.7x, `cont-201` 2.4x, `liswet8`'s bound (`mm-ref1-regressed.txt`) | same answers | same lines, +0.2% work |
| only while the duals are past `BARRIER_DIVERGE` times the data (`ref1far`) | +1104, 1282 / -1572, 1710 | 1.000x | 1.0089x, 6 dearer, `huestis` 1.55x | `QPLIB_9002` 1.19x | same |
| a second walk with the step after a walk that ends with no answer (`ref1retry`) | +195, 1104, 1282, 1458 / none | 1.0000x | `dtoc3` 2.1x regressed, `ksip` 1.9x, `ubh1` 1.4x | same | same |
| a second walk with the step only after a walk that converged on a point the checker refuses (`ref1refused`) | +195, 1104, 1282, 1458 / none | 1.0000x | 1.0000x, 0 regressed | same | same |

The step changes a walk's last point. Where the base walk's point
passes, the new point can fail the push: on seed 1572 a pinned variable
keeps a reduced cost of the wrong sign after three freeings and every
polish after it fails. Where the base walk ends with no answer, the new
walk is the better start. `dtoc3`, `ksip` and `ubh1` end with no answer
in another way: the walk stops at `BARRIER_MAX_ITER` without converging,
and the conic interior point solves them, so a second walk there is only
cost. Seeds 1504, 1624 and 40-node 1076 converge, are refused and are
refused again after the second walk, which costs them 1.3x to 1.6x.

QPLIB's 17 convex MIQPs at 1e10 under `ref1refused` (`miqp-*.txt`): 15
read as before; `QPLIB_5527` ends its root with bound 30434332.34 where
the base has none, and `QPLIB_5543`'s bound moves in the 14th digit.

## The change

`ref1refused`: `jm_barrier` marks the model when its walk converged and
the checker refused every point after it (`barrier_refused`), and
`jaos_solve` then walks once more with `cfg.barrier_refine` set, under
what is left of the work and time limits, before the conic interior
point. `BARRIER_QP_REFINE` is 1. `tests/data/qp_refined_walk.mps` is seed
1104, which takes the second walk and ends optimal and checked
(`test_a_refused_walk_walks_again_refined`).

Of 02-398's seven seeds, which `nq-12n-*.txt` carries, 1104 and 1282 are
now optimal and 1163 is left; 02-398's `knobs.py` no longer applies to
`newton`, whose solve it patched.

The gates on the change over b08f2c17: `make test` and `make sanitize`
pass, and `make maros-meszaros` reads 0 regressed with its record
unchanged.

## What is left

12 circulations end `numerical_error`: seven on the growth stop (288,
1411, 1537 on a primal iterate past 1e10 of the data; 110, 1163, 1514,
1746 on a dual one), two at the iteration cap (127, 1535), and 1504, 1624
and 40-node 1076, which converge and are refused under both walks. TODO
row J24 holds them.
