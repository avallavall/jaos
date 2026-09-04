# 02-177 — block triangular form of the published basis (D272)

D271 asked how much of the gate an exact verifier could prove and answered
with the Hadamard bound on the whole basis: 97 of 110 inside 4096 bits, 13
outside. This directory asks what block triangular form does to that, because
the bound is a sum over columns and splitting the basis splits the sum.

## What was run

```
bash bench/measurements/02-177/run-blocks.sh
```

`blocks.c`, built against the library at `9efbf20`, over all 139 gate
instances. 110 solve OPTIMAL and have a basis to read; the 29 reference
infeasibles do not and are skipped, which is the same population D271 used.

Two deterministic stages, both from the literature and neither randomized:

1. A maximum transversal, so every diagonal entry is nonzero. Duff, ACM TOMS
   7(3):315-330, 1981.
2. Strongly connected components of the digraph with an edge `i -> j` when
   the permuted `B[i][j]` is nonzero. Pothen and Fan, ACM TOMS
   16(4):303-324, 1990. Tarjan's algorithm, iterative, because 105127 nodes
   would take the stack down.

**The figure that decides is the largest block, not the sum.** A block
triangular solve holds one block at a time.

## The answer

**110 of 110.** Every basis that D271 refused fits once it is split.

| instance | rows | whole basis | blocks | largest | worst block |
|---|---|---|---|---|---|
| ken-11 | 14694 | 7855.2 | 14694 | 1 | 0.0 |
| ken-13 | 28632 | 14707.9 | 28632 | 1 | 0.0 |
| ken-18 | 105127 | 52522.7 | 105000 | 67 | 34.1 |
| osa-60 | 10280 | 6395.8 | 10280 | 1 | 1.2 |
| pds-20 | 33874 | 15681.6 | 32098 | 1542 | 855.5 |
| stocfor3 | 16675 | 36846.7 | 16099 | 39 | 118.8 |
| woodw | 1098 | 8643.3 | 1076 | 21 | 216.3 |
| d2q06c | 2171 | 6596.4 | 1196 | 892 | 3061.5 |
| dfl001 | 6071 | 4230.3 | 2909 | 3159 | 2279.4 |

`ken-11` and `ken-13` come out fully triangular: every block is a single row,
so their determinant is a product of scalars and the bound is zero. `ken-18`
is nearly so, 105000 blocks over 105127 rows with the largest at 67.

The two D271 named as the other family, `stocfor3` at 9.85 bits per column
and `d2q06c` at 11.18, both split far enough. `d2q06c` keeps an 892-row block
and still lands at 3061.5, inside 4096.

## What this does not say

**This bound is not the one the verifier has to meet.** It counts the basis
entries as if they were already integers. They are not, and Bareiss is a
fraction-free elimination over the integers. `bench/measurements/02-178/`
measures the bound once the rows are scaled to make them integral, and that
figure is much larger for every instance carrying decimal data. Read that
one before believing any count here.

## The cost, and the thing that looked like a hang

The block work is free. Split by phase on the Kennington instances:

| instance | rows | solve | read + basis build |
|---|---|---|---|
| ken-11 | 14694 | 6.88 s | 0.00 s |
| ken-13 | 28632 | 59.57 s | 0.00 s |
| pds-10 | 16558 | 21.30 s | 0.00 s |

A run of this instrument that appeared to be stuck for 33 minutes was
solving `ken-18`, and its output file lagged behind by a buffer. `blocks.c`
does not flush per line; a reader who trusts the file's last line will place
the run about ten instances behind where it is.

`augment` carries the lookahead of Duff's MC21A. **It buys nothing
measurable on this population** and it is there because the cited algorithm
has it. It changes no answer: the 81 netlib instances that a pre-lookahead
run had finished are byte-identical under it, which is also the empirical
check on Pothen and Fan's result that the fine decomposition does not depend
on which maximum transversal is chosen.

## Files

- `blocks.c` — the instrument
- `run-blocks.sh` — builds and runs it over all three sets
- `blocks.txt` — the reading
