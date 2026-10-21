# 02-410 — models on the edge of feasibility, under every LP algorithm

Taken on 2026-10-21 on the tree of c1ecd23. `read.sh` takes the reading.
No code changed.

## The harness

02-409's bug lived on a model moved exactly onto the edge of
feasibility. `edge.c` makes such models on purpose: for each of 02-235's
LPs the dual simplex finds infeasible, it adds `jaos_feasrelax`'s moves
under each of the three scopes to the bounds. What is left is feasible
only on its edge, often at a single point, and its sides carry the
rounding of the moves. Each such model is solved by the dual, the primal,
the barrier, PDLP and the concurrent solve, and each must end optimal
with a point and duals the checker passes at 1e-7.

## The reading

Seeds 1 to 3, 3000 models each, give 11768 edge models
(`edge-new.txt`). On the tree, no algorithm breaks either property. On
c1ecd23's parent (`edge-pre.txt`) the one model 02-409 found, seed 2's
model 1156 under the rows' scope, ends infeasible under all five
algorithms, since they share presolve; every other edge model passes.
