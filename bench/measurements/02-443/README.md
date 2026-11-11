# 02-443 — a reduced cost is judged against the size of its terms

Taken on 2026-11-11 on the tree of e368c84 with this change, for TODO row
J21. The maintainer approved the change to the checker's contract.
02-411's `read.sh` takes `scaled.txt`. 02-432's `gate.sh` takes the
`*-head.txt` and `*-new.txt` records, and 02-427's `warmqc.c` with each
library takes `warmqc-head.txt` and `warmqc-new.txt`.

## What was refused

The checker's test on a reduced cost was absolute: a reduced cost over
`tol` on a column off its bound was a violation, whatever the size of the
terms it is a difference of. With terms near 1e12, 1e-7 is below a
double's rounding. 02-411 has the count on scaled LPs. 02-386 has the cone
model `bench/measurements/02-382/seed4-533.mps`, where the refused reduced
cost is -4.56 over terms that sum to 2.1e8.

## The change

**The checker (`src/check.c`).** For each column the checker adds up the
magnitudes of the terms of its reduced cost: the cost, each entry times
its row dual, each quadratic term of the objective and of the rows at the
point, and each cone dual. `d` is the larger of 1 and that sum. A reduced
cost within `tol · d` asks nothing of its column. The report gains
`max_dual_violation_relative`, each reduced cost's violation over its `d`,
and `dual_feasible` reads it. A row dual's sign and a cone dual's distance
are judged as before. The gap test is unchanged, so a reduced cost that is
excused still adds its product with the column's distance to its bound.

The field is the last of `jaos_check_report`. The CLI prints it as the
nineteenth line of the report, and the Python, .NET, Java, R and Julia
records carry it.

**The conic settle (`src/conic.c`).** The dual refit is a least-squares
fit of the multipliers to the stationarity residual. It weighted every
column alike. It now divides each column's residual by the size of its
terms (`cm_term_sizes`), with the weights taken relative to the largest
and floored at `CONIC_REG`. A settle step is taken or refused on the
relative dual violation.

On model 533 the refit used to leave a residual of 3.4e-3 on the free
column C2, whose terms sum to 1159, and 1.87 on C3, whose terms sum to
1.1e8. The weighted refit leaves 1.7e-8 of its terms on the worst column,
and the model ends optimal at 319179.01678939949 with a gap of 2.8e-9.

**The simplex (`src/simplex.c`).** A cold solve whose settled point
breaches `DUAL_TOL` on an unscaled reduced cost ended `numerical_error`.
It now publishes the point when every breach is within `DUAL_TOL` of the
size of its terms (`settled_violation_by_terms`), and the log says so. A
warm solve restarts cold first, as before.

**The barrier (`src/barrier.c`).** Its refusal messages print the relative
number.

## The scaled LPs

02-411's reading again, 4734 solves of an optimal model at each scale:

| K | status changed | checker refuses | 02-411 on 758b21c |
|---|---|---|---|
| 2 | 0 | 0 | 0 / 0 |
| 4 | 0 | 1 | 1 / 103 |
| 6 | 0 | 27 | 4 / 1070 |

Of the 28 refusals left, 26 are the primal simplex's and 2 the dual's.
The violations they print are 2.3e-7 to 5.6e-3.

## Two tests changed

`tests/data/qp_dual_shift.mps` needed the third push to pass the absolute
test. Its polished point now passes: the same objective to 14 digits, a
gap of 1.8e-11, 558,162 work units against 570,872. Its duals keep their
shift, so the test no longer bounds them, and it is now
`test_a_polished_point_passes_on_the_size_of_its_terms`.
`tests/data/qc_sqp_refused.mps` needed the quadratic programs of 02-372.
Its settled point now passes with a gap of 5.6e-17 in 952,118 work units
against 1,167,511, so
`test_quadratic_programs_on_the_ball_rows_finish_a_stalled_walk` asks for
those programs on `qc_sqp_stalled.mps` alone.
`test_a_reduced_cost_is_judged_against_its_terms` in `tests/test_check.c`
holds the rule on a one-column LP with terms of 1e12.

## The gates

`make test`, `make sanitize`, `make python-test`, `make dotnet-test`,
`java-test`, `r-test` and `julia-test` pass.

`make netlib netlib-infeas netlib-kennington miplib cblib`: 0 regressed,
and no line of `bench/results/` moves for them. `make maros-meszaros`
reads 137 of 138 as before; `ubh1` takes 7,031 iterations and
1,732,865,846 work units against 7,166 and 1,816,976,391. In `make warm`,
`dfl001`'s cold solve after the branch takes 2,088,575,852 work units
against 2,096,562,414 in the same 19,355 iterations. `make primal barrier
pdlp concurrent` leave their files as they were.

02-432's gate, HEAD against the tree:

| set | HEAD | tree |
|---|---|---|
| 02-255, 8 seeds x 1000 | 3 unsure solves | 2: the first solve of seed 4's model 533 passes; seed 29's digest moves |
| 02-326, 6 seeds x 1000 | 4 unsure solves | 4; seed 2's digest moves |
| CBLIB mixed-integer, 80 at 1e10 | 30 optimal, 50 at the limit | the same; 8 lines move in work units or by two nodes |
| QPLIB mixed-integer QCQPs | | one line moves in work units |
| 02-427 `warmqc`, 16 runs x 1000 | 17 edits end `numerical_error` | 19 |

The edits of `warmqc` depend on the answers before them, so its two
counts are not the same models. Its failures are TODO row J27: their dual
violations are 0.005 to 0.56 of their terms, and this change does not
reach them.

## What is left of J21

The second solve of model 533 and 02-326 seed 2's model 620 still end
`numerical_error`. On 533 the columns, rows, cones and duals now pass and
the gap does not.
