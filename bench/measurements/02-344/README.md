# 02-344 — nested dissection beside minimum degree

Taken on 2026-10-05 on the tree of 0395434, for TODO row J9 (QPLIB_9008
runs out of memory: the barrier's normal matrix of 989604 rows, a grid of
about 99 by 99 points over about 100 time steps, gets a minimum-degree
factor of 7.67e9 nonzeros).

## What was built

`src/chol.c` gains a nested dissection ordering after George and Liu: in
each connected part, a breadth-first level structure from a
pseudo-peripheral node (the search restarted from a node of least degree
in the last level while the height grows, at most 8 times); the level
where half the part's nodes are reached is the middle, its nodes that
touch the next level form the separator, the levels before it (with the
middle's other nodes) and after it are dissected in turn, and the
separator is ordered after both. Parts of at most `CHOL_ND_LEAF` (200)
nodes, and parts whose level structure has fewer than three levels, are
ordered by minimum degree. Dense nodes go last as before.

## QPLIB_9008

`q9008-nd.log`: nested dissection orders it in 9 seconds (4.3e9 work units
with the symbolic count) for a factor of 2.80e9 nonzeros and 1.75e13
operations, against minimum degree's 7.67e9 and 2.8e14, which took 3
minutes and 41.6e9 work units. The factor still needs about 45 GB, so the
solve still ends out of memory.

## Where it wins

`ndtry.txt`: the factor's operations under each ordering on four models,
both solves ending at the same optimum. `picks.txt` and `picksum.txt`:
every Maros-Meszaros factorization of 1000 rows or more with both
orderings counted. Nested dissection wins on the grid models only
(`cont-*`, `aug2d*`, `aug3d*`), where minimum degree costs from 137
(`aug2d`) to 1340 (`cont-200`) operations per nonzero of the input.
Elsewhere it costs 2 to 2e9 times minimum degree's operations (`boyd2`:
9.5e14 against 4.0e5), on models from 0.03 to 29800 operations per
nonzero.

## The rule and the readings

Minimum degree is counted first. Only when its factor costs more than
`CHOL_ND_TRY` (100) operations per input nonzero, on 1000 rows or more
(`CHOL_ND_MIN`), is nested dissection ordered too; then both are counted
under a cap that starts at twice the trigger and grows fourfold, and the
one with fewer operations is kept, minimum degree on a tie. Against the
base (`*-ndbase.txt`):

| reading | trying it on every factor of 1000 rows or more (`nd1k`) | with the trigger (`ndt100`) |
|---|---|---|
| Maros-Meszaros, work geometric | 0.988x | 0.973x |
| Maros-Meszaros, work sum | 0.909x | 0.879x |
| worst | `boyd2` 1.582x, 30 models at 1.02x to 1.17x | `ksip` 1.074x |
| CBLIB, work geometric | 1.063x (`sched_200_100_orig` 1.277x) | 1.001x, none past 2% |

With the trigger, 14 grid models gain: `cont-200` 0.514x, `cont-201`
0.551x, `cont-300` 0.727x, `cont-100` 0.743x, `cont-050` 0.744x,
`aug3d*` 0.818x to 0.831x, `aug2d*` 0.821x to 0.843x, `cont-101` 0.868x.
`cont-300`'s suboptimality bound reads 3.89e-9 against 1.32e-9, a
rounding-level change of a model that ends at the same work limit. The
barrier's Netlib reading (`barrier-ndt100.txt`) keeps every iteration count
and answer; `bnl2`, `d2q06c`, `dfl001`, `maros-r7` and `pilot` pay 0.1% to
0.7% more work for the second ordering, which loses there.
