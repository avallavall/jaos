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

**And width 0.1 brings back one `ERROR`** — `pilot87` itself, and its message
names the cause, because D209 made these sites say which floor rejected them:

> column 975 prices at 7.74039e-10 in row 758 of the primal phase 1 on a
> freshly built factorization, against a floor of 1e-09

That floor is `PIVOT_MIN`, the **stability** floor D209 identified, not the
relative one. So at 0.1 `pilot87` does not diverge: it brings its objective
under control, runs 106444 phase-1 iterations instead of 372035, and then
stops on a pivot **1.3 times** below an absolute 1e-9 on a fresh
factorization, where there is nothing left to rebuild.

That is a different and much smaller failure than 1e+24, and it is the class
D207 and D209 have been circling from the other side. It also separates the
two questions: the width decides whether the objective stays under control,
and `PIVOT_MIN` decides whether the solve can finish.

## D212's three regressions: two of them are not the relaxation

Read from the four `delta-*.txt` records already here, no new run.

| instance | before D212 | width 0 | 0.1 | 1 | 10 |
|---|---|---|---|---|---|
| `israel` | ok | **ok** | DISAGREE | DISAGREE | DISAGREE |
| `pilot-ja` | ok | DISAGREE | DISAGREE | DISAGREE | DISAGREE |
| `pilotnov` | ok | DISAGREE | DISAGREE | DISAGREE | DISAGREE |

`pilot-ja` and `pilotnov` disagree at width 0, where there is no relaxation at
all. What loses them agreement is the **pivot preference** — pass two taking a
larger pivot than the exact minimum did. Only `israel` belongs to the
relaxation, and every width above 0 loses it.

All three fail the same way, on dual feasibility at the settled point, and the
size of the breach moves with the width without following it: `pilotnov`
breaches its bound by 4.75223 at width 0, by 0.221786 at 0.1 and at 1, and by
95.8043 at 10.

## Every verdict that moves, width against width

| pair | instances that differ |
|---|---|
| 0 vs 1 | `brandy` DIS→ok, `finnis` DIS→ok, `scrs8` DIS→ok, `d6cube` overrun→DIS, `fit1p` ok→DIS, `israel` ok→DIS |
| **0.1 vs 1** | `wood1p` **ok**→DIS, `pilot87` **ERROR**→overrun |
| 10 vs 1 | `bandm` DIS→ok, `pilot` overrun→ok |

Only two instances separate 0.1 from 1. One is a gain for 0.1 (`wood1p`), the
other is the same instance failing in a different way (`pilot87`, which fails
at both).

## Open, and what would settle it

1. **0.5 is the published value and it is not measured.** GMSW 1989 ship
   `delta_i = delta_f / 2`, and `docs/research/harris-primal.md` bounds the
   width above by `PRIMAL_TOL` — which is exactly what ships. One more run of
   `sweep-delta.sh 0.5` (with the setup adapted, because the script measures a
   working-tree diff and stage 2 is committed now).
2. Then the choice, **with the gate re-run at whichever wins**, because the
   gate reaches this code on three instances and one of them already publishes
   a different vertex (D212). Nothing here is a gate reading; all four settings
   are the forced-primal campaign only.

Nothing here changes `PIVOT_MARGIN`; that constant is D207's and was swept
separately in `bench/measurements/02-122/`.
