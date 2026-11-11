# 02-444 — a certificate through quadratic rows with cross terms and fixed columns

Taken on 2026-11-11 on the tree of 60ec0f8 with this change to
`src/check.c`, for TODO row J22. The maintainer approved the change to the
checker's contract. 02-432's `gate.sh` takes the records here.

## What the checker refused

A certificate of infeasibility is a multiplier `y_i` per row. The checker
bounds `L(x) = y'Ax + ½ x'Hx` over the column bounds from above, with `H`
the sum of the rows' matrices times their multipliers, and compares the
bound with what the rows' sides ask for. It read `H` only when every row
with a multiplier was diagonal, and it bounded a curved column by its peak
over all values. So it refused three kinds of true certificate:

- a row with cross terms (02-255 seed 16's model 69, through C1·C3);
- a curved column that is fixed, where the peak over all values is not
  the value at the fixed point;
- a curved column whose peak lies outside its bounds (02-255 seed 17's
  model 451: with C4 and C6 fixed, R4 asks
  `2.176 + 2.122·C7 + 0.4016·C7² <= 1.634`, which holds only below
  C7's lower bound of -0.1356).

A model infeasible only through such a row ended `numerical_error`: the
two above, and 02-326 seed 2's model 240, seed 22's model 162 and seed
25's model 357.

## The change

`certificate_core` builds `H` from every row with a multiplier:

1. A fixed column's quadratic terms are read at its value: a diagonal
   entry gives a constant, and a cross term with a free column joins that
   column's linear coefficient.
2. A free column alone in `H` takes its value at the peak of
   `a·x + ½ h·x²` when the peak lies inside its bounds, and at the nearer
   bound when it does not. `h > 0` refuses the certificate.
3. Free columns joined by cross terms form a block. `-H` on the block is
   factored densely with diagonal pivots (`curve_block`). Each pivot adds
   `½ t²` to the bound, with `t` the forward-solved coefficient. When the
   largest diagonal entry left is within `tol` of the block's largest,
   the factor ends, and the columns left are bounded through their column
   bounds with their coefficients reduced by the factor. What is left of
   the block must be zero within `tol`, or the certificate is refused. A
   block of more than `CHECK_CURVE_BLOCK` (3000) columns is refused.

The pivoted columns of a block are bounded as if they were free, which is
a valid bound and can be a loose one. The rows' multipliers no longer have
to curve the right way one by one: only their sum does.

Four tests in `tests/test_check.c` hold the cases: cross terms, an
indefinite matrix that is refused, a fixed column, and a column held at
its bound. Each infeasible model is also solved, and its own certificate
judged.

## The reading

02-432's gate, HEAD against the tree:

| set | HEAD | tree |
|---|---|---|
| 02-255, 8 seeds x 1000 | 2 unsure solves, model 69 of seed 16 `numerical_error` | 1 unsure solve: seed 4's model 533, of TODO J21 |
| 02-255 work units | 3,077,602,600 | 3,053,462,734 |
| 02-326, 6 seeds x 1000 | 4 unsure solves | 1: seed 2's model 620, of TODO J21 |
| 02-326 work units | 2,424,706,318 | 2,412,076,027 |
| CBLIB mixed-integer, 80 at 1e10 | | every line the same |
| QPLIB mixed-integer QCQPs | | 9 lines move in work units, none in status |
| 02-427 `warmqc`, qcp, 64,000 edits | 588 infeasible answers with no certificate | 0 |

The digests of 02-255 move on seeds 16 and 17, where an answer replaces
`numerical_error`; no other digest moves. `warmqc` still counts 19 edits
that end `numerical_error`, which are TODO row J27.

`make sanitize` and `make python-test` pass, and `make test` with this
folder in place. `make netlib netlib-infeas netlib-kennington miplib
cblib`: 0 regressed, and no line of `bench/results/` moves.
`make maros-meszaros` reads 137 of 138 as before.
