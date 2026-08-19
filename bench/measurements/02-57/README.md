# The certificate guard lands: 0 wrong of 80 where HEAD published 26, and the gate is bit-identical

Taken 2026-08-19. The repair D147 shaped and 02-56 gave its margin. Closed
as D148; the change is in the same commit as this record.

## The change

`jm_dual_simplex`'s driver settles once more after `reenter_after_settling`
returns and reads `settled_dual_violation` before believing the point. An
uncertified point from a warm start is thrown away whole and the solve
restarts once, cold, from the slack basis, with the work accumulator and
the clock origin carried; an uncertified cold start publishes
`JAOS_SOLVE_NUMERICAL_ERROR`, and `jm_postsolve_expand`'s interrupted
branch now matches `publish()`'s whitelist so no warm memory is offered for
that outcome on the reduced path either. No new constant anywhere: the
tolerance lives inside `settled_dual_violation` and the zero it returns is
a thresholded excess, not a float equality.

## The judgement

- `make test`, `make sanitize`, `-DJAOS_NO_PRESOLVE` variant: green.
- `numerics-reviewer` (fourth delivery this run): one HIGH and five lower
  findings, all dispositioned before the campaigns. The HIGH — a restore
  exit whose refresh fired `repair_singular_basis` re-lends costs and the
  guard would read the evidence the lend arranged — is why the driver
  settles before reading; the reviewer's other fixes are the
  `NUMERICAL_ERROR` test in the expand branch, the `rowc` leak on the
  early return, and the abandoned attempt's counters in the restart log.
  Carried, low: `m->err` can hold a first-attempt message behind a final
  OPTIMAL, a pre-existing class with a new route.
- **The 02-54 probe on the candidate: 80 trials, 0 wrong, 0 refused, 0
  errors** (`hostile-after.raw`; HEAD read 26 + 5). Run twice, before and
  after the review fixes, same result.
- **The gate: 94 + 29 + 16 instances bit-identical** to the committed
  records — which also settles the reviewer's finding 4 empirically: no
  gate solve reaches the guard with anything to say.

## What this restores and unlocks

`jaos.h`'s promise — a hostile basis costs time and never correctness — is
true again and its comment says so with the guard named. D145's refusals
row's reopen condition is met: the warm count-repair candidate
(`bench/measurements/02-53/warm-count-repair-candidate.diff`) is live
again, with the warm campaigns as its judge.

## Reproducing it

`bench/measurements/02-54/run-hostile.sh` at this commit reads 0 of 80;
at the parent it reads 26 wrong and 5 refused.
