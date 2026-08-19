# Every legitimate settling exit publishes a dual violation of exactly zero, and the hostile one reads 35.34

Taken 2026-08-19, the two-sided measurement D147 required before its guard.
The decision it feeds is D148 (the guard and cold restart); this record is
the margin.

## The measurement

One line per `take_best_if_better` call — every settling exit passes
through it — across the three gate sets at HEAD, carrying both candidates
the publish chooses between and the dual tolerance:

| set | settling exits | published dviol ≤ `dual_tol` | worst published |
|---|---|---|---|
| netlib | 188 | **188** | **0.000e+00** |
| netlib-infeas | 0 — infeasible solves exit elsewhere | — | — |
| Kennington | 32 | **32** | **0.000e+00** |

`settled_dual_violation` counts only the excess beyond `dual_tol` per
variable, so exactly zero means every variable within tolerance. The
hostile `degen2` solve reads **35.34** at the same point (02-55).

## What it decides

The two populations are perfectly separated with no margin question left:
the guard `settled_dual_violation(s) != 0.0` at the settling's end uses no
new constant — the tolerance is already inside the function — and by this
measurement it never fires on any gate solve, so the three sets stay
bit-identical under it by construction. What remains is the repair itself
and its own judgement, which is D148's.

## Reproducing it

`run-bstdv-distribution.sh`, beside this file with its output. `src/` is
read and never written; the patch lives in a throwaway worktree.
