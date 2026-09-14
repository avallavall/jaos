# 02-230 — the box round a freed integer column, and what the sweep found

`relax --cols` did not finish on a model whose rows admit no integer point,
because the elastic copy frees every column and a free integer column
gives the tree an unbounded space (`TODO.md` row 0, 980c565). The fix holds
a freed integer column in its own bounds widened by `M` on each freed side
and grows `M` until the total comes out at or below it; then that total is
the answer for the free box too, because any cheaper point would hold some
column more than `M` outside its own bounds, and that alone costs more than
`M`. `M` starts from the copy solved with the integer marks dropped, which
is a lower bound on the answer, at twice that or 1, rounded up, and doubles
when the box is too narrow or holds no integer point. A model with no
integer column keeps the free box and its single solve.

The reading asks two things of it: no answer may move, and the cost of
the extra rounds has to be read. `relax.c` generates models and prints one
line per model and scope; `relax.sh` runs it under the library in the tree
and under the `src/relax.c` of 916fc15, the commit before the box, and
compares the lines.

## The models

2000 per seed, six seeds. Three to seven columns in boxes `[0, U]` with `U`
from 0 to 4, two to five rows with coefficients in -3 to 3, an integer mark
on about half the columns of three models in four and on none of the
fourth. Every model plants an integer point `z` with entries in -6 to 6,
well outside the boxes, and sets each row's bounds around `A z`: six in ten
rows are equalities at `A z`, the rest one-sided or ranged with slack of 0
to 2 on the open side. So once the columns are freed the rows admit an
integer point, and both the free tree and the growing box have something to
find. Each model is relaxed over the columns, over both, and over the rows,
the last as a control the box must not touch.

Two properties are read on every line: the model with every move applied
solves feasible (`moved`), and the total is the sum of the sizes of the
moves (`sum`).

## The reading

Against the free box, per seed, 2000 models and 12000 lines each:

| seed | cols: exact / within 1e-9 / moved | cols work | worst | both work | rows |
|---|---|---|---|---|---|
| 1 | 1965 / 35 / 0 | 1.138x | 4.43x | 1.162x | byte-identical, 1.000x |
| 2 | 1958 / 42 / 0 | 1.169x | 6.78x | 1.163x | byte-identical, 1.000x |
| 3 | 1958 / 42 / 0 | 1.103x | 5.31x | 1.148x | byte-identical, 1.000x |
| 4 | 1970 / 30 / 0 | 1.116x | 25.99x | 1.155x | byte-identical, 1.000x |
| 5 | 1956 / 44 / 0 | 1.129x | 4.69x | 1.155x | byte-identical, 1.000x |
| 6 | 1954 / 46 / 0 | 1.269x | 7.80x | 1.165x | byte-identical, 1.000x |

No total moves past 1e-9 of itself, nothing the free tree finished is lost
to a work limit, and every applied model solves feasible. The rows scope
is the same output to the last digit: the box does not touch that path.
The columns scope costs 1.10x to 1.27x the work of the free tree per seed,
most of it the LP solve that sets `M`; the one model at 26x (seed 4, m74)
is a small tree whose box needed several doublings. The totals within
1e-9 rather than exact are where the total is now the sum of the published
moves rather than the copy's objective, which is the second defect below.

## The pass is not vacuous

`relax.sh control` patches the stop condition to accept the first round
whatever the total, builds, and runs seed 1 again: one model of 2000,
m1028 over the columns, comes back with a total of 4 against 3. The first
box was too narrow there, and only the check that decides the rounds saw
it. So the rounds are rarely paid and the check is not dead code.

## Two defects the sweep found, both older than the box

The first run reported a model in a hundred whose applied model did not
solve, under the old library as much as the new one. Model 108 of seed 1
(`tests/data/relax_snap.mps`) shows it: the tree lands the integer column
`C3` at -0.99999999999999989, the move was read off the two elastic
columns, so `C3`'s lower bound moved to that number, and the tree rounds
an integer column's bounds inward, so `C3 >= 0` and the applied model was
infeasible. The continuous column `C5` had the same move, and a bound one
ulp short of the point is one presolve refuses. Model 250 of seed 3 is a
plain LP with the same shape: `C6`'s upper bound moved to 4.222222222222221
for a point at 4.2222222222222223.

Two fixes. The tree now rounds an integer column on the incumbent's own
publication, where before only the republished point was rounded, so an
integer column's value is exact. And a column's move is read off the
column's own value once the elastics say which side moved, and nudged by
an ulp until the moved bound holds the value; an elastic that sits a
rounding hair below zero, which the old code published as a move of
-4.4e-16 and counted, is no move at all. The total is then the sum of the
published moves whenever an integer column moved, because the copy's
objective still carries the tree's integrality slack. With both in, every
line reads `moved=ok sum=ok` over the six seeds, and the MIPLIB and Netlib
gates are byte-identical, so the rounding moved no published answer there.

## How to run

```
make all
bench/measurements/02-230/relax.sh            # six seeds, ~10 minutes
bench/measurements/02-230/relax.sh control    # seed 1, the patched stop
bench/measurements/02-230/relax.sh report     # the comparisons again
```

Both runs swap `src/relax.c` out and put the working copy back, and leave
the tree built under the current source.
