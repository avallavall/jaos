# 02-427 — models with quadratic rows edited between solves

Taken on 2026-10-22 on the tree of aba9958. `read.sh` takes the first
reading (`warmqc-head.txt`), and `gate.sh` takes the gate of the change
in `src/conictree.c` with the reading after it (`warmqc.txt`).

## The harness

`warmqc.c` is 02-425's `warmcone.c` with quadratic rows in place of
cones. A quadratic row has a linear part over about a third of the
columns and a diagonal quadratic part over 1 to 3 of them, with weights 1
to 3: convex with a finite upper side, or concave with a finite lower
side, drawn near the row's value at the last optimum. Each model starts
with one or two. The two cone edits give way to a new quadratic row and a
dropped one (deleted, or its quadratic part cleared), and an edit of a
quadratic row's sides keeps only the side that leaves it convex. P1 and
P2 are 02-425's. P3 judges a certificate when one is published. With
quadratic rows the checker's certificate test is not exact (TODO's J22),
and an infeasible answer whose certificate it refuses publishes none, by
design; those are counted, not failed. With no quadratic row left, an
infeasible answer must carry a certificate. Pairs that end with any other
status are counted as "other". Modes qcp and miqcp, seeds 1 to 8, 1000
models a seed, eight edits a model.

## The first reading

| mode | optimal | infeasible | of which no certificate | unbounded | capped | other | P1, P2, P3 |
|---|---|---|---|---|---|---|---|
| qcp, 64,000 edits | 18,150 | 45,105 | 588 | 729 | 0 | 16 | 0, 0, 0 |
| miqcp, 64,000 edits | 17,246 | 45,888 | 0 | 711 | 153 | 2 | 0, 0, 0 |

## The continuous cases

All 16 qcp pairs that end with no answer end `numerical_error`, warm and
cold: the point passes the checker, and its duals miss by 0.07 to 34.7.
Moving every quadratic row's side outward by 1e-4 makes all 16 solve
optimal. On the eight with one quadratic row, a QP gives the row's
smallest value over the other rows and bounds (largest for a concave
row), and it equals the row's side each time: -10, -10, -10, -18, 5, 5,
5 and -6. So no point lies strictly inside the row, and finite duals need
not exist. This is TODO's J27.

## The integer cases

Both miqcp pairs (seed 8, model 95 after edit 1 and model 496 after edit
1) end `numerical_error` at a rough leaf. 02-426's bound from a rough
node's duals skipped models with quadratic rows, so the leaf kept its
parent's bound: -22.03 against the incumbent -15.5000093, and 19.26
against 21.3333325.

## The change

`ct_dual_bound` now reads diagonal quadratic rows and a diagonal
quadratic objective. The Lagrangian of a node with row duals `y`, cone
duals `z` and the objective `c·x + Σ q·x²/2` is, for a column,
`d·x + h·x²/2` with `d = c - Aᵀy - z` and `h = q - Σ y·v`, `v` the
column's weights in the quadratic rows. Its smallest value over the
column's box has a closed form: at `-d/h` held in the box when `h > 0`,
at the cheaper end when `h < 0`. A row's value counts `v·x²/2`, as the
checker reads it. A term across two columns, in the objective or in a
row with a nonzero dual, leaves the node with no such bound.

| model | aba9958 | the change |
|---|---|---|
| seed 8, 95-1 | incumbent -15.500009337761226, bound -22.027, 91 nodes, 196,697,942 units | optimal at the same value, 5 nodes, 8,328,328 units |
| seed 8, 496-1 | incumbent 21.33333247392029, bound 19.262, 43 nodes, 78,705,089 units | optimal at the same value, 15 nodes, 9,233,876 units |

`tests/data/qc_rough_leaf.mps` is 95-1. The three test models of 02-425
and 02-426 give the same answers and nodes.

## The gate

`gate.sh` is 02-426's on aba9958 against the change, with the reading
below beside it.

| set | aba9958 | the change |
|---|---|---|
| 02-255, 8000 models | 02-426's lines, byte for byte | the same lines |
| 02-326, 6000 models | the same | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | the same | the same lines |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | the same | the same lines |
| 02-425's `warmcone.c`, misocp, seeds 1 to 3 | 02-426's `warmcone.txt` | the same counts |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

## The reading after the change

`warmqc.txt`, with the first reading for comparison:

| mode | | optimal | infeasible | of which no certificate | unbounded | capped | other | P1, P2, P3 |
|---|---|---|---|---|---|---|---|---|
| qcp | aba9958 | 18,150 | 45,105 | 588 | 729 | 0 | 16 | 0, 0, 0 |
| qcp | the change | the same | | | | | | |
| miqcp | aba9958 | 17,246 | 45,888 | 0 | 711 | 153 | 2 | 0, 0, 0 |
| miqcp | the change | 17,177 | 45,972 | 0 | 708 | 142 | 1 | 0, 0, 0 |

The continuous mode has no tree, so it does not move. In the integer
mode, an edit's values depend on the last answer. Once two trees answer
an edit differently, the later edits can draw a different number of
values from the random stream, and then the rest of the seed draws other
models (seed 5 shares its edits only up to model 221). `pairs.sh`
compares the miqcp pairs whose edits so far are the same on both trees
(`pairs.txt`): 57,753 give the same answer and 8 change, and the other
6,239 of the 64,000 have different edits on the two trees. Of the 8,
five go from the work limit to optimal, the two models above go from
`numerical_error` to optimal, and one goes from the work limit to
`numerical_error` (seed 6, model 708 after edit 2: the tree, no longer
stopped by the limit, sets aside a node whose relaxation stopped after 74
iterations with no answer). No pair changes between optimal, infeasible
and unbounded.
