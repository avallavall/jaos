# 02-278 — the dual prices by Devex after a steepest-edge weight drifts

Taken on 2026-09-21 on edc7aa8 plus the change (TODO B2).

## Before

The dual simplex prices by steepest edge. At each pivot it compares the
pivot row's carried weight with the exact one, and when the two differ by
more than `DSE_DRIFT` it sets every weight to 1.0. Once the weights are
exact, 1.0 is far from them, so the next comparison fails too.
`restarts-before.txt` counts the restarts per instance on edc7aa8
(`jaos solve --log summary`): pilot87 34494 of 37362 iterations (92%),
d2q06c 26602 of 27935 (95%), pilot 17926 of 20304 (88%), greenbeb 81%,
perold 69%, pilot-ja 63%, agg3 59%, fffff800 46%, pilotnov 27%, maros-r7
7%, wood1p 4%, and none on the other 83. On those instances the pricing is
largest infeasibility with extra steps.

## The change

After the weights are exact, a drift hands the pricing to dual Devex for
the rest of the solve. The reference framework is the basis of that
moment, so every weight starts at 1.0 and is exact for it. A row's weight
is its tableau row's squared norm over the framework; the pivot row's is
read off the pivot row the ratio test already computed, and the other rows
grow by the max rule. When the pivot row's carried and read weights differ
by more than `DUAL_DEVEX_RESET`, the framework is reset. Devex solves no
second column per pivot, which the steepest-edge update does.

Two parts were needed for the gates to pass:

- **The published duals are refined once when the solve ended under
  Devex.** With plain duals (`netlib-plain-duals.txt`, the reset at 3.0)
  three answers lost accuracy: pilot's suboptimality bound went from
  1.28e-7 to 1.18e-6, past the gate's 1e-6 ceiling, perold's from 2.6e-14
  to 5.8e-12 and wood1p's from 1.1e-9 to 1.7e-8. The published duals came
  from one BTRAN with no refinement. One refinement step with the residual
  summed in compensated arithmetic takes them to 1.0e-13, 3.5e-14 and
  9.9e-15. Refining every solve instead cost 0.4% of the work over the set
  and improved no steepest-edge answer by 2x, so only a Devex solve pays.
- **Node solves of the MIP tree keep the old restart.** With Devex there
  too (`miplib-node-devex.txt`), the set read 0.8913x in work (rgn 0.360x,
  p0201 0.384x, l152lav 0.422x) but stein27 read 2.34x, 10778 to 23493
  iterations, past the gate's 2.0x. `bench/refusals.txt` has it as
  mip-node-devex.

## The reset, swept

Work over the standard 94 against the baseline, every run with the refined
duals and no Devex in the tree (`sweep-r*.txt`, `netlib.txt` is 10):

| `DUAL_DEVEX_RESET` | work | iterations | note |
|---|---|---|---|
| 2 | 0.9584x | 0.9744x | |
| 3 | 0.9472x | 0.9632x | the primal's `DEVEX_RESET` |
| 5 | 0.9471x | 0.9592x | pilot 1.97x |
| **10** | **0.9276x** | **0.9439x** | |
| 30 | 0.9279x | 0.9447x | |
| 100 | 0.9479x | 0.9623x | pilot ends `NUMERICAL_ERROR` |
| 1000 | 0.9352x | 0.9510x | |
| never | 0.9731x | 0.9938x | |

## The gates at 10

| set | gate | work | iterations |
|---|---|---|---|
| netlib, 94 | PASS, 0 regressed | 0.9276x | 0.9439x |
| netlib-infeas, 29 | PASS, 0 regressed | 0.9869x | 0.9844x |
| netlib-kennington, 16 | PASS, 0 regressed | 0.9952x | 0.9937x |
| miplib, 24 | PASS, 0 regressed | 1.0000x | 1.0000x |

No instance passes 2x. The largest gains: pilot87 0.223x (37362 to 11745
iterations), pilot 0.288x, d2q06c 0.296x, perold 0.440x, greenbeb 0.464x,
agg3 0.586x, pilot-ja 0.589x, fffff800 0.701x, maros-r7 0.848x. The one
loss past 1.01x is wood1p at 1.181x (550 to 573 iterations). On the
infeasible set gosh reads 0.633x and bgprtr 1.078x; on Kennington osa-60
reads 0.925x.
