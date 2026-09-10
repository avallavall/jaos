# 02-224 — the same model with its rows and columns in another order

A model does not change when its rows and columns are written down in
another order. The status has to match and the objective with it. Nothing
had read that, and reading it found three defects, all of them older than
this session.

`perm.c` generates a model, solves it, permutes its rows and columns,
solves that, and compares. It builds every discrete shape the library
carries: integer marks, an SOS1 or SOS2 set, a semi-continuous column, an
indicator row. `perm.sh` runs it over six seeds twice, once with that
structure and once with it taken off, so a disagreement is placed inside
the LP or inside the tree.

`before.txt` is the reading at 3bf0585 and `after.txt` the reading with the
fixes. 13 disagreements over 72000 pairs became 0.

## The first defect: an unbounded model read `numerical error`

Dual phase 1 lends an artificial bound to a column that has none of its
own (`build_initial_basis`, `ARTIFICIAL_BOUND`). When the walk ends with a
column resting on one of those lent bounds and still wanting to move,
`classify_optimum` has to decide whether the model is unbounded. It tries
two directions: that column on its own, and every held column together at
unit rate. Neither is enough.

The model of `STATUS t=1249`, after presolve, is one row and three
columns:

    min -3 x0 - 3 x2 - 4 x3
    3 x0 + 3 x2 - 3 x3 <= 2
    x0 in [0, 1], x2 >= 0, x3 >= 0

It is unbounded: `x2` and `x3` up together by the same amount leave the row
where it was and take the objective down by 7 per unit. Neither test sees
that ray. `x3` alone is stopped by the row. Both held columns at unit rate
is the same direction as `x3` alone once `x0` has entered the basis, so it
is stopped too. And `x2` is not in the set the second test sums over,
because its reduced cost is zero, which is not "improving".

Which of the two answers came out depended on the column order, because
`x0` and `x2` tie in the dual ratio test of the first iteration. Entering
`x2` leaves `x3` improving through a basic column with no upper bound, and
the first test then answers `unbounded`. Entering `x0` leaves `x3`
improving through a basic column bounded at 1, and nothing answers at all.

The fix does not add a third direction. A model neither test decides
restarts cold on the primal with nothing lent: a column with no bound of
its own is nonbasic free, and the primal prices a free column in both
directions and reads its ratio test against the bounds the column really
has. The primal simplex then decides, as it decides any other model. The
restart is behind `s.no_loans`, it happens at most once per solve, and it
is only reachable where the old code refused, so no model that answered
before takes a different path.

`test_two_held_columns_whose_sum_is_still_blocked` in
`tests/test_simplex.c` had pinned the old refusal. That model is bounded:
its two rows hold both columns at 1e11 and the optimum is -2e11. The test
now asserts the optimum.

## The second defect: a free column came back resting on a bound

Presolve removes a column with no entries and fixes it at a value
(`ps_empty_col_value`). Where the column has neither bound and no cost,
that value is 0. `ps_fixed_status` then had to name a basis status for it
and named `at lower`, a bound the column has not got.

`rg_build` refuses such a basis, with "column 5 is nonbasic on a bound it
does not have", so `jaos_bound_ranging` and `jaos_rhs_ranging` refuse the
answer of any model carrying an empty free column. A removed column with
no finite bound now comes back `JAOS_BASIS_FREE`, which is the status whose
value is 0.

## The third defect: that refusal ended the whole MIP

`gomory_round` returned -1 for any failure of `jm_tableau_build`, and
`jm_branch_and_bound` turns -1 into `goto done` with `rc` still at the
`JAOS_ERR_OUT_OF_MEMORY` it is initialised to. So the 6-column MIP of
`RC t=3439`, whose sixth column is free, empty and free of cost, came back
`out of memory` on a model of two rows.

The Gomory round is optional. Only an allocation failure ends the search
now; any other refusal skips the round and the node keeps its answer.
