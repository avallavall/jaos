# 02-287 — the crossover push

Taken on 2026-09-22 on the tree of 2e04b47 with TODO row B6 applied, by
`bench/barrier` on the standard 94 at the default cap of 10x the dual's
work per instance. Each arm is one build of the same tree:

| arm | build | agreed | overrun | barrier / dual work, geometric mean | iterations (barrier + 1) / (dual + 1) |
|---|---|---|---|---|---|
| control, the guess to the dual | `-DJAOS_CROSS_PUSH_VALUE=0` | 77 | 17 | 3.2602 | 0.4437 |
| the push, then the primal | the default | 80 | 14 | 3.1775 | 0.2567 |
| the push, then the dual | `-DJAOS_CROSS_PUSH_PRIMAL_VALUE=0` | 79 | 15 | 3.3449 | 0.3068 |

The files are `barrier-control.txt`, `barrier-push-primal.txt` and
`barrier-push-dual.txt`.

## What the push does

The crossover ranks the barrier's point into a basis guess, as before. The
push then puts every nonbasic column on one of its bounds. A column whose
point sits within `CROSS_PUSH_SNAP` of a bound is snapped there. Every other
nonbasic column moves toward its nearer bound, along the direction that
keeps the rows satisfied, and the basic columns move with it. A two-pass
ratio test (Harris) finds the first basic column to reach its own bound.
That column leaves the basis, the moving one enters, and the LU is updated.
When no basic column blocks, the moving column reaches its bound. The result
is a basis whose point is primal feasible, so the primal simplex finishes
from it. This is the primal half of Bixby and Saltzman's push.

The push gives up and leaves the guess to the dual simplex, as before, when
the guess does not have one basic per row, when the guess or a basis during
the push factors singular, or when a free column has no basic column to stop
it.

## What moved

Three overruns finish under the push: d2q06c (61503 simplex iterations
after the barrier become 5846, 2.74x the dual's work), pilot87 (41925 become
23020) and tuff. truss goes from 17667 iterations to 303 and grow22 from
16715 to 692. Some small instances get dearer: scrs8 1.994x the control's
work, brandy 1.932x, woodw 1.762x, ship04l 1.750x.

The dual after the push reads worse than the control, 3.3449, and leaves
d2q06c and pilot87 over the cap. The primal after the push is the arm that
lands.

## The bar

TODO B6 set the bar at fewer than 21 overruns and a geometric mean under
2.705x the dual. Both numbers were read before the aggregator (2e04b47) made
the dual cheaper, which moved the control on the same set to 17 overruns at
3.2602x. Against that control the push reads 14 overruns and 0.975x the
control's mean.

## The readings

`bench/results/barrier.txt` is retaken on the final tree and reads the
default arm above, 80 agreed and 14 overruns at 3.1775x. The one before it,
73 and 21 at 2.7048x, was taken before the aggregator made the dual
cheaper. `bench/results/barrier-infeas.txt` is retaken too: it had not been
read since the aggregator either, and its changes are the dual's, since the
barrier hands those models to the dual simplex before any crossover. The
three LP gates are byte-identical to the baselines.
