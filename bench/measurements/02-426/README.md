# 02-426 — refused conic solves inside the tree give bounds and points

Taken on 2026-10-22 on the tree of e92ca4c. `gate.sh` takes the gate and
the misocp reading (02-425's `gate.sh` on this change), and `pairs.sh`
the pair comparison.

## The change

Four parts in `src/conictree.c`. `src/conic.c` only shares its cone
projection.

**A bound from any duals.** A node of the conic tree whose relaxation
stops rough kept its parent's bound. `ct_dual_bound` now gives it a bound
from the rough answer's own row duals `y` and cone duals `z`. For any `y`
and any `z` in the cones' dual cones, and any feasible `x`,

    c·x = y·(A x) + z·x + d·x,   d = c - Aᵀy - z,

where `z·x >= 0`, `y·(A x) >= Σ y·side` when each `y` takes the side it
needs, and `d·x >= Σ min(d·lower, d·upper)`. So
`Σ y·side + Σ min(d·lower, d·upper)` bounds the node from below,
whatever the duals' quality. The cone
duals are first projected into their cones (`jm_cone_project`, which is
`conic.c`'s certificate projection on any array), and a row dual whose
side is infinite is taken as 0. When a column needs an infinite bound,
or the model has a quadratic objective or quadratic rows, there is no
such bound. The sums are compensated, as in the checker. The node's key
is the larger of this bound and its parent's.

**A leaf closes only on a proof.** A node whose point is integral solves
the model with its integer columns fixed there, and an optimal fixed
model closed the node. When the node's relaxation was rough, that proves
nothing about the box's other integer points, and 02-425's clamp can
make a rough point integral. Now an optimal fixed model closes the node
only when the relaxation passed the checker, or when the box fixes every
integer column and the model has no SOS sets, semi-continuous columns or
indicator rows, so that the fixed model is the node.

**The fixed model of a whole box.** In that second case, a fixed model
that ends infeasible closes the node, and one that fails gives the node
the bound of its own duals. Before, both were set aside.

**A point from a failed fixed solve.** A fixed solve that ends
`numerical_error` at a point the checker passes as primal feasible,
cones and integrality included, now gives an incumbent. A MIP answer
carries no duals the checker judges.

A leaf that none of these closes is split, or set aside when its box
fixes every integer column. The tree's summary line counts the rough
nodes whose own duals raised their bound and the leaves the last two
parts closed.

## The models of J26

Edited copies of 02-425's misocp mode that end `numerical_error` on
e92ca4c, warm and cold: all seven of seed 1, and the four of seeds 2 and
3 that the first part alone still left. "257-1" is model 257 after
edit 1.

| seed, model | e92ca4c: incumbent, bound, nodes, work | the change: optimum, nodes, work |
|---|---|---|
| 1, 257-1 | -53.601306625692168, -53.6279, 140, 95,836,526 | -53.601306625692168, 110, 42,112,962 |
| 1, 257-2 | the same as 257-1 | the same as 257-1 |
| 1, 138-3 | 54.803061543300934, 50.991, 112, 182,435,908 | 54.803061543300934, 24, 7,997,620 |
| 1, 138-4 | 45.333333333333336, 41.167, 113, 133,198,862 | 45.333333333333336, 50, 15,162,186 |
| 1, 138-5 | 45.333333333333336, 41.167, 119, 142,709,482 | 45.333333333333336, 50, 15,222,839 |
| 1, 138-6 | 25.333333333333332, 21.167, 122, 156,842,833 | 25.333333333333332, 42, 13,464,419 |
| 1, 269-2 | -90.222222222222229, -95.059, 55, 53,766,776 | -90.222222222222229, 21, 7,026,244 |
| 2, 208-5 | -150, -155.600, 113, 199,854,765 | -155.60000239514866, 20, 25,892,895 |
| 3, 136-5 | 68.666666666666671, 67.333, 57, 71,322,543 | 67.33333169134896, 15, 15,458,228 |
| 3, 136-6 | 74.888887972868091, 74.333, 25, 26,746,142 | 74.44444280237451, 19, 17,798,868 |
| 3, 169-1 | -197.2, -197.875, 85, 56,643,872 | -197.67647296188269, 73, 38,788,615 |

On the first seven the change keeps e92ca4c's incumbent and closes the
gap with the rough nodes' own bounds. On the last four the incumbent
improves: a leaf whose box fixes every integer column has a fixed solve
that ends `numerical_error` at a feasible point. That point becomes the
incumbent, and it lies within the gap of the leaf's bound, so the leaf
closes.
`tests/data/cone_touch_leaf.mps` is 257-1 and
`tests/data/cone_leaf_point.mps` is 208-5. 02-425's
`cone_rough_branch.mps` still ends at 48.5, in 278 nodes instead of 290.
Models 257-1, 138-3 and 269-2 maximised, with every cost and the offset
negated, end at the negated optima on the change (114, 24 and 21 nodes)
and `numerical_error` on e92ca4c.

## The gate

The change's records are `mip.txt`, `dis.txt`, `cblib-mip.txt` and
`miqcqp.txt`. HEAD's are 02-425's files of the same names, byte for byte.

| set | e92ca4c | the change |
|---|---|---|
| 02-255, 8000 models | 2 unsure, 1 tree numerical error | the same lines |
| 02-326, 6000 models | 4 unsure | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | 30 optimal | 30 optimal, five lines move |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | 14 at the limit | the same lines |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

The CBLIB lines that move:

| instance | e92ca4c | the change | CBLIB's optimum |
|---|---|---|---|
| `sssd-weak-25-4` | optimal 311,721.13, 1763 nodes, 7.549e9 units | the same optimum, 2130 nodes, 8.980e9 units | 311,719.29 |
| `sssd-weak-15-8` | limit, incumbent 941,852.9, bound 234,134.2 | limit, 911,781.0, 253,800.1 | 622,505.4 |
| `sssd-weak-20-8` | limit, 1,052,864.9, 241,687.7 | limit, 912,911.8, 241,208.5 | 600,342.9 |
| `sssd-weak-30-8` | limit, 1,199,541.2, 200,810.3 | limit, 1,187,203.0, 195,906.5 | 528,760.9 |
| `turbine07_lowb` | limit, 1.8873, 0.74437 | limit, 2.0361, 0.74437 | 0.8993 |

`sssd-weak-25-4` has no rough node. On the change a fixed solve at node
38 ends `numerical_error` at a feasible point, and the tree takes it as
its first incumbent, 1,468,487.8; e92ca4c's first is 1,494,558.3 at node
42. The later incumbents are the same values, found 4 to 7 nodes
earlier, but the tree leaves its depth-first phase sooner, and it closes
after 367 more nodes. The other four stop at the limit on both trees, in
other orders, with three better incumbents and one worse.

## The reading after the change

02-425's `warmcone.c` in misocp mode, 300 models a seed and eight edits
a model (`warmcone.txt`). The column "other" counts the pairs that end
with none of the four statuses; on seed 1 they are all
`numerical_error`.

| seed | | optimal | infeasible | unbounded | capped | other | P1, P2, P3 |
|---|---|---|---|---|---|---|---|
| 1 | e92ca4c | 824 | 1499 | 60 | 10 | 7 | 0, 0, 0 |
| 1 | the change | 842 | 1493 | 60 | 5 | 0 | 0, 0, 0 |
| 2 | e92ca4c | 852 | 1447 | 75 | 20 | 6 | 0, 0, 0 |
| 2 | the change | 865 | 1445 | 75 | 15 | 0 | 0, 0, 0 |
| 3 | e92ca4c | 791 | 1558 | 34 | 13 | 4 | 0, 0, 0 |
| 3 | the change | 800 | 1558 | 34 | 8 | 0 | 0, 0, 0 |

An edit takes its values near the last answer's point, so once the two
trees answer an edit differently, the model's later edits differ. That
moves the infeasible counts. `pairs.sh` compares seed 1 pair by pair over
the edits whose history is the same on both trees (`pairs.txt`): 2374
pairs give the same answer, 7 go from `numerical_error` to optimal and 5
from the work limit to optimal. No pair changes between optimal,
infeasible and unbounded.
