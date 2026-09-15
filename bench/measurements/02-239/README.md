# 02-239 — a point another solver produced, judged

SPECS row 107, check a point another solver produced, said `done` and
said nothing else (`TODO.md` row 6). The feature is `jaos_read_point`,
`jaos_read_duals` and `jaos_check_solution` behind the CLI's `check FILE
--point POINT [--duals DUALS]`: a point file is one `NAME VALUE` line per
column, the reader also takes the shapes Gurobi, MIPLIB, SCIP, HiGHS and
CPLEX write, and the independent checker judges the point, and the duals
when they come, from the model alone. `point.c` reads six properties.

## The models

1000 per seed, six seeds, 02-237's generator: eight to twenty-four
columns, six to sixteen rows, integer data, a planted integer point `z`
inside the boxes with each row's bounds set around it. `z` is a feasible
point no solver produced; the optimum `x*` and its duals `y*` are
JAOS's. Every one of the 6000 came out optimal.

The harness reads the checker's arithmetic itself, in extended precision
and sharing no code with it: activities, bound and row violations, the
reduced costs, the sign condition of every multiplier against the side
its value rests on, the dual objective and the complementary-slackness
gap, with the checker's documented conventions (a row side the model
leaves open is the side its columns' boxes imply; a multiplier at or
below the tolerance is negligible).

## The properties

1. the point file and the duals file JAOS writes read back equal; the
   checker accepts `(x*, y*)` on both halves with the gap certified; its
   primal objective is the solve's to 1e-9; a second check gives the same
   report byte for byte
2. the same point in five other solvers' shapes reads back equal:
   Gurobi with comments and blank lines, MIPLIB and SCIP with the zero
   columns left out, HiGHS with the duals in its second `# Rows` block,
   CPLEX XML; the duals from the HiGHS and the CPLEX files read back
   equal; every file has its lines shuffled
3. the planted point `z` is judged primal feasible, with both violations
   exactly zero and the objective exactly the integer sum; without duals
   `checked_duals` is false
4. a point moved off `x*` by an integer in one column, and `z` with one
   column pushed past its box, are judged by the numbers the harness
   reads: `max_col_violation`, `max_row_violation` and
   `primal_objective` agree to 1e-9, and `primal_feasible` agrees
   wherever the maximum is not within 1e-9 of the tolerance
5. the dual half agrees with the harness's reading on `(x*, y*)`,
   `(x*, y*)` with its largest multiplier's sign flipped, `(x*, 2y*)`
   and `(z, y*)`: `max_dual_violation`, `dual_objective` and
   `objective_gap` to 1e-9, `gap_certified` true, `dual_feasible`
   wherever the margin is clear
6. a plain file that names a column twice, leaves one out, or names one
   the model has not got is refused; the MIPLIB shape with a column left
   out reads it as zero

Every 50th model is also written out with `x*` in one of the five
shapes, rotating, the duals in JAOS's shape, and the pushed point, and
the CLI runs on them: `check --point` exits 0 with `status point`,
`primal_feasible yes` and `checked_duals no`; with `--duals` it exits 0
with `checked_duals yes` and `dual_feasible yes`; on the pushed point it
exits 1 with `primal_feasible no`.

## The reading

| seed | shapes read | moved points | refused | flipped duals | refused | ambiguous | CLI |
|---|---|---|---|---|---|---|---|
| 1 | 5000 | 2000 | 1975 | 1000 | 1000 | 0 | 20 of 20 |
| 2 | 5000 | 2000 | 1985 | 998 | 998 | 0 | 20 of 20 |
| 3 | 5000 | 2000 | 1984 | 1000 | 1000 | 0 | 20 of 20 |
| 4 | 5000 | 2000 | 1982 | 1000 | 1000 | 0 | 20 of 20 |
| 5 | 5000 | 2000 | 1980 | 999 | 999 | 0 | 20 of 20 |
| 6 | 5000 | 2000 | 1981 | 1000 | 1000 | 0 | 20 of 20 |

**No defect.** 30000 foreign-shaped files read back equal; 12000 moved
points judged, 11887 of them infeasible, every number agreeing with the
harness; 5997 flipped duals, every one refused, and 12000 more dual
readings (doubled, and the optimal duals on the planted point) agreeing
to 1e-9; no reading fell in the ambiguous band; 120 CLI runs as
expected. The 113 moved points the checker accepted are the ones the
harness also finds inside the model: the move stayed in the box and no
row was tight.

## The pass is not vacuous

Properties 4 and 5 compare numbers, not verdicts, so a checker that
reported a violation of the wrong size would be counted even where the
verdict happened to be right. Two one-line breaks in the library were
also measured on 200 models of seed 1, then reverted:

| break | where | fires |
|---|---|---|
| the reader drops the every-column rule | `read_named_values` | P6 on 200 files |
| the checker scores every positive multiplier as fine | `sign_condition` | P5 on 254 readings |

(02-238 carries two more, on the basis reader and the verifier.)

## How to run

```
make all cli
bench/measurements/02-239/point.sh            # six seeds
```
