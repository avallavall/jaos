# 02-113 — two of phase 1's four refusals now refresh before refusing, and neither is reached on the standard set

2026-08-26. `TODO.md` §0's remainder list: phase 1 is not interruptible, and
both phase-1 refusals conclude on carried numbers.

## What landed

**Phase 1 has four refusals and two of them were verdicts read off carried
numbers.** `pivot()` steps `d` in place every iteration and the factorization
is patched rather than rebuilt, so:

- `q < 0` — "nothing improves the phase-1 objective"
- `r < 0` — "no declared bound stops this column"

are both statements about the model made from numbers that may have drifted.
D20 refuses exactly that, and phase 2's optimality test has gated on
`!s->verified` since it landed. Both now refresh once and price again; only the
second reading refuses.

**The other two are deliberately left alone.** The iteration guard is a defect
guard rather than a verdict. The tiny-pivot refusal already rebuilds and
retries, which is why its message says "on a freshly built factorization".

**Phase 1 also gains the progress callback**, which phase 2 and the dual both
have. A caller could neither watch nor stop the part of a forced-primal solve
that spends 39.5% of its iterations (D197). `infeas_best` now carries phase 1's
own total; `run_primal` used to set it to 0.0 at entry, which claimed the point
was feasible through the whole of the part that is not, and it becomes 0.0 at
the hand-over where that is true.

## The measurement: the gate is correct and unfired

**The primal campaign came back byte-identical** — `diff` on the whole record,
not the summary line. That rules out "fired and agreed", which would have cost
an extra refresh and moved the units.

Counting confirms it (`refusal-census.txt`), over all 94:

| refusal | reached |
|---|---|
| `q-retry` (the new gate) | **0** |
| `q-refuse` | **0** |
| `r-retry` (the new gate) | **0** |
| `r-refuse` | **0** |
| `tinypivot` | **1** |
| `tinypivot-retry` | **13** |

**The positive control is inside the table.** `tinypivot` is not gated and
`pilot87` ends there — it is the campaign's one ERROR — so a probe that could
not see a refusal would show zero in that row too. It does not.

**A fact nobody had counted**: phase 1's tiny-pivot retry fires **13 times
across 5 instances** — `dfl001` 7, `d6cube` 2, `greenbeb` 2, `pilot87` 1,
`tuff` 1. That is `s->n_stability`'s phase-1 share.

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical. `make configs` exits 0 on all five.

## The test, and what actually caught the fault

`test_a_watcher_can_stop_the_primal_phase_1` stops on the first callback and
requires `INTERRUPTED` **without** the line phase 1 logs when it finishes,
because the status alone cannot tell a stop inside phase 1 from one just after
it.

**Validated by injecting the fault** (`validate-test.sh`, `validate-test.txt`):
phase 1's callback block guarded on `!s->in_phase1`, which restores the
pre-change behaviour exactly and compiles cleanly where `false` would be dead
code under `-Werror`. Exactly one FAIL, and it is this test.

**It failed on the status assertion, not the log one** — `Expected 7 Was 1`.
Without phase 1's callback the solve reaches OPTIMAL, because on a two-row
model phase 2 takes one iteration and the dual finishes before another
callback beat. The log assertion is the belt-and-braces for a model where it
would not, and it is written down this way rather than claimed to be the half
that fired.

## The verdict

**The D20 gate stays, unfired.** It costs nothing when it does not fire and it
removes a class of wrong refusal that no campaign here can currently produce —
the same standing `jm_bland_pick` has had since D26.

**Reopen conditions**: any instance reaching `q-refuse` or `r-refuse`, and a
starting basis that is not the slack basis, since a crossover's basis reaches
phase 1 with drift the cold start never has.
