# Every under-count is gone, Kennington's worst error falls 100x, and the sum was the wrong target

Taken 2026-08-18. The first repair to land on the published basis. Closed as
D138.

## The change

`ps_singleton_row_status` decides a restored singleton row's status from its
own dual and its **final** activity, in a second pass after the replay, on
both postsolve paths. The replay no longer decides it.

Two rules, both derived and both published (D136, D137):

- `y_i == 0` → `BASIC`. The replay publishes a zero dual exactly when it
  leaves the folded column alone, so the logical takes the basis position the
  restored row is owed.
- `y_i != 0` → the row rests on a bound, so `AT_LOWER`/`AT_UPPER` from the
  final activity.

## What it did

| | before (02-41) | after |
|---|---|---|
| netlib, exact count | 56 | **88** |
| netlib, wrong | 132 | **100** |
| netlib, **under-counting** | 60 | **0** |
| netlib, worst error | +596 / −169 | +596 / **−0** |
| Kennington, exact count | 8 | **24** |
| Kennington, wrong | 24 | **8** |
| Kennington, **under-counting** | 16 | **0** |
| Kennington, worst error | +12104 / −406 | **+119** / −0 |

**Every under-count on both sets is gone.** Kennington's worst published error
falls from 12104 to 119, and three quarters of its solves now publish an exact
count.

**The gate is bit-identical on all three sets** — 94, 29 and 16 — which is the
proof that no value moved. It is also the reason the gate cannot judge this
change: `bench/run.c` says the digest "covers x and y and NOT the basis … no
predicate this runner reports observes a status, so a change that moves only
the basis is invisible to all three sets."

## The sum was the wrong target, and this is how that showed

netlib's total went from **+3904 to +5942** — worse — while every other
measure improved.

The difference is exactly **+2038**, which is D134's `SINGLETON_ROW` net of
**−1998** removed, plus the ~40 firings that still fall through. **The
under-count was cancelling part of the over-count.** `TODO.md` asked for "the
closing sums, +3904 and +25654, must go to zero"; that target is satisfied by
a change that makes both halves worse in equal measure, and it is dropped.

**The measure is the count of solves publishing a wrong basis**: 132 → 100 and
24 → 8.

## What is left

`SINGLETON_COL`, untouched. It is the whole of the +5942 and +482 that remain:
5902 and 482 by D133's attribution, and D135 has the rule — the column enters,
the logical of its surviving row leaves — valid on 96.8% and 100% of firings.

And the 40 netlib singleton rows whose final activity misses its bound by
about 1e-16 of the row's traffic. They fall through to `BASIC` and no
tolerance for them has been measured.

## The test that caught the first attempt

`ps_replay_one` is shared by both postsolve paths. The first version removed
the status write from the replay and added the second pass to
`jm_postsolve_expand` only, so on the `jm_postsolve_solved` path the status
fell back to that path's `memset`, which is `BASIC`.

`test_singleton_col_between_two_removals_solved_path` pins the basic count as
a change detector and reads 3 today, with a comment predicting the repair
takes it to 2. It read **4** and failed immediately. Its own model comment says
why: *"Every column leaves, so `rcol == 0` and postsolve runs on the
`jm_postsolve_solved` path."*

That test still reads 3, so this change is neutral on it. Its 3 comes from a
different defect the same comment names: on the solved path a **surviving**
row's status comes from nowhere but the `memset`.

## Reproducing it

`run-verify-count.sh`, beside this file. It copies the working tree's
`src/presolve.c` into a worktree at HEAD before instrumenting, so it measures
the candidate rather than HEAD.
