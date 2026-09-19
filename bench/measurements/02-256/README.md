# 02-256 — QPLIB's convex instances, a reader fix and a barrier fix

QPLIB (Furini et al. 2019, `qplib.zib.de`) marks 63 of its 453 instances
as having a convex continuous relaxation. JAOS reads them from their
`.qplib` files, against the objective values of `qplib.solu`, at a work
limit of 1e11 units each:

| class | instances | what they are |
|---|---|---|
| `CCB`, `CCL`, `DCL` | 19 | convex QPs over boxes or rows |
| `LCD` | 13 | continuous QCQPs, a linear objective over convex quadratic rows |
| `CBL`, `CML`, `DML` | 17 | mixed-integer convex QPs |
| `LMC`, `LMD` | 14 | mixed-integer QCQPs |

`qpfetch.sh` fetches the table and the files, `table.py` parses the
table, and `qpall.sh` is the reading.

## The first reading found two faults

**The reader doubled every pair of the quadratic matrix.** QPLIB's
documentation defines each `Q` as the lower-left triangle of
`½ x^T Q x`, so an entry `i j v` with `i > j` is the term `½ v x_i x_j`.
JAOS read it as the entry of a symmetric `Q`, the term `v x_i x_j`, and
wrote it back the same way, so its own round trip agreed with itself.
The first reading showed it four ways:

- 10 of the 12 `LMC` files were refused, their one quadratic row not
  convex: with the pairs doubled, row 2 of QPLIB_10010 has 47 negative
  eigenvalues, and as the file means it none (the other two were refused
  for a row over 3000 columns, `CONIC_QC_DENSE`);
- 7 files with a `C` objective were refused as not convex, for the same
  reason;
- QPLIB_8845 solved to 13453115.78 against 10907992.49, QPLIB_8906 to
  3014980.45 against 2699111.51, QPLIB_8991 to -1.2045 against -0.00167
  and QPLIB_8792 to 1.90 against 3593.52;
- QPLIB's own LP versions of the same files (`qplib.zib.de/lp/`), read
  by JAOS's LP reader, solve to the references: 10907992.494,
  2699111.513, -0.0016678674, 3593.516, and QPLIB_10050 to -25.6976616;
  `jaos diff` of QPLIB_10050's two files finds all 10878 pairs of the
  `.qplib` at twice the LP's value, and the GAMS version of QPLIB_10010
  writes the pairs of its row with a coefficient of 1 where the `.qplib`
  has 2.

The reader now halves a pair's entry, in the objective and in a row, and
the writer doubles it. `tests/data/g_pair.qplib` and `g_pair.lp` hold one
model in both formats, and `jaos diff` finds no difference between them.
`e_quad_offdiag.qplib`, written by hand under the old reading, is
corrected. The diagonal was always right.

**The QP barrier lost its walk when it left dense columns out.** Five
`DML` relaxations (QPLIB_3871, 3792, 3694, 3861 and 3698) end the root of
the tree as a numerical error or at a push: their barrier leaves 20 to
40 columns out of the normal matrix, the facilities' open variables that
each sit in every customer's row, and corrects for them. On the
augmented system each solves in 10 iterations. A generated
facility-location QP relaxation with 3 to 25 facilities over 35 to 60
customers does the same on each of 18 models: at 3 facilities and 35
customers the walk reaches a primal residual of 3e-10 and a gap of 4e-6
at iteration 5, the primal residual jumps to 1.7e-5 at iteration 6 and
0.12 at 7, and the walk never comes back. With 2 facilities the walk
converges.

A quadratic walk that stops with dense columns left out now starts
again, from its first point, on the augmented system (the log says so),
and the push follows it as on any walk. `tests/test_quadratic.c` holds
the 3-by-35 model. On Maros-Meszaros boyd2, whose walk had stopped at a
gap of 3.8e-6 with two dense columns left out, ends at the reference,
21.256767, in 270 iterations; every other instance is the same to the
bit, work and digest, and the baseline is rewritten with boyd2 in it.
The 6000 generated QPs of 02-248 print the same six summaries before and
after, work to the unit: none of them takes the restart.

## The reading with both fixes

| class | instances | `OPTIMAL` | checker at 1e-7 | work limit | numerical error | refused |
|---|---|---|---|---|---|---|
| convex QPs | 19 | 10 | 9 | 8 | 0 | 1 |
| continuous QCQPs | 13 | 10 | 2 | 1 | 2 | 0 |
| mixed-integer QPs | 17 | 3 | 3 | 13 | 1 | 0 |
| mixed-integer QCQPs | 14 | 0 | 0 | 12 | 0 | 2 |

- Every optimum is within 5.7e-7 of `qplib.solu`'s value, relative, and
  19 of the 22 with a value within 7e-8.
- The 8 continuous QCQPs the checker refuses are primal feasible to
  2e-13 with gaps under 3e-9; their duals miss by 8.5e-7 to 3.6e-5.
  QPLIB_2676 and QPLIB_2468 end `NUMERICAL_ERROR`: the walk stops without
  progress and the checker refuses its point.
- QPLIB_9002, a convex QP with no reference value, ends `OPTIMAL` on the
  barrier's own test with its rows 8.9e-7 off, relative, and a dual
  violation of 2.1e4: the push leaves 931 pinned columns with the wrong
  sign, and the longer walk cannot reach 1e-10.
- The 8 convex QPs at the work limit are the largest, 10000 to 1003001
  columns; QPLIB_9008, 1009306 columns, runs out of memory.
- Of the mixed-integer QPs, the three `CBL` files of 150 to 200 binaries
  end `OPTIMAL`. The `DML` files QPLIB_3871, 3698, 3792, 3694 and 3861,
  whose root failed before the barrier fix, reach the work limit with
  incumbents 27% to 71% above the reference; QPLIB_3547's incumbent is 0
  against -0.56; QPLIB_3708 ends `NUMERICAL_ERROR` at a node after an
  incumbent 33% above; QPLIB_5577, 5924, 5527 and 5543 (6014 to 25700
  columns), the `CBL` files 3980 and 3913 and the `CML` file 4270 end
  with no incumbent.
- Of the mixed-integer QCQPs, the two `LMD` files end within 1e-2 of the
  reference (8.1e-4 and 9.9e-3 above). Of the 12 `LMC` files, 10 reach
  the work limit, 2 of them with an incumbent (1.1e-2 and 6.1e-2 above),
  and 2 are refused for a quadratic row over 3000 columns
  (`CONIC_QC_DENSE`).

## Files

- `qpfetch.sh`: fetches the table, the 63 files and `qplib.solu`.
- `table.py`: QPLIB's instance table as tab-separated rows.
- `qpall.sh`: the reading.
