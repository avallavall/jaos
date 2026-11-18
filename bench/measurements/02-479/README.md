# 02-479 — neos5: a node relaxation prices by Dantzig's rule in all but name

Taken on 2026-11-18 on b53a1ff, for TODO row J7's `neos5`. `read.sh`
takes every reading one solve at a time, from copies of b53a1ff with
`trace.patch` (one line per node relaxation under `N5TRACE`), `modes.patch`
on top of it (`N5DSE`, the node solves' dual pricing, at the root too),
`census.patch` on top of both (every node relaxation below the root solved
again under each pricing from the same start, outside the work count) and
`change.patch` (the change; `landing.patch` is it with its tests and docs):
- `trace-neos5.txt`: a node's iterations by whether it is its
  predecessor's child, its branch direction and how long ago its parent was
  solved, on b53a1ff and on the change, at 2e9 work units;
- `modes-neos5.txt`: `neos5` under each pricing at 2e9 and 1e10;
- `census.txt`: the paired census on MIPLIB 2017 and MIPLIB 3 at 5e8;
- `highs-neos5.txt`, `scip-neos5.txt`: HiGHS 1.15.1 and SCIP 10.0 on
  `neos5` to 30,000 nodes, one thread, under their pricing and node order
  options (`scip_lp.py`);
- `miplib3/*.txt`, `m3cmp.txt`, `miplib2017/*.txt`, `m17cmp.txt`,
  `sum.txt`: the arms on both sets, by 02-474's and 02-328's scripts;
- `lp-gates.txt`: the netlib, infeasible netlib and Kennington sets, and
  the primal, barrier, PDLP, concurrent and warm readings, under the change.

`modes.patch` at `N5DSE=0` reads as b53a1ff node for node: `neos5` at 1e10
ends at 32,557 nodes and 10,000,004,839 work units, the record's.

## Why a node takes three times HiGHS's iterations

`trace-neos5.txt`, b53a1ff, the 6,459 node relaxations after the presolve
trial at 2e9 work units. Every node starts from its parent's final basis,
and 6,410 of them start with one row infeasible, the branched column's:
the warm start is the right one. The order matters as a selection, not as a
state: a node solve keeps nothing from the solve before it but memory.

| nodes | count | iterations | weight restarts | work |
|---|---|---|---|---|
| all | 6,459 | 34.4 | 13.7 | 258,933 |
| child of the node solved before it | 2,462 | 10.8 | 2.9 | 86,781 |
| any other | 3,997 | 49.0 | 20.3 | 364,973 |
| other, down branch | 2,236 | 22.2 | 8.1 | 166,910 |
| other, up branch | 1,761 | 83.0 | 35.8 | 616,459 |
| parent 101 to 1000 nodes ago | 1,161 | 57.3 | 24.2 | 428,932 |
| parent more than 1000 nodes ago | 1,280 | 68.3 | 28.9 | 505,369 |

The estimate order takes the down child next (2,369 of 2,462); the up
child, `x_j = 1` on `neos5`'s covering rows, waits and takes 83
iterations. Bound flips are 2.6 a solve, refactorizations 1.9 (6% of the
work), and node presolve is off after the trial, whose 50 nodes took 72.1
iterations each.

What the iterations come from is the pricing. A node solve starts its dual
steepest-edge weights at 1. At each pivot the carried weight of the leaving
row is checked against the exact one, and when they differ by more than
`DSE_DRIFT` the whole set restarts at 1. On `neos5`'s dense basis the exact
weight of a leaving row is often more than ten times the carried 1, so the
set restarts at 13.7 of a solve's 34.4 pivots, and 1,497 solves restart ten
times or more: the dual prices by the largest infeasibility, and still pays
the steepest edge's second solve each pivot.

## The pricing, four ways

`modes-neos5.txt`; `N5DSE` 1 keeps updating a drifted set with the leaving
row's exact weight, 2 computes exact weights at the start, 4 prices by dual
Devex from the start:

| pricing at the nodes | nodes at 2e9 | iterations a solve | work a node | nodes at 1e10 | bound at 1e10 |
|---|---|---|---|---|---|
| b53a1ff | 6,510 | 34.4 | 258,933 | 32,557 | 14 |
| steepest edge, no restart | 12,254 | 19.4 | 140,700 | 83,369 | 14.1667 |
| steepest edge, exact at the start | 11,751 | 13.3 | 144,452 | 86,305 | 14.25 |
| dual Devex, at the root too | 17,295 | 18.8 | 99,546 | 143,223 | 14.25 |
| the change: dual Devex below the root | 15,511 | 18.4 | 111,040 | 116,376 | 14.25 |

Exact weights give HiGHS's count, 13.3 against 12.6, and cost 63 solves
with the inverse at each node. Devex needs no second solve a pivot, so an
iteration costs 5,300 units against the steepest edge's 7,300. The change
differs from `N5DSE=4` at the root, which keeps steepest edge: its first 50
nodes average 22.7 iterations, past `MIP_NODE_PRESOLVE_ITERS`, so its nodes
skip presolve, where `N5DSE=4`'s keep it. With node presolve kept
(`MIP_NODE_PRESOLVE_ITERS` at 1e9) the change reaches 150,788 nodes and
bound 14.333 at 1e10.

Under the change, `trace-neos5.txt`, the child of the node before it takes
8.5 iterations and any other node 22.8; the up children solved away from
their parent take 34.0 iterations, 189,144 units each, 63% of the node
work.

## HiGHS and SCIP

HiGHS 1.15.1 to 30,000 nodes (`highs-neos5.txt`): 431,708 LP iterations,
379,165 of them in node LPs, 12.6 a node. It has no option for its node
order, and its MIP does not read `simplex_dual_edge_weight_strategy`: under
0 (Dantzig), 1 (Devex), 2 (steepest edge) and the default, the count is
431,708 every time. So HiGHS cannot be read under other orders or pricing.

SCIP 10.0 to 30,000 nodes (`scip-neos5.txt`), its dual LP's iterations:

| SCIP setting | iterations | a node | a call with any | bound |
|---|---|---|---|---|
| default (`lp/pricing` `d`, Devex, reads the same) | 598,211 | 19.9 | 15.5 | 14.0 |
| steepest edge | 528,691 | 17.6 | 13.5 | 14.08 |
| steepest edge from weights of 1 (`q`) | 719,680 | 24.0 | 17.8 | 14.0 |
| partial pricing (`p`) | 1,413,202 | 47.1 | 35.8 | 14.0 |
| depth first | 187,482 | 6.2 | 6.0 | 13.50 |
| best bound, no plunging | 706,260 | 23.5 | 16.1 | 14.26 |
| best estimate, no plunging | 646,463 | 21.5 | 16.1 | 14.15 |

SCIP's node LPs price by Devex. Under partial pricing they take 35.8
iterations a call, b53a1ff's 34.4; under steepest edge 13.5, JAOS's 13.3
with exact weights; from weights of 1, 17.8. A depth-first SCIP takes 6
iterations a node and ends with the worst bound; without plunging it takes
16, about what its default takes.

## The census

`census.txt`: every node relaxation below the root of both sets, at 5e8
work units, solved from the same start under each pricing before the
tree's own solve, so the trees are b53a1ff's. Work over the tree's own
solve:

| pricing | geometric mean over 42 instances | above 1 |
|---|---|---|
| steepest edge, no restart | 0.893 | 15 |
| steepest edge, exact at the start | 1.279 | 35 |
| dual Devex | 0.855 | 12 |

Devex saves the most where a node takes many iterations: `crna` 0.252,
`neos5` 0.351, `mad` 0.567, `l152lav` 0.673, `misc07` 0.683, `tr12-30`
0.720, `assign1-5-8` 0.722, `misc03` 0.756, `timtab1` 0.804, `pk1` 0.827.
Where a node takes 2 to 6 iterations it costs up to 4% (`bell3a` 1.025,
`mas76` 1.018, `markshare2` 1.020), the duals' refinement a Devex solve
pays at its end; `gt2` reads 1.267 over its 5 nodes.

## The change

Below the tree's root, the dual simplex of a node relaxation prices by dual
Devex from its first iteration (Forrest and Goldfarb, 1992; the reference
framework is the start basis, so every weight starts at 1 and is exact for
it). The tree sets `node_devex` on its relaxation from node 2 on, so the
root's solve, its cut rounds and its probes keep steepest edge; a model
with a quadratic objective keeps its own path. The copies a batched round
solves (`mip_tree_batch` past 1, off by default) set it too; that line came
after the readings, all at the default batch of 1, which it leaves alone. Everything else of the dual
is as before, Devex's reset (`DUAL_DEVEX_RESET`) and the refinement of the
published duals included. No constant is new.

The first form, Devex at the root too (`N5DSE=4`), moves the roots of
`enigma` and `mod010`, which close there: 18.8x and 24.0x on MIPLIB 3.

## MIPLIB 3

Against `bench/miplib.baseline` (`m3cmp.txt`):

| arm | geometric mean | total | worst |
|---|---|---|---|
| Devex at the root too (`devex.txt`) | 1.256x | 0.932x | `mod010` 24.0x, `enigma` 18.8x |
| the change (`land.txt`) | 1.019x | 0.955x | `blend2` 1.746x |
| the change, node presolve kept (`landpre.txt`) | 1.097x | 1.225x | `dcmulti` 1.847x |

All 24 solve in every arm with the objective, the checker and the second
solve as the gate wants. Under the change:

| instance | work | nodes |
|---|---|---|
| `blend2` | 1.746x | 3,441 to 5,053 |
| `bell5` | 1.639x | 16,919 to 27,699 |
| `lseu` | 1.105x | 229 to 259 |
| `rgn` | 1.082x | 139 to 193 |
| `bell3a` | 1.060x | 26,497 to 26,393 |
| `p0201` | 1.051x | 53 to 80 |
| `l152lav` | 0.925x | 473 to 829 |
| `p0282` | 0.922x | 269 to 214 |
| `misc03` | 0.921x | 209 to 307 |
| `misc07` | 0.926x | 8,228 to 9,199 |
| `stein27` | 0.893x | 1,231 to 1,137 |
| `stein45` | 0.852x | 55,644 to 54,578 |
| `dcmulti` | 0.830x | 370 to 263 |

The census puts `blend2`'s and `bell5`'s node work at 0.823x and 0.994x of
b53a1ff's; what grows is their trees, taken at other vertices. `stein27`,
whose 2.34x refused the Devex fallback in the tree in 02-278
(`mip-node-devex`), reads 0.893x.

## MIPLIB 2017

At 1e10 (`m17cmp.txt`, `sum.txt`; `nulls.txt` reads each instance against
02-474's five null readings, taken on older code):

| reading | solved | primal | dual | sum |
|---|---|---|---|---|
| the record, b53a1ff | 9 | 5.4067 | 6.3032 | 11.7099 |
| the change | 9 | 5.5085 | 6.1379 | 11.6464 |

The same nine close; of the four that must, `pk1` at 8.95e9 work units in
220,268 nodes (7.49e9, 174,316), `mas76` at 6.87e9 (7.64e9), `binkar10_1`
at 5.16e9 (4.51e9), `gen-ip054` at 2.17e9 (2.05e9). The dual side gains
0.165, below the record and every null reading (6.42 to 6.55):

| instance | bound, the record | the change | nodes |
|---|---|---|---|
| `graphdraw-domain` | 13,228.6 | 14,836.4 | 30,497 to 58,952 |
| `neos-911970` | 51.643 | 52.879 | 1,022 to 3,040 |
| `neos5` | 14 | 14.25 | 32,557 to 116,376 |
| `timtab1` | 543,299 | 557,458 | 210,540 to 257,892 |
| `assign1-5-8` | 196 | 198 | 50,359 to 61,150 |
| `neos-3046615-murg` | 467 | 480 | 230,267 to 239,632 |
| `supportcase26` | 1,465.8 | 1,476.1 | 69,732 to 81,099 |
| `csched007` | 304 | 305 | 3,800 to 6,287 |
| `beasleyC3` | 742 | 743 | 4,553 to 8,682 |
| `mas74` | 11,287.6 | 11,277.4 | 358,947 to 321,147 |

`glass4`, `tr12-30` and `neos-3627168-kasai` lose under 0.0012 of dual
gap. The primal side loses 0.10, on incumbents that move with the trees'
paths:

| instance | incumbent, the record | the change | the nulls' range |
|---|---|---|---|
| `glass4` | 1.500e9 | 1.625e9 | 1.500e9 to 1.700e9 |
| `beasleyC3` | 784 | 824 | 780 to 823 |
| `neos-3046615-murg` | 1,632 | 1,705 | 1,668 to 1,722 |
| `supportcase26` | 1,776.8 | 1,802.2 | 1,755.5 to 1,789.9 |
| `mas74` | 11,801.2, its optimum | 11,891.3 | 11,801.2 to 14,689.5 |
| `graphdraw-domain` | 21,653 | 21,742 | 21,381 to 21,653 |
| `neos-3754480-nidda` | 12,997.3 | 13,013.1 | 12,941.7 to 12,997.3 |
| `neos-911970` | 54.83 | 54.96 | 54.96 |
| `timtab1` | 1,291,624 | 1,216,454 | 1,154,825 to 1,200,238 |
| `tr12-30` | 132,030 | 130,638 | 130,599 to 144,007 |
| `csched007` | 405 | 398 | 405 |
| `gen-ip002` | 0.0025 from its optimum | its optimum | its optimum |

Four end just outside the nulls' range: `supportcase26` 0.7% over their
worst (02-475's first reading of its change ended at the same 1,802.2),
`graphdraw-domain` 0.4%, `nidda` 0.1% and `beasleyC3` by one unit.
`timtab1`'s incumbent improves on the record's, and both sit above the
nulls' worst. The trees are larger nearly everywhere, since a node costs
less: `neos-2657525-crna` 2,680 to 8,239 nodes, `mad` 16,100 to 39,822,
`tr12-30` 30,682 to 53,850.

## The LP sets

`lp-gates.txt`: the netlib, infeasible netlib and Kennington sets read as
their baselines, 0 regressed and 0 at other nodes, and the primal, barrier,
PDLP, concurrent and warm readings read as their records line for line: no
LP solve sets `node_devex`.

## The verdict

Landed, as `landing.patch`. A JAOS node relaxation took three times
HiGHS's iterations because it priced by the largest infeasibility: its
steepest-edge weights started at 1 and the drift test restarted them at
nearly every second pivot. The warm start, the bound flips and the
refactorizations were not it, and the order picks which nodes come next
but carries no state. Dual Devex below the root takes `neos5` from 32,557
nodes to 116,376 at 1e10 and its bound from 14 to 14.25 (dual gap 0.067 to
0.050). It does not close: a node costs 8.6e4 work units, and closing in
HiGHS's 206,031 nodes needs 4.9e4. Its up children solved away from their
parent hold 63% of the node work.

MIPLIB 2017 solves the same nine, its dual side 6.30 to 6.14 with ten
bounds raised, its sum 11.71 to 11.65; four incumbents end just outside
02-474's nulls. MIPLIB 3 reads 0.955x in total and 1.019x geometric, worst
`blend2` 1.746x. Refused: Devex at the root too (`enigma` 18.8x, `mod010`
24.0x) and node presolve kept on every node, `node-presolve-always`
(MIPLIB 3 1.225x). `mip-node-devex`'s refusal expires.

`make test` and `make sanitize` pass on the change. The LP sets were read
with the change's `bench/run`, `bench/primal`, `bench/barrier` and
`bench/warm` against their records; MIPLIB 3 with `bench/run` against
`bench/miplib.baseline` at `-j 1`, not through `make miplib`.
