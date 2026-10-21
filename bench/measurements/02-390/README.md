# 02-390 — a cone member fixed at 0 leaves its cone

Taken on 2026-10-21 on the tree of bc216cc. `read.sh` takes the readings.

## The models J23 listed

| model | on HEAD | with the change |
|---|---|---|
| 02-255 seed 13, model 456 | three solves stop after 208 iterations | optimal in 8 iterations |
| 02-255 seed 17, model 245 | an optimum refused, duals off by 8.6e-6 | optimal |
| 02-326 seed 24, model 918 | a walk stalls next to a refused infeasibility certificate | infeasible, with no certificate published |
| 02-326 seed 22, model 162 | the same as model 918 | unchanged |
| 02-326 seed 25, model 357 | the same as model 918 | unchanged |

## Model 456

C7 is fixed at 0 and is the third member of the rotated cone K2 over C4,
C5 and C7. The walk kept C7 in K2's third row and in an equality row of
its own. Its primal and dual residuals fell to 1e-15 while the gap stayed
between 0.05 and 0.12. The point repeated every four iterations while
`tau` and `kappa` shrank together, and C5 jumped between 0.006 and 0.6
(its optimum is 0.293). With K2 over C4 and C5 alone, the walk solves the
model in 8 iterations at -13.745760975730368. Freeing C7, or replacing K2
by the bounds C4 >= 0 and C5 >= 0, gives the same optimum.

Model 245 has the same shape: C4 is fixed at 0 in the cone K1 over C1, C4
and C5. Model 918's C5 is fixed at 0 in the rotated cone K1.

## The change

`conic_solve` in `src/conic.c` gives no cone row to a member whose two
bounds are 0, unless it is a head. The cone's dual for that member is
reported as 0, and the column keeps its equality row. Dropping such a
member leaves the same set. `tests/data/cone_fixed_member.mps` is model
456.

## Models 162 and 357

Both are infeasible through a quadratic row over a column fixed at 3. In
model 162 the row R2 holds only C2, and C2 = 3 breaks it (QCMATRIX has no
factor 1/2). In model 357, C6 = 3 breaks R3 for every C4. For a column
that a quadratic row curves, the checker takes the term's maximum over all
values of the column, even when the column is fixed. So no certificate it
takes exists for either model.
Model 918 is the same case through R4, whose cross terms put it in J22.

`fold.patch` makes the walk take every fixed column as a constant in its
rows and gives the column its reduced cost back from the row duals after
the walk. Its readings are in `fold/`, on the same seeds as below without
CBLIB:

| set | HEAD unsure, failed | the fold | work against HEAD |
|---|---|---|---|
| 02-255, 8000 models | 4, 1 | 3, 0 | -3.1% |
| 02-326, 6000 models | 5, 0 | 2, 0 | -3.1% |

The fold ends models 162, 357, 240 (02-326 seed 2) and 451 (02-255 seed
17) infeasible, and model 69's tree (02-255 seed 16) solves. It also turns
three solves that ended infeasible into `numerical_error`: model 308
(02-255 seed 3), model 748 (seed 16), and another solve of model 162. All
seven are infeasible through a quadratic row, and no certificate passes the
checker in any of them. The answer therefore depends on whether the walk
reaches the infeasibility test at 1e-8 or stalls first. On model 748 the
folded walk reaches 1.14e-8 at iteration 7, and its next direction is not
finite; HEAD's walk reaches the test at iteration 8. The fold is refused
(`conic-fold-fixed-columns`).

## Model 620

02-326 seed 2's model 620 ends unsure on HEAD and with the change: an
optimum refused, duals off by 0.002. C2 costs -0.134 and C8 costs 0.189,
and R1 ties them, so a unit of C8 buys 1.375 units of C2 at a net cost of
0.0039. The optimum is far out: C2 = 28434, C8 = 20674, C7 = -241.6. The
walk ends with R4 0.069 inside its side, which is 1.7e-6 of its terms, and
with R4's dual at -0.002, so the checker reads R4 as slack. The settle's
wide pass puts R4 on its side and the duals then pass, but its refit holds
the point and leaves stationarity at 3.3e-8. Times C2 and C8, that leaves
the objective gap at 5.2e-6. A Newton step on the same five active
constraints raises the KKT residual from 3.3e-8 to 1.1e-5. This is J21's
case with small duals.

## The readings

| set | HEAD | the change |
|---|---|---|
| 02-253, seeds 1 to 3 | 0 failed | the same answers, work -0.001% |
| 02-255, 8000 models | 4 unsure, 1 failed | 2 unsure (models 533 and 451), 1 failed (model 69), work -0.24% |
| 02-326, 6000 models | 5 unsure | 4 unsure, work -0.43% |
| QPLIB's 13 continuous QCQPs | | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | 30 optimal | the same lines |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | | the same lines |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

On 02-255, model 533 (seed 4) is J21's, and models 451 (seed 17) and 69
(seed 16) are J22's. On 02-326, models 240 (seed 2), 162 and 357 are
J22's, and model 620 is J21's.
