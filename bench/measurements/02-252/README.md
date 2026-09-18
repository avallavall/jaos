# 02-252 — every model JAOS writes, read by a reader JAOS did not write

The format sweeps before this one (02-229, 02-240, 02-245) wrote a model
and read it back with JAOS's own reader. A writer and a reader written
together can agree on a form no other tool takes, and the round trip
cannot see it. This reading hands the files to HiGHS 1.15.1, the build
`bench/compare/fetch-solvers.sh highs` makes, and compares its verdict and
objective with JAOS's own solve of the model it wrote.

## The models

`writers.c`: 5 to 20 columns, 3 to 12 rows, a planted point inside the
boxes with each row's bounds set around its activity there. Minimised or
maximised, an objective offset, boxes of six shapes (on `[0, U]`, `[L, U]`
with `L` below zero, open below, open above, free, fixed), rows of four
shapes (`<=`, `>=`, `=`, ranged). A third of the models carry integer
marks, some binaries and some semi-continuous columns; a fifth of the
continuous ones carry a convex `Q`, diagonal or with pairs built as `B'B`
plus a diagonal. Half carry names. Every tenth model gets a row of one
column and an empty ranged row. Each is written as MPS and as LP.

`writers.sh` runs HiGHS on both files with `mip_rel_gap = 0`,
`mip_abs_gap = 0` and a limit of 20 seconds, and a file agrees when the
status is the same and the objective is within 1e-6 relative. HiGHS says
`Primal infeasible or unbounded` where presolve cannot tell the two
apart, and that is taken as agreeing with either. A disagreement goes to a
third reading: SoPlex 8.0.3 for a model with no integer mark and no `Q`;
for a QP, `jaos check` on the answer JAOS published, the ray when JAOS
calls the model unbounded and the point with its duals when it calls it
optimal; for a MIP JAOS calls unbounded, whether the same model with its
objective at zero has an integer point the checker takes, found by
`writers.c`, which refutes an `Infeasible`.

## What the first run found, 100 models at seed 1

**The LP writer's ranged row.** JAOS wrote `R2: -12 <= 1 C6 - 6 C8 <= -11`,
the two-sided form its own reader takes. The CPLEX LP format has no such
form and HiGHS does not read it: `R2: 12 <= C6 + 6 C8 <= 14` is a parser
error, and the form JAOS wrote, with a coefficient of 1 in front, HiGHS
reads without a word as the row `-12 <= 1`, empty, and a second row
`C6 - 6 C8 <= -11`. 38 of the 51 LP models' files gave another answer in
HiGHS. HiGHS itself writes a ranged row as two rows, `r1lo:` and `r1up:`.

**The MPS writer's integer column with no bounds.** An integer column on
`[0, +inf)` got no `BOUNDS` line, which is JAOS's reading and CPLEX's.
HiGHS reads an integer column in a `MARKER` block with no bound at all as
a binary, the old MPSX convention: 9 of 38 MIPs gave another optimum.

## The fixes

- The LP writer prints a ranged row as two rows, `A: ... >= l` and
  `A_hi: ... <= u`, and a `\ range A A_hi` comment at the top. The reader
  folds the two back into one row when the pair matches exactly (same
  terms in the same order, same indicator, `l < u`); any other pair stays
  two rows, so a file edited by hand reads as what it says.
- A free row, which the LP writer refused, is written `A: ... >= -inf`.
  HiGHS reads that as a free row, and JAOS's reader now takes an infinite
  right-hand side too (`>= -inf`, `<= inf`), refusing `>= inf`, `<= -inf`
  and `= inf` by line.
- The MPS writer gives an integer column on `[0, +inf)` an explicit `PL`
  bound.

## The reading after the fixes

Each model gives two files, so 2000 files per seed.

| seed | models (LP / MIP / QP) | files agree | a third reading sides with JAOS | HiGHS past 20 s | differ |
|---|---|---|---|---|---|
| 1 | 1000 (536 / 327 / 137) | 1966 | 25 | 9 | 0 |
| 2 | 1000 (557 / 319 / 124) | 1976 | 18 | 6 | 0 |

**Nothing JAOS writes is read wrong any more.** Every file HiGHS and JAOS
disagree on is one where a third reading shows HiGHS is the one wrong:

- 10 LP files, where HiGHS calls an unbounded model infeasible from the
  LP file with the ranged row in two halves, and unbounded from the MPS
  file of the same model; SoPlex 8.0.3 calls it unbounded from both.
- 3 MIP files, the same split: HiGHS says `Infeasible` from the LP file
  and `Primal infeasible or unbounded` from the MPS; the relaxation is
  unbounded, and the model with its objective set to zero has an integer
  point that the checker takes, so the model is unbounded.
- 30 QP files: a QP JAOS calls unbounded (a free column with a cost and
  no row, or a flat pair), where HiGHS's QP solver reports an optimum at
  2.5e8 or runs into its limit; JAOS's ray passes `jaos check`, rate,
  escapes and curvature. One QP HiGHS calls unbounded with a `-nan`
  objective is optimal: JAOS's point and duals pass `jaos check` on both
  sides with a gap of 6e-15.

HiGHS runs out of its 20 seconds on 15 QP files, 13 of them unbounded
models and 2 of one optimal model, and says nothing there.

Before the fixes the first 100 models of seed 1 read 76 files agreeing
and 124 differing.

## How to run

```
make all
bench/compare/fetch-solvers.sh highs
bench/measurements/02-252/writers.sh 1000 1
bench/measurements/02-252/writers.sh 1000 2
```
