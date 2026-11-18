# 02-486 — the rows together leave no integer point

Taken on 2026-11-18 on the tree of 5c159065 plus the proof, for TODO row
J17.

## What was missing

Over the columns, `jaos_feasrelax` holds a freed integer column in a box
that grows, and when the caps stop the search it tries to prove that no
box holds a point. 02-362 proves it from the equality rows, 02-369 from one
ranged row. Rows that are each satisfiable but hold no integer point
together still ended "the rows and the integrality may admit no point at
all". Three such models:

- `tests/data/relax_pugh.mps`, Pugh's example: `27 <= 11x + 13y <= 45` and
  `-10 <= 7x - 9y <= 4`, as four one-sided rows. Bounded, with real points;
  each row alone has integer values between its sides.
- `pinch.lp`: `2x - 2y >= 1` and `x - y <= 0.75`, unbounded.
- `slab.lp`: `x + y + c` in `[0.5, 0.75]` and `c - y` in `[0, 0.125]` with
  `c` continuous, four one-sided rows, unbounded.

## The proof

`rx_rows_empty` in `src/relax.c` runs when the two earlier proofs find
nothing. It reads every row without an indicator. Over the columns every
column bound is free, and it ignores the SOS sets and the semi-continuous
marks, so it works on a relaxation: a proof that it has no point is a proof
that no box holds one.

1. A row that a column can satisfy alone is set aside: every row of a
   column whose rows are all one-sided and pull it the same way (it goes
   far enough that way), or the one row of a continuous column in no other
   row. Repeated until no such column is left. This is exact.
2. The rows left are split into blocks that share no column. A block of
   more than `RELAX_LATTICE_CELLS` numbers, a ranged row counting as two
   rows, or with no integer column, is skipped.
3. Each row is scaled to whole numbers by a power of two, which is exact
   for doubles, and held as `a x >= b` or `a x = b` in `jm_bigint`s of at
   most `RELAX_LATTICE_BITS` bits. Then, until the block is settled:
   - a row over integer columns is divided by the gcd of its coefficients
     and its side rounded up, the Chvátal–Gomory rounding; an equality
     whose gcd does not divide its side, or a row `0 >= b` with `b > 0`,
     proves there is no point. A row with a continuous column is divided
     by the gcd of all its numbers;
   - parallel rows are merged; two opposite rows whose sides cross prove
     there is no point, and two that touch become an equality;
   - an equality with a continuous column eliminates it and is dropped
     (as in 02-362); an equality over integer columns is brought to one
     column by unimodular column steps, which keep the integer points
     integer, and fixes that column;
   - with no equality left, a column whose rows all pull it one way is
     dropped with them; else a continuous column is eliminated by
     Fourier–Motzkin, which is the exact projection; else an integer
     column `z` with the fewest new rows. Its elimination is exact when
     every coefficient of `z` on one side is 1. Otherwise it follows the
     Omega test (W. Pugh, "The Omega test: a fast and practical integer
     programming algorithm for dependence analysis", 1991): the real
     shadow, which is a relaxation, is tried first; when it has a point,
     the dark shadow, each pair `b z >= beta`, `a z <= alpha` read as
     `b alpha - a beta >= (a - 1)(b - 1)`, must have none, and so must
     every splinter, the block with one bound row of `z` made the equality
     `b z = beta + k` for `k` from 0 to `floor((a_max b - a_max - b) /
     a_max)`, on the side with fewer splinters.
4. It stops, and claims nothing, past `RELAX_SHADOW_WORK` work units or
   what the work limit leaves, with more than `RELAX_SHADOW_CELLS` numbers
   held at once over its open branches, at a number past
   `RELAX_LATTICE_BITS`, or at a splinter coefficient past 31 bits.

Every step either keeps the integer points or drops constraints, except
the splinters and the dark shadow, which together cover the integer points
by Pugh's theorem. The proof also knows when a block has a point: no row
left, or a dark shadow with a point. The reading below uses that to check
the proof from the other side too.

The report then says `JAOS_SOLVE_INFEASIBLE`, the message says why, and the
call still returns `JAOS_ERR_NUMERICAL`. `work_units` now counts the
proofs' work whether or not one succeeds; before, a proof that failed was
in the message's count and not in `work_units`.

## Where it fires that nothing did before

`cli.sh` runs `jaos relax --cols` on the three models under 5c159065 and
under the tree (`cli.txt`). All three ended with "may admit no point" and
now end with "none in any box: the rows together ...", exit 5 both times.
The box search is the same; the proof adds 1,471 work units on
`relax_pugh.mps` (5,592,721 to 5,594,192), 14 on `pinch.lp` and 100 on
`slab.lp`.

`shadow.sh` builds `shadow.c`, which includes `src/relax.c` and calls the
proof directly, over two seeds (`shadow.txt`). "Old proofs" are 02-362's
and 02-369's on the same model.

- `plant`: 2000 models a seed, 1 to 8 rows over 2 to 8 columns, three in
  four integer, coefficients 1 to 5 in size, one in ten halved up to three
  times. A point `z`, integer on the integer columns, is planted; three
  rows in ten are equalities at `A z`, three ranged, four one-sided, with
  slack from 0 to 2 on each finite side. `plant-wide` takes coefficients up
  to 15. The proof refuses none of the 8000.
- `half`: the `plant` models with `z` moved by a half on the first integer
  column. The proof settles 876 and 831 of 2000 as having no point, 215 and
  177 of them past the old proofs. A MIP solve with the integer columns in
  [-64, 64] finds a point on none of those (it stops at its work limit on
  33 and 21). Of the 1097 and 1149 it reads as having a point, the MIP
  finds one on every model where it ends, 16 and 14 of them only in
  [-1e5, 1e5]. It leaves 27 and 20 open, at its work budget: the MIP finds
  a point on 13 and 8 of those, and none in [-1e5, 1e5] on 13 and 12.
- `pugh`: 2000 models of 2 or 3 integer columns and as many ranged rows or
  one more, coefficients up to 15 in size, sides up to 1.5 from a point
  with eighths. 1939 and 1946 have no point, 1071 and 1082 past the old
  proofs; exact brute force over the box the LP gives agrees with every
  verdict it can read (it could not read 1 and 5 models, too wide).
  `pugh-wide` (500, coefficients up to 60): 491 and 493 with no point, 291
  and 268 past the old proofs, every verdict agreed.

Every model an old proof settles, this one settles too ("old only" is 0
throughout). Two runs of seed 1 print the same lines apart from the
seconds.

## Where it does not

- A block past `RELAX_LATTICE_CELLS`, a number past `RELAX_LATTICE_BITS`,
  a splinter coefficient past 31 bits, a row with an indicator, and an
  infeasibility that rests on SOS sets or semi-continuous marks.
- The budget: 27 and 20 of `half`, 14 and 27 of `plant`, 69 and 74 of
  `plant-wide` stop at it. A feasible block is where it goes, because
  showing there is a point takes the whole elimination, and the proof only
  runs once the boxes found none.

## The constants

`RELAX_SHADOW_WORK` is 4,000,000. On `half` the proof settles 870 and 823
at 400,000, 876 and 831 at 4,000,000, and 881 and 835 at 40,000,000, for
14.2e6 and 9.7e6, 72.6e6 and 62.7e6, and 546e6 and 455e6 work units over
the 2000 models. The proof runs about 1.4e8 units a second (the `plant`
lines: 26.1e6 units in 0.183 s, 61.4e6 in 0.438 s), so a stopped proof
costs about 0.03 s; the box search before it costs 4e7 units on
`tests/data/relax_runaway.mps`.

`RELAX_SHADOW_CELLS` is 16384. On `half` the proof settles 876 and 830 at
4096, 876 and 831 at 16384, and 876 and 832 at 65536: it rarely binds.
With numbers of 4096 bits it would hold about 9 MB.

`dense` plants a point in single blocks at the `RELAX_LATTICE_CELLS` cap
(2 by 1023, 8 by 255, 32 by 63, ranged rows of width a half): the rows
round to equalities and the Hermite step finds the point in at most
175,663 work units.

## The earlier readings

On 5c159065 and on the tree, `02-362/lattice.sh` and `02-369/range.sh`
print the same lines apart from the seconds, `02-297/compare.sh` reads the
two trees identical on both seeds, and 02-230's `relax.c` prints the same
6001 lines on each of its six seeds under both (`earlier.txt`). Those
models all plant a point, so the new proof has nothing to settle there.

## How to run

```
bench/measurements/02-486/shadow.sh            # two seeds, ~1 minute
bench/measurements/02-486/shadow.sh 2000 400000
bench/measurements/02-486/cli.sh TREE_A TREE_B
```

The sweep of `RELAX_SHADOW_CELLS` ran `shadow.sh` against copies of
`src/relax.c` with the constant edited.
