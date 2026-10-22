# 02-431 — models with cones and quadratic rows in another order

Taken on 2026-10-22 on the tree of ee5b4c7. `read.sh` takes the
permutation reading, on HEAD and on the change (`*-head.txt`,
`*-new.txt`), and `gate.sh` the gate of the change in `src/conictree.c`.
`pairs.sh` and `cross.sh` read the gate's warm sweep pair by pair, and
`coneout.c` probes the continuous disagreements.

## The harness

`permcone.c` is 02-224's `perm.c` with 0 to 2 cones (a quadratic one
over 2 to 4 columns or a rotated one over 3 to 4) and 0 to 2 diagonal
quadratic rows (convex with a finite upper side, or concave with a
finite lower side), at least one of the two in every model, on top of
02-224's integer marks, SOS set, semi-continuous column or indicator
row. Each model is built in its own order and in a random permutation of
its rows and columns, cones and quadratic entries moving with them, and
both are solved under 2e7 work units. The statuses must match and the
objectives agree within 2e-6 of (1 + |a|). The mixed mode keeps the
discrete shapes and the continuous mode takes them off. Seeds 1 to 10,
20,000 models a seed.

## The reading

| mode | tree | compared | disagree | no answer either way |
|---|---|---|---|---|
| mixed | ee5b4c7 | 198,250 | 97 | 810 |
| mixed | the change | 198,286 | 49 | 697 |
| continuous | ee5b4c7 | 200,000 | 168 | 847 |
| continuous | the change | the same lines | | |

Every disagreement but six is one order optimal (or infeasible or
unbounded) and the other `numerical_error`. The six are objectives that
differ by 3e-5 to 9e-5 relative, two on ee5b4c7 and four on the change,
discussed below.

## The mixed disagreements

On ee5b4c7, a rough leaf whose box fixed every integer column was set
aside when the model had a semi-continuous column, an SOS set or an
indicator row, because the fixed model was then not the node. If the
leaf's bound did not reach the incumbent, the tree ended
`numerical_error`. The change:

- A node is settled when its box fixes every integer column, holds every
  semi-continuous column on one side of its gap, and leaves every SOS set
  only its own pattern. A settled rough leaf in a model with those
  pieces solves its own relaxation in full (`ct_node_full`), which is
  then the node itself, and that solve closes it, bounds it or gives an
  incumbent as the fixed model of 02-426 does.
- A node that is split because every integer column is fixed is split on
  an open semi-continuous column, then on an open SOS set
  (`ct_split_pieces`), before it is set aside.
- The bound from a node's duals reads each column's bounds tightened by
  those the rows imply (the checker's `implied_bounds`, exposed as
  `jm_implied_bounds`) and those the cones imply (a head at least 0, a
  member within the head's upper bound, or within `sqrt(2·a·b)` of a
  rotated cone's heads).

`tests/data/cone_semi_leaf.mps` is seed 1's model 562 in its permuted
order: ee5b4c7 ends it `numerical_error`, and the change ends it optimal
at -10.5, as the other order does.

Of the 49 disagreements left on the change, 45 have one order end
`numerical_error`. The one taken apart, seed 1's model 1171, is of J27's
kind: a settled leaf whose own relaxation has a cone pressed to its
boundary, so its duals are not finite. The leaf's point becomes the
incumbent at 1.0, but its duals bound it only at 1.958.

## The objectives that differ

Seed 1's model 2952 (`t2952a.mps`, `t2952b.mps`) is three columns:
minimise -C1 - 5·C2 - C3 with C3² <= 0, the rotated cone 2·C1·C3 >= C2²,
C1 semi-continuous in [1, 4] and C3 in [0, 5]. C3 = 0 forces C2 = 0, so
the optimum is -4. The two orders end at -4.0133755 and -4.0132884, with
C3 near 1.4e-6: that breaks C3² <= 0 by 2e-12, inside every tolerance,
and through the cone it lets C2 reach about 0.0027, worth 0.013 of the
objective. So on such a row a point within tolerance moves the objective
by about the square root of the tolerance, and each order's tree closes
on its own such point. In seed 9's model 7011 an SOS1 set over a rotated
cone's two heads, and in seed 10's model 8215 an integer head at 0,
leave the cone no point strictly inside in the same way. This is J27's
family: a quadratic row or cone with no point strictly inside.

## The continuous disagreements

The continuous mode has no tree, so the change does not touch it.
`coneout.c` solves a model as it is, then with every quadratic row's
finite sides moved out by 1e-4 and every cone's head raised by 1e-4
through a new column. `coneout.txt` is its reading of seed 1's first
five continuous disagreements in both orders (`c25a.mps` to
`c3431b.mps`).

In four of them a quadratic row or a cone has no point strictly inside.
In c3431, for example, the cone C1 >= ||(C7, C2)|| with C1 <= 1 and
C7 >= 1 holds only at C1 = C7 = 1 and C2 = 0. Moved out, both orders of
each of the four end optimal at objectives within 1e-7 of each other.

The fifth, c2939, has an optimum that no point reaches. Its objective
holds 3·C1 + 3·C2 with C2 >= ||(C4, C1)||, and that sum falls toward 0
only as C1 goes to minus infinity, so the infimum -12 is a limit. One
order stops at -11.99999 with a large C1, which the checker passes, and
the other ends `numerical_error`. Moving out does not change that.

## The gate

`gate.sh` is 02-427's on 7b29b7b, whose source is ee5b4c7's, against the
change.

| set | 7b29b7b | the change |
|---|---|---|
| 02-255, 8000 models | | the same lines |
| 02-326, 6000 models | | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | | the same lines but one (`cblib-mip.txt`) |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | | the same lines |
| 02-427's `warmqc.c`, both modes, seeds 1 to 8 | 02-427's `warmqc.txt` | the same lines |
| 02-425's `warmcone.c`, misocp, seeds 1 to 3 | 02-426's `warmcone.txt` | P1, P2, P3 all 0 (`warmcone.txt`) |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

The CBLIB line that moves is turbine07_lowb_aniso. Both trees stop at
the work limit with incumbent 2.2470576 and bound 1.1508001, after 1333
nodes on 7b29b7b and 1332 on the change.

The `warmcone.c` counts move: capped goes from 5, 15 and 8 on seeds 1
to 3 to 3, 14 and 14. An edit's values depend on the last answer, so
once two trees answer one edit differently, the rest of the seed draws
other edits and other models. `pairs.sh` runs 02-426's `pairs.py` on
the three seeds (`pairs.txt`). It matches edits by their text, which
leaves out the drawn values, and it lists nine pairs that change, five
of them between infeasible and optimal. `cross.sh` dumps those nine
from both runs and solves each dump with both trees (`cross.txt`, with
`dumps.patch`). Each of the five that change between infeasible and
optimal is two different models. Both trees give the same answer on
nine of those ten models. On the tenth (seed 1, model 88, edit 5, from
the change's run) 7b29b7b stops at the work limit and the change ends
optimal. The other four pairs are one model each: 7b29b7b stops it at
the work limit and the change ends it optimal. So on no model does the
change move an answer between optimal, infeasible and unbounded.
