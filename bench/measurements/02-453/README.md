# 02-453 — walks that break at the cones' boundary, parked

Taken on 2026-11-15 on the tree of bbddb40, for TODO row J27. No code
changed. The models are 02-452's dumps: `why.txt` is, for each of the 20
pairs whose pieces all have room, the order that ends `numerical_error`
on bbddb40, with how its walk stops and the error it ends with
(`read.sh` of 02-452 makes the dumps; the line comes from
`jaos solve FILE --log detail`). `trace.patch` prints the NT scaling and
the Jordan division of every cone at every iteration;
`lambda-det.patch` and `corrector-fallback.patch` are the two remedies
tried. Each patch applies to bbddb40's `src/conic.c`.

## Two kinds

Of the 20 pairs, one ends the same in both orders on bbddb40; 19 do
not, and they split in two:

| kind | orders | how the walk stops |
|---|---|---|
| a limit no point reaches | 10 | the step falls to 0 at iteration 40 to 48 with a direction that is not finite, the walk ends with no answer, and the feasibility walk finds no ray. The other order stops at a point the checker passes, on seven of them just short of a round value: -11.99998935, -7.99999694, 14.99998351, -23.99998774, 14.99998227, 14.99999359, -10.99999002 |
| a walk that breaks at the boundary | 9 | the walk converges, then its step falls below 1e-100 at iteration 12 to 15 (4), it stops making progress (2), or it ends at its tolerance (2); the point it stands on is refused for duals that miss by 1.1e-7 to 3e-6 of their terms. The ninth (`s5/t17371b`) breaks at iteration 12 with its rows 2.48 off |

The first kind is 02-431's `c2939`: no point attains the optimum, and
which order stops early enough to stand is luck.

## The second kind

`s6/t11013a.mps` is a maximisation whose optimum is 12, with C7 at its
upper bound 3 and every cone column but C7 at 0. The walk reaches a gap
of 1.5e-8 at iteration 13, and the step then falls to 1.2e-180.

With `trace.patch`, the KKT solve is not what fails. On iteration 13
its two solves stay below 4.4, the step's `dx` and `dz` below 3e-1, and
the tau equation's denominator is -5.5e-5, but the step direction `ds`
reaches 3.4e+157 on cone 1 and 5.3e+177 on cone 2. Cone 3 (four
columns) has its slack and its dual both near the boundary and both
away from 0: s0 = 0.665 and
z0 = 0.356 with det s = 7.0e-11 and det z = 2.0e-12, so the pair tends
to opposite rays of the boundary. Its NT scaling has |w| = 1.25e5, and
the Jordan division of the corrector's right-hand side by λ gives
x0 = -3.1e6 against λ0 = 4.5e-6. `ds` is recovered as `-W (W dz + g)`,
through W², and a scaling that wide multiplies an error in `dz` by about
|w|², 1.6e10.

`s3/t5565a`, `s9/t13034b` and `s9/t4559b` show the same: a direction of
1e+139 to 1e+212 on a cone whose point lies on its boundary 1.8 to 2.2
from its tip.

## What was tried

- **The determinant of λ taken as `sqrt(det s · det z)`**, which the NT
  scaling gives exactly, in place of recomputing it from λ
  (`lambda-det.patch`). `s9/t13034b` and `s9/t4559b` end optimal at -24
  and -6.2e-13, as their other orders do; the other six still end
  `numerical_error`, two with the step below 1e-147.
- **The corrector retried without Mehrotra's second-order term when its
  step falls below `CONIC_STALL_STEP`** (`corrector-fallback.patch`).
  On the four whose step collapsed it still falls, to 2e-73 to 5e-50,
  and none of the eight, nor three of the first kind, changes status.

Neither was read beyond these 8 models, nor on any gate.

## Why it is parked

The remedy left is to recover `ds` without going through W², from the
linearised primal rows, or a scaling that stays accurate when both
points of a cone near its boundary. Either changes every iterate of
every conic walk, so every conic gate moves and would have to be read
and re-based, for about one model in 20,000 of 02-431's reading. The
maintainer chose on 2026-11-15 to park it and take TODO row J29 first.
