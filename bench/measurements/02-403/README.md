# 02-403 — a model edited after a solve, solved again warm

Taken on 2026-10-21 on the tree of 0a701f1. `read.sh` takes the reading.
No code changed.

## The generator

No reading edited generated models between solves. `warmmod.c` takes
02-235's models, solves each, then edits it eight times and solves it
again after each edit, warm from the basis the model keeps. Each edit is
one of nine: a cost, a column's bounds, a row's sides, a coefficient (0
removes it), a new row, a new column, a deleted row, a deleted column, the
objective sense. After an optimal solve, new bounds and sides are drawn
near the point, so that about half the edited models stay feasible. Each
warm answer is held against a cold solve of a copy of the edited model:
the same status; an optimum within 1e-6 whose point and duals the checker
passes (integral and primal feasible for a MIP); a certificate or a ray
the checker takes for an LP.

## The reading

Three arms: the LP solved warm by the dual simplex, the LP solved warm by
the primal simplex, and the MIP (each column integer with probability one
half) solved by the dual. Seeds 1 to 3, 1000 models each, so 24000 edits
an arm (`warm.txt`). No answer breaks a property in any arm. 42% to 46%
of the edited models are optimal, 51% to 55% infeasible, the rest
unbounded.
