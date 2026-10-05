# 02-352 — node presolve off where it breaks the warm start

Taken on 2026-10-05 on the tree of 048124c, for TODO rows J7 and J11 and
the refusal `warm-node-no-presolve` (02-300). MIPLIB 3 at J=2 and 4 GB a
solve, the 2017 set at 1e10 work units a model. The patches read switches
from the environment and apply to the tree they name in their header.

## Skipping presolve at every warm node, read again

`nodenopre.py` (`JAOS_NODENOPRE`, arm `npre`): a node LP that starts from
its parent's basis skips presolve. MIPLIB 3 reads 0.861x in the geometric
mean and 0.450x in the sum over the 23 that finish (`l152lav` 0.066x,
`dcmulti` 0.245x, `khb05250` 0.561x; `mod008` 1.759x, `bell3a` 1.338x),
and `bell5` runs out of memory as in 02-300. `enigma`'s 50x of 02-300 is
gone (0.846x). The 2017 gap sum reads 15.87 against 14.61, most of it
`markshare_4_0`'s incumbent (1 to 4).

On `bell5` the tree with node presolve finds the optimum by RINS at node
2711; without it the incumbent stays at 12419324 (the feasibility jump's),
nothing is pruned and the open set grows. The node LPs reach other optimal
vertices, and the heuristics others points.

## Where the work goes

`preshare.py` counts the presolve's own work in node solves: it is at
most 0.081 of node LP work on every MIPLIB 3 model but `misc06` (0.114)
and `mod008` (0.180), and 0.004 on `l152lav`. The cost is elsewhere. Simplex
iterations per node, with node presolve and without:

| model | with | without |
|---|---|---|
| `l152lav` | 463 | 39 |
| `dcmulti` | 112 | 19 |
| `khb05250` | 27 | 13 |
| `misc03` | 32 | 26 |
| `mod008` | 7 | 6.5 |
| `bell3a` | 5.3 | 3.2 |
| `lseu` | 4.7 | 4.1 |

Where presolve removes rows and fixes columns that the parent's basis
holds basic, the reduced model starts from a basis that is short or far
from optimal, and the node solve runs many iterations. `nodeshort.py`
(`JAOS_NODESHORT`) drops the presolve only when the reduced basis is short
by more than `WARM_REPAIR_MAX_SHORT`: `l152lav` 0.769x, `khb05250` 0.954x,
`dcmulti` 0.995x, the others unchanged. Most of the loss happens with
bases that are not short.

## The rule

`nodeadapt.py` (`JAOS_NPA`, arm `npa20`): the tree counts the simplex
iterations of its first 50 node solves below the root (with presolve, as
before); when they average more than 20, the node solves after them that
start from a basis skip presolve. A model whose nodes average less keeps
its tree to the bit.

MIPLIB 3 reads 0.903x in the geometric mean and 0.526x in the sum, all 24
solved, none past 2x: `l152lav` 0.136x, `dcmulti` 0.494x, `khb05250`
0.849x, `p0201` 0.855x, `stein45` 0.944x, against `misc07` 1.501x,
`stein27` 1.143x and `misc03` 1.082x. The 2017 set holds 4 solved and 25
incumbents, and its gap sum goes from 14.61 to 14.53 (`binkar10_1`'s
bound 6712.8 to 6718.4, `csched007`'s 293.8 to 299.8; `beasleyC3` 782 to
804).

The 50 and the 20 were set once from the iteration counts above and not
swept.
