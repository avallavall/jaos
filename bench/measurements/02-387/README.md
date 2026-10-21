# 02-387 — what HiGHS's presolve removes from beasleyC3

Taken on 2026-10-21 on the tree of e29c6e2, for TODO row J7. No code
changed.

`beasleyC3` is a fixed-charge network: 1250 arcs, each a flow column
(no cost, in its capacity row and two node rows) and a binary (the fixed
cost, in its capacity row only), with the capacity row `x - u·y ≤ 0`;
and 500 node rows, all equalities. `shape.py` and `signs.py` read the
file's structure.

| | rows | columns |
|---|---|---|
| the model | 1750 | 2500 |
| HiGHS 1.15.1's presolve | 1153 (-597) | 1704 (-796) |
| JAOS's MIP presolve | 1589 (-161) | 2339 (-161) |

`bc3pre.sh` has HiGHS write its presolved model. It keeps 852 whole arcs
(flow, binary, capacity row) and 301 node rows: it removed 398 whole arcs
and 199 node rows, two arcs per node. HiGHS's `presolve_rule_off`
bitmask changes nothing on this model, so the rule could not be named
from outside.

What the structure rules out:

- No node row has all its arcs of one sign, so no node forces its arcs to
  zero.
- 160 node rows have two arcs, one in and one out (134 with a zero side).
  JAOS's presolve substitutes 161 such rows, one flow column each, and
  keeps both arcs' binaries and capacity rows.

## The arcs that go

`arcs.py` sets the presolved model's names against the model's. Every
arc has a reverse arc, so the network is an undirected graph with an arc
each way. Of the 398 arcs that go, 314 touch a node with one edge (two
arcs, degree 2 in the table), and the rest touch nodes that became such
leaves once the first ones went:

| degrees of the gone arc's two nodes | arcs |
|---|---|
| 2 and 6 | 102 |
| 2 and 8 | 82 |
| 2 and 10 | 54 |
| 2 and 4 | 40 |
| 2 and 12 | 24 |
| 2 and 16 | 12 |
| 4 and 8 | 16 |
| 4 and 6 | 12 |

At a leaf with no demand, 134 of them at the start, the node row reads
`x_uv - x_vu = 0`: the two arcs form a cycle of two, and flow around it
only pays fixed charges. So both flows can be 0, both binaries 0, and the
leaf, its two arcs and their capacity rows go; the neighbour may become a
leaf in turn.

As presolve steps this is: substitute the doubleton equation (JAOS does);
the substituted flow's two entries in the neighbour's row cancel; the
flow, with no cost and only `x - u·y ≤ 0` rows left, is fixed at 0 by dual
fixing (D246, refused for the LP presolve in `bench/refusals.txt`); each
binary is then alone in a row that always holds, with a positive cost, and
is fixed at 0.

## What the smaller model buys

JAOS on HiGHS's presolved model, the same problem with 597 rows fewer, at
1e10 work units (`bc3solve.sh`):

| | nodes | bound | incumbent |
|---|---|---|---|
| beasleyC3 | 5461 | 742 | 830 |
| HiGHS's presolved model | 4042 | 745 | 851 |

The optimum is 754. The smaller model moves the bound by 3 and the
incumbent the wrong way, so these reductions would not close the gap on
their own; the root's bound and the incumbent are what keep it open.
