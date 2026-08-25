# 02-105 — `refresh` lends a cost too, 30 times inside phase 1, and the repair trades one instance for two

2026-08-25. `TODO.md` §0, the second of D191's answer-changing findings.

## The question

D191 left it in these words: `refresh`'s repair sweep is a third unguarded
lending path. `repaired || shift_pending` runs `shift_to_feasible` over every
variable, and neither `run_primal` nor the sweep itself reads `in_phase1`,
which is the flag the other two lending sites check before they lend.

Two things had to be established in order. **Does the path execute at all?** A
path that never runs needs a different entry from one that runs and does
nothing. **And if it runs, does the guard pay?**

## The census — it runs, and not rarely

`run-sweep-census.sh` builds a worktree at the parent, applies `patch.py` (one
counting log line inside the sweep, naming `in_phase1` and how many costs
actually moved), and solves all 94 with `cfg.force_primal`.

**30 sweeps fire inside phase 1, on 11 instances. 8 fire outside it, on 2.**
Only the instances with at least one sweep are listed (`sweep-census.txt`):

| instance | phase-1 sweeps | costs shifted |
|---|---|---|
| `pilot` | 3 | 3990 |
| `dfl001` | 2 | 3560 |
| `pilot87` | 2 | 1924 |
| `pilot-ja` | 3 | 1450 |
| `greenbeb` | 1 | 1177 |
| `tuff` | 5 | 549 |
| `greenbea` | 1 | 396 |
| `pilot4` | 1 | 252 |
| `d2q06c` | 1 | 103 |
| `pilotnov` | 2 | 29 |
| `perold`, `stair`, `wood1p` | 1, 1, 7 | **0** |

The three shifting nothing are the census's own control: the sweep ran and
found every reduced cost already on the feasible side.

Outside phase 1: `scsd1` 3 sweeps shifting 334, `scsd6` 5 shifting 1233. Phase
2 is deliberately not guarded (D191), so those are left alone.

## The repair, and what it cost

One condition: the sweep does not run while `in_phase1`. `shift_pending` is
left standing rather than cleared in that case, so a warm start's owed sweep is
not dropped — `run_primal` already clears it before phase 1 for its own reason,
so today that only matters if that stops being true.

**The gate saw nothing.** All three sets `gate: PASS`, `0 regressed, 0
improved, 0 new`, every file in `bench/results/` byte-identical to the
committed record. `in_phase1` is set only inside `run_primal_phase1`, so the
dual cannot reach the changed line.

**The primal campaign, compared per instance against the parent** — the
campaign's own geometric mean is over its measured set, and that set grew from
54 to 55, so the two means are not comparable and the parent was run
(`primal-parent.txt`, `compare.txt`):

- **53 instances are `ok` on both sides. 52 have bit-identical primal work
  units.** The only one that moved is `pilot`, at **0.9673**.
- **30 are `DISAGREE` on both sides, and 4 moved**: `greenbeb` 1.0596,
  `tuff` 0.9994, `d2q06c` and `greenbea` at 1.0000 to four figures.
- **Three changed category.** `pilot-ja` **overrun → ok**, solving in 18536
  iterations for 940900493 units. `pilotnov` **`NUMERICAL_ERROR` → ok**, and
  cheaper with it: 18014 iterations and 809015777 units become 12640 and
  598949184. `pilot4` **ok → `NUMERICAL_ERROR`**, going from 4148 iterations
  and 112003171 units to 5920 and 160747384.

**Net: 54 agreeing becomes 55, and 8 overrun becomes 7.** Two gained, one lost.

**Every instance that moved was on the census list, and no instance off it
moved.** The three census instances that shifted zero costs — `perold`,
`stair`, `wood1p` — did not move either, which is the prediction the control
makes.

## `pilot4` is a real regression and is not diagnosed here

It runs 43% longer and then ends `JAOS_SOLVE_NUMERICAL_ERROR` **with no error
message**, which is the D146 guard's status rather than a raised error — the
same failure class as the other 30 disagreers (D191). Removing its loan of 252
costs changed its trajectory and the new one does not reach a point the guard
accepts. That is `TODO.md` §0's to pick up.

This is the failure shape `jaos-measure` names: a repair that fixes what is in
front of it and breaks something else. It was caught because the parent was run
beside the candidate, and not by any summary line.

## Re-running

All three scripts write beside themselves and **replace this directory's
evidence**. Both worktrees go under `mktemp -d`, outside the repository,
because `make clean` is `rm -rf build`.
