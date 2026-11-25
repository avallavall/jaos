# 02-506 — the elimination's pass over each active column, without a branch where the pivot column is long

Taken on 2026-11-24 and 25 on 7b5784ac, for J36's next step: the same
pivots in less time. 02-505 put the factorization at 24% of `mzzv11`'s root
LP, 20 ms each.

## Two instruments

`fbench.sh` dumps the bases the root LP factorizes (`dump.py` on a copy of
the tree: every 16th of `mzzv11`'s in 80 s, every 10th of `dano3_3`'s and
every 6th of `qap10`'s, 192 in all; later every 10th of
`chromaticindex1024-7`, `ns1116954`, `ex9` and `supportcase19` in 120 s) and
runs `jm_lu_factor` over them, with an FNV hash of L, U, the permutations
and the work units. Every variant below hashes the same as 7b5784ac on every
basis. It runs the factorizations back to back, with their arrays warm.

`cycles.sh` wraps `jm_lu_factor` in the cycle counter (`cycles.py`) and
runs the root LP alone with every build of a round at once
(`cycles-read.txt`). Between two factorizations the solver runs about 65
iterations, so there they start cold. On the large bases the two disagree:
`ns1116954`'s factorization reads 0.96x in `fbench.sh` and 1.17x in the
solver for the same code. The solver's reading decides.

## Where a factorization's time went

On 7b5784ac, split by cycle counter on `mzzv11`'s 63 bases, the loop over
the pivot row's columns takes 68%, `compact_pivot_row` 14%, building the
active matrix 5%, laying out U and L's rows 6%, `find_pivot` 4% and the
pivot column 3%. Callgrind puts 54% of the instructions in the pass over
each column and 17% in the pass over the pivot column after it, which met
161.6M rows and filled 3.8M. In the column pass 70% of the entries sit in a
row of the pivot column, in no order a predictor learns, and the branch on
it is where the time goes.

## The change

- Where the pivot column holds at least an eighth as many rows as the
  column (`LU_DENSE_PASS`), the column's pass takes `a - m * urow` for every
  entry, with `m` the row's multiplier and 0 outside the pivot column. A
  live entry is larger than the drop tolerance, never 0, so `a - 0 * urow`
  is `a` bit for bit while `urow` is finite. A shorter pivot column, or an
  `urow` that is not finite, keeps the branch on each row. The rows of the
  pivot column it meets are counted, not branched on.
- The pass over the pivot column runs only when some of its rows are
  missing from the column: 40% of the columns on `mzzv11`, holding 23% of
  the entries. Those columns mark their rows in a second pass, with a stamp
  per column that nothing has to clear.
- Before the elimination every column of the pivot row moved to the bucket
  of its count less one, and the elimination then moved it again. The first
  move is gone: no other column changes place and nothing reads the buckets
  between the two, so the lists come out the same.
- The column pass is a function of its own that is not inlined. Inlined, it
  reloads its arrays from the stack at every entry and runs 5% to 9% slower
  in `fbench.sh`; `restrict` on its pointers changes nothing.

The branch-free pass on every column was the first version. It wins where
most entries meet the pivot column, but where few do the branch is well
predicted, and reading every row's multiplier from an array the solver has
let go cold costs more: `chromaticindex1024-7` read 1.02x the cycles a
factorization took before and `ns1116954` 1.17x. Taking the pass only on
the columns of a long pivot column keeps the gain and loses nothing; 4, 8
and 16 read alike.

Mcycles a factorization in the solver (`cycles-read.txt`, two rounds where
there are two):

| instance | 7b5784ac | after | ratio |
|---|---|---|---|
| `mzzv11` | 69.7, 68.7 | 49.1, 48.8 | 0.70, 0.71 |
| `dano3_3` | 17.3, 17.8 | 13.4, 13.6 | 0.77, 0.77 |
| `qap10` | 14.9, 15.3 | 11.9, 12.1 | 0.80, 0.79 |
| `ex9` | 1123 | 839 | 0.75 |
| `ns1116954` | 66.9 | 63.5 | 0.95 |
| `chromaticindex1024-7` | 157.5, 126.2 | 152.1, 120.4 | 0.97, 0.95 |
| `supportcase19` | 9.17 | 9.17 | 1.00 |

## The stale row positions

A row pattern keeps, beside each column, the position its entry had when it
was written. The column pass compacts the column, so the positions go stale
and `compact_pivot_row` searches. Of its 3.95M lookups on `mzzv11`'s 63
bases, 2.22M find the entry at its position and 1.73M search: the entry
sits 58 places into a column of 126 on average, 36 places before the
position it was written at.

- Positions kept exact (`tombstone-candidate.diff`): the column pass leaves
  dead entries in place, each entry carries its place in its row's pattern,
  and a column is compacted, rewriting those places, once its dead entries
  pass a quarter of its live ones. The same hash, and in `fbench.sh` 17.1
  ms against 14.3 on `mzzv11`, 4.69 against 3.76 on `dano3_3` and 5.05
  against 3.57 on `qap10`; compacting at half the live entries reads 22.4,
  5.46 and 6.03. The dead entries cost the column pass more than the search
  costs.
- The search going back from the written position, then forward past it:
  `compact_pivot_row` takes 12% fewer cycles on `mzzv11`, the same on
  `dano3_3` and 6% more on `qap10`. Four entries a step with one branch:
  more on all three.

`compact_pivot_row` spends about 84 cycles a lookup on `mzzv11`, most of
them reaching the column rather than scanning it, and on
`chromaticindex1024-7`, whose bases are mostly singletons, it is the
factorization's largest part. The positions stay as they are.

## 02-505's profile

`prof.sh` is 02-505's, taking a source tree: built with `-pg -fno-inline`,
the root LP alone for 60 s (`gprof-before-*.txt`, `gprof-after-*.txt`). The
share is the self time of the factorization's own functions
(`gprof-factor-share.txt`); with `-fno-inline` every call to the column
pass pays for gprof's count, 25M calls on `mzzv11`. Both builds of the
three ran at once.

| instance | iterations | factorizations | share before | after | ms each before | after |
|---|---|---|---|---|---|---|
| `mzzv11` | 30,226 and 33,736 in 60 s | 466 and 520 | 33.8% | 21.9% | 25.6 | 14.0 |
| `dano3_3` | 45,213, ended | 699 | 12.9% | 8.8% | 3.36 | 2.36 |
| `qap10` | 22,721, ended | 351 | 22.2% | 14.5% | 3.25 | 2.36 |

## The 27 root LPs

`root.sh` is 02-505's on its `root27.txt`: both builds of an instance at
once for 300 s, two instances at a time (`root27-read.txt`). The two walk
the same path, so the iterations each reaches are its speed. After against
before reads 1.019x geometric over the 26 that run out of time, from 0.992x
(`savsched1`) to 1.214x (`ex9`), and `rail01` ends in both. Most of these
factorize rarely against what their solves and pricing cost.

## The gates

`make test` and `make sanitize` pass, with a test that takes a basis whose
elimination overflows into a pivot row's entry while a column of that row
still holds a row outside the pivot column, which the branch-free pass would
turn to NaN. `netlib`, `netlib-infeas`, `netlib-kennington` and `make
miplib` read 0 regressed, 0 improved, 0 new, 0 at other nodes, and every
record that they and `primal barrier pdlp concurrent warm` write is
byte-identical to the committed one.
