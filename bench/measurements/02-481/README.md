# 02-481 — neos5: the tree keeps it open, not the node

Taken on 2026-11-18 on 1235543, for TODO row J7's `neos5`. `read.sh`
takes every reading from copies of 1235543 that `build.sh` makes:
`head` (the record), `pre` (node presolve on every node,
`MIP_NODE_PRESOLVE_ITERS` out of reach, 02-479's `landpre`), `skip`
(`MIP_NODE_PRESOLVE_ITERS` 0: node presolve dropped after the trial
whatever it reads), `trace`
(`trace.patch`: one `N` and one `F` line per node relaxation under
`N5TRACE`, an `X` line every 10,000 nodes with the open nodes and the
explored share; `N5PRE` keeps node presolve on) and `prof`
(`profile.patch`: the node relaxations' work units by source line under
`N5PROF`). Two solves run at a time.
- `jaos.txt`, `jaos/*`: every JAOS solve, one line each, and its output
  and summary log; `jaos/*-5e11.log` keep the progress line of every
  10,000th node;
- `trace-neos5.txt` (`split.py`), `profile-neos5.txt` (`profile_sum.py`):
  the node relaxations at 2e9 work units;
- `expl/*.x`, `explored.txt` (`explored.py`): the open nodes and the
  explored share, 1 minus the sum of 2^-depth over the open nodes, every
  10,000 nodes at 1e10;
- `root.txt`: the root under more cut rounds and families;
- `highs/*.txt`, `highs.txt` (`highs_sum.sh`): HiGHS 1.15.1 on `neos5`,
  one thread, gap 1e-6, under four seeds and its options;
- `scip-root.txt` (`scip_root.py`): SCIP 10.0's root; `scip-params.txt`
  (`scip_params.py`) its conflict defaults, bound-exceeding LPs analysed.

## The premise was wrong

02-477 and 02-479 took HiGHS's 206,031 nodes as the size of the tree to
close and asked for 4.9e4 work units a node. JAOS's own tree is 14 times
that:

| copy | work limit | nodes | work units | bound | status |
|---|---|---|---|---|---|
| the record | 1e10 | 116,376 | 1.00e10 | 14.25 | open |
| node presolve on every node | 1e10 | 150,788 | 1.00e10 | 14.333 | open |
| the record | 1e11 | 1,526,036 | 1.00e11 | 14.75 | open |
| node presolve on every node | 1e11 | 2,232,772 | 1.00e11 | 14.75 | open |
| the record | 5e11 | 2,977,069 | 1.913e11 | 15 | optimal |
| node presolve on every node | 5e11 | 2,293,837 | 1.024e11 | 15 | optimal |
| no cuts below the root (`--cut-depth 0`) | 5e11 | 1,496,753 | 6.896e10 | 15 | optimal |
| the same, node presolve dropped after the trial | 5e11 | 1,741,169 | 1.130e11 | 15 | optimal |
| no cuts below the root, node presolve on every node, `--rins 0` | 5e11 | 1,456,423 | 6.063e10 | 15 | optimal |

Without cuts below the root the trial's 50 nodes average under 20
iterations (19.8 in `skip-depth0-5e11.log`), so `--cut-depth 0` keeps
node presolve on every node and `skip` (`MIP_NODE_PRESOLVE_ITERS` 0)
drops it to read the cuts alone. The four corners:

| | node presolve dropped after the trial | on every node |
|---|---|---|
| Gomory cuts to depth 3 | 2,977,069 nodes, 6.4e4 a node (the record) | 2,293,837, 4.5e4 |
| no cuts below the root | 1,741,169, 6.5e4 | 1,496,753, 4.6e4 |

The local cuts make the tree 1.5x to 1.7x larger; node presolve on every
node makes it 1.2x to 1.3x smaller and a node 0.7x as dear. The record
closes `neos5` at 1.9e11. Inside 1e10, its tree would need 3.4e3 work
units a node, and the smallest tree here, 1.46M nodes, 6.9e3. A node
relaxation spends 2.1e3 before its
run, 4.4e3 to 7.5e3 on each factorization (1.2 to 2.1 a solve) and 5.1e3
to 5.9e3 on each dual iteration (`trace-neos5.txt`), and every node
pays 2.1e3 more for the rounding of its point: a node of one pivot costs
1.4e4. No node cost reaches either figure, so the node relaxations
cannot close `neos5` inside 1e10; their cost is refused as the way to it
(`neos5-node-cost`).

## The node relaxations

What the TODO named, read anyway. `trace-neos5.txt`, the 15,460 node
relaxations after the presolve trial at 2e9 (the record). The work sits
in the up branches of columns that the parent holds low: a branch whose
column moves 0.6 or more to its new bound.

| nodes | count | iterations | work a node | share of the node work |
|---|---|---|---|---|
| all | 15,460 | 18.4 | 111,040 | 1 |
| down, step under 0.4 | 8,379 | 6.8 to 9.4 | 57,012 to 62,998 | 0.294 |
| up, step 0.6 to 0.8 | 2,351 | 31.8 | 170,103 | 0.233 |
| up, step 0.8 and more | 2,361 | 45.5 | 254,584 | 0.350 |

Pseudocost branching takes columns the parent holds at 0.4 or less (8,379
of the 9,597 down branches), whose down child is near and whose up child
(`x_j = 1` on 32 covering rows) is far. Of the 4,712 far up branches,
4,551 end optimal under 15 and 18 stop at the cutoff: their iterations
are the distance between two optima, and no early stop saves them. A child solved right after its parent costs
less because it is mostly a down child (4,587 of 4,716), not because of
what it keeps.

Node presolve on every node (`N5PRE`) drops 20 fixed columns and 5 rows
of 63, keeps the iterations (18.8 against 18.4) and makes a node 0.875x as
dear, 97,123 work units; the far up branches go from 212,434 to 171,881.
`profile-neos5.txt`: 71% of a node relaxation's work is the LU (BTRAN
24%, FTRAN 28%, the factorization 9.6%, its updates 9.1%), 9.8% the pivot
row and 3.8% the entering column's prices; the basis is dense, 63 rows
and every column in 32 of them. The tree's heuristics take 12.5% of the
work at 1e10, most of it the sub-MIPs under `MIP_SUBMIP_SHARE`; one of
them finds the optimum at node 5,801, and after it they can find nothing
better: `--rins 0` reads 133,528 nodes at 1e10 and `--no-heuristics`
134,618.

## The tree, against HiGHS

`highs.txt`. HiGHS closes `neos5` in 110,582 to 206,031 nodes over four
seeds (1.86x between them). Its options take its parts away one at a
time:

| HiGHS setting | nodes | against 206,031 |
|---|---|---|
| default, seeds 0 to 3 | 206,031, 127,945, 179,048, 110,582 | 0.54x to 1 |
| no restart | 239,035 | 1.16x |
| reliability 0 | 229,473 | 1.11x |
| no heuristics | 247,767 | 1.20x |
| pools off (`mip_pool_soft_limit` 1, `mip_pool_age_limit` 0) | 315,809 | 1.53x |
| no symmetry | 352,311 | 1.71x |
| no cut separation at the nodes | 352,356 | 1.71x |
| no symmetry, node cuts, restart or heuristics | 677,327 | 3.29x |
| the same, and the pools off | 1,612,417 | 7.83x |
| the same, and reliability 0 | 1,506,798 | 7.31x |

Each part alone sits inside the seeds' spread. Either pool option alone
empties the conflict pool as well as the cut pool (`probe-soft1.txt`,
`probe-age0.txt`, 60 s each: 0 to 10 conflicts against 5,000 to 7,600 in
`alloff.txt`), so the two cannot be read apart; with node separation off
the cut pool holds only the root's cuts, about 400, against thousands of
conflicts. The four parts off together cost 3.3x, and the pools on top of
them 2.4x more, the largest single part. Bare, HiGHS's tree is JAOS's
size. SCIP analyses bound-exceeding LPs by default (`scip-params.txt`)
and holds 14.75 after 2.48M nodes (02-477), so a conflict's form, not
only its presence, decides what it is worth here. The restarts lift HiGHS's root from 13.28-13.35 to as much
as 13.75, but the run without them takes 1.16x the nodes and seed 3,
which never restarts, the fewest.

JAOS's root is 13.23 after one Gomory round, 13.47 after ten and no
higher; zero-half reaches 13.26, MIR and covers add nothing (`root.txt`).
SCIP's is 13.50. The root is not what divides the trees: HiGHS's seed 1
closes in 127,945 nodes from 13.48.

The trees differ in how soon they end their nodes (`explored.txt`). At
110,000 nodes JAOS holds 78,742 open nodes and has explored 1.2% of its
tree; HiGHS at 117,764 holds 2,687 and reports 76.7%. Node order does not
make the difference: with the optimum as incumbent from node 5,801 (HiGHS
has it at its root), the set of nodes to solve is nearly the same under
any order, and best-bound order (`--node-select bound`) reads 3.7% at
110,000.

JAOS's own switches at 1e10 (`jaos/arm-*.out`):

| JAOS setting | nodes at 1e10 | bound | explored at 100,000 |
|---|---|---|---|
| the record | 116,376 | 14.25 | 0.0114 |
| `--node-select bound` | 118,538 | 14.5 | 0.0271 at 90,000 |
| `--cut-rounds 5` (root 13.447) | 115,887 | 14.25 | |
| `--cut-rounds 10` (root 13.472) | 118,483 | 14.25 | 0.0103 |
| `--cut-depth 0` | 157,873 | 14.375 | 0.0141 |
| `--cut-depth 10` | 101,420 | 14.25 | |
| `--cut-depth 60` | 29,237 | 14.158 | 0.0406 at 20,000 |
| `--cut-depth 60 --node-cut-cap 0` | 22,659 | 14.135 | 0.0691 at 20,000 |
| `--reliability 0` | 115,622 | 14.25 | |
| `--reliability 8` | 119,540 | 14.25 | |
| `--branching most-fractional` | 107,757 | 14.2 | 0.0213 |
| `--no-orbital` | 103,032 | 14.042 | 0.0001 |
| `--restart` (never fires) | 116,376 | 14.25 | |
| `--rins 0` | 133,528 | 14.275 | |
| `--no-heuristics` | 134,618 | 14.25 | |

Orbital branching is what JAOS's tree leans on most: without it the
explored share stays under 0.0001. It acts near the root: 186 of the
record's 2,977,069 branchings widen to an orbit, and no column is fixed by
orbits. The local Gomory cuts to depth 3 make the closing tree 1.5x to
1.7x larger (the four corners above); at every node they end nodes sooner
(0.069 explored at 20,000 nodes) but a node costs four to five times as
much.

## The verdict

`neos5` does not close inside 1e10, and the next step the TODO named
cannot close it: its tree, 2.98M nodes as the record takes it and 1.46M
at its smallest here, needs under 6.9e3 work units a node, under the
1.4e4 a node of one pivot costs. Refused as `neos5-node-cost`. Nothing
was built; the patches are readings.

The gap to HiGHS's 110,582 to 206,031 nodes is the tree's. Bare, with
its symmetry, node cuts, restarts, heuristics and pools off, HiGHS takes
1,612,417 nodes, JAOS's size. Its pools, conflicts above all, are the
largest single part, 2.4x on the bare tree; symmetry, cuts separated at
the nodes, heuristics and restarts each cost it 1.1x to 1.7x, inside its
seeds' spread, and 3.3x together. No part but the conflicts accounts for
most of the gap, so nothing was prototyped here. In JAOS the conflicts
from nodes whose relaxation reached the incumbent are J7's next step for
the other instances, and they come first here too, read against SCIP,
which has them and a tree of JAOS's size; after them, symmetry handling
that reaches below the shallow nodes where orbital branching now acts,
and cuts at the nodes that hold for the whole tree, since JAOS's local
Gomory cuts to depth 3 make `neos5`'s tree 1.5x to 1.7x larger.

`make test`, `make sanitize` and the solver sets were not run: no source
changed.
