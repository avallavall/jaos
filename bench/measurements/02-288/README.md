# 02-288 — the barrier's Cholesky on N threads

Taken on 2026-09-22 for TODO row B9, on a 12-thread machine, first on the
tree of ae25a70 and then on e4a9f82 (the crossover push), each with the row
applied. Wall-clock times are one run each, so they carry noise; the
answers, iterations and work units are exact.

## Where the barrier spends its time

`timing-subtree.txt` is the first design with a timer inside the numeric
factor. On dfl001 the barrier makes 201 factors, and on one thread they
take 99.5 s of a 114.3 s solve: 87%. The factor is an up-looking row
Cholesky over a minimum-degree order. Its elimination tree has one heavy
top: 1053 of 4919 rows hold 5.01e8 of the 5.29e8 eliminations of one
factor, 95%.

The first design split the tree below that top into subtrees, one set per
thread, and ran the top on one thread afterwards. It gave the right factor
and 11% on dfl001 (99.5 s of factor time to 88.4 s: 4.2 s for the subtrees,
84.2 s for the top), since the top is where the work is. pilot87 spends 3.5
s of 72.3 s in the factor and maros-r7 3.8 s of 7.3 s, so the factor is not
where their time goes.

## The design that landed

The rows go in blocks of `CHOL_BLOCK` (32) consecutive rows. For a block
that holds at least `CHOL_BLOCK_WORK` eliminations (counted once in the
symbolic phase):

1. Each row of the block, on its own thread, computes its reach, gathers
   its row of the matrix, and solves against every column before the
   block. It only reads the factor's columns up to the block's first row,
   which no thread writes during this step, and it keeps its own mark,
   work vector and stack.
2. One thread then takes the block's rows in order. For each row it walks
   the same reach in the same order as the one-thread factor, applies the
   updates the first step left out (the ones into the block's own rows)
   and solves against the block's earlier rows.

Every entry of the factor then receives the same terms in the same order
as in the one-thread factor, so the factor is bit-identical at any thread
count and any block size. `tests/test_chol.c` compares the factor at 1, 2,
3, 4 and 7 threads on a dense and a sparse matrix whose heaviest block
passes the threshold. A lighter block, and every block at one thread, runs
the one-thread code.

`timing-blocked.txt` is the first reading, on the tree before the crossover
push and with other work on the machine: dfl001 71.75 s on one thread and
41.92 s on four, pilot87 65.85 s and 61.59 s, maros-r7 6.45 s and 5.73 s,
d2q06c 19.65 s and 19.61 s, each pair with the same answer, iterations and
work units.

## The sweep

`sweep-1.txt` and `sweep-2.txt`, on the tree with the push and nothing else
running, one run each. Every line of an instance carries the same objective
and work units.

dfl001 by thread count, at the defaults:

| threads | 1 | 2 | 4 | 8 |
|---|---|---|---|---|
| wall | 67.67 s | 50.77 s | 38.76 s | 38.05 s |

dfl001 on four threads by block and threshold (`JAOS_CHOL_BLOCK_VALUE`,
`JAOS_CHOL_BLOCK_WORK_VALUE`):

| block | threshold 0 | 1e4 | 1e5 | 1e6 | 1e7 |
|---|---|---|---|---|---|
| 16 | | | | 43.01 s | |
| 32 | 41.84 s | 37.92 s | 32.80 s | 38.76 s | 39.63 s |
| 64 | | | 36.56 s | 35.87 s | |
| 128 | | | 39.93 s | 39.99 s | |

maros-r7 reads 4.00 s on one thread and 2.85 s on four at the defaults, and
3.11 s to 4.26 s at the other settings; pilot87 22.90 s on one thread and
20.89 s to 23.20 s on four, since its time is in the crossover.

Blocks of 32 and 64 with a threshold of 1e5 or 1e6 read within one run's
noise of each other. Blocks of 16 and 128 and a threshold of 0 read slower:
small blocks start threads more often, large ones move work into the
ordered part, and a threshold of 0 starts threads for blocks too light to
pay for them. The defaults stay at 32 and 1e6.
