# What d2q06c's 2.2163x at margin zero is made of

The measurement `TODO.md` §1b named as its blocker, taken 2026-08-17. It
does not close §1b; it removes the "unexplained" from the number that
refused margin zero. Method and calibration discipline are 02-14's.

## The record split, from `02-12/sweep/` — no new run needed

| | margin 8 | margin 0 | ratio |
|---|---|---|---|
| `d2q06c` reduced model | 2098/4880/32044 | 2094/4876/32034 | **4 more rows** of 2171 |
| `d2q06c` iterations | 6740 | 11812 | **1.7525x** |
| `d2q06c` work | 428597453 | 949907413 | 2.2163x |
| `d2q06c` work per iteration | | | 1.2646x |
| `bore3d` reduced model | 136/155/688 | 84/103/544 | **52 more rows** of 233 |
| `bore3d` iterations | 134 | 77 | 0.5746x |
| `bore3d` work | 360244 | 90934 | 0.2524x |

The two poles of the sweep are not symmetric. `bore3d`'s win is structural:
margin zero removes 38% more of the model. `d2q06c`'s loss follows a 0.2%
structural change — four rows — and is 75% more iterations. The sweep's
cost verdict against margin zero rides on a trajectory flip.

## The profile — same discipline as 02-14

`run-callgrind.sh`: two diagnostic builds of the same tree (`-O2`, no LTO),
one at the shipping margin 8, one at
`-DJAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0`. Each reproduced its 02-12
sweep record exactly before profiling (6740/428597453 and 11812/949907413).
Flat tables in `profiles/`.

Program totals: 22.88e9 → 46.11e9 instructions, **2.015x** against 1.7525x
iterations, so **1.15x per iteration** in instructions (1.2646x in work
units — the billed kernels grew more than the instruction stream, as on
`scfxm3` in 02-14).

Per iteration, margin 0 against margin 8:

| function | per-iteration ratio |
|---|---|
| `jm_harris_pick` | **1.42x** |
| `jm_lu_factor` | 1.29x |
| `admit_candidate` | 1.24x |
| `run` | 1.22x |
| `jm_lu_ftran_sparse` | 1.21x |
| `jm_lu_update` | 0.97x |
| `jm_dse_update` | **0.45x** |

## The reading

The 2.2163x is the D108 `greenbeb` class: a small exact structural change,
a different dual path, most of the cost in the iteration count. The
per-iteration drift adds a quarter on top, in the ratio test and in denser
factors.

`jm_dse_update` is the pointer. Its total work FALLS to 0.79x while
iterations rise 1.75x: the extra iterations are running with degraded
pricing, consistent with the weight-restart mechanism D63 documented
(weights discarded, pricing by largest infeasibility, longer Harris scans —
`jm_harris_pick` at 1.42x per iteration agrees). This is an instruction
profile, not a restart counter; if certainty is ever needed, count the
restarts on both sides. It is not needed to unblock §1b, because the
question there was whether 2.2163x hides a correctness or relaxation
defect, and it does not: the sweep already read 94 `objective ok` at every
setting, and the cost is now attributed to trajectory, not to anything the
wider margin admitted incorrectly.

## What this leaves for §1b

The floor question stands on its own now. Removing `max(1, scale)` is not
margin zero: it takes only the cases where the comparison carries no error
to protect against. Whether those include `d2q06c`'s four rows is exactly
what a floor-less counter would say before any build, and that counter plus
its sweep is the remaining §1b work. It is a tolerance change, so
`fp-numerics` and the D106 sweep discipline apply to it.
