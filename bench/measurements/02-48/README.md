# Kennington publishes a valid basis on every solve, and netlib's worst error falls from 596 to 23

Taken 2026-08-18. The second and last repair of the published basis in this
run. Closed as D139.

## The change

`ps_singleton_col_swap` performs the exchange the reduction owes. When a
restored cost-0 bounded column singleton lands strictly inside its own bounds
it must be `BASIC` — a nonbasic variable rests on a bound and this one rests
on neither — but its row **survives**, so nothing was restored to pay for the
basis position. The swap takes the row's own logical out.

It runs in the same second pass as D138's, after the replay, on both postsolve
paths.

**The partner is forced, not chosen.** If the column came back interior, the
reduced activity was strictly inside the widened row bounds, so row `i`'s
logical was basic in the reduced solve. It is the only other variable this
record touches. The exchange removes `e_i` and inserts a column whose one
nonzero is `a_ij`, so the pivot is `a_ij` and presolve already required it
non-zero: no rank test and no fallback.

**And it moves no numbers.** `c_j = 0`, so a basic `x_j` needs `y_i = 0`,
which the reduced solve already had.

## The whole chain, measured the same way each time

| | D131, before | D138, row only | **now** |
|---|---|---|---|
| netlib, exact count | 56 | 88 | **140** |
| netlib, wrong | 132 (70%) | 100 | **48 (26%)** |
| netlib, worst error | +596 / −169 | +596 / −0 | **+23** / −0 |
| Kennington, exact count | 8 | 24 | **32 — all of them** |
| Kennington, wrong | 24 (75%) | 8 | **0** |
| Kennington, worst error | +12104 / −406 | +119 / −0 | **0** |

**Kennington publishes a valid basis on every solve.** netlib's worst
published error falls from 596 basic variables too many to 23.

## The two pinned change detectors both fired, in the direction they predicted

`test_singleton_col_between_two_removals_solved_path` read 3 against
`num_row = 2` and carried a comment saying *"The count is what says the repair
landed — expect this 3 to become 2 … re-pin there, deliberately."* It now
reads **2**.

`test_the_basis_count_promise_breaks_on_a_declined_column` read 2 against
`num_row = 1` and asserted the reference build's 1 separately, *"which is why
both are asserted rather than the presolved one alone"*. It now reads **1**.

**Both now agree with `-DJAOS_NO_PRESOLVE`**, the project's only oracle for
output no predicate reads. The `#if` in each test existed to state the gap
between presolve and the oracle; the gap is closed and both are collapsed to
one assertion, re-pinned deliberately as their own comments instructed.

## What the gate can and cannot say

Bit-identical on all three sets, which proves no value moved. It cannot judge
the repair: `bench/run.c` states the digest covers x and y and not the basis,
so *"a change that moves only the basis is invisible to all three sets"*.

**That is now worth revisiting.** The runner declined to widen the digest to
cover the basis because *"a published basis that breaks the row-count promise
is a live defect (TODO.md), so a basis hash would pin today's wrong answer."*
Kennington no longer breaks it on any solve.

## What is left, and it is 48 netlib solves

Two shapes decline the swap, both leaving the count one too high, both counted
in D135:

- **80 firings whose row logical is already nonbasic.** This contradicts the
  derivation above and is unexplained. D135 read the *published* status rather
  than the reduced one, so re-measure before believing it is a second defect.
- **108 firings whose row is not on a bound**, where making the logical
  nonbasic would claim a bound the row does not rest on.

And **40 singleton rows** whose final activity misses its bound by about
1e-16 of the row's traffic (D136), which fall through to `BASIC`.

## Reproducing it

`run-verify-count.sh`, beside this file. It copies the working tree's
`src/presolve.c` into a worktree at HEAD before instrumenting, so it measures
the candidate rather than HEAD.
