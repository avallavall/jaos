# 02-429 — reduced-cost fixing in the conic tree, refused

Taken on 2026-10-22 on the tree of 9f52dae. `rcfix.patch` is the change,
and `rcfix-implied.patch`, against e6df11a, its second form. No code is
kept.

## The idea

The refusal `conic-node-propagation` reopens with reduced-cost fixing
from a node's duals. 02-426's `ct_dual_bound` already gives, from a
node's row and cone duals, a lower bound `L` and each column's reduced
cost `d`. The patch runs at every node that has an incumbent, after the
leaf logic and before branching. For an integer column with `d > 0` and
a finite lower bound `l`, a value `l + k` costs at least `L + d·k`, so
the column's upper bound drops to `l + floor((T - L)/d)`, where `T` is
the bound at which the tree closes a node against the incumbent;
`d < 0` moves the lower bound the same way. The node's working copy
takes the new bounds, its children inherit them as fixings, and the
branch on a tightened column keeps to the tightened box. The switch is
`jaos_set_mip_rcfix`, as in the linear tree.

## The reading

Seven CBLIB mixed-integer instances at 1e10 work units (`robust_50_1`,
`estein5_A`, `classical_50_1`, `shortfall_50_1`, `uflquad-psc-10-100`,
`sssd-strong-20-4`, `turbine07`): every line is the same, and
`fixed_cols` is 0 on all seven. These models have columns with an
infinite bound whose reduced costs are small but not 0, and the bound
needs every such column bounded, so `L` is minus infinity at their nodes.

02-255's `misocp.c`, seeds 1 to 4, 1000 models each, with 02-376's
`unsure.patch`:

| seed | 9f52dae: nodes, work | with the patch: nodes, work |
|---|---|---|
| 1 | 2396, 380,821,330 | 2396, 380,818,548 |
| 2 | 2391, 392,832,218 | 2391, 392,495,155 |
| 3 | 2458, 395,190,325 | 2458, 395,141,408 |
| 4 | 2429, 393,044,055 | 2425, 392,713,108 |

The answers are the same, and no tree fails the brute-force check. The
work falls by 0.001% to 0.09%.

## With implied bounds

On `classical_50_1` the columns that block the bound are portfolio
weights in [0, ∞) with reduced costs near 1e-18, which only a row
bounds. `rcfix-implied.patch`, against e6df11a, adds to the patch two
kinds of implied bounds inside the dual bound: the checker's bounds from
the rows (`implied_bounds`, exposed as `jm_implied_bounds`), and the
cones' (a head at least 0, a member within the head's upper bound, or
within `sqrt(2·a·b)` of a rotated cone's heads). A column's reduced cost
is passed to the fixing only when the bound it used is the column's own.

| instance | e6df11a | with implied bounds |
|---|---|---|
| `robust_50_1` | optimal, 70 nodes, 1.7948e9 units | the same optimum, 70 nodes, 1.7842e9 units, 11 bounds tightened |
| `estein5_A` | optimal, 971 nodes, 2.340e9 units | work limit at 1e10, 44,702 nodes, bound 0.277 |
| `classical_50_1` | limit, bound -0.096630 | limit, bound -0.096736, 49 tightened |
| `shortfall_50_1` | limit, incumbent -1.101070 | limit, incumbent -1.100996, 100 tightened |
| the other three | | the same lines, nothing tightened |

On `estein5_A` the tightened bounds lead the tree to nodes whose
relaxation fails: at 3e9 units, 11,917 of 12,101 nodes are split after a
failed solve, against none without the fixing. The first such node,
`estein5_A-node62.mps`, ends `numerical_error` on its own too: the walk
starts with a dual objective of -2.0e41 and stops after 2 iterations.
The fixing gave C80 an upper bound of 2.0e41 and C94 one of 4.2e16: a
column with an infinite upper bound and a reduced cost near 1e-18 may
move by `(T - L)/d`, about 1e18 or more, so the fixing wrote that huge
finite bound in place of infinity.

Restricted to integer columns with a finite box, the fixing tightens no
bound on any of the seven instances, and every line is e6df11a's. So all
the tightenings above were on columns with an infinite bound. The idea
is refused (`conic-rcfix-every-node`).
