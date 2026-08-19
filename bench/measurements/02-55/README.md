# The solver measures the violation it publishes: the best point ends the solve with bstdv = 35.34 and nothing reads it against a tolerance

Taken 2026-08-19, D146's diagnosis step 1, trajectory before values. Closed
as D147.

## The trajectory, cold against hostile, on `degen2`

One line per `settle_shifts` call plus `run()`'s OPTIMAL exit and
`classify_optimum`'s verdict; `d0viol` counts nonbasic sign violations of
`d0 = d − (cost − cost0)`, the reduced cost in the model's own cost space.

**Cold (control):** OPTIMAL at 578 iterations, 7 lends of 1e-14 scale,
`d0viol = 0` everywhere. Clean.

**Hostile (02-54's shift 1):**

| point | iters | outstanding lends | true dual violations |
|---|---|---|---|
| `run()` declares optimal | 636 | **402**, sum 3254, max 45.5 | **186**, max 35.05 |
| settle rounds 1–32 | 688–1836 | **0** — every lend repaid | **never below 100**, max 40–495, oscillating |
| published | 1836 | 0 | 171, and `bstdv = 35.34` |

The best point is picked at **round 1** with `settled_dual_violation =
35.34` and objective −1352.64; no later round beats it; the rounds run out
(`SETTLE_ROUNDS = 32` confirmed) and `take_best_if_better` publishes it.
`classify_optimum` passes trivially — no lends outstanding, and bounds are
all it interrogates. The published point's true `d0max` is 35.34, equal to
`bstdv` bit for bit, so the solver's own number and the instrument agree.

## What this locates

The defect is not that dual feasibility goes unread. **The solver computes
it every round, stores the best (35.34 — eight orders of magnitude past
any tolerance), and publishes OPTIMAL without ever comparing that number
to one.** D89 turned "the rounds oscillated, fail" into "publish the best
point instead"; on `pilot87`'s 8.37e-09 residue that was benign, and on a
hostile start it launders a 5.7% objective error into `OPTIMAL`.

## The repair shape this selects, and what to measure first

`jaos.h`'s intended contract — a hostile basis costs time, never
correctness — is honoured by a guard that already has its number: when the
settling ends and the best point's `bstdv` exceeds the dual tolerance, do
not publish it as OPTIMAL; restart once, cold, from the slack basis, and
solve the problem the way a caller without the hostile basis would have.
No new constant: `dual_tol` is the existing bar and `bstdv` the existing
number.

Measure before building (the two-sided rule): the distribution of `bstdv`
at publish across all 139 gate instances at HEAD. If every legitimate
solve ends at or under the tolerance while the hostile ones read 35+, the
guard has orders of magnitude of margin and the gate stays bit-identical
by construction; any legitimate instance above it names the margin
question before the guard can land.

## Reproducing it

`run-termination-traj.sh` and `probe-traj.c` beside this file with the
output; `src/` is read and never written, the patch lives in a throwaway
worktree.
