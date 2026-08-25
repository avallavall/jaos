# 02-106 — 60.5% of the "primal" campaign is dual iterations, and `pilot4` is not a primal regression

2026-08-26. Opened to diagnose the instance D193 broke. It ended somewhere
else.

## Where it started: `pilot4`

D193 took `pilot4` from `optimal` to `NUMERICAL_ERROR`, and the status carries
no message. Four things came out of chasing that, in order.

**The message-less refusal is one line.** `src/simplex.c:5496`: a cold start
whose settled point has a non-zero `settled_dual_violation` sets
`outcome = JAOS_SOLVE_NUMERICAL_ERROR` and calls no `jm_set_err`. Every other
site that produces this status writes a message first. **That is why
`jaos_model_error` is empty for all 31 disagreeing instances** — it is the D146
guard refusing the point, and D191 named the class without naming the line.

**`pilot4`'s phase 1 is bit-identical across D193.** Same infeasibility at
iterations 0, 1000 and 2000, and `phase 1 reached a feasible point in 2596
iterations` on both sides. The guard did not touch it, because phase 1 prices
on `c1` and never reads `cost`.

**There were no loans to call in, on either side.** Instrumented at the guard:
`0 columns, largest 0` before `settle_shifts`, before and after D193. So the
loan story is not `pilot4`'s mechanism.

**What differs is one phase-2 primal iteration.** Before D193: 2598 primal
iterations of 4148, of which 2596 are phase 1 — so **two** phase-2 iterations,
and 1550 dual. After: 2597 primal, so **one** phase-2 iteration, and 3323 dual.
The dual re-entry then takes a different path and diverges — best infeasibility
251 → 397 → 91084 where before it went 801 → 281 — and leaves a published
breach of **6.72712** at variable 668, a column whose own cost is zero and
which carries no shift.

**So `pilot4` is not a primal regression.** Both its old success and its new
failure belong to the dual's settling re-entry. The primal contributed one or
two iterations either way.

## Where it ended: the primal barely runs

That "two phase-2 iterations" is not a `pilot4` quirk. `split.c` reads the
solver's own end-of-solve summary, which carries the total and the primal
count, and the phase-1 line, which carries phase 1's. Over all 94 with
`cfg.force_primal` (`split.txt`):

| | shipping |
|---|---|
| phase-2 primal iterations **exactly 1** | **80 of 94** |
| 2 to 10 | 6 |
| more than 10 | **8** |
| finishing with **zero** dual iterations | **8** |
| **dual share of every iteration run** | **60.5%** (515522 of 852279) |

**The 8 that run a real phase 2 are exactly the 8 whose phase 1 is zero
iterations** — they arrive primal feasible, so nothing hands over. Seven of the
eight hit the work limit; the survivor is `pilot87`.

## Why, and it is measured rather than argued

`update_dual` and the tail of `pivot()` run `shift_to_feasible` on every
variable the pricing row touches, once per iteration, and they are guarded only
while `in_phase1`. `shift_to_feasible` sets `d[v] = 0.0` on every breached
nonbasic. `primal_price` prices on `dual_breach`, which reads `d`. So after the
first phase-2 pivot there is nothing left to price, the primal declares
optimality, `settle_shifts` finds the point dual infeasible, and the dual's
re-entry does the rest of the solve.

**Guarding phase 2 as well confirms it** (`split-guarded.txt`, phase 2 guarded
in a worktree, the dual's own re-entry still lending):

| | shipping | phase 2 guarded |
|---|---|---|
| optimal | 56 | **17** |
| numerical error | 31 | 58 |
| work limit | 7 | 19 |
| phase-2 iterations exactly 1 | 80 | **0** |
| phase-2 iterations over 10 | 8 | **91** |
| zero dual iterations | 8 | **94** |
| dual share of all iterations | 60.5% | **0.0%** |

`truss` goes from 2802 phase-2 iterations to 422576. 44 instances lose
`optimal`; 5 gain it — `80bau3b`, `cycle`, `fit1p`, `ship08l`, `ship12l`.

## What this refutes

**D191 read "54 agreeing becomes 20" as an over-correction. It is not.** It is
the removal of the dual's 60.5%. The primal's own reach, on the instances where
it actually runs the method, is **17 of 94 optimal**, not 55. The 55 is phase 1
plus one primal step plus the dual.

D188 left this open in a single line — `reenter_after_settling` calls `run()`,
so a forced-primal solve can still finish with dual iterations — and treated it
as a harness detail. It is the dominant term.

**This is not a claim that guarding phase 2 is the right change.** It is a
claim that the number in `SPECS.md` does not mean what it reads as. Which of
the two the project wants is a decision, and it is in `TODO.md` §0.

## Re-running

Every script writes beside itself and **replaces this directory's evidence**.
All worktrees go under `mktemp -d`, outside the repository, because `make
clean` is `rm -rf build`. `run-tail.sh` and `run-split-guarded.sh` patch a
worktree and never the working tree.
