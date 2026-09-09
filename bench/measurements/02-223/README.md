# 02-223 — BARRIER_DENSE_FACTOR, 5 and 20 against the shipped 10

Sets `BARRIER_DENSE_FACTOR` (`docs/tolerances.md`). Taken 2026-09-09 at
fbc8f76, the commit that left dense columns out of the barrier's normal
matrix and corrected for them by Sherman-Morrison-Woodbury.

A column is dense when its count exceeds the factor times the average
column count (and `BARRIER_DENSE_MIN`). Before the commit the campaign
read 72 of 94 inside 10x the dual's work; at the commit, with the factor
at 10, it reads 73: fit2p comes in at 1.64x, fit1p and seba stay out at
12.2x and 23.8x. The question is whether a lower factor, which treats
more columns as dense, brings more in, or a higher one loses fit2p.

## What is here

| file | what it is |
|---|---|
| `dense.sh` | builds a worktree of the named commit per factor with only the constant changed and runs the barrier campaign, the standard 94 at 10x the dual's work |
| `barrier-dense-5.txt`, `barrier-dense-20.txt` | those campaigns; the 10 reading is `bench/results/barrier.txt` at fbc8f76 |

## What it says

| factor | inside 10x | past it | disagreeing |
|---|---|---|---|
| none (7938f84) | 72 | 22 | 0 |
| 5 | 74 | 20 | 0 |
| 10 | 73 | 21 | 0 |
| 20 | 73 | 21 | 0 |

20 is line for line the same file as 10: no column on the set sits
between 10 and 20 times the average, so the same three instances fire.

5 reads one more, and the reading is worse. Its two gains are hand-offs,
not barrier answers: on d2q06c the barrier runs 97 iterations and gives
up, and on fit1p it reaches the 200-iteration cap, and in both the dual
simplex then finishes from the slack basis inside the limit (27935 and
494 iterations, the cold dual's own counts). What 5 costs is the barrier
itself on the instances it newly touches: fffff800 goes from 31 iterations
and 3.0x to 89 iterations and past the limit, pilot-ja from 29 iterations
and 3.4x to 68 and 8.9x, israel from 4 iterations to 17 before the limit
stops it, and pilot from 4.31e9 to 5.42e9 work units. Columns of five to
ten times the average are not dense enough for the correction to beat
the factor.

**10** ships.
