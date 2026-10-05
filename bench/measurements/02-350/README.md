# 02-350 — why strong branching grows some trees

Taken on 2026-10-05 on the tree of c367b5e, for TODO rows J7 and J11 and
the open question of 02-336: under strong branching (`--reliability 4`)
`rgn`, `misc03`, `enigma` and `mod008` end with two to three times the
nodes they reach without it. Single solves of MIPLIB 3 models, no work
limit. The patches read switches from the environment and apply to
`src/mip.c` of c367b5e.

## The probes are right

`pairlog.py` prints each probe (column, direction, parent bound, probe
bound) and each tree child (column, direction, parent bound, child bound),
and `paircmp.py` pairs them. On `mod008` 283 of 309 pairs agree to the
bit and 26 children sit lower, from the cuts the child drops; on `rgn` 73
of 77 agree. A probe's LP is also the same LP solved cold. The probes
measure the child they describe.

## The averages are not

`gainlog.py` and `gaincmp.py` compare, per column and direction, the mean
gain per unit of fraction from probes against the mean from tree
children:

| model | probe mean / tree mean, median | zero gains, probes | zero gains, tree |
|---|---|---|---|
| `rgn` | 0.449 | 0.283 | 0.102 |
| `misc03` | 0.888 | 0.238 | 0.080 |
| `mod008` | 0.826 | 0.000 | 0.001 |

The tree records a column's gain only at the nodes where it was chosen,
where its gain was high; a probe records it also where it lost. Both feed
the same pseudocost average, so a column that was probed looks cheaper
than one learned in the tree, and later nodes choose by that bias.

## Probes that score only their own node

`sbnolearn.py` (`JAOS_SBNOLEARN`) keeps the probes' gains out of the
pseudocosts: a node scores its probed candidates by their fresh gains and
the rest by the tree's pseudocosts. `sbfresh.py` (`JAOS_SBFRESH`) scores
the same way but still stores the gains.

| model | reliability 0 | 4 | 4, fresh | 4, no learning | 2, no learning | 1, no learning |
|---|---|---|---|---|---|---|
| `rgn` nodes | 127 | 421 | 395 | 71 | 75 | 75 |
| `misc03` nodes | 240 | 343 | 367 | 143 | 141 | 161 |
| `mod008` nodes | 683 | 1613 | 1099 | 767 | 851 | 963 |
| `enigma` nodes | 1983 | 4995 | 881 | 878 | 795 | 8368 |

Without learning the trees fall below the trees without probes, but the
work does not: a column becomes reliable only by the tree's own
observations, so the probes go on. Against reliability 0 the work reads
2.5x (`rgn`), 3.3x (`misc03`), 4.9x (`mod008`) and 4.5x (`enigma`) at
reliability 4, and `bell5` at reliability 1 ran past 50 minutes and was
stopped. A probe here is a full LP solve from the parent's basis.

Other node features were read with and without probes, and none of them
explains the growth on its own: clique fixing, reduced-cost fixing,
conflicts, the node cuts (`--cut-depth 0`: `rgn` 169 to 391 nodes,
`mod008` 1555 to 553), or best-bound node selection (`rgn` 135 to 353).

What this leaves for D293: probes whose gains enter the pseudocosts with
the selection bias removed (for example a probe count that makes a column
reliable while its gain stays out of the average), or cheaper probes (a
capped dual simplex that still publishes its bound).
