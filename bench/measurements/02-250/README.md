# 02-250 — the Maros-Meszaros convex QP set, read for the first time

SPECS rows 22 and 24 lacked a reading on a published QP set. This is the
first: the 138 convex quadratic programs of Maros and Meszaros (1999), as
`make maros-meszaros`, against the `OPT` column of the set's own
`00README.QP`, the value BPMPD reached at its defaults to eight digits.
The manifest is `bench/maros-meszaros.manifest`; the QPS originals are
mirrored at github.com/YimingYAN/QP-Test-Problems and pinned by sha256.

## What the first pass found and 808022a fixed

Before anything else the set found defects, four kinds, all fixed and
pushed as 808022a and read again:

| defect | instances | fix |
|---|---|---|
| a point of NaNs published as `OPTIMAL` | qscfxm1, qscfxm2, qscfxm3, qscorpio, qbrandy | the push's rows carry `QP_PUSH_DELTA` (1e-8) instead of `BARRIER_DELTA`, its solve gets `QP_PUSH_REFINE` passes of iterative refinement, and a NaN anywhere fails the round |
| a row activity 1e-6 past the bound its slack was pinned on, the checker refusing the dual side | qsctap1 and 19 others | the same refinement |
| a file that would not read | values, exdata (a BOUNDS line with no set name), qgfrdxpn (an RHS line with no set name), qforplan (fixed layout, spaces inside names) | the reader takes all three; a space inside a fixed-layout name reads as an underscore |
| the handoff of a QP the barrier could not settle running the dual simplex with the LP objective, four million iterations on liswet1 | every `NUMERICAL_ERROR` on a large model | the handoff is a feasibility probe with zero costs, and an `INFEASIBLE` verdict is kept only with a certified ray, since on ksip the dual called a feasible system infeasible with a ray of zeros |

And one more, fixed with the reading below: `hues-mod`, costs of 1e-21 to
1e-4, was called divergent on the iteration its residuals fell from 1e12 to
3.6e3, because the dual iterate read 2.8e6 times `1 + |c|`. The divergence
test now fires only on an iteration that made no progress
(`BARRIER_DIVERGE` in `docs/tolerances.md`).

## The reading

`bench/results/maros-meszaros.txt`, at 4 jobs, the library after the fixes
above.

| outcome | instances |
|---|---|
| `OPTIMAL`, objective at the reference to 1e-6, checker takes both sides | 111 |
| `OPTIMAL`, checker takes it, objective away from the reference | hues-mod (3.48244638e7 against 3.4824690e7, ours certified by the checker with a gap of 8e-15), liswet2 (24.998048 against 24.998076, checker ok) |
| `OPTIMAL`, objective at the reference, checker refuses the dual side | qisrael (dual 3e-5), qpilotno (1e-4), qsierra (0.1), boyd1 (a row 0.015 past its bound in absolute terms on coefficients of 1e12, 2e-14 relative) |
| `OPTIMAL`, objective at the reference, checker refuses, the push did not settle | qgfrdxpn (the barrier's point stands, its duals off by 5e5) |
| `OPTIMAL` and wrong | dtoc3: the published point is the origin, objective 3e-67 against 235.26, rows off by 15. All 14999 columns are free, two are fixed; its scale factors ran from `2^-84` to `2^91` (`EXP_LIMIT` was 512), the scaled data to 1e27, and the barrier's relative residual read 4e-23 at a point that satisfies nothing. The checker refuses the point. With the exponent capped at 20 in the commit after this reading it ends `NUMERICAL_ERROR`, rows 1.4e-6 off in scaled space at the `BARRIER_DELTA` floor, like the liswet family |
| refused as not convex | values: `Q` fails the LDL convexity test at a ridge of 1e-10, 1e-8 and 1e-6 of its largest entry. A dense Jacobi eigenvalue sweep over its 202 columns finds 60 eigenvalues below zero, the smallest -1.27e-5 against a largest of 10.77: the entries are given to six digits and a covariance rounded that way loses semi-definiteness by about that much. JAOS refuses an indefinite `Q` by contract; BPMPD's 1.3966211 is a stationary point of a slightly non-convex QP |
| `NUMERICAL_ERROR`, the barrier did not settle | 18: boyd2, cvxqp1_l, cvxqp3_l, huestis, ksip, liswet1, liswet7 to liswet12, powell20, q25fv47, qforplan, qgrow22, qpcboei2, ubh1 |
| over the runner's suboptimality ceiling, though the checker takes them | aug2dcqp, aug2dqp, aug3dqp: the certified bound is `Σ d_j (x_j - l_j)` and their columns run to 1e6, so reduced costs of 1e-9 sum to 1e-3 |

138 instances: 119 solved, 116 at the reference objective, 113 taken by the
checker, 119 deterministic across two cold solves, 19 with no answer.

## What the 18 look like

- **liswet1, 7 to 12** (10000 rows, 10002 free columns): the primal
  residual stalls at 1e-8 to 1e-7 while mu falls to 1e-57; the gap stays at
  1e-3 to 0.3. The dual regularisation `BARRIER_DELTA` leaves a row residual
  of `delta` times the dual step, and the dual step is large here. One
  refinement pass on the Newton direction was tried and made the generated
  set worse (3 of 1000 failing), so it is not in.
- **ksip, cvxqp1_l, cvxqp3_l, powell20, huestis, qforplan, qpcboei2**:
  the dual iterate grows past 1e6 times `1 + |c|` within the first 20
  iterations with the residuals not falling. ksip has 1001 rows on 20 free
  columns; on its LP alone the dual simplex calls a feasible system
  infeasible with a ray of zeros, which is a defect of its own.
- **q25fv47, ubh1, boyd2**: 200 iterations, the last residual or the gap
  just above tolerance (q25fv47 dual 2.9e-8, gap 2.5e-10).
- **qgrow22**: diverges at iteration 41.

## Cost

Reading the set took about 40 minutes at 4 jobs on the machine of
2026-09-15, most of it cont-300 (90298 rows) and the two cvxqp*_l whose
handoff to the dual simplex runs hundreds of thousands of iterations.

## How to run

```
make maros-meszaros J=4              # reads against bench/maros-meszaros.baseline
make maros-meszaros-baseline J=4     # rewrites the baseline after reading the diff
```
