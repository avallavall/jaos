# 02-309 — minimal covers with exact sequential lifting

Taken on 2026-09-24 for TODO row H2, on the tree of 7f9911d.

HiGHS closes `p0282` at the root and `lseu` in 7 nodes, where JAOS takes
3355 and 6315 (`bench/compare/results/mip-miplib.txt`). Both are binary
knapsack models. JAOS's cover separator takes one greedy cover per row
side and lifts the columns outside it with the simple bound: `h` when the
column's weight lies between the `h` and `h + 1` largest cover weights.
More rounds of every cut family raise `p0282`'s root bound only from
253933 to 256322 against its optimum 258411.

`cover-exact.patch` (behind `MIP_COVER_EXACT`, off in the patch) shrinks
the greedy cover to a minimal one, dropping the columns with the smallest
LP value first while it stays a cover, and lifts every other column of the
row in turn, the highest LP value first, with the exact coefficient: a
table of the least weight that reaches each profit over the cover and the
columns lifted so far gives the best profit that still fits beside the
column (Zemel's form of sequential lifting). The fit test leans towards
"fits", which can only lower a coefficient.

MIPLIB 3 (`make miplib J=2`) against 7f9911d, every objective at the
reference:

| arm | work | better | worse |
|---|---|---|---|
| every row | 1.085x | enigma 0.220x, l152lav 0.610x, lseu 0.887x | misc03 2.910x, air03 2.334x, p0033 1.861x, gen 1.791x, mod008 1.546x, p0201 1.296x |
| rows of at most 64 columns | 1.086x | l152lav 0.604x, lseu 0.887x | misc03 2.910x, p0033 1.861x, gen 1.791x, p0201 1.352x |

`air03` stays at one node and pays for the table itself, which grows with
the square of the row on its set-partitioning rows. `p0282` goes from 3355
nodes to 2049 at the same work and a root bound of 254702. Refused as
`cover-exact` in `bench/refusals.txt`.
