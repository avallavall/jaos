# 02-123 — stage 8b: the floor does not weaken Bland's rule, and `pilot87`'s phase 1 diverges

## The question

`TODO.md` §0 stage 8b, raised by `numerics-reviewer` on D207's diff and left
open by `jaos-measurer`.

Bland's rule needs the lowest-index basic among those attaining the minimum
ratio, over a **fixed** candidate set. `PIVOT_MIN` narrowed that set the same
way for every column. D207's `PIVOT_MARGIN * DBL_EPSILON * cmax` narrows it
**per column**, because `cmax` is that column's own norm. So at a degenerate
vertex a row can be eligible for `q1` and rejected for `q2`, the set the
tie-break ranges over changes between iterations, and Bland's finiteness
argument no longer covers it. Determinism is not at risk — the choice is a
function of the data and identical on every machine. Termination is.

The evidence that raised it was circumstantial: `pilot87`'s phase 1 goes
17165 → 387235 iterations at `C = 1`, a 22x blow-up ending in `overrun`.

## Answer 1 — the floor does not fight the anti-cycling rule

`bland-vs-floor.sh`, `bland.txt`. `n_bland` counts how many times a solve gave
up on Dantzig and armed Bland's rule after a stall. The solver already prints
it as `stalls` in its own `JAOS_LOG_SUMMARY` line; `bench/primal` installs no
callback, so it was silent. This installs one and reads it at both settings
over the fifteen instances D207 moves plus three controls it cannot reach.

| | |
|---|---|
| instances whose phase-1 iteration count moved | **13** |
| instances whose `n_bland` moved | **1** — `pilot87`, 0 → 1 |
| the three controls | unchanged in both columns |

**Twelve of the thirteen movers arm Bland's rule zero times at either
setting.** The shape of the worry — a set that shifts per column fighting the
rule — does not appear on this population.

On `pilot87` the rule arms **once, at iteration 343682**, after 66031
iterations without reducing infeasibility. That is 89% of the way through the
run. The stall detector firing once is the machinery working, not a defect.

## Answer 2 — but `pilot87`'s phase-1 objective diverges, and Bland is not the cause

`pilot87-stall.sh`, `pilot87-stall.txt`. `pilot87` alone, `-j 1`, at `C = 1`,
with `JAOS_LOG_DETAIL` on. The phase-1 objective is a **sum of bound
violations**, so the method must never increase it.

| iteration | infeasibility |
|---|---|
| 0 | 7.71859e+13 |
| 199000 | 1.34605e+12 |
| 341000 | 1.24365e+12 — still falling |
| **342000** | **3.24522e+12 — it turns** |
| 350000 | 5.2699e+14 |
| **351000** | **1.88282e+24** |
| 352000 | 6.7572e+25 |
| 385000 → 387235 | alternating 3.24653e+20 / 3.23341e+20 |

**The rise begins at ~341000 and Bland arms at 343682, after it.** So the
divergence is not a consequence of the anti-cycling rule being weakened; the
stall that armed the rule is a symptom of the divergence, not its cause.

The end state is numerically dead, not merely slow. Late iterations cost about
**27x** more work each — 12e9 work units per thousand iterations against
440e6 earlier — across 6246 refactorizations, 50419 weight restarts and 3139
stability rebuilds. Two log lines share an iteration number at 385000, 386000
and 387000 with different work totals, which is the loop running without
`s->iters` advancing.

## The control, and the first one that was too short to be one

`phase1-divergence.sh`. Is the divergence what this phase 1 does whenever it
runs long, or is it specific?

**`pilot87` cannot be compared against itself.** At `C = 0` it refuses at
iteration 17165 and never reaches 341000, so the two states are not both
reachable on that instance. The control has to be another instance that runs
phase 1 to a work limit, which several already did before D207 existed.

**First attempt, `divergence.txt` — `d6cube`, `scsd8`, `scrs8`, and it proves
nothing.** All three come back clean at both settings: `d6cube` 9363 → 9363,
`scsd8` 90 → 90, `scrs8` 7.17736e+12 → 32.162. But their budgets end phase 1
at **1000 to 3000 iterations**, and `pilot87` does not turn until **341000**.
A control that stops three hundred thousand iterations before the effect
cannot see it. Kept here because it was nearly written up as reassurance.

**The control that works, `divergence-dfl001.txt`.** `dfl001` reaches 136695
phase-1 iterations, the furthest any other instance gets.

| setting | start | **maximum** | end |
|---|---|---|---|
| `C = 0` | 8209 | **8209** | 6565.03 |
| `C = 1` | 8209 | **8209** | 6488.85 |

**The maximum is the starting value at both settings.** Over 136695 iterations
the objective falls monotonically and never once rises, with the floor on or
off. The floor leaves it slightly lower at the end.

So the divergence is **not** a general property of long phase-1 runs. It is
specific to `pilot87`.

## What is concluded, and what is not

**Concluded: stage 8b's premise does not hold on this population.** The floor
does not weaken Bland's finiteness argument. Twelve of thirteen movers never
stall; the one that does, stalls after its numbers have already gone wrong;
and the longest clean phase-1 run in the set is unaffected at both settings.

**Not concluded: whether the floor causes `pilot87`'s divergence or merely
uncovers it.** Without the floor that solve stops at 17165 with a
self-declared defect, so the diverging regime is unreachable. The two states
cannot both be produced on this instance, and no amount of re-running changes
that. What can be said is that the floor is what lets the solve reach a regime
where the basis degrades past use.

**And a plain reading of D207's own gain.** `pilot87` went `ERROR` → `overrun`,
which reads as an improvement and removed a self-declared defect. Both are
failures. The new one costs 387235 iterations and 179.6e9 work units and ends
with a point 1e25 outside its bounds; the old one cost 17165 and said so. The
`ERROR` count in `SPECS.md` is honest, and this is what is behind it.

## What this opens

The divergence is a phase-1 defect and not a floor defect. `TODO.md` §0
carries it: what happens between iterations 341000 and 352000 on `pilot87`,
and whether phase 1 should stop when its own objective rises rather than grind
to a work limit — a monotonicity it can check for the cost of one comparison.
