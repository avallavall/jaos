# 02-372 — quadratic programs finish a QCQP the walk leaves refused

Taken on 2026-10-08 on the tree of 2a21354, for TODO row J3.

## The change

`cm_sqp` in `src/conic.c`. A minimizing QCQP with no cone, no integer, no
indicator and no SOS reaches it in two places:

- the checker refuses the walk's optimum, and the Newton finish and the
  settle did not repair it;
- the walk ends with no answer, the feasibility walk finds a point, and no
  ray passes the ray checker.

Each round solves, with JAOS's own QP solver, the program

    min  (c + Σ w_i Q_i x_k − ρ x_k)'x + ½ x'(Q0 − Σ w_i Q_i + ρI)x
    s.t. (a_i + Q_i x_k)'x  within the row's sides plus ½ x_k'Q_i x_k
         the column bounds

where `x_k` is the current point, `w_i` the current dual of row `i` (zero
when its sign points at an infinite side), and `ρ = CONIC_SQP_PROX`. The
program's point and duals become the next point. The first point the
checker passes is the answer. If none passes in `CONIC_SQP_ROUNDS`, the
point before the rounds stands.

The proximal term vanishes at a fixed point, so a fixed point satisfies
the QCQP's own optimality conditions.

## The small models (02-371's 450)

`rounds.sh` lists each model the rounds touched (`rounds.txt`).

| | 02-371 (HEAD) | this tree |
|---|---|---|
| 12 nodes, 30 arcs, 1 ball | 146 | 150 |
| 12 nodes, 30 arcs, 3 balls | 144 | 150 |
| 30 nodes, 80 arcs, 3 balls | 102 | 150 |

58 models take the rounds. 26 start from a refused optimum and take 1 to
4 rounds. 32 start from the feasibility walk's point and take 5 to 7.

## J3's three models

| model | HEAD | this tree | work units (HEAD) | objective | QPLIB reference |
|---|---|---|---|---|---|
| QPLIB_2456 | numerical_error | optimal, 1 round | 2.69e9 (1.97e9) | -0.94496654741 | -0.9449662766 |
| QPLIB_2468 | numerical_error | optimal, 1 round | 1.42e10 (9.86e9) | -0.93640507304 | -0.9364048895 |
| QPLIB_3105 | numerical_error | optimal, 1 round | 4.75e10 (4.23e10) | -0.93780352549 | -0.9378032911 |

The checker passes all three. Their rows are off by at most 1.6e-9 of
their traffic. The three objectives are 2e-7 to 3e-7 below QPLIB's
reference values.

## ρ

With no proximal term the small models pass, and the three QPLIB models
do not: their programs have thousands of free columns with no curvature,
and the QP barrier stalls with its dual residual near 1e-8 until the work
limit (1e11). The sweep (`JAOS_SQP_RHO`, a switch that existed only for it):

| ρ | small models passed (of 58) | QPLIB_2456 |
|---|---|---|
| 0 | 58 | refused, work limit |
| 1e-6 | | refused after 8 rounds |
| 1e-5 | 58 | optimal, 1 round |
| 1e-4 | 58 | optimal, 1 round |
| 1e-3 | 57 | optimal, 1 round |
| 1e-2 | 37 | optimal, 2 rounds |
| 1 | 12 | |

At 1e-2 QPLIB_2468 and 3105 also pass, at 1.63e10 and 5.06e10 work units.

## The reading

`read.sh`: HEAD against the tree on 02-253's 3000 generated conic models,
QPLIB's 13 continuous QCQPs, and `make cblib`. Taken on 2026-10-20.

- The 3000 generated models (`gen-head.txt`, `gen-new.txt`): the same
  digests on both trees (`77438d2b93ecb3e1`, `20f86061945b890c`,
  `71d98d7c05410cb3`) and 0 failed models. None of them reaches the
  rounds.
- QPLIB (`qcqp-head.txt`, `qcqp-new.txt`): QPLIB_2456, 2468 and 3105 go
  from `numerical_error` to `optimal`, as in the table above. The other
  ten write the same line on both trees.
  QPLIB_3312 stops at the work limit (1e11) on both.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved, and
  `bench/results/cblib.txt` does not change.

`make test` and `make sanitize` pass.
