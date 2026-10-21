# 02-404 — quadratic models edited between solves

Taken on 2026-10-21 on the tree of d768080. `read.sh` takes the reading.
No code changed.

## The generator

`warmqp.c` is 02-403's `warmmod.c` on convex quadratic models: the same
LPs, minimising, with a diagonal curvature on each column (0 with
probability one third, else 1 to 4). The sense flip gives way to a
curvature edit, and a new column gets its own curvature. The barrier keeps
no basis, so each solve after an edit starts from nothing; the reading
asks whether anything the model kept from the last solve spoils the next
one. Each answer is held against a solve of a copy of the edited model by
the same properties as 02-403's.

## The reading

The QP, and the MIQP with each column integer with probability one half.
Seeds 1 to 3, 1000 models each, eight edits a model, so 24000 edits a mode
(`warm.txt`). No answer breaks a property. 44% to 49% of the edited models
are optimal, the rest mostly infeasible.
