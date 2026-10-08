# 02-362 — the proof that the relaxation's widest box is empty

Taken on 2026-10-08 on the tree of 5599038 plus the proof, for TODO row
J17.

## What was missing

Over the columns, `jaos_feasrelax` holds a freed integer column in a box
that grows. A model whose rows and integrality admit no point at all has
none in any box, so the search stops at `RELAX_BOX_ROUNDS` or
`RELAX_ROUND_WORK` (02-297) and said "the rows and the integrality may
admit no point at all". It could not say whether they do.

## The proof

When the box search stops that way and the scope is the columns alone,
`src/relax.c` reads the equality rows without an indicator, in blocks
that share no column, and ignores every bound, every inequality row and
every SOS set. Dropping constraints only adds points, so a block with no
integer point proves the model has none. A block over more than
`RELAX_LATTICE_CELLS` (4096) numbers, or with no integer column, is
skipped. For each block:

1. Each row is scaled by a power of two to whole numbers. A double is an
   exact dyadic, so the scaling is exact. The numbers are `jm_bigint`,
   4096 bits each, and an overflow ends the block with no proof.
2. Each continuous column is eliminated with the row that holds its
   smallest nonzero entry. That row is then dropped, because a continuous
   column with no bounds can meet it at any value of the others. Every
   other row becomes a whole-number combination of itself and the pivot
   row, divided by its content.
3. The rows left are over integer columns only. They are brought to
   Hermite normal form by column steps of the extended Euclid kind, which
   keep the integer points integer. A row whose pivot does not divide its
   right-hand side, or a row of zeros with a nonzero right-hand side,
   proves there is no integer point.

The report then says `JAOS_SOLVE_INFEASIBLE` and "none in any box", and
the call still returns `JAOS_ERR_NUMERICAL`, as for the other models with
no relaxation. The proof's steps count in `work_units`, one per number
updated.

`tests/data/relax_runaway.mps` ends with the proof:

    jaos: cannot relax tests/data/relax_runaway.mps: no point in the
    columns' box widened by 896, after 8 rounds and 40198408 work units,
    and none in any box: the equality rows, with the continuous columns
    eliminated, ask for an integer combination that no integer point gives

The proof's own share is 14 work units of the 40198408.

## The reading

`lattice.sh` builds `lattice.c`, which includes `src/relax.c` and calls
the proof directly, and runs it on two seeds (`lattice.txt`).

- `plant`: 2000 models a seed, 1 to 8 equality rows over 2 to 10 columns,
  about half of them integer, coefficients in -3 to 3 with one in ten
  halved up to three times. The right-hand side is `A z` with `z` integer
  on the integer columns. The proof claims no model of the 4000.
- `half`: the same models with `z` moved by one half on the first integer
  column. The proof settles 1116 and 1082. A MIP solve with the integer
  columns in [-20, 20] finds no point on any of the 2198 settled models
  (it stops at its work limit on 71 and 73 of them). Of the rest, it finds
  none on 43 and 40. In [-1e6, 1e6] it finds a point on 24 and 18 of
  those, and none on 19 and 22. Two of that kind, from a run of 300 models
  on seed 1, were written out (`miss-1.lp`, `miss-2.lp`, with the box of
  the second solve). `latcheck.py` eliminates their
  continuous columns with exact fractions: each leaves one integer row
  whose gcd divides its right-hand side, with coefficients near 1e7, so
  each has integer points, outside the box.
- `dense`: one block at the cap, all integer, 32 rows over 127 columns,
  and 16 by 255 and 8 by 511 with the last row's coefficients doubled and
  its right-hand side made odd. Each runs in 0.011 s or less and at most
  170849 work units, and each odd one is proved. At the cap the matrix
  holds 4096 numbers of 528 bytes, 2.2 MB.
