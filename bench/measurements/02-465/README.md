# 02-465 — how far J7's seven are from closing

Taken on 2026-11-16 on f6818bc, with strong branching on by default
(02-464), for TODO row J7. `read.sh` takes the readings one solve at a
time:
- `seven-5e10.txt`: the seven at 5e10 work units;
- `node-presolve-off.txt`: `pk1` and `mas76` at 5e10, with
  `MIP_NODE_PRESOLVE_ITERS` at 0, so every node after the first 50 skips
  the LP presolve;
- `profile-pk1.txt`, `profile-mas76.txt`: callgrind's instructions by
  function, at 1e9 work units.

## The seven at five times the budget

| instance | at 1e10 (02-464) | at 5e10 |
|---|---|---|
| `pk1` | dual gap 0.203 | optimal at 1.64e10, 337,705 nodes |
| `mas76` | 0.0057 | optimal at 1.95e10, 632,196 nodes |
| `gen-ip002` | 0.0043 | bound -4795.71 against -4783.73, 1.81M nodes |
| `gen-ip054` | 0.0060 | bound 6813.33 against 6840.97, 3.12M nodes |
| `neos5` | 0.067 | bound 14.375 against 15 |
| `assign1-5-8` | 0.092 | bound 194.69 against 212 |
| `binkar10_1` | 0.0035 | bound 6725.03 against 6742.20 |

`pk1` needs 1.64 times less work to close inside the benchmarks' budget,
and `mas76` 1.95 times. The other five stay open at five times the
budget, so their bound per node, not their node cost, is what is short.

## Where a node of `pk1` and `mas76` goes

The tree's own split at 1e10:

| instance | node relaxations | heuristics | strong branching | propagation | the rest |
|---|---|---|---|---|---|
| `pk1` | 87.5% | 12.2% | 0.1% | 0.0% | 0.1% |
| `mas76` | 80.5% | 16.1% | 0.1% | 1.7% | 1.6% |

Both find the optimum at node 1, so the heuristics' 12% to 16% buys
nothing on them.

A `pk1` node takes 12 dual iterations, about 4,000 work units each. A
`mas76` node takes 6, with node presolve kept: its trial nodes average
under `MIP_NODE_PRESOLVE_ITERS`.

By instructions, `mas76`'s nodes spend about 20% in presolve
(`ps_row_range` 12.5%, `jm_presolve_run` 4.0%, `ps_build_rowwise`,
`ps_bound_shift`) and about 15% in Curtis-Reid scaling of each reduced
model (`scale_curtis_reid` 10.7%, `log2` 4.2%). `pk1`'s nodes spend them
in the simplex: factorisation 9.3%, `price_all` 8.5%, and the solves.

In work units, presolve is the smaller share. With it off after the trial,
`mas76` closes at 1.84e10 in 564,475 nodes, 0.94x. `pk1`, which skips it
already, reads the same.

## A node's work units, stage by stage

`stages.py` patches a copy of HEAD's `src/simplex.c` to count, over the
node solves of one run at 1e9 work units, the units before the simplex
runs (presolve and setup), in the run, in the settling re-entry, and in
publishing:

| instance | node solves | before the run | run | settling | publishing | iterations a solve | refactorisations a solve | units an iteration |
|---|---|---|---|---|---|---|---|---|
| `pk1` | 17,547 | 2.0% | 94.9% | 0 | 1.8% | 14.2 | 1.7 | 3,578 |
| `mas76` | 33,280 | 16.9% | 81.6% | 0.03% | 1.4% | 5.7 | 1.2 | 3,507 |

On these dense rows an iteration costs about the matrix's nonzeros, the
dual simplex's floor. The bound-flipping ratio test is already in. 21 of
`pk1`'s node solves are perturbed and none of `mas76`'s. So the lever is
the count of iterations and nodes, not the cost of each.

## What the cheap levers reach

To optimality, at 5e10:

| setting | `pk1` | `mas76` |
|---|---|---|
| default (02-464) | 1.64e10, 337,705 nodes | 1.95e10, 632,196 nodes |
| `--rins 0` | 1.66e10, 376,007 | 1.72e10, 608,001 |
| `--rins 0 --no-heuristics` | 1.46e10, 341,255 | 1.90e10, 671,903 |
| node presolve off after the trial | 1.64e10, the same tree | 1.84e10, 564,475 |

Each moves the work by at most 12%, and the node counts move by up to 11%
with it.

`plunge.py` counts how often the next node is a child of the node just
solved: 1,210 of `pk1`'s 16,715 nodes (7%) and 4,791 of `mas76`'s 32,042
(15%). So solving a child on its parent's kept factors, as 02-461 does for
the probes, reaches few nodes under the current node selection.

## A child from its parent's whole state

`children.py` patches a copy of HEAD's `src/mip.c`. At every
strong-branching child, it solves the child twice:
- from the parent's kept state, an uncapped `jm_probe` with the parent's
  factors, pricing weights and reduced model;
- as the tree solves a node: a copy of the node's model, the child's
  bound, the parent's basis, and a solve with fresh factors and weights.

Over one run at 1e9 work units, per child:

| instance | children | from the kept state | as the tree does | work, kept over tree | iterations, kept over tree |
|---|---|---|---|---|---|
| `pk1` | 1,560 | 25.7 iterations, 116,663 units | 31.7, 150,253 | 0.78 | 0.81 |
| `mas76` | 4,893 | 6.1, 16,791 | 6.3, 21,518 | 0.78 | 0.97 |
| `gen-ip002` | 3,043 | 6.1, 37,362 | 7.1, 46,330 | 0.81 | 0.86 |
| `binkar10_1` | 674 | 17.8, 202,202 | 31.0, 392,233 | 0.52 | 0.58 |

The parent's whole state saves 19% to 22% of a child's work on `pk1`,
`mas76` and `gen-ip002`, and 48% on `binkar10_1`. On `pk1` and
`binkar10_1` much of it is iterations, the pricing weights. On `mas76` it
is presolve and the factorisation.

## The verdict

Nothing changes in the code. For `pk1` and `mas76` the target is 1.64x and
1.95x less work. None of the cheap levers reaches it, alone or together,
and neither does the parent's whole state: at 0.78x on every node with
heuristics off, `pk1` would take about 1.1e10. What remains:
- fewer iterations a node, for instance pricing weights carried from the
  parent to its children;
- a node order that keeps the parent's factors live, where the dive was
  refused (D289-dive);
- or fewer nodes.

For the other five it is the bound per node. TODO J7 holds both.
