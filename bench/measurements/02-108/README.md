# 02-108 — D194 read a log line that is only printed on success, and phase 2 turns out to run 97 iterations in total

2026-08-26. Corrects `bench/measurements/02-106/` and D194.

## The defect in D194's instrument

`02-106/split.c` read phase 1's iteration count from the log line
`phase 1 reached a feasible point in N iterations`. **That line is printed only
when phase 1 succeeds.** When phase 1 runs and does not finish — a budget, a
refusal — no line appears, the probe left its counter at 0, and the instance
read as *phase 1 took zero iterations*. The probe then computed
`phase2 = primal - phase1 = primal - 0 = primal`, so those instances read as
pure phase-2 runs.

D194 published that as: *the 8 that run a real phase 2 are exactly the 8 whose
phase 1 is zero iterations — they arrive primal feasible, so nothing hands
over.* **Both halves are false.**

**What caught it.** 02-107's canary targeted those 8 instances to force a
phase-2 flip and got **2634 phase-1 flips and zero phase-2 flips**. A phase 1
that never ran cannot flip 2634 times.

## The corrected measurement

The count is now logged after every exit from `run_primal_phase1`, at one site
placed after the call, so no exit path can be missed. An instance that skips
phase 1 prints no line at all, which is the honest reading of "did not run"
(`split.txt`, `summary.txt`).

| | |
|---|---|
| phase 1 **skipped**, point already primal feasible | **0 of 94** |
| phase 1 ran and **finished** | 86 |
| phase 1 ran and did **not** finish | **8** |

The 8 are `d6cube`, `degen3`, `dfl001`, `maros-r7`, `pilot87`, `scrs8`,
`scsd8`, `wood1p` — the same 8, reclassified from *never entered phase 1* to
*never left it*. `wood1p` is 3820 iterations of phase 1 and 0 of phase 2, where
D194 recorded 0 and 3820.

## What that does to D194's headline, and it strengthens it

| phase-2 primal iterations | D194 | corrected |
|---|---|---|
| exactly 0 | 0 | **8** |
| exactly 1 | 80 | **80** |
| 2 to 10 | 6 | **6** |
| more than 10 | 8 | **0** |

**No instance on the standard set runs more than 10 phase-2 primal
iterations.** Across all 94 solves:

| | iterations | share |
|---|---|---|
| phase 1 | 336660 | **39.5%** |
| **phase 2** | **97** | **0.0%** |
| dual | 515522 | **60.5%** |

**97 phase-2 iterations in the whole campaign.** D194's 60.5% dual share was
right; what it got wrong is that the other 39.5% is phase 1 and essentially
none of it is the phase-2 simplex.

## A second flaw in the probe, found and bounded

The status column is read with `jaos_status_of` and the probe ignored
`jaos_solve`'s return value, so an instance whose solve returns a hard error
shows the **previous** solve's status. `pilot87` therefore prints `optimal`
when it in fact raised `column 478 prices at 0 in row 790 of the primal phase
1`, which is what `bench/primal.c` reports for it. **Exactly one instance is
affected**: the status counts are 31 / 56 / 7, matching the campaign's 55
measured plus `pilot87`. **The iteration figures above come from log lines and
are untouched by this.**

## Re-running

Writes beside itself and replaces this directory's evidence. The worktree goes
under `mktemp -d`, outside the repository.
