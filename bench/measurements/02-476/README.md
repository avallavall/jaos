# 02-476 — gen-ip002 and gen-ip054: branching on a reduced basis of the improving region

Taken on 2026-11-17 on 4dc8e97, for TODO row J7's `gen-ip002` and
`gen-ip054` line. `read.sh` takes every reading one solve at a time, from
copies of 4dc8e97, of 4dc8e97 with `proto.patch` (node switches read from
the environment) and `hook.patch` (an earlier form of the change), and of
the change itself:
- `highs-*.txt`, `scip-*.txt`: HiGHS 1.15.1 and SCIP 10.0 at 300 s, one
  thread; `scip-abl.txt`: SCIP at 200,000 nodes with parts of its tree off;
- `jaos-root-*.txt`, `jaos-arms-2e9.txt`, `jaos-cutcap-2e9.txt`,
  `jaos-arms-1e10.txt`, `jaos-sb-*.txt`, `rcnode-2e9.txt`,
  `jaos-1e11-gen-ip054.txt`: JAOS's root, its switches and a tree at 1e11;
- `lattice.py`, `lattice-widths.txt`, `lattice-trees.txt`,
  `lattice-cutoff.txt`: the lattice prototype from outside the tool;
- `jaos-lattice.txt`, `jaos-lattice-switches*.txt`, `lattice-sweep.txt`,
  `sweep.c`, `miplib3/`, `miplib2017/`, `sum.txt`: the change, and as
  `*-1000.txt` its first commit, 2cb06169, which ran 1000 nodes first.

`read.sh` uses `git archive`, so it runs in the main checkout, where the
WSL side reads the repository.

## The models

Both are pure general-integer models with dense rows and no upper bounds.
`gen-ip054` has 27 rows of `<=` over 30 columns, 66% of the entries
nonzero, most of them negative, so the rows cover; it minimises positive
costs. `gen-ip002` has 24 rows over 41 columns, 94% nonzero, a fifth of the
entries negative, and maximises. The optima are 6840.9656 and -4783.7334.

## HiGHS and SCIP

| | `gen-ip054` | | `gen-ip002` | |
|---|---|---|---|---|
| | nodes | bound | nodes | bound |
| HiGHS, 300 s | 1,679,986 | 6803.83 | 1,136,298 | -4817.34, incumbent -4769.20 |
| SCIP, 300 s | 2,172,504 | 6800.10, incumbent 6848.92 | 2,265,259 | -4801.29 |
| JAOS, 1e10 (the record) | 220,574 | 6808.06 | 331,854 | -4804.34 |
| JAOS, 1e11 | 2,606,036 | 6832.64, 612,690 open | | |

Neither closes either model in 300 s. Per node JAOS's bound leads: SCIP
reads 6785.24 and -4815.22 at 200,000 nodes, JAOS 6807.28 on `gen-ip054` at
200,000 and -4812.24 on `gen-ip002` at 60,102. The roots are alike: JAOS
6766.94 and -4839.32, HiGHS 6768.38 and -4836.97, SCIP 6768.67 and
-4837.28, from LPs of 6765.21 and -4840.54. J7 read JAOS's 1.5 to 9 times
the nodes of HiGHS and SCIP in 20 s as bound missing per node; on these
two it is JAOS's nodes being cheaper.

`scip-abl.txt`: at 200,000 nodes, SCIP with its reduced-cost propagator,
its objective propagator, linear propagation, node cuts, conflicts, strong
branching or restarts off reads within 0.9 of its default on both. With
all of them off but strong branching (`bare`) it reads 6785.11 against
6785.24 and -4815.38 against -4815.22. SCIP's tree on these is the LP and
the branching.

At 1e11 work units JAOS's tree on `gen-ip054` is still open, with 612,690
open nodes, so closing it inside 1e10 takes a tree ten times smaller or
more, and nothing in HiGHS's or SCIP's trees gives that.

## JAOS's switches

`jaos-arms-2e9.txt`, `jaos-cutcap-2e9.txt`, `jaos-arms-1e10.txt`. At 1e10:

| arm | `gen-ip054` bound | `gen-ip002` bound |
|---|---|---|
| default | 6808.06 | -4804.34 |
| Gomory cuts at every node | 6808.13 | -4805.76 |
| node cut cap 2 | 6811.24 | -4804.33 |
| best bound first | 6809.65 | -4801.21 |
| reduced-cost tightening at every node | 6808.51 | -4804.28 |

The last is `proto.patch`'s `JAOS_RCNODE`: each node pulls in the far
bound of every column at a bound for its subtree, as far as its reduced
cost and the incumbent allow. It tightens 224,682 and 487,424 bounds in
2e9 work units and moves the bound by under 0.5 (`rcnode-2e9.txt`).

Strong branching, `jaos-sb-*.txt`. Reliability 4 is reached after 154
probes on `gen-ip054`, 0.2% of the work. `proto.patch`'s `JAOS_PSHARE`
lifts the tenth of the work probing may take, and `JAOS_SBSEL` picks the
branch from the node's own probes instead of the pseudocosts they feed.
Under best bound with no node MIR, at 20,000 nodes:

| arm | `gen-ip054` | work | `gen-ip002` | work |
|---|---|---|---|---|
| pseudocosts | 6787.24 | 4.3e8 | -4813.92 | 7.5e8 |
| every node probed, pseudocosts pick | 6787.86 | 5.2e9 | -4812.49 | 9.0e9 |
| every node probed, the probes pick | 6788.38 | 5.1e9 | -4811.82 | 8.9e9 |

Twelve times the work for 1 to 2 of bound.

## The lattice

`lattice.py` takes the region the optimum leaves to improve on, the LP
relaxation with the objective at most the optimum's, rounds it by the
Dikin ellipsoid at its analytic center, and reduces the integer lattice
under that ellipsoid by LLL. Its widths (`lattice-widths.txt`): along the
unit directions the region is 4.38 to 11.50 wide on `gen-ip054` and 2.54 to
8.74 on `gen-ip002`; along the reduced basis 2.29 to 11.49 and 2.54 to
11.17. No direction is thin.

Written in the reduced basis, `y = U^T x` with every row over `y` and a row
for each column's bounds (`lattice-trees.txt`), the trees read:

| region cut at | `gen-ip054` | `gen-ip002` at 1e10 |
|---|---|---|
| the optimum | optimal at 2.02e9, 58,587 nodes | -4801.69, no incumbent past 0 |
| the incumbent 7065.94 / -4217.16 | optimal at 6.09e9, 249,106 nodes | -4804.96, no incumbent |
| the incumbent 6866.23 / -4766.44 | optimal at 1.69e9, 53,482 nodes | -4801.56, no incumbent |

`gen-ip054` closes in the reduced basis whatever incumbent shapes it, in
53,482 to 249,106 nodes, where the columns' tree is open after 2,606,036.
The widths do not show why. `gen-ip002`'s bound moves by about 3, and its
tree in the reduced basis finds no point but `x = 0`: a point rounded in
`y` moves `x` by `V` times the rounding and leaves the bound rows
`V y >= 0`. A cutoff at the optimum in place of an incumbent
(`lattice-cutoff.txt`) reads worse, -4820.81, since a tree with no
incumbent dives from node 1000.

## The change

`MIP_LATTICE`, on by default, `jaos_set_mip_lattice`, `--lattice`,
`mip_lattice`:
- It takes a model whose columns are all integer with bounds more than 1
  apart, at most `MIP_LATTICE_COLS` (100) of them, at least
  `MIP_LATTICE_DENSITY` (0.5) of the matrix nonzero, no equality row, no
  SOS set, semi-continuous column or indicator row, a linear objective and
  no incumbent or node callback. In MIPLIB 3 and MIPLIB 2017 only the two
  gen-ip models have the shape (`02-463/`'s census).
- The tree runs over the columns until it has taken `MIP_LATTICE_NODES`
  (20000) nodes and holds an incumbent. A tree that ends first, or never
  finds an incumbent, is the answer, so no node is solved twice. Then it
  finds a strictly interior point by an LP for the region's Chebyshev
  center, walks to the analytic center by damped Newton steps, takes the
  Dikin ellipsoid there, and reduces the lattice under it by LLL with
  `MIP_LATTICE_DELTA` (0.99). The basis `U` and its inverse `V` are
  integers kept exactly through the reduction.
- The model is written in `y = U^T x` and the tree starts again from its
  root with the incumbent as its start. Its incumbent and pool are mapped
  back by `x = V y` and checked on the model's own rows. An optimum is
  published with the model's duals; at a limit the incumbent, the pool and
  the bound are published as a limited tree publishes them, with
  `jaos_solution` refusing and `jaos_mip_incumbent` giving the point. The
  report's counts add the first phase's to the reduced tree's.
- A region with no ball of radius `MIP_LATTICE_RADIUS` (1e-6), a basis
  that is the identity, a reduction past `MIP_LATTICE_ENTRY` or
  `MIP_LATTICE_SWAPS`, or a mapped point that breaks a row keeps the
  columns: the tree starts again on them.
- A node limit of 20000 or fewer keeps the columns, so the sub-MIPs of
  other models (500 nodes, 1000 for local branching and partial starts)
  never take it.

How many nodes run first (`jaos-lattice.txt`). The shape follows the
incumbent, and the reduced tree finds few points of its own, so the first
phase must hand it a good one:

| first phase | `gen-ip054` | `gen-ip002` at 1e10 |
|---|---|---|
| 1 node | optimal at 6.46e9, basis at 7065.94 | -4805.29, the optimum found in the reduced tree |
| 1000 nodes (2cb06169) | optimal at 1.81e9, basis at 6933.26 | -4800.85, incumbent -4754.42 |
| 12000 nodes | optimal at 2.51e9, basis at 6911.03 | -4802.27, the optimum |
| 20000 nodes, the change | optimal at 2.77e9, 75,989 nodes, basis at 6866.23 | -4802.43, the optimum |

The columns' tree finds `gen-ip002`'s optimum at node 10,326, and after
1000 nodes the reduced tree finds no point better than the first phase's
-4754.42: its primal gap widened from 0 to 0.0061. At 20000 nodes it holds
the optimum and its bound still moves from -4804.34 to -4802.43. A first
form that ran the MIP presolve on the reduced model read 4.67e9 and 1.89e9
on `gen-ip054` at 1 and 1000 nodes; the presolve there only mirrors
columns, so the change skips it. Best bound first on the columns reads
-4801.21 on `gen-ip002` with the optimum held, so there the change wins
less than a node order does.

| | `gen-ip054` | `gen-ip002` at 1e10 |
|---|---|---|
| the record | open, 6808.06, incumbent the optimum | -4804.34, incumbent the optimum |
| the change | optimal at 2.77e9, 75,989 nodes | -4802.43, incumbent the optimum |

`jaos-lattice-switches.txt`: with reliability 8 or 0, best bound first,
Gomory cuts at the root only, no node MIR or no heuristics, `gen-ip054`
still closes under the change, at 2.25e9 to 6.13e9 work units in 71,469
to 261,979 nodes, the slowest with no heuristics
(`jaos-lattice-switches-1000.txt` has the same arms with 1000 nodes
first, 1.65e9 to 5.34e9).

`hook.patch` tried to keep the primal side with a 1-node or 1000-node
first phase: each node of the reduced tree maps its point to the columns,
rounds it there, checks it on the model's own rows and, when it holds,
hands it back in `y`. It costs 4% to 7% of the work. After 1 node
`gen-ip002` reaches the incumbent -4774.65 with the bound at -4805.44, and
`gen-ip054` closes at 5.87e9; after 1000 nodes `gen-ip002` reaches -4761.24
with -4802.17, and `gen-ip054` closes at 1.85e9. An earlier form checked the
point on the reduced model's dense rows at 8% to 9% of the work: after 1
node it found `gen-ip002`'s optimum at node 153,026 with the bound at
-4805.92, and closed `gen-ip054` at 6.01e9. None of them holds the optimum
and the bound together, so the change has no rounding in the columns.

`lattice-sweep.txt`: 120 random dense models of general integers, covering,
packing with mixed signs and boxed with ranged rows, solved with the
reduced basis taken as soon as an incumbent exists and with the columns;
96 take the basis. Every pair ends with the same status and, when
optimal, the same objective.

`tests/test_mip.c` reads a thin knapsack slab of six general integers
whose columns' tree takes 26,261 nodes: the change starts again after
20,000 at the incumbent 400 and the reduced tree finds 426 at its node 6.
At a node limit of 20,007 the solve ends at the limit with 426 published
as the incumbent and the pool's first point, feasible and with its own
objective.

## MIPLIB 3 and MIPLIB 2017

MIPLIB 3 against `bench/miplib.baseline` (`miplib3/lattice.txt`): 0
regressed, 0 improved, 0 at other nodes, as at 2cb06169
(`miplib3/lattice-1000.txt`).
No model there has the shape.

MIPLIB 2017 at 1e10 (`miplib2017/lattice.txt`, `sum.txt`, 02-328's
`m17sum.py` against `bench/results/miplib2017.txt`):

| | solved | primal | dual | sum |
|---|---|---|---|---|
| the record | 6 | 4.4435 | 6.4500 | 10.8936 |
| 2cb06169, 1000 nodes first | 7 | 4.4496 | 6.4445 | 10.8941 |
| the change, 20000 nodes first | 7 | 4.4435 | 6.4448 | 10.8883 |

The other 28 read as the record, node for node and work unit for work
unit, in both readings. Under the change `gen-ip054` gives up its dual gap
of 0.0048 and `gen-ip002` narrows its dual gap from 0.0043 to 0.0039 with
its primal gap still 0. Under 2cb06169 `gen-ip002` traded a primal gap of
0.0061 for 0.0007 of dual gap.

## The verdict

Landed: `gen-ip054` closes at 2.77e9 work units in 75,989 nodes, where
JAOS stayed open at 1e11 and HiGHS and SCIP at 300 s. `gen-ip002` stays
open with its optimum held, its bound moved from -4804.34 to -4802.43. The
other 52 models of both sets read as before, node for node.

## On top of 02-477

The change landed after 02-477's second symmetry search, so MIPLIB 2017
was taken again on the landed commit, one solve at a time
(`combined/miplib2017.txt`, against `combined/before.txt`, 02-477's
record). `gen-ip054` closes at 2.77e9 work units in 75,989 nodes,
`gen-ip002` holds its optimum, the other 28 read as before node for
node, and the gap sum goes from 10.8874 to 10.8822 with 7 of 30
solved.

`make test`, `make sanitize`, `make python-test`, the netlib,
infeasible and Kennington sets and `make miplib J=2` pass on the landed
commit, and MIPLIB 3 reads its baseline with no instance at other
nodes.
