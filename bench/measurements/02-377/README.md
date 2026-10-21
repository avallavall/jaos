# 02-377 — the certificate solve on cone models and at tree nodes

Taken on 2026-10-21 on the tree of 718fa97, for TODO row J20.

## The failure

02-376 found that on 02-255's seed 2, 46 brute-force solves (continuous
cone models with the integer columns fixed) end `numerical_error`. The
walk reaches an infeasibility certificate the checker refuses, and with
no quadratic row the checker's test is exact, so no answer is published.
02-373's conic program over the multipliers ran only on models with
quadratic rows.

## The change

`src/conic.c` and `src/conictree.c`:

- `cm_cert_solve` runs on any model whose certificate is still refused,
  with or without quadratic rows. Its own program carries
  `no_cert_solve`, so it never comes back to this step.
- At a tree node it runs only when the node's status depends on it: no
  quadratic row, or a walk that stalled next to the certificate. With
  quadratic rows a refused certificate leaves the node infeasible anyway.
- The tree's fixed-integer and feasibility solves set `no_cert_solve`
  only when the model has quadratic rows, for the same reason.

## The reading

`read.sh`: HEAD against the tree. The mixed-integer generator is built
with 02-376's `unsure.patch`, so each unsure brute-force solve is printed.

| 02-255, mixed-integer | seed 1 | seed 2 | seed 3 |
|---|---|---|---|
| brute force unsure, HEAD | 0 | 5 | 2 |
| brute force unsure, tree | 0 | 1 | 0 |
| failed models, HEAD | 0 | 0 | 0 |
| failed models, tree | 0 | 0 | 0 |
| nodes, HEAD | 2396 | 2479 | 2460 |
| nodes, tree | 2396 | 2391 | 2458 |
| work units, HEAD | 381554867 | 411815175 | 396566618 |
| work units, tree | 381554867 | 393326998 | 395907505 |

The unsure brute-force solves fall from 58 to 4. All 54 that ended at a
refused certificate now end `infeasible` with a certificate the checker
takes. The 4 left stop after 206 iterations with no direction the ray
checker takes.

With the brute force sure, 4 models showed a failure of the tree that
the unsure brute force had hidden: seed 2's m00434 and m00709 and seed
3's m00174 and m00559 ended `numerical_error` at a node whose
certificate was refused. That is why the step now runs at nodes. With it
all 4 agree with the brute force. Seed 2's tree takes 88 fewer nodes and
4.5% less work, because those nodes were split before and are now cut
off.

- 02-253's 3000 generated conic models (`gen-*.txt`) and QPLIB's 13
  continuous QCQPs (`qcqp-*.txt`): the same lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.

`make test` and `make sanitize` pass.
