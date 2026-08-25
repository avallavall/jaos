# 02-109 — the iteration cap really is shared, and phase 1 spends at most 1.68% of it

2026-08-26. `TODO.md` §0, the last of D191's answer-changing findings.

## The question

`run_primal_phase1`, `run_primal` and `run` each compute
`ITER_SANITY_FACTOR * (nrow + ncol + 1)` and each test the **cumulative**
`s->iters` against it. So phase 2's real allowance is the cap minus phase 1's
spend, and the dual's settling re-entry is third in the queue behind both.
D191 put it as: a phase 1 that uses most of the cap makes phase 2 trip the
guard and report phase 1's iterations as its own.

D195 made this worth measuring rather than assuming. Phase 1 is 39.5% of every
iteration the campaign runs and the dual is 60.5%, so the queue is real.

## The measurement

One log line at each of the three sites, printed where the cap is computed so
it carries that method's reading at its own start. All 94 with
`cfg.force_primal` (`cap-census.txt`).

| | |
|---|---|
| instances that start phase 2 with the cap already partly spent | **86 of 94** |
| **largest share of the cap spent before phase 2 starts** | **1.68%**, on `pilot-ja` |
| next largest | `25fv47` 1.2% |
| typical | 0.1% to 0.8% |

`ITER_SANITY_FACTOR` is **200** times the model's size. Phase 1 uses at most
**three** of those 200.

The 8 instances that never leave phase 1 (D195) show `-1` for phase 2 and the
dual, which is the probe saying those methods did not run rather than that they
spent nothing.

## The verdict

**The cap sharing is left as it is, and the margin is the reason.** Rebasing it
per phase would change no outcome on any of the 94 and would add a second
constant with no measurement behind it, which is what this project's rules
forbid.

**The conditions that end that are written beside the cap in the source**:
`ITER_SANITY_FACTOR` dropping below about 60, or a phase 1 given a harder job
than reaching feasibility from the slack basis — a crossover's basis, for
instance, which is the motivating case for this whole feature.

## What did change

**The guard's message.** Phase 2's said `after N primal iterations` using the
cumulative count, which after a phase 1 is phase 1's number wearing phase 2's
label. Both primal messages now carry the phase's own count, the position in
the solve, and the cap itself. `phase2_entered` is the only new state and it is
read in one error string.

This is a reporting fix and not a behaviour one, and the campaign says so: all
three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file in
`bench/results/` byte-identical to the committed record.

**Two sessions in a row lost time to a count that meant something other than
its label** — D194's phase-1 figure, and this message. That is why a message
nothing on this set can print was worth changing.

## A note on the probe

It checks `jaos_solve`'s return value, which D194's did not, and marks the one
instance whose solve raises a hard error rather than printing a stale status:
`pilot87`.

## Re-running

Writes beside itself and replaces this directory's evidence. The worktree goes
under `mktemp -d`, outside the repository, because `make clean` is `rm -rf
build`.
