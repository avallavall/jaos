# 02-139 — the checker half of the assert debt, and one guard that only exists in a debug build

D227. Five tests in `tests/test_check.c`, one per sentence that landed in
`src/check.c` at D221 and D223 with nothing beside it. `run-check-controls.sh`
is the proof each one goes red when its sentence is broken, and `controls.txt`
is the record.

Seven arms, five breakers. One breaker runs twice, and that pair is the
reason this directory is worth reading.

| arm | unit suite | verdict |
|---|---|---|
| intact | 23 tests, 0 failures, exit 0 | — |
| an exempt multiplier stops contributing `w * bound` | **4 failures** | its test is red |
| `note_dropped` ignores anything under 1e-12 | **1 failure** | its test is red |
| `certified_step` stops clamping its room at zero | **no summary, exit 134** | the assert fires |
| the same break under `-DNDEBUG` | 23 tests, **0 failures**, exit 0 | nothing catches it |
| the implied lower bound is one unit too tight | **2 failures** | its test is red |
| an infinite term stops being counted | **2 failures** | its test is red |
| the recipe again, nothing broken | 23 tests, 0 failures, exit 0 | — |

## The finding: one sentence is enforced by an assert and by nothing else

`certified_step` clamps its room at zero, and `src/check.c` says why: a
negative distance would turn a certified suboptimality into a claim that the
point is better than optimal. D219 put an assert under that sentence.

Take the clamp out and the suite **aborts** — exit 134, no summary line
printed, because the assert fires before Unity can report anything. That is
already worth knowing, and it is why every arm here records the test
binary's exit code rather than only the failure count.

Take the clamp out and compile with `-DNDEBUG`, which is what
`RELEASE_CFLAGS` uses and therefore what ships, and **the whole suite is
green**.

So the test beside that sentence does not catch the defect. It pins the
value the report carries. The reason a negative distance is harmless in the
shipping build is a third thing, in the caller:

```c
long double gain = (long double)fabs(w) * t;
if (gain > a.certified)
    a.certified = gain;
```

A negative `gain` never wins that comparison, so it never reaches the
report. The clamp, the assert and this maximum are three guards on one
property, and only one of them survives `-DNDEBUG`. If the maximum ever
becomes a sum, the clamp is what stops the defect and the assert is what
would have caught it going away — in a debug build.

**None of that was written down before this campaign. It was measured by
running the same breaker twice.**

## Two arms move two tests each, and that is honest rather than hidden

`test_the_implied_box_is_exactly_what_the_row_implies` and
`test_an_infinite_term_is_counted_not_summed` both read `implied_bounds`, so
either break turns both red. They are still separate tests, because the two
arms leave different signatures:

| | dual objective | dropped terms |
|---|---|---|
| implied bound one unit too tight | wrong | **0** — the bound is still finite |
| infinite term not counted | wrong | **1** — no bound is implied at all |

An arm passes when its own test is red; the file lists everything else that
moved with it.

## The exemption is load-bearing well past its own test

Making a negligible multiplier skip `a->dual_obj += w * bound` turns **four**
tests red, three of which existed before this campaign:
`test_a_tiny_multiplier_on_a_large_bound_still_counts`,
`test_a_waived_sign_condition_is_still_caught_by_the_gap` and
`test_the_gap_can_be_two_large_halves_cancelling`.

That is the same shape D224 found with `jm_alloc_array(0)`: a one-line
contract that four separate tests depend on without saying so. The new test
is the one that states it.

## Running it

```
bash bench/measurements/02-139/run-check-controls.sh
```

Each arm is its own worktree of `HEAD` plus the working-tree copies of
`src/check.c` and `tests/test_check.c`, so a break never touches this tree
and the script runs before the change is committed as well as after. Exit 0
when every arm behaved, 1 when one did not, 2 when the harness failed.
