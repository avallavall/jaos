# 02-77 — the published-basis count, re-measured at three trees, and it had been stale before this session began

D167. No source change. It re-dates a number `TODO.md` has carried since D139
and attributes what moved it.

## Why it was re-measured

`jaos-measurer`, judging D165, counted `JAOS_BASIS_BASIC` on the four netlib
instances whose `basis=` hash moved and found `bandm` at +18 where `TODO.md`
records netlib's worst over-count as +23. It said the cell was no longer exact
and that four instances are not enough to replace it. Both were right.

## The reading — `basis-count.txt`

`bench/measurements/02-48/run-verify-count.sh`, the instrument that owns this
number, run unchanged at three trees. It reads the MAIN tree's
`src/presolve.c`, so each parent was placed there and restored on exit.

| tree | exact | **WRONG** | worst over | sum |
|---|---|---|---|---|
| what `TODO.md` recorded (D139, 2026-08-19) | 140 | **48** | +23 | +272 |
| `4c5f58f` — this session's starting point | 142 | **46** | +23 | +262 |
| `cd68630` — D164, before the compensation | 142 | **46** | +23 | +262 |
| `HEAD` — after D165 and D166 | 142 | **46** | **+18** | **+250** |

Kennington reads `exact=32 WRONG=0` at all three, unchanged since D139.

## Two things, and the first is the bigger one

**1. The table was already stale when this session started.** `4c5f58f` reads
46 wrong and 142 exact against the recorded 48 and 140. So something between
D140 and D161 — the previous session — fixed two solves' published basis and
nobody re-measured. The number sat wrong in the file that calls it "the largest
open correctness item" for a day.

Nothing in the gate reads it. `bench/run.c`'s `basis=` is a hash, so it detects
a CHANGE but never reports a COUNT, and the count only exists when this probe
is run by hand. That is exactly the shape of a figure that drifts: no owner
executes it.

**2. D165 moved the worst over-count and not the count of wrong solves.**
`cd68630` and `4c5f58f` are identical, so D162, D163 and D164 changed nothing
here — consistent with all three being bit-identical on the sets. Between
`cd68630` and `HEAD` the worst goes +23 → +18 and the sum +262 → +250, while
WRONG stays at 46.

**By the measure this file insists on, that is not an improvement.** `TODO.md`
says it in bold: "Do not use the summed error as the target … the measure is
the count of solves publishing a wrong basis." 46 before, 46 after. The
compensation makes four instances' bases different and two of them less wrong,
which is worth recording and is not progress on the item.

## What this does not settle

The 46 are not attributed per instance here — the probe reports set totals. The
per-instance split is 02-49's and would need its own run.
