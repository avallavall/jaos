# 02-104 — Bland's rule reaches the primal's leaving variable, and it arms nowhere on netlib

2026-08-25. `TODO.md` §0, the first of D191's four answer-changing findings.

## The question

D191 left it in these words: phase 1 has no Bland's rule at all, and phase 2
has half of one — `primal_price` picks the lowest-indexed entering column under
`bland`, and `primal_ratio_test` breaks equal ratios on the first **row
position** it scanned rather than on the lowest basis **variable** index. Half
the rule terminates nothing. Bland's proof is a promise about both choices an
iteration makes.

Expected: fixing it changes no answer today, because a cycle needs a degenerate
vertex revisited in a particular order and nothing on this set reaches one. The
point of measuring was to find out whether the machinery is even reachable.

## What landed

`jm_primal_row_wins(step, var, best_step, best_var, bland)` in
`src/jaos_internal.h`, defined beside `jm_bland_pick` in `src/simplex.c`, and
called by both primal ratio tests. Seven unit tests, including the direction of
the comparison and the refusal to open a window at the minimum.

`bland` is a parameter rather than a read of `s->bland`, because
`primal_cleanup` must not have it: its candidate set is a snapshot taken once
and each entry pivots at most once, so it is bounded by construction, and
reading the flag there would let the dual's own settling re-entry change under
a flag this feature armed. It passes `false`.

Phase 1 gets the stall detector as well, on the shared `s->last_gain` and
`s->bland` that `run_primal` already resets on both sides of the call.

## The measurement

**The gate saw nothing, and that is the whole of the correctness claim.**
`make netlib netlib-infeas netlib-kennington J=12`: `gate: PASS` on all three,
`0 regressed, 0 improved, 0 new`, and every file in `bench/results/` **byte-
identical to the committed record** — `git status bench/results/` came back
empty. That covers the seven `src/` commits the records were stale by, not this
one alone.

**`make primal J=12` reproduced D191's figures exactly**: measured 54,
overrun 8, disagreed 31, errors 1; iterations geometric mean 2.1306; work
geometric mean **3.8332**, best `lotfi` 0.9817, worst `sctap2` 14.8415. Every
count and both means match D191 to four figures, so the change is a no-op on
this campaign too.

**And identical figures do not say whether the branch is live.** `probe.c`
solves with `cfg.force_primal` at `JAOS_LOG_DETAIL` and counts the arming
lines. Over all 94 (`bland-arms.txt`):

| | |
|---|---|
| phase-1 arms | **0**, on every instance |
| other arms | **5**, on three instances: `grow15` 1, `grow22` 3, `grow7` 1 |

`other` is phase 2 and the dual's settling re-entry together, because the two
print the same sentence. All three `grow` instances still reach `optimal`.

**The 0 is validated.** `canary.sh` rebuilds at this tree in a worktree with
`STALL_FACTOR` forced from 10 to 0, so the detector arms on the first iteration
that does not improve, and re-runs the probe on five instances
(`canary.txt`): `afiro` 5, `adlittle` 6, `share2b` 15, `stocfor1` 44, `sc50a`
0. Four of five move off zero, so the probe can see a phase-1 arm and the 0 on
the real tree is a fact about the set.

## What this does not say

The rule was never reached on netlib, so **no instance here exercises the
leaving-variable half at all** and the campaign is not evidence that it is
correct. The unit tests are. That is the same shape as `jm_bland_pick`, which
D26 landed for a case no instance produces either.

`sc50a` arming zero times even at `STALL_FACTOR = 0` is worth one line: its
phase 1 improves on every iteration, so it is not a stalling failure. It is
still `NUMERICAL_ERROR` and still the thread `TODO.md` §0 names first.

## Re-running

`run-bland-arms.sh` and `canary.sh` write beside themselves and **replace this
directory's evidence**. Redirect elsewhere for a fresh reading. `canary.sh`
puts its worktree under `mktemp -d`, outside the repository, because `make
clean` is `rm -rf build`.
