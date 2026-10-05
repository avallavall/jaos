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

## Probes that count toward reliability

`sbcount.py` (`JAOS_SBCOUNT`) keeps the gains out of the pseudocosts as
above, and counts each probe toward its column's reliability in a separate
counter, so the probing stops. A reliable column with no tree observation
is scored by the tree's average pseudocost. At a work limit of 3e9 a
model (`sbcount.txt`, work and nodes):

| model | reliability 0 | counted, 1 | counted, 2 | counted, 4 |
|---|---|---|---|---|
| `rgn` | 7.84e6, 127 | 13.2e6, 73 | 14.6e6, 71 | 16.4e6, 87 |
| `misc03` | 46.4e6, 240 | 81.9e6, 211 | 108e6, 263 | 160e6, 171 |
| `mod008` | 26.8e6, 683 | 52.6e6, 1439 | 59.5e6, 1343 | 68.9e6, 1441 |
| `enigma` | 35.8e6, 1983 | 128e6, 5925 | 87.2e6, 3008 | 81.6e6, 2390 |
| `gt2` | 22.8e6, 669 | 3.40e6, 5 | 3.40e6, 5 | 3.40e6, 5 |
| `p0201` | 71.0e6, 362 | 115e6, 159 | 111e6, 53 | 110e6, 46 |
| `blend2` | 700e6, 6869 | 614e6, 6010 | 612e6, 5383 | 660e6, 5504 |
| `bell5` | 296e6, 20089 | 302e6, 20375 | 390e6, 26443 | 3e9 (limit), 204196 |

The counted form gains on `gt2`, `p0201` and `blend2` and loses on
`mod008`, `enigma` and `bell5`: a column made reliable by probes alone
falls back on the average pseudocost, which misleads the choice as the
probes' own averages did.

What this leaves for D293: a pseudocost for a probed column that the
tree's observations can be compared with (a probe gain corrected for the
selection it misses), or cheaper probes (a capped dual simplex that still
publishes its bound).
