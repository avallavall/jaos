# D97's over-tightening, derived: the window takes its scale from a bound tightening invented

The derivation D97 named as its first reopen precondition, taken
2026-08-17. D114 is the decision it feeds. Method: `git archive 7c7375c` —
feat(02-04), the minimal failing design, "a bound that is reasoned with
rather than published" — built with the diagnostic flags, instrumented
with throwaway prints (`patch1.py`, `patch2.py`), never touching the
repository.

## Reproduction

At `7c7375c`: `pilot` and `pilot87` INFEASIBLE with **0 iterations** —
presolve itself declares them — `agg` and `maros` solve but fail the
checker, `afiro` clean. Matches D97's design table. Both refusals fire at
the empty-row site: `pilot` row 1095 goes empty holding -1.15, `pilot87`
row 1324 holding -0.92 (D97's worked case, reproduced to the digit).

## The chain, every link with its number (`pilot-exhibit.txt`)

1. **Tightening materializes an astronomical bound.** Column 3556's
   original box is `[0, inf)`. The implied-bound pass gives it
   `cur = [0, 6.687e10]` — a valid implication, but now a *finite number
   with a magnitude*.
2. **The magnitude poisons the window.** Row 1094 (`rl = ru = 0`) computes
   its activity range over the tightened boxes: `min_act = -9.42e11`
   (mostly `-14.08 × 6.687e10`), `max_act = 5.8644`. The activity pass
   uses one window for every verdict on the row:
   `rtol = PRESOLVE_TIGHTEN_EPS × range magnitude = 1e-9 × 9.42e11 =
   941.58`.
3. **Windowed forcing certifies real slack as binding.** `force_lo` asks
   `max_act <= rl + rtol`, i.e. `5.8644 <= 0 + 941.58`. The row has 5.86
   of genuine slack and is not forcing, but the test fires and pins every
   live column at its extreme — column 3554 at 1.15, its own upper bound.
4. **The next equality exposes it.** Row 1095 needs column 3554 at 0. It
   goes empty holding -1.15, and presolve correctly declares the model
   those boxes describe infeasible. The boxes were wrong.

## Why no epsilon could fix it, and where the repair already lives

The window is a judgement constant times the *activity's* magnitude, but
the claim it certifies in the forcing test is about the *row bound's*
scale. On this row any epsilon down to ~6e-12 still yields a window above
5.86; and chains with larger materialized bounds survive smaller epsilons,
which is why D97's nine-setting sweep moved nothing. The infeasibility
test on the same row may legitimately use the activity-scaled window (the
compared quantity is that sum); the forcing test may not — and the
forcing family that ships today already takes its window from
`ps_bound_scale(cur_rl, cur_ru)`, the row bounds themselves, which on
`rl = ru = 0` is a window of ~1e-15. That is why the shipped family is
green while the 02-04 design refused feasible models.

The same lesson was later learned independently for the emptied-row test
(02-09: "a judgement constant 5.6e5 times wider"). This derivation says
the 02-04 family died of that constant's scale choice, compounded by
tightening feeding materialized magnitudes into it — not of the implied
bounds' values, and not of anything an epsilon sweep could reach.

## What a future design must therefore carry

- Forcing and pinning verdicts windowed by the row bound's own scale (as
  the shipping family already does), never by the activity range's.
- A pin only where attainment is within the *arithmetic's* error of the
  sum that computed it (`eps × traffic`, the D103 form), never within a
  judgement constant.
- Materialized implied bounds kept out of scale computations, or the
  activity range recomputed against original boxes for verdict purposes.
- D97's second precondition, the dual postsolve for an imposed bound,
  is untouched by this and still open.
