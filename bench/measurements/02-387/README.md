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

The 398 arcs HiGHS drops whole are the next thing to read: which arcs,
and what in the model lets both their columns go.
