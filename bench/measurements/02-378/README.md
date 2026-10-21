# 02-378 — fixed columns leave the walk's quadratic objective

Taken on 2026-10-21 on the tree of be8efbf, for TODO row J20.

## The failure

After 02-377, 4 of 02-255's brute-force solves on seed 2 (model 698, the
integer columns fixed) still stopped after 206 iterations with no answer.
One is now `tests/data/qc_fixed_quad.mps`. After the lone columns are set
aside the walk has 4 columns: C1, C2 and C6 fixed at 2, 0 and 3, each
with a quadratic objective term, and C3 in a quadratic row. Tau and kappa
both go to zero and the gap stays at 0.47.

| variant of the model | status | iterations |
|---|---|---|
| as it is | `numerical_error` | 206 |
| no quadratic objective term on C1, C2, C6 | optimal | 6 |
| C1 and C6 fixed at 0 | optimal | 7 |
| no quadratic row | optimal | 4 |

## The change

`conic_solve` in `src/conic.c`. A column fixed by its own bounds (and not
set aside by the walk's other rules) leaves the walk's objective matrix:
its diagonal term becomes a constant, and an off-diagonal term `v` with a
column that is not fixed adds `v` times the fixed value to that column's
linear cost. The column stays fixed by its equality row. After the walk,
its reduced cost gets back the `(Qx)_j` the matrix no longer carries;
that pass runs only when some column is fixed.

## The reading

`read.sh`: HEAD against the tree.

| 02-255, mixed-integer | seed 1 | seed 2 | seed 3 |
|---|---|---|---|
| brute force unsure, HEAD | 0 | 1 | 0 |
| brute force unsure, tree | 0 | 0 | 0 |
| failed models, HEAD and tree | 0 | 0 | 0 |
| work units, HEAD | 381554867 | 393326998 | 395907505 |
| work units, tree | 381220029 | 393187439 | 395618929 |

All 4 stalled solves now end optimal in 6 or 7 iterations and pass the
checker, and seed 2 has 599 optima where it had 598. The digests change
on all three seeds, since the trees' nodes fix columns that carry
quadratic objective terms.

- 02-253's 3000 generated conic models (`gen-*.txt`): the same counts
  and no failure. The digests change; the work moves by -0.010%, -0.019%
  and +0.003%.
- QPLIB's 13 continuous QCQPs (`qcqp-*.txt`): the same lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.
- `make maros-meszaros`: 0 regressed, 0 improved, the same answers and
  the same one failure as HEAD. `dtoc3` and `ubh1` have a fixed column
  and a quadratic objective, so the reduced-cost pass runs on them: +14999
  and +18009 work units, under 0.003%.

`make test` and `make sanitize` pass.
