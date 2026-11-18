# 02-478 — gen-ip002: the reduced tree goes back to the columns when its bound trails theirs

Taken on 2026-11-18 on b53a1ffd, for TODO row J7's `gen-ip002` line.
`read.sh` takes every reading one solve at a time, from copies of b53a1ffd
(the record), of b53a1ffd with `MIP_LATTICE_NODES` at 1, 100, 1000 and
5000, of 52cc3a45 (before 02-475), of b53a1ffd with `proto.patch` (the
change's check with the first phase left at 20000 nodes, switches read
from the environment, and dumps of the basis, the reduced model and every
node's point), and of the change, also with its first phase at 5000,
10000 and 20000 nodes:
- `jaos-change.txt`: both models at 1e10 work units, the record and the
  change, with `--no-lattice` and with `--node-select bound`;
- `jaos-first.txt`: the record and the change with other first phases;
- `jaos-reduced.txt`: `proto.patch`'s switches on the reduced tree, with
  and without the check;
- `jaos-reduced-switches.txt`: `gen-ip002`'s reduced model written out and
  solved on its own from the first phase's incumbent, under the tool's
  switches;
- `jaos-optstart.txt`, `jaos-restart.txt`, `jaos-52cc3a45.txt`:
  `gen-ip002` from its optimum, from the first phase's incumbent on the
  columns, and the columns' tree before 02-475;
- `offline-*.txt` with `offline.py`, `coff.py`, `rad.py`, `rad2.py`,
  `near.py`, `dives.py`, `center.py`, `rins.py` and `anamod.py`: the
  nodes' points, from outside the tool;
- `sweep.txt`, `sweep-1e10.txt`, `sub3.py`: 40 sub-models of the two;
- `miplib3/change.txt`, `miplib2017/change.txt`, `sum.txt`: both sets
  with the change.

`read.sh` uses `git archive`, so it runs in the main checkout, where the
WSL side reads the repository; `LANDED` names the change's commit.

## Where gen-ip002 stood

At b53a1ffd `gen-ip002` ends at 1e10 with the incumbent -4771.64, 0.0025
from its optimum -4783.73, and the bound -4807.19. Its first 20,000 nodes
over the columns find -4771.64 at node 18,410; before 02-475's fixing at
every node they found the optimum at node 10,326 (`jaos-52cc3a45.txt`).
The tree on the reduced basis then takes 174,427 nodes and finds no point.
The columns alone (`--no-lattice`) find the optimum at node 74,569 and end
at -4804.20.

## The reduced tree's own points

`proto.patch`'s `JAOS_NODEDUMP` writes the point of each of the reduced
tree's 47,126 nodes in its first 4e9 work units, and `JAOS_NODECOLS` the
first 80,105 nodes of the columns' tree. Against the incumbent the basis
was taken at, -4771.64 (`offline-reduced.txt`, `offline-radius.txt`,
`offline-dives.txt`, `offline-center.txt`):

| rounding of the reduced tree's points | nodes with a better point |
|---|---|
| `y` to nearest | 0 |
| `x = V y` to nearest, down, or four times at random | 0 |
| every pattern of up to 12 fractional `y` (8,367 nodes) | 0 |
| one column of the rounded `x` moved by one | 0 |
| two such moves | 2: -4774.65 at node 22,925, the optimum at node 34,953 |
| SCIP at every 500th node: a dive, a dive with the zero columns fixed, RENS, a 500-node sub-MIP over the non-zero columns | the sub-MIP finds -4774.65 at 2 of 94 |
| depth-first central rounding, Chebyshev centers, 3000 LPs, in `y` and in `x` | none |

Rounding in `x` holds the bound rows `V y >= 0`, which answers
`lattice-round-columns`'s reopening: the reduced nodes' points sit at
vertices of the region cut at -4771.64, a mean of 22 of their 41 columns
fractional, and the readings find three integer points past the
incumbent: -4773.20, -4774.65 and the optimum (`rins.py`'s SCIP count of
the points at least as good as the incumbent reads 2, short of those four,
and is not used). The optimum is within an
`x` distance of 2 of one reduced node's rounding and of 3 of six
(`offline-near.txt`). The columns' nodes come nearer: of the first 80,105,
four round to within one move of the optimum (nodes 10,403, 31,025, 31,028
and 74,571), and in the first 20,000 a radius-one search reaches it at node
10,403 by mending 14 broken rows with one move (`offline-columns.txt`,
`offline-radius.txt`). In a run the path moves: `proto.patch`'s
`JAOS_REPAIR`, that move at every node where plain rounding breaks rows,
finds 6 incumbents in the first phase and still ends it at -4771.64
(`round-one-column-moved`).

The tool's own switches on the reduced model alone, from its incumbent, at
5e9 work units (`jaos-reduced-switches.txt`): a dive from every node, dives
with five backtracks, the dive heuristic to depth 5, RINS, the pump with
general integers and no heuristics all end with the start as the only
point. At b53a1ffd a first phase of 1, 100, 1000 or 5000 nodes leaves the
reduced tree at -4766.68, -4770.73, -4770.73 and -4771.08
(`jaos-first.txt`).

## The reduced tree's bound on gen-ip002

From their roots, at equal node counts (the record's progress log):

| nodes | `gen-ip002` columns | reduced basis | `gen-ip054` columns | reduced basis |
|---|---|---|---|---|
| 1,000 | -4829.17 | -4828.74 | 6778.42 | 6805.94 |
| 10,000 | -4820.28 | -4820.36 | 6788.01 | 6822.54 |
| 20,000 | -4817.25 | -4817.19 | 6791.70 | |

On `gen-ip002` the reduced tree's bound per node is the columns', and a
reduced node costs twice the work: 65 dense rows, the 41 bound rows among
them, against 24. With the optimum given from the root
(`jaos-optstart.txt`) the two read alike at 1e10, -4804.75 on the reduced
basis and -4804.41 on the columns, and -4800.93 and -4800.75 by bound.
On `gen-ip054` the reduced tree leads from its first thousand nodes.

## The change

The tree on the reduced basis carries a mark at the first phase's work
and that phase's final bound. When its work reaches the mark, its bound
(the open nodes', the node in hand's, a node set aside), on the
objective's grid, must have passed the first phase's. If it has not, the
reduced tree stops and the tree starts again on the columns, from the
better of the two trees' incumbents mapped back and checked, with what is
left of the work, node and time limits. The report adds the three trees'
counts and keeps the first phase's first incumbent, and at a limit the
bound published is the best of the three trees'. The summary log gives the
two bounds compared. The check has no constant: the mark is the first
phase's own work, and the bound must pass, by any amount.

With the check the first phase no longer has to find `gen-ip002`'s
optimum, which was what held it at 20000 nodes in 02-476, so
`MIP_LATTICE_NODES` goes to 1000. `jaos-change.txt`, `jaos-first.txt`:

| first phase | `gen-ip002` at 1e10: bound, incumbent | the check | `gen-ip054` | the check |
|---|---|---|---|---|
| the record, 20000 | -4807.19, -4771.64 | | optimal at 2.05e9 | |
| 1000, the change | -4804.32, the optimum | -4830.12 against -4829.17 at 5.3e7, back | optimal at 1.36e9 | 6806.40 against 6778.42 at 5.5e7 |
| 5000 | -4803.88, -4771.08 | -4823.02 against -4823.26 at 2.0e8, kept | optimal at 1.34e9 | kept |
| 10000 | -4804.83, the optimum | -4821.21 against -4820.28 at 3.8e8, back | optimal at 1.58e9 | kept |
| 20000 | -4805.21, the optimum | -4819.62 against -4817.25 at 7.2e8, back | optimal at 2.05e9 | 6834.80 against 6791.70 at 9.6e8 |
| `--no-lattice` | -4804.20, the optimum | | open, 6806.77 | |

Under the change `gen-ip002`'s columns' second tree starts from the first
phase's -4754.42 and finds -4771.08 at its node 1,744 and the optimum at
its node 43,939. `gen-ip054`'s reduced tree, on a basis taken at 6919.88,
finds 6849.99 by rounding at its node 18,924 and the optimum at its node
25,608: with a basis taken that early the region holds points the reduced
tree's rounding reaches.

The check is a knife edge where the two trees' bounds a unit of work are
alike, as on `gen-ip002`: with 5000 nodes first it passes by 0.24 and
`gen-ip002` ends without its optimum. With `--node-select bound` for every
tree it passes by 1.00, -4826.22 against -4827.22 at 5.7e7, and
`gen-ip002` ends at -4800.77 with the incumbent -4737.98, its bound within
0.02 of the best reading and its incumbent the worst; `gen-ip054` closes
at 1.54e9.

`sweep.txt`: 40 sub-models of the two, each without 8 to 14 columns and up
to 4 rows, at 2e9 work units under the record, the change and the change
with 20000 nodes first. The check alone, with 20000 nodes first, fires on
15 and reads as the record on all but `a_04` and `a_14`, where it fails;
the change fires on 23 and fails on `a_14` alone. Against the record:

| | `a_*`, from `gen-ip002` | `b_*`, from `gen-ip054` |
|---|---|---|
| closes sooner, or a better bound at 2e9 | 15 | 9, 1.04x to 10.3x sooner |
| worse | `a_02` 1.07x, `a_15` 1.20x, `a_16` bound and incumbent | `b_00` 1.38x, `b_16` 1.34x, `b_19` 1.03x, all under 3e7 work units |
| a better bound and a worse incumbent, or the reverse | `a_11`, `a_19` | |
| as the record | | 8, closed before 1000 nodes or infeasible |

At 1e10 (`sweep-1e10.txt`) the six left open at 2e9 read: `a_04`,
`a_14`, `a_17` and `a_19` close at 0.79x, 0.75x, 0.78x and 0.94x the
record's work, `a_16` closes at 1.53x, and `a_11` stays open with its
bound at -4761.92 against -4758.85.

## The dual side

`jaos-reduced.txt`, the check off and 20000 nodes first: the reduced tree
taking its nodes by bound reads -4802.60 on `gen-ip002` but takes
`gen-ip054` to 2.19e9, and 2.56e9 with its heuristics off too, which alone
read -4806.50 and 2.00e9 (`lattice-tree-by-bound`). Closing `gen-ip002`
needs the bound at -4783.73; with the optimum given from the root the best
reading is -4800.75, by bound on the columns, and SCIP reads -4801.29
after 2.27M nodes in 300 s (02-476). Neither the basis nor the node order
moves the bound by 5, so the next step is the relaxation.

## MIPLIB 3 and MIPLIB 2017

MIPLIB 3 against `bench/miplib.baseline` (`miplib3/change.txt`): 0
regressed, 0 improved, 0 at other nodes; no model there has the shape.

MIPLIB 2017 at 1e10 (`miplib2017/change.txt`, `sum.txt`, 02-328's
`m17sum.py` against `bench/results/miplib2017.txt`):

| | solved | primal | dual | sum |
|---|---|---|---|---|
| the record | 9 | 5.4067 | 6.3032 | 11.7099 |
| the change | 9 | 5.4042 | 6.3026 | 11.7067 |

The other 28 read as the record, node for node and work unit for work
unit. `gen-ip002` gives up its primal gap of 0.0025 and narrows its dual
gap from 0.0049 to 0.0043; `gen-ip054` closes at 1.36e9 in 59,222 nodes
against 2.05e9 in 65,993.

## On top of 02-479 and 02-480

These readings were taken on b53a1ff. The change landed after 02-479's
dual Devex at the nodes and 02-480's Gomory cuts over the implied
integers, so MIPLIB 2017 was taken again on the landed code, one solve
at a time (`combined/miplib2017.txt`, against `combined/before.txt`,
02-480's record). `gen-ip054` closes at 1.38e9 work units against
2.17e9; `gen-ip002` holds its optimum and its bound at 1e10 goes from
-4805.67 to -4803.89. The other 28 read as before node for node, the
gap sum goes from 11.6179 to 11.6175, and MIPLIB 3 reads its baseline.

## The verdict

Landed. `gen-ip002` holds its optimum at 1e10 again, with its bound moved
from -4807.19 to -4804.32, and `gen-ip054` closes at 1.36e9 work units
against 2.05e9. `gen-ip002` does not close: the reduced basis gives it no
bound a node, its optimum is a point few roundings reach, and with the
optimum known from the root no node order takes the bound past -4800.75.

`make test` and `make sanitize` pass on the landed code. The Netlib,
infeasible and Kennington sets, taken with the check and 20000 nodes
first (the LP solves never reach the lattice), read their baselines with
no instance at other nodes. `make primal barrier pdlp concurrent warm`
were not taken: `src/simplex.c` is untouched.
