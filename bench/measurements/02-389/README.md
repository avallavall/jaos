# 02-389 — a second certificate program with the quadratic rows left out

Taken on 2026-10-21 on the tree of 8e01614.

## The sweep

02-255 on seeds 13 to 30, 02-253 on seeds 10 to 20 and 02-326 on seeds
17 to 25. 02-253 reads clean. The rest:

| model | what ends unsure or fails |
|---|---|
| 02-255 seed 13, model 456 | three solves stop after 208 iterations with no answer and no ray |
| 02-255 seed 16, model 69 | the tree fails: its relaxation is unbounded, and in the search for a feasible point a node's walk stalls next to an infeasibility certificate the checker refuses |
| 02-255 seed 17, model 245 | an optimum refused, duals off by 8.6e-6 |
| 02-255 seed 17, model 451 | a walk stalls next to an infeasibility certificate the checker refuses |
| 02-255 seed 29, model 903 | the same as model 451 |
| 02-326 seeds 22, 24, 25 | one unsure model each, not read yet |

## Model 903

Its row R2 holds only the fixed column C1, and C1 = -3 puts R2 outside
its range: one row and one bound prove the model infeasible. 02-373's
program missed that certificate. C1 also sits in the quadratic row R5, so
the program took C1 as a column R5 can curve, and with R5's multiplier at
0 that forced C1's coefficient to 0. The checker would have taken C1
through its bounds.

`cm_cert_solve` in `src/conic.c` takes a `plain` flag. When the first
program finds no certificate on a model with quadratic rows, a second one
runs with every quadratic row's multiplier held at 0, so every column is
judged by its bounds.

## Models 451 and 69

Model 451 is infeasible only through its quadratic row R4, which has cross
terms. The checker reads a quadratic row's curvature only when the row is
diagonal, so no certificate in its form exists, and `numerical_error` is
the honest answer. Model 69's failing node is the same case, through its
row R5's cross term C1·C3. That is TODO row J22.

## The reading

`read.sh`: HEAD against the tree, 02-255 on seeds 1 to 4, 13, 16, 17 and
29.

- 02-255 (`mip-*.txt`): seed 29's model 903 is settled and its brute
  force sure; every other line, digest and work figure reads the same.
- 02-253 (`gen-*.txt`) and QPLIB's 13 continuous QCQPs (`qcqp-*.txt`):
  the same lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.

`make test` and `make sanitize` pass.
