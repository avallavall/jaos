# 02-313 — the LU's column singleton step without the search and the shift

Taken on 2026-09-24 for TODO row H1 with `tools/icount.sh -r 1a826dc` on
ten Netlib instances (`icount.txt`, the dual simplex's instructions).

## The change

When the LU factor pivots on a column singleton, every other column of the
pivot row loses its entry in that row. The factor found that entry by
searching the column from its start, then moved the column's tail down one
place to drop it. On `fit2p` the basis holds a few columns of about 3000
entries, and each is searched and shifted once per pivot row it meets.

Now each row entry keeps the position its column held it at, and the search
starts there (it falls back to the search from the start when the column
has moved since). The singleton step leaves the entry where it is: the row
is done, and every loop over a column already skips done rows. The next
elimination that rewrites the column drops those entries. Every column
keeps its order, so every pivot choice is the one it was.

## Readings

The netlib, infeasible and Kennington gates write the same files as before.
`tests/test_lu.c` factors a 40 by 40 matrix with three dense columns through
37 singleton steps, twice in the same object, and requires the same
permutations and pivots.

| instance | ratio |
|---|---|
| fit2p | 0.447 |
| fit1p | 0.794 |
| degen3 | 0.993 |
| stocfor3 | 0.998 |
| d2q06c | 1.000 |
| greenbea | 1.001 |
| truss | 1.001 |
| 80bau3b | 1.001 |
| pilot87 | 1.002 |
| ship12l | 1.015 |

Geometric mean 0.903. The small rises come from the second array each row
entry now carries and from loops that step over the done entries a column
keeps until its next rewrite.
