# 02-166 — the published basis has the right count on every gate solve

D257. `jaos.h` promises exactly `num_row` basic statuses. Since D139
netlib had published a wrong count on 46 of its 188 optimal solves, all
of it from the singleton-column and singleton-row replays, and every
local repair had been refused (D140, D141). Every one of those solves
was an exact-equality test on a value the replay had rounded: the
recovery division landing ulps inside a bound whose exact value is the
bound, or a folded activity missing its row bound by an ulp. The
statuses are now decided from each reduction's structure, and where the
structure says a value rests on a bound, the bound is what is published.

## What is here

| file | what it does |
|---|---|
| `run-count-and-signs.sh` | 02-78's published-basis count, with `num_col` on the line so a solve can be attributed, then 02-80's two public-API detectors on netlib; builds the WORKING tree's `src/` in a worktree outside the repository and writes beside itself, never into an older record. `run-count-and-signs.sh <label>` |
| `count-before.txt` | the count at `7c5b288`, the parent |
| `count-after.txt` | the count on the candidate |
| `wrong-solves-before.txt` | the 46 wrong solves at the parent, by dimensions, each instance twice (the gate solves each instance twice) |
| `signs-before.txt`, `signs-after.txt` | 02-80's detector: published reduced costs against the published statuses |
| `control-without-sign.txt` | the candidate's first build, whose singleton-row ownership test lacked the reduced cost's sign; the count is exact and the checker refuses 15 solves |

## The reading

| | parent `7c5b288` | candidate |
|---|---|---|
| netlib, optimal solves publishing the promised count | 142 of 188 | **188 of 188** |
| netlib, worst over-count | +21 | 0 |
| Kennington | 32 of 32 | 32 of 32 |
| netlib instances whose published reduced cost contradicts the published status (D170) | 5, worst 15018.5 on `nesm` | **0**, worst 4.09e-10 on `dfl001` |
| 02-80's crosstab | 23 instances fail the count, 5 of them the signs | clean, 94 of 94 |
| the probe's own gate, both sets | PASS | PASS |

The probe runs the gate's runner and reads its verdict as a control on
itself: a candidate that fixed the count by moving an answer would read
`gate: NOT MET` here first, and the first build did.

## The control that says the sign condition is load-bearing

`control-without-sign.txt`. The singleton row's rule was first written
as "the column rests, nonbasic, on a bound this row induced and not on
one of the caller's": the count came back exact on all 220 solves, and
the checker refused 9 netlib and 6 Kennington solves on dual
feasibility, worst 9.27e+04 on `pds-10`. A column a collapsed fold fixed
at a point sat fixed in the reduced solve, where its reduced cost obeys
no sign, and the rule handed that sign to the row's multiplier at
whichever end the fold's flags named. The rule now reads the sign: a
bound holds a variable whose reduced cost points into it, and the end
the row holds the column from is the one whose multiplier carries that
sign. That is the sign condition the old `this_row_owns` had, kept, with
the zero reduced cost and the caller's-own-bound cases decided by the
same reading.

## What the old exchange asked, and why it declined

`ps_singleton_col_swap` (D139) took the surviving row's logical out
when the column came back interior, but only if the folded activity
equalled the row's bound bit for bit, and only if the logical was still
basic. D140 measured the two classes it left: 80 where the reduced
solve had already put the logical on the relaxed end and the division
put the column one ulp inside the bound the end had absorbed, and 152
where the activity missed the bound by rounding. Both are the same
number computed two ways and compared with `==`. The replay now reads
the logical's status, which is the structure: nonbasic at an end means
the exact recovery is the column's bound that end absorbed, and that
bound is published; basic with an interior recovery means the row's
logical leaves for the end the division targeted, whatever the
rounding of the sum says afterwards.

## The gate

Run on `4555b70`, the commit that carries the change, at `J=12`. All
three sets `gate: PASS`, `0 regressed, 0 improved, 0 new`. Read with
`record_diff.py` against the committed records:

| set | bit-identical | basis hash moved | x/y digest moved | work or iterations moved |
|---|---|---|---|---|
| netlib, 94 | 39 | 55 | 6: `bandm`, `finnis`, `nesm`, `perold`, `pilot-ja`, `pilotnov` | 0 |
| infeasible, 29 | 29 | 0 | 0 | 0 |
| Kennington, 16 | 4 | 12 | 0 | 0 |

The six digest moves are the published duals on D170's five instances,
which now belong to the basis published beside them, and the ulp-sized
snap of a recovered column onto the bound that is its exact value.
Every checker predicate holds on all 139, and no baseline is rewritten
because nothing a baseline reads moved.
