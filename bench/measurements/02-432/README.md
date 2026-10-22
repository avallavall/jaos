# 02-432 — a failed node whose cones leave its box empty

Taken on 2026-10-22 on the tree of 94bf254. `dump.sh` takes apart the
mixed disagreements 02-431 left, `read.sh` takes 02-431's permutation
reading in mixed mode on the change, and `gate.sh` the gate of the change
in `src/conictree.c`.

## The 45 disagreements

02-431's mixed reading on its change (`../02-431/mixed-new.txt`) has 45
models where one order ends optimal and the other `numerical_error`.
`dump.sh` dumps both orders of each and runs 02-431's `coneout.c` on them
with 94bf254's library (`coneout.txt`): each order as it is, then with
every quadratic side and cone head moved out by 1e-4.

In 39 of the 45, both orders moved out end optimal at the same
objective, so the model as it is has a quadratic row or cone with no
point strictly inside (TODO J27). In the other six, moving out does not
help. In five of them a node's own box makes a cone touch, which moving
the model's sides out does not change. The sixth is described below.

Seed 1's model 14354 in its permuted order (`tests/data/cone_empty_leaf.mps`)
is one. C2 is fixed at 0 and heads the rotated cone 2·C2·C4 >= C3², so
C3 must be 0. A leaf of the tree with C3 = 1 holds no point, yet points
with C2 near 0 and C4 large come arbitrarily close to it. So its solve
fails, and no certificate of infeasibility exists. 94bf254 set that leaf
aside, and the tree ended `numerical_error`.

## The change

When a node's relaxation fails, or a settled node's own solve fails, the
tree now tightens a copy of the node's box with the bounds the cones
imply (`ct_cone_bounds`, the rule the dual bound already used). If a
column's lower bound then passes its upper bound, the node holds no
point and closes (`ct_cone_empty`). A node that solves is not touched.

The rule had a gap: for a rotated cone with one head's upper bound 0 and
the other's infinite, it computed `sqrt(2·0·∞)`, which is NaN, and so
implied nothing. A head at 0 holds every member at 0 whatever the other
head is, and the rule now says so. That also reaches the dual bound.

With the change, the CLI ends both orders of 9 of the 45 optimal at the
same objective (`cli.txt`): five of the six and four of the 39. Of the
36 left, 35 are of J27's kind. The last, seed 10's model 1487, has the
other form 02-431 found in c2939: with C2 = 6 the cone needs
2·C3·C4 >= 36, and the objective improves as C3 goes to 0 and C4 grows
without limit, so its optimum 15 is a limit no point reaches.

## The reading

`read.sh` runs 02-431's `permcone.c` in mixed mode on the change
(`mixed.txt`). The continuous mode has no tree, so it does not move.

| tree | compared | disagree | no answer either way | skipped at the work limit |
|---|---|---|---|---|
| 94bf254 (02-431's `mixed-new.txt`) | 198,286 | 49 | 672 | 1,714 |
| the change | 198,371 | 41 | 615 | 1,629 |

"No answer either way" counts the pairs where both orders end with no
answer, as in 02-431's table. On the change 57 fewer pairs end with no
answer in both orders, and 85 fewer stop at the work limit.

Nine disagreements are gone, the nine above. One is new: seed 4's model
2939. On 94bf254 both its orders stop at the 2e7 work limit, so the
reading skipped it. On the change one order ends optimal and the other
still ends `numerical_error`.

## The gate

`gate.sh` is 02-431's on 94bf254 against the change.

| set | 94bf254 | the change |
|---|---|---|
| 02-255, 8000 models | | the same lines |
| 02-326, 6000 models | | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | | the same lines |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | | the same lines |
| 02-427's `warmqc.c`, both modes, seeds 1 to 8 | 02-427's `warmqc.txt` | the same lines |
| 02-425's `warmcone.c`, misocp, seeds 1 to 3 | 02-431's `warmcone.txt` | P1, P2, P3 all 0 (`warmcone.txt`) |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

The `warmcone.c` counts move on seed 1 only: capped goes from 3 to 1
and optimal from 847 to 849. 02-431's `pairs.sh`, run again with 94bf254
as HEAD (`pairs.txt`), finds every pair of the three seeds with the same
edits on both trees. Two change: seed 1's model 202 after edits 6 and
7, which 94bf254 stops at the work limit and the change ends optimal.
