# 02-429 — reduced-cost fixing in the conic tree, refused

Taken on 2026-10-22 on the tree of 9f52dae. `rcfix.patch` is the change.
No code is kept.

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
`fixed_cols` is 0 on all seven. These models have free columns, such as
cone heads, whose reduced costs are small but not 0, and the bound needs
every such column bounded, so `L` is minus infinity at their nodes.

02-255's `misocp.c`, seeds 1 to 4, 1000 models each, with 02-376's
`unsure.patch`:

| seed | 9f52dae: nodes, work | with the patch: nodes, work |
|---|---|---|
| 1 | 2396, 380,821,330 | 2396, 380,818,548 |
| 2 | 2391, 392,832,218 | 2391, 392,495,155 |
| 3 | 2458, 395,190,325 | 2458, 395,141,408 |
| 4 | 2429, 393,044,055 | 2425, 392,713,108 |

The answers are the same, and no tree fails the brute-force check. The
work falls by 0.001% to 0.09%. The idea is refused
(`conic-rcfix-every-node`).
