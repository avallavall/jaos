# 02-133 — the phase-1 stop rule's threshold, swept from one campaign and then paid for

`TODO.md`'s item 1, reopened by D215. D211 refused a stop on the primal phase
1's objective rising, because `pilot-ja` rose 25.0449 above its running
minimum at iteration 2091 and still finished `ok`. D212's two-pass ratio test
removed that rise: `pilot-ja` rises 3.3348e-12 now. What was missing was the
constant, and a constant needs a measurement on both sides.

## The instrument, and why one campaign is the whole sweep

The rule is a per-iteration gate on one quantity. At threshold `T` an
instance either never reaches `T`, and then the rule cannot change anything
about it, or it reaches `T` first at one particular iteration and the rule
stops it there. So a single instrumented run that records the first crossing
of every decade predicts every setting at once (`jaos-measure`; the method is
D151's).

`rise-sweep.sh` is that run: a `JAOS_DIAG` patch on a worktree copy of `src/`
that adds counters and reads nothing the solver writes. Per instance it
records the largest relative rise, the first crossing of each of 25 decades
from 1e-12 to 1e+12, and — the reading that decides the question — the
phase-1 iteration at which the running minimum last improved.

`rise-sweep.txt` is the standard 94, `rise-sweep-kennington.txt` the 16. The
first was taken at `7a1575d` and the second at `b9fd328`; `git diff` between
those two commits touches no file under `src/`, `include/`, `bench/` or the
Makefile, so both readings are of one solver.

## Where the threshold can go

Instances crossing each decade, standard set:

| decade | instances that reach it |
|---|---|
| 1e-12 | `dfl001` `pilot` `pilot-ja` `pilot87` `woodw` |
| 1e-11, 1e-10 | `dfl001` `pilot` `pilot87` `woodw` |
| 1e-9, 1e-8 | `pilot87` `woodw` |
| **1e-7 through 1e+11** | **`pilot87`** |
| above 8.06882e+11 | none |

Kennington crosses nothing at all. Its largest rise is `cre-b`'s
**3.75749e-13** and twelve of its sixteen instances never rise. Five of them
overrun the campaign's budget and none would be stopped, which is the
distinction the rule has to make: a basis going bad, not a slow solve.

So over 110 forced-primal solves the band in which only `pilot87` stops runs
from 1e-7 to 1e+11.

**Inside that band there is a plateau, and the plateau is what chooses.**
`pilot87`'s first crossing is iteration 9977 at 1e-7, 13991 at 1e-6, and
**19532 for every threshold from 1e-5 to 1e+2** — one rise of 633.034, with
nothing at all between 2.04558e-06 and it. Every setting on that plateau
produces the identical campaign, so the measurement does not choose inside it.
The same shape D213 found for the Harris width.

`PHASE1_RISE_MAX = 1.0` is the plateau's middle and reads as a sentence rather
than a fitted number: phase 1's total infeasibility may double. It stands
2.3e+7 times above `woodw`, which must not stop, and 633 times below
`pilot87`'s own crossing value, which must.

## Whether stopping throws anything away

This is the reading D211 did not have, and it is why the refusal does not
simply come back.

| instance | phase-1 iterations | last improvement of the running minimum | gap |
|---|---|---|---|
| `pilot87` | 381886 | **19532** | **362354** |
| `degen3` | 13813 | 13809 | 4 |
| `ken-11` | 12146 | 12143 | 3 |
| `dfl001` | 125807 | 125807 | 0 |
| the other 106, both sets | | their own last iteration | 0 |

Three of the 110 stop improving before they stop running, and the gap is what
tells them apart. `degen3` and `ken-11` are unreachable at any threshold at or
above 1e-12: `ken-11` never rises at all, `degen3` rises 3.98055e-16.

`pilot87`'s minimum last improved at the very iteration where it crosses 1.0.
The 362354 iterations after it lowered the objective by nothing. Stopping
there costs no progress and saves 97.6% of the instance's work — 4.23e+09
units against 1.79e+11.

`dfl001` is the control: it grinds 125807 phase-1 iterations, is still
improving at the last one, and is not touched at any threshold above
3.30714e-10.

## Whether a relative test needs a floor — the review's one open question

`numerics-reviewer` objected that a purely relative test has no floor under
its base: if `total` can descend to a few ulps of the traffic it is summed
from, "it doubled" is a statement about cancellation and not about the basis.
It called this a suspicion and named the measurement that settles it.

`rise-sweep.sh` records it: per instance, the smallest value
`total / (DBL_EPSILON * traffic)` ever reaches, where `traffic` is
`Σ(|bound| + |xb[i]|)` over the violated rows — the same quantity D209 judged
`PIVOT_MIN` against.

| instance | smallest, in ulps of its own terms | at a total of |
|---|---|---|
| `wood1p` | **7.14466e+10** | 1.07876e-04 |
| `osa-30` (Kennington's lowest) | 1.61682e+12 | |
| `d6cube` | 1.9884e+12 | 1.76527e-03 |
| `degen2` | 2.07826e+12 | 1.75439e-02 |

Over both sets the minimum is nearly **eleven decades** above the one ulp
where the objection starts. No floor is added, and the reason is this reading rather
than an argument: a constant with no measured side is what the project
refuses. If a model population ever brings that figure within two decades of
1, the floor is `best_total > DBL_EPSILON * traffic` and
`primal_phase1_costs` already walks the terms.

## What was refuted on the way

02-126 reports that `pilot87`'s objective turns at iteration 341234. That
describes the tree before D212. At HEAD it rises 633x at 19532 and never
recovers. The turn moved with the ratio test, which is D212's own headline
seen from the other side, and anyone re-reading 02-126 for a current
trajectory gets the wrong one.

## What the review changed in the rule itself

Three of `numerics-reviewer`'s four findings are code, and all three are
fixed before any campaign ran, which is the point of the review coming first:

- The refusal was taken on a carried point. Every other refusal of this kind
  in `run_primal_phase1` rebuilds the point and retries once, the D20 shape.
  The branch does that now, and the first fix moves `pilot87`'s work units,
  so a campaign run before it would have had to be thrown away.
- `total == +inf` left `best_total` at `HUGE_VAL` and switched the rule off
  for the rest of the solve. `!isfinite(total)` is tested apart now.
- `docs/tolerances.md` called the ratio dimensionless. It is a scaled-space
  test, invariant only while the violated set is unchanged.

The retry has a consequence for the low arm of the campaign below: a crossing
that a refactorization removes no longer stops anything, so at 1e-12 the
firing set is a **subset** of the five instances that cross, not necessarily
all five. The script's prediction is written as a subset relation for that
reason, with `pilot87` as the member it must contain.

## The paid half — `run-rise-sweep-campaign.sh`

The free sweep is an argument until a campaign checks it, so the script names
its prediction per setting before it runs, and exits 1 if any of the three
misses:

| setting | predicted to stop | what stopped | ok / disagree / overrun |
|---|---|---|---|
| pre-rule | — | — | 61 / 29 / 4 |
| `1e-12` | a subset of `dfl001` `pilot` `pilot-ja` `pilot87` `woodw`, containing `pilot87` | `dfl001` `pilot` `pilot87` | **60** / 32 / 2 |
| `1.0` (shipped) | `pilot87` | `pilot87` | 61 / 30 / 3 |
| `1e+12` | nothing, record identical to the pre-rule tree | nothing, and it is identical | 61 / 29 / 4 |

**All three held.** Two readings are worth more than the verdict line:

- **At `1e-12`, `pilot` goes from `ok` to disagreeing.** The rule takes an
  answer off a solve that was going to finish. That is the failure a threshold
  set too low produces, measured rather than projected, and it bounds the
  constant from below the way `pilot87` bounds it from above.
- **Five instances cross `1e-12` and only three stop.** `pilot-ja` and `woodw`
  cross and survive, because their rise does not repeat once the point is
  recomputed. That is exactly what the review's first fix is for, and a rule
  refusing on first sighting would have stopped both.

Each setting gets its own worktree, its own `make clean` and its own binary,
because `make` does not see a changed constant any better than it sees a
changed `EXTRA_CFLAGS`, and a five-point sweep once read exactly 1.0000x at
every setting for that reason (D82). The `1e+12` arm is the strongest control
in the set: above every rise ever measured, the branch must be inert.

`rise-sweep-campaign.txt` is its output.

## What this does not touch

`run_primal` is reached only through `cfg.force_primal`, a development switch
and not an option (D64, D188). No shipped solve reaches this branch, so the
three gate sets cannot move; they are run anyway, and byte-identical records
are the claim.
