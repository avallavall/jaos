# 02-127 — stage 2's width, swept

`sweep-delta.sh` measures the working tree with `jm_harris_pick`'s widening
tolerance read from the environment as a multiple of `primal_tol`, so one
binary serves every setting (D154's trap). `0` is pass two alone: the largest
pivot among **exact** ties, no relaxation. `1` is what D212 shipped. D211's
two counters ride along, per instance.

| width | ok | DISAGREE | overrun | ERROR | `pilot87` phase-1 iters | its worst relative rise |
|---|---|---|---|---|---|---|
| 0 | 59 | 30 | 5 | 0 | 457002 | 7.2291e+11 |
| **0.1** | **61** | **29** | **3** | **1** | **106485** | **1351.31** |
| 1 (shipped) | 60 | 30 | 4 | 0 | 372035 | 8.33038e+13 |
| 10 | 58 | 31 | 5 | 0 | 191834 | 4.61607e+11 |

**Most of the gain is the pivot preference, not the relaxation.** At width 0,
with no relaxation at all, 59 of 94 already agree against 56 before the
change. The relaxation is worth at most two more.

**Width 0.1 is where `pilot87`'s divergence nearly stops.** Its worst
relative rise of the phase-1 objective falls from 8.33e+13 to **1351**, ten
orders of magnitude, on 106485 phase-1 iterations against 372035. Nothing
else measured has moved that number. D211 named it as the reading that says
whether the tiny pivots were the whole story.

**And width 0.1 brings back one `ERROR`**, a solve that declares itself
defective. Which instance, and whether it is a new defect or `pilot87`'s old
refusal returning, is not measured here.

## Open, and what would settle it

1. Name the instance that `ERROR`s at 0.1 and read its message.
2. The three regressions D212 carries — `israel`, `pilot-ja`, `pilotnov`,
   `ok` → DISAGREE, each on the settled point failing dual feasibility — and
   whether the width moves them.
3. Then the choice between 1 and 0.1 on both sides, with the gate re-run at
   whichever wins, because the gate reaches this code on three instances and
   one of them already publishes a different vertex (D212).

Nothing here changes `PIVOT_MARGIN`; that constant is D207's and was swept
separately in `bench/measurements/02-122/`.
