# 02-451 — the conic tree and the sign of an integer column

Taken on 2026-11-12 on the tree of d853918 with this change to
`src/conictree.c`, for TODO row J29. The `cblib-mip-*.txt` records are
02-447's harness (`modes.patch` on 02-440's `mirrorfile.c`) on CBLIB's 80
mixed-integer instances under 2e9 work units with seed 1; the `-head`
ones are d853918's library. 02-432's `gate.sh` takes `mip-*.txt` and
`dis-*.txt`.

## Which part moves a conic tree

On d853918, of the 80 instances:

| what is mirrored | models whose node count differs |
|---|---|
| rows negated | 0 |
| continuous columns only | 0 |
| integer columns only | 34 |
| columns mirrored | 34 |

The conic tree reads only the sign of an integer column.

## The change

`jm_conic_branch_and_bound` mirrors an integer column whose lower bound
lies farther from zero than its upper bound, as the MIP presolve does
(02-447). It runs the tree on a copy of the model (`ct_mirror_copy`): the
column's bounds, cost and entries are negated, and so is each quadratic
term of the objective or of a row that pairs it with a column that is not
mirrored. A cone's head, a member of an SOS set, a semi-continuous column
and an indicator's column are left as they are, and so is every column
under an incumbent or node callback. `ct_mirror_back` hands the answer to
the model: the point, the reduced costs, the cone duals of mirrored
members, the incumbent and the pool with their signs turned, a column's
basis status with its sides swapped, and the root's certificate when the
relaxation is infeasible.

A first form ran the tree again on the model as loaded whenever the
mirrored tree ended infeasible or unbounded. On 02-255 and 02-326 that
cost 3,713,577,392 and 2,659,412,662 work units against 3,059,286,187
and 2,412,711,684, so the answer is mapped back in every case.

`test_a_mirrored_integer_column_gives_the_same_conic_tree` in
`tests/test_conic.c` solves a three-column model with a cone and its
mirror: the same objective, nodes and work units, and the mirrored
point. It fails on d853918.

## The reading

CBLIB's mixed-integer set, models whose node count differs:

| what is mirrored | d853918 | tree |
|---|---|---|
| rows negated | 0 | 0 |
| columns mirrored | 34 | 0 |
| both | | 0 |

With columns mirrored all 80 lines match in every number. With rows
negated three models differ in work units alone.

## The gates

`make test`, `make sanitize` and `make python-test` pass. 02-432's gate,
HEAD against the tree:

- CBLIB's 80 mixed-integer instances at 1e10 and QPLIB's mixed-integer
  QCQPs: every line the same.
- 02-255, 8 seeds x 1000: the same verdict counts and one unsure solve
  both ways; 3,061,101,618 work units against 3,059,286,187. Five seeds'
  digests and node counts move, since their models hold integer columns
  the tree now mirrors.
- 02-326, 6 seeds x 1000: the same verdict counts; 2,412,726,353 work
  units against 2,412,711,684. Four seeds' digests move.
- `make cblib`: 0 regressed.
