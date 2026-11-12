# 02-446 — a quadratic row with no interior

Taken on 2026-11-11 on the tree of 66a6975 with this change, for TODO row
J27. The maintainer approved an optimum published without duals.
`read.sh` takes `warmqc-head.txt` (66a6975's library) and `warmqc-new.txt`
(the tree's). `warmqc.py` turns 02-427's `warmqc.c` into the harness both
use: an optimum whose duals are all NaN is counted, not failed.

## What failed

18 of 02-427's 64,000 continuous edits ended `numerical_error` on 66a6975:
the point passed the checker and the duals missed by 0.005 to 0.56 of
their terms. On each, one quadratic row's smallest value over the rest of
the model is exactly its side. The feasible set then lies on the row's
boundary, no point is strictly inside the row, and finite duals need not
exist. No walk, refit or relative test finds duals that are not there.

## The change

`cm_flat_rows` in `src/conic.c` runs when the checker still refuses the
optimum of a continuous model after the settle and the quadratic
programs:

1. For each quadratic row with one finite side that the refused point
   holds at that side, up to `CONIC_FLAT_ROWS` (8) rows, a copy of the
   model takes the row as its objective, with the row itself set free,
   and is solved (`cm_flat_probe`).
2. A row whose smallest value (largest, for a row with a lower side) is
   its side within the dual tolerance has no interior. For a convex
   quadratic `g`, every point of the model then has the same `Q x` and
   the same `a'x` as the probe's point `x*`. So the row is held there
   (`cm_flat_hold`): a column that is alone in `Q` is fixed at its value
   in `x*`, a column joined to others by cross terms gets the row
   `Q_r x = Q_r x*`, and the row keeps its linear part on its side at
   `a'x*`.
3. The model with those rows held is solved. When it ends optimal and the
   checker passes its point on the model as loaded, that point is
   published, with every row dual, reduced cost and cone dual as NaN.

The three wrappers that solve a reduced model and map it back carry the
NaN duals to every row and column (`jm_model_no_duals`).

**The contract.** Duals that do not exist are NaN. `jaos_check_solution`
takes row duals that are all NaN, with the cone duals all NaN when given,
as no duals: it judges the primal side and leaves `checked_duals` false.
A NaN among finite duals is still an invalid input. The solution file
writes `nan` in the dual fields and reads it back, so `jaos check` judges
such a file's point alone. No call is added, and the bindings hand the
NaN through. (`jaos_has_duals` was added after this reading; `docs/api.md`
has the contract as it stands.)

The point's optimality rests on the solve of the held model. The checker
cannot confirm it, as it cannot for a mixed-integer optimum.

## The reading

`warmqc`, seeds 1 to 8, 1000 models a seed, 8 edits a model:

| mode | library | optimal | of which no duals | infeasible | unbounded | capped | other |
|---|---|---|---|---|---|---|---|
| qcp | 66a6975 | 18,146 | 0 | 45,107 | 729 | 0 | 18 |
| qcp | tree | 18,232 | 24 | 45,021 | 747 | 0 | 0 |
| miqcp | 66a6975 | 17,213 | 0 | 45,942 | 708 | 136 | 1 |
| miqcp | tree | 17,213 | 0 | 45,942 | 708 | 136 | 1 |

P1, P2 and P3 are 0 in every run. The edits depend on the answers before
them, so the two qcp rows are not the same 64,000 models. All 18 models
that failed on 66a6975, dumped as files, end optimal on the tree, each
with one held row; `tests/data/qc_flat_row.mps` is seed 8's model 144
after edit 7. The one miqcp failure is a node whose walk ends with no
answer, not this case.

`tests/data/qc_rough_leaf.mps`, 02-427's mixed-integer test model, now
ends at -15.5 where it ended at -15.500009337761226: the solve of its last
leaf holds a row with no interior, and the point no longer uses the
tolerance on that row. Its tree is the same 5 nodes and 218 iterations,
at 9,751,210 work units against 8,333,368.

## The gates

`make test`, `make sanitize`, `make python-test`, `make dotnet-test`,
`java-test` and `r-test` pass. `make netlib netlib-infeas
netlib-kennington miplib cblib primal barrier pdlp concurrent warm`:
0 regressed, and no line of `bench/results/` moves.
`make maros-meszaros` reads 137 of 138 as before. 02-432's gate, HEAD
against the tree: 02-255, 02-326, CBLIB's 80 mixed-integer instances and
QPLIB's mixed-integer QCQPs read the same line for line.

## What is left of J27

- A cone with no point strictly inside, and an optimum that is a limit
  no point reaches. 02-431's permutation reading holds those cases; it
  was not taken again here.
- A node of a mixed-integer tree: `cm_flat_rows` runs on continuous
  solves only.
