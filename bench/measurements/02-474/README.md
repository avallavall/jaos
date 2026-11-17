# 02-474 — binkar10_1: MIR cuts at the nodes, refused

Taken on 2026-11-17 on 003a8f9, for TODO row J7's `binkar10_1` line.
`read.sh` takes every reading one solve at a time, from copies of 003a8f9
with `prototype.patch`, `c1.patch` and `node-mir.patch` (the change's two
forms) and `null.patch`. `landing.patch` is `node-mir.patch` with its
tests, docs and the tests it pins:
- `highs-binkar10_1.txt`, `scip-binkar10_1.txt`, `scip-root.txt`,
  `scip-cuts.txt`, `scip-tree.txt`: HiGHS 1.15.1 and SCIP 10.0 on
  `binkar10_1`, one thread;
- `jaos-root.txt`, `jaos-tree.txt`: JAOS's root and tree;
- `census.txt`: the node MIR's lift against the branching gain;
- `miplib3/*.txt`, `miplib2017/*.txt`, `sum.txt`: the arms on both sets.

`m3cmp.py` and `m17cmp.py` print the per-instance tables below.

## The model

170 binaries and 2,128 continuous columns. Each binary `y` sits in one of
ten knapsack rows, `sum w_j y_j <= 84.34` over 17 binaries with the same
weights in each row (10.18 to 28.81), and in one row `y = x_a + x_b` over
two flows. Its cost is its weight. The flows meet 556 conservation rows
and 116 demand rows `sum x = 1`, with costs up to 1e6.

## The root

The LP reads 6637.19 and the optimum is 6742.20.

| solver | root bound |
|---|---|
| JAOS | 6693.16 after 96 cuts (76 MIR, 18 covers), nine rounds |
| HiGHS | 6701.03, then 6701.22 after a restart on 2.9% inactive columns |
| SCIP | 6707.70 after its cut rounds, 6710.27 after root strong branching |

`scip-root.txt` takes SCIP's separators away one at a time:
- without the aggregation separator the root reads 6666.44, the same as
  Gomory alone;
- c-MIR alone on single rows, with no aggregation, reads 6711.25;
- flow covers alone read 6673.20 and knapsack covers alone 6681.93.

Without presolve and root strong branching, c-MIR on single rows reads
6709.41. All 81 cuts left in its LP lie on the binaries of one of the
knapsack rows (`scip-cuts.txt`). So the root's bound comes from MIR on the
ten knapsack rows.

JAOS cuts those rows too, but `mir_side` takes Marchand and Wolsey's
complementation only on rows with a continuous column (`02-357/`), and
never halves the divisor outside network mode. JAOS's other switches move
nothing: more MIR, Gomory, cover or flow cover rounds, and a longer
aggregation, read 6692.2 to 6693.2 (`jaos-root.txt`). Round 10 finds no
violated cut in any family. The aggregated MIR finds none either.

`prototype.patch` reads switches from `JAOS_MIRX`:

| bits | what changes at the root | root |
|---|---|---|
| 0 | nothing | 6693.16 |
| 1 | complementation on every row | 6693.79 |
| 2 | the best divisor halved up to three times | 6697.98 |
| 3 | both | 6703.39 |
| 11 | both, and MIR on the root's cut rows too | 6704.73 |
| 27 | all three, and no `MIP_MIR_MORE_STALL` stop | 6710.75 |

With bits 27, JAOS's root matches SCIP's.

## The tree

`scip-tree.txt`, SCIP to optimality:

| SCIP setting | nodes |
|---|---|
| default | 1,859 |
| no conflict analysis | 1,693 |
| pseudocost branching | 2,048 |
| no cut rounds below the root | 68,820 |
| none of those, no restart, no reduced-cost fixing | 85,293 |

SCIP closes `binkar10_1` with its node cuts. Without them it needs 37
times the nodes. HiGHS keeps a pool of about 3,800 cuts and 96 in its node
LPs (`highs-binkar10_1.txt`).

JAOS at 1e10 work units, `jaos-tree.txt`. JAOS's default cuts nodes only
to depth 3, and only with Gomory cuts.

| root | node cuts | `binkar10_1` |
|---|---|---|
| as today | as today | open, bound 6718.88, 35,620 nodes |
| bits 3 | as today | open, 6722.66 |
| bits 27, root 6710.75 | as today | open, 6730.80, 37,601 nodes |
| as today | Gomory and MIR to depth 10 | open, 6726.85 |
| bits 27 | Gomory and MIR to depth 10 | closes at 8.43e9, 23,328 nodes |
| as today | Gomory and MIR at every node | open, 6733.55, 9,690 nodes |
| bits 1 | the same, complementation at the nodes too | closes at 9.89e9, 8,796 nodes |
| bits 3 | the same, both steps at the nodes too | closes at 7.88e9, 6,916 nodes |
| bits 27 | the same | closes at 4.11e9, 4,044 nodes |
| as today | MIR at every node as today, Gomory to depth 3 | open, 6733.09 |
| as today | MIR at every node with both steps, Gomory to depth 3 | closes at 5.84e9, 5,502 nodes |
| bits 27 | the same | closes at 4.57e9, 4,893 nodes |

A stronger root alone leaves the tree open. The knapsack rows cut with
the nodes' own bounds close it, and only with the two c-MIR steps. HiGHS
needs 4,066 nodes and SCIP 1,859.

## MIPLIB 3

Against `bench/miplib.baseline`, work:

| arm | geometric mean | total | worst |
|---|---|---|---|
| `x27`: bits 27 at the root | 1.393x | 1.009x | `enigma` 154x, `gt2` 8.1x, `p0201` 2.5x |
| `x0nm`: Gomory and MIR at every node | 1.081x | 2.444x | `bell5` 44x, at the work limit |
| `x3nm`: the same with both steps | 1.083x | 1.766x | `bell5` 15x |
| `p123nm`: bits 27, MIR with both steps at every node | 1.356x | 1.250x | `enigma` 154x, `gt2` 12.4x |
| `p227nm`: MIR with both steps at every node | 0.892x | 1.144x | `blend2` 1.71x |

The root's steps fail on `enigma`, `gt2` and `p0201`. Gomory cuts at
every node fail on `bell5`, as in `02-460/`. MIR at every node with
Gomory to depth 3 passes, and closes `lseu` (0.20x), `flugpl` (0.11x)
and `mod008` (0.41x) sooner. It costs `bell3a` 1.45x and `blend2` 1.71x
for nothing: their node MIR finds a cut at under 1% of the nodes.

MIPLIB 2017 at 1e10 work units, 02-328's `m17sum.py`: `x27` reads 13.78
against the record's 10.85. `csched008` loses its point and
`markshare_4_0` its optimum. `p227nm` reads 11.05. `binkar10_1` closes,
but `pk1` loses 0.10 of dual gap and `glass4` 0.10, since a node of those
trees costs more for cuts that barely lift it.

## The census

`census.txt`: bits 227, MIR with both steps at every node, at 2e9 work
units. It reads the bound the cuts lift against the bound branching
gained at the same nodes, each node's bound over its parent's. It counts
the ratio of the two sums at 50, 100, 200 and 500 nodes and at the end:

| instance | 50 | 100 | 200 | 500 | end |
|---|---|---|---|---|---|
| `binkar10_1` | 1.03 | 1.12 | 1.17 | 1.38 | 1.28 |
| `lseu` | 0.75 | 0.88 | 1.00 | | 0.86 |
| `gen-ip054` | 0.80 | 0.38 | 0.61 | 0.71 | 0.71 |
| `p0282` | 0.56 | 0.63 | 0.38 | | 0.27 |
| `misc07` | 0.25 | 0.22 | 0.18 | 0.15 | 0.14 |
| `l152lav` | 0.22 | 0.13 | 0.07 | 0.04 | 0.04 |
| `misc03` | 0.14 | 0.13 | 0.12 | | 0.11 |
| `pk1` | 0.11 | 0.01 | 0.02 | 0.02 | 0.02 |
| `bell5` | 0.03 | 0.07 | 0.04 | 0.04 | 0.01 |
| `blend2` | 0.02 | 0.01 | 0.01 | 0.00 | 0.00 |

The models it helps read above a quarter, and the ones it costs below.

## The change

`node-mir.patch` (`c2`):
- `MIP_NODE_MIR` is on for a linear objective, and the node MIR runs at
  every depth, beside the Gomory round down to the cut depth.
- It cuts only the rows whose columns are all integer and whose
  coefficients or sides are not all ±1 and whole. A row of ±1 with whole
  sides gives no MIR cut. `pk1`, `mas74` and `glass4`, which the first
  form below moved most, have no such row. A model with none is solved as
  before.
- On those rows the MIR takes both c-MIR steps. The root keeps today's
  MIR.
- A node whose bound already reaches the incumbent gets no MIR round; the
  node is pruned after it.
- After `MIP_NODE_MIR_TRIAL` (50) nodes, the cuts stop for the rest of the
  tree once the bound they lifted is no more than `MIP_NODE_MIR_PAYS`
  (0.25) of what branching gained at those nodes. A node the cuts make
  infeasible counts its gap to the incumbent as lifted, or its branching
  gain when there is no incumbent.
- The summary log says how many rows the node MIR cuts, at how many
  nodes, what it lifted against what branching gained, and whether it is
  still on.

The census was taken before the rule, over every row and every node, so
its ratios are not the rule's to the digit.

The first form, `c1` (`c1.patch`), cut every row. MIPLIB 3 read 0.832x,
worst `blend2` 1.39x, and MIPLIB 2017 11.04: the trial's 50 nodes moved
`pk1`, `mas74`, `glass4` and `neos-3046615-murg`. With node MIR off, the
same build reproduces the record on `pk1`, `mas74` and `murg`
(`c1-node-mir-off.txt`). On `mas74` the trial found no cut, so the work
it charged moved the tree.

`binkar10_1` under `c2`: optimal at 5.22e9 work units in 6,280 nodes,
and `c1`: 4.82e9 in 5,322 nodes.

MIPLIB 3, `miplib3/c2.txt`: 0.802x geometric and 0.923x in total, worst
`misc03` 1.14x, no instance past 2x:

| instance | work | nodes |
|---|---|---|
| `flugpl` | 0.104x | 2,907 to 73 |
| `lseu` | 0.181x | 4,989 to 245 |
| `mod008` | 0.402x | 1,221 to 43 |
| `p0282` | 0.686x | 1,629 to 291 |
| `misc07` | 0.800x | 10,245 to 8,617 |
| `misc03` | 1.137x | 253 to 291 |
| `bell5` | 1.047x | 21,249 to 22,191 |

The nine models with no row the node MIR cuts read as the baseline.

MIPLIB 2017 at 1e10, 02-328's `m17sum.py` (`sum.txt`):

| arm | solved | primal | dual | sum |
|---|---|---|---|---|
| before (the record) | 5 | 4.4267 | 6.4227 | 10.8494 |
| `c1`, every row | 6 | 4.4844 | 6.5604 | 11.0448 |
| `c2`, the change | 6 | 4.4435 | 6.4500 | 10.8936 |
| `null` | 5 | 4.6099 | 6.5232 | 11.1331 |

Under `c2` 20 of the 30 read as the record, node for node: they have no
row the node MIR cuts. Of the ten that have one, two close at the root
as before, eight end at other nodes, and four move the sum:

| instance | before, primal and dual | `c2` |
|---|---|---|
| `binkar10_1` | 0.0007, 0.0035 | optimal, 6,280 nodes |
| `gen-ip054` | 0, 0.0060 | 0, 0.0048 |
| `neos-3046615-murg` | 0.0456, 0.7113 | 0.0631, 0.7119 |
| `graphdraw-domain` | 0.0999, 0.2990 | 0.0999, 0.3304 |

The census puts the node MIR's lift on `murg` at nothing, no cut in
25,661 nodes, and on `graphdraw-domain` at 0.02 of the branching gain, so
both stop after 50 nodes. What moves them is those 50 nodes: their cuts
on `graphdraw-domain`, and on `murg` only the work they charge.

## How much the 2017 sum moves on its own

`null.patch` adds to the root's work what 50 node MIR rounds would charge,
450 times the model's nonzeros, rows and columns, and changes nothing
else. The 2017 sum reads 11.1331, 0.28 above the record: `mas74`'s
incumbent goes from the optimum to 0.18 away, `tr12-30`'s to 0.10 away,
`glass4` loses 0.10 and `timtab1` gains 0.14. MIPLIB 3 reads 16 trees at
other nodes. The sum moves by more on a work-only change than `c1` or
`c2` move it, so it cannot judge a change that closes one instance and
leaves the rest to the trial's noise.

## The verdict

Refused: MIPLIB 2017's sum reads 10.8936 against 10.8494, the bar. The
change closes `binkar10_1` and reads 0.802x on MIPLIB 3 with no instance
past 1.14x. `bench/refusals.txt`'s `node-mir-every-depth` and
`mir-root-cmir-steps` carry what reopens them; TODO J7 names the next
step: the sum's spread over several null offsets, then `landing.patch`
read against it.

`make test`, `make sanitize` and `make python-test` passed with
`landing.patch` applied (`c2`'s code, its three tests in
`tests/test_mip.c`, and node MIR pinned off in the tests whose subject is
a tree without node cuts). The netlib sets were not run: no LP path
changed.
