# 02-315 — a QP optimum the checker refuses is no longer published

Taken on 2026-09-24 on the tree of a95d0a2, for TODO row J1.

## The defect

QPLIB_9002 (1649 rows, 2890 columns) ended `OPTIMAL` with its rows 8.9e-7
off, relative, and a dual violation of 21193. Its walk stalls at iteration
31, the push leaves 931 pinned columns with a reduced cost of the wrong
sign (the worst 8.7e9), the walk cannot reach 1e-10 in 50 more
iterations, and "the point at 1e-8 stands": `bx_publish` called it optimal
without asking the checker. Maros-Meszaros `qgrow22` took the same road
and was published with a dual violation of 2.98e-6.

## The change

In `jm_barrier`, an optimum whose push did not settle is put to
`jaos_check_solution` at the primal tolerance. One the checker refuses
becomes `NUMERICAL_ERROR`, which hands a quadratic model to the conic
interior point (`src/model.c`). `jm_conic_after_barrier` now puts that
answer to the checker as well, and ends `NUMERICAL_ERROR` when it is
refused too: on `qgrow22` the conic point had a dual violation of 0.572.

| model | before | after |
|---|---|---|
| QPLIB_9002 | `optimal`, duals off by 21193 | `numerical_error` after the conic walk (its infeasibility certificate is refused) |
| qgrow22 | `optimal`, duals off by 2.98e-6 | `numerical_error`, 123434269 work units (2.8x) |

The other 136 Maros-Meszaros models and all 29 of CBLIB write the same
lines. The Maros-Meszaros baseline is re-based for `qgrow22`.

## A wider check, measured and left out

Putting every QP optimum to the checker, the settled pushes included,
turned `liswet1` into `numerical_error` as well: its pushed point passes
the runner's checker at 1e-6 and fails the internal one at 1e-7. QPLIB_8785
went to the conic walk and stopped at 1e11 work units; its pushed point
has rows off by 3e-15, duals by 0, and an objective gap of 1.31e-7, which
fails at 1e-7 and passes at 1e-6. A settled push is exact on its active
set, so the check stays on the unsettled ones.
