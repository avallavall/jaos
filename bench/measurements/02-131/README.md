# 02-131 — `lu.c`'s kept contracts become asserts, and the control that proves they work

`TODO.md`'s assert debt, `lu.c`. The 2026-08-26 comment purge kept a set of
sentences because other code depends on them; D30's rule and D201's receipt
say such an invariant is an assert, not a sentence. Eight of `lu.c`'s are
asserts now. `run-assert-control.sh` is what says they are worth having.

## Why the unit suite is not enough

Asserts are compiled out by `-DNDEBUG`, which the release build sets. So
`make configs` and `make sanitize` exercise them only over the unit suite,
whose matrices are small, and the gate — the only thing that factors a real
basis tens of thousands of times — never runs them at all. An assert in that
position has never seen the code path it guards.

`EXTRA_CFLAGS` is appended last to `RELEASE_CFLAGS`, so
`make netlib EXTRA_CFLAGS=-UNDEBUG` builds the shipping configuration with the
asserts live. That is the `live` arm.

## The two arms

| arm | `src/lu.c` |
|---|---|
| `live` | the eight asserts |
| `control` | the eight asserts, **and the `mult_set` clear loop deleted** |

The control exists because a check that has never rejected anything is not
known to work (`jaos-testing`). Deleting the two lines that clear `mult_set`
at the end of each pivot step is the exact defect the step-top assert is
written against: the next pivot's columns would read the previous step's
multipliers as their own.

## The reading

    live asserts fired: 0    control asserts fired: 85
    PASS: the asserts hold on 94 real instances AND catch the defect they exist for

| arm | assertion failures | instances |
|---|---|---|
| `live` | **0** | 94 solved, 94 checker ok, `gate: PASS` |
| `control` | **85** | 9 solved, 85 failed, `gate: NOT MET` |

The control's failures are all one line:

    run: src/lu.c:490: jm_lu_factor: Assertion `!e.mult_set[i]' failed.

Run twice, and both arms reported the same two numbers.

## The verdict line lied, and the numbers under it did not

The first run of this script printed

    STOP: the control did not fire, so the step-top assert checks nothing

directly beneath its own `control asserts fired: 85`. The cause is a shell
detail: **`grep -c` prints `0` and exits 1 when it matches nothing**, so
`l=$(grep -c ... || echo 0)` produced two zeros on one line, and every numeric
test after it died with "integer expression expected" and fell through to the
last branch. The readings were right the whole time.

It is recorded because it is this project's own lesson arriving from the other
direction: the summary line is not the result, and here the summary line was
wrong about data printed four lines above it. The script is fixed and re-run,
and it derives its root from its own location rather than hardcoding it —
the defect D215 found in `02-126/relrise.sh` the same day.

## What was left as prose on purpose

One item on `lu.c`'s list was **not** turned into an assert. The suggested
`compact_pivot_row` check — every kept entry has a nonzero value — restates
the `if (aij == 0.0) continue;` three lines above it, so it can only fail if
the compiler is wrong. The duplicate-column half of that contract needs a
stamp array the function does not own, and inventing one would make the debug
build write state the release build does not. Both stay in `TODO.md`.

`keep <= k` in the one-walk update is locally provable too, and is kept: it
guards a memory hazard (the loop writes `cv->idx[keep]` while reading
`cv->idx[k]` in the same array), so it is a change detector for anyone who
later adds a second increment. That is a different thing from restating a
branch.
