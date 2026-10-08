# 02-371 — small QCQPs whose optimum the checker refuses

Taken on 2026-10-08 on the tree of 327dc35, for TODO row J3.

## Why

J3's three models (QPLIB_2456, 3105 and 2468) have thousands of columns.
`qcgen.py` builds small ones with the same failure: a circulation with
every arc bounded around a planted flow, a separable quadratic objective on
half the arcs, and one to three ball rows `sum(x_k^2) <= R` that the
planted point satisfies. Every model has an optimum.

## The reading

`qcrun.sh` solves 150 models of each size and runs the checker
(`qcrun.txt`):

| nodes | arcs | balls | optimal and checked | `numerical_error` |
|---|---|---|---|---|
| 12 | 30 | 1 | 146 | 4 |
| 12 | 30 | 3 | 144 | 6 |
| 30 | 80 | 3 | 102 | 48 |

## What fails, on `q-12-30-1-88` (30 columns)

The conic walk nearly converges: at iteration 17 the primal residual is
2.7e-10, the dual 4.9e-13 and the gap 1.2e-9. The τ denominator of the
homogeneous embedding then falls toward zero (-1.4e-9 at iteration 18), the
step collapses, and the next direction is not finite. The walk's last
point within 1e-8 (iteration 17) stands. The checker refuses its duals by
1.2e-3: some bound columns sit a little off their bound with reduced costs
of that size, a tiny product with neither factor under the tolerance. The
Newton finish on the 33 active constraints and the settle's projection and
dual refit do not repair it.

These models are J3's case at 30 to 80 columns.
