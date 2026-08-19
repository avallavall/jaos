# The replay publishes a value inside the column's box, and the assert that could not be enabled is gone rather than widened

Taken 2026-08-19, the standing debt TODO.md called "worth more than it looks".
Closed as D152.

## What was open, and one number in it was wrong

The debt said: eleven of the 94 standard instances reach `ps_replay_one` with
`want_lo` above `want_hi` by 2.2e-16 to 1.3e-15, publish `want_lo`, and so put
a value outside the column's own declared box — which is why
`assert(want_lo <= want_hi)` cannot be enabled and no assert-enabled build can
run those eleven at all.

**The first half is exactly right and reproduced. The second half is right
about two instances, not eleven.**

| | count | which |
|---|---|---|
| instances tripping `assert(want_lo <= want_hi)` | **11** | 80bau3b, bandm, bnl1, cycle, dfl001, finnis, nesm, perold, pilot, pilot-ja, pilotnov |
| empty intersections behind them | **138** | all on netlib; Kennington and netlib-infeas have none |
| of those, publishing OUTSIDE the column box | **10 records, 2 instances** | `bnl1`, `finnis` |

On the other 128 records `want_lo` equals `rec->lo` with the box open above it
(`reclo=0 rechi=inf` is the common shape), so the intersection is empty while
the published value was inside the box all along. The debt's own worked
example is one of the ten real ones and is correct as written: `bnl1` wanted
2.1850000000000005 from a column whose upper bound is 2.1850000000000001.

`assert-build-before.txt` is the eleven reproduced at the parent commit, run
one process per instance so no abort can hide another.

## The repair

The published value is clamped into the column's own recorded box:

```c
const double xv = want_lo < rec->lo ? rec->lo
                : want_lo > rec->hi ? rec->hi
                                    : want_lo;
```

Which end to trust is the whole question and it is not symmetric. `rec->lo`
and `rec->hi` are the column's bounds as stored; `lo_j` and `hi_j` come out of
`(rl - rest) / rec->coef`, so they carry the rounding of a subtraction and a
division the stored pair never went through. The derived end is the one with
the error.

## Two windows for the old assert, both built and both refuted

The obvious move is to keep `assert(want_lo <= want_hi)` with a tolerance, so
it still catches a genuinely infeasible model (the `x0 + x1 = 100` case in the
source, whose gap is 93 rather than 1e-14). Two scales were built and measured
and neither works:

| window | result |
|---|---|
| eps × the division's own inputs, `(\|rest\| + \|row bounds\|)/\|coef\|` | 11 aborting instances → **2**. Worst gap 4.72e-14 against a window of 1.78e-15 |
| eps × the row's accumulated traffic | 22 uncovered records → **18**, and traffic reads **exactly zero on 86 of the 138** |

**The first fails for the reason `fp-numerics` names**: the surviving rows are
equalities at zero whose partial activity has cancelled, so `rl = ru = 0` and
`rest` *is* the residue. The scale collapses onto the quantity it exists to
bound.

**The second fails structurally, and the reason is two lines of this file.**
`src/presolve.c:2707` sets `orig->sol_row[i] = red->sol_row[ri]` — a direct
copy from the reduced solve — and `JM_PS_EMPTY_ROW` and `JM_PS_SINGLETON_ROW`
assign it outright rather than accumulating. So the residue is the **simplex's**,
and its error budget does not exist in the replay to be read. Accumulating
traffic inside `ps_row_add` cannot see it, which is why 86 records read zero.

So the emptiness assert is **removed, not widened**. Fitting a third window
would be fitting a constant to two instances, which is what D8 forbids.

What the site asserts instead is what the clamp establishes and what `jaos.h`
promises: the published value lies inside the column's own box. That check is
enforceable, and it is enforced.

**Detecting an actually infeasible model stays open and stays where it was.**
This site cannot tell 1e-14 from 93 without an error budget it has no access
to; the old assert only appeared to because it was never enabled. `TODO.md`
carries it as the separate defect it already was.

## What it cost

**The build configuration came back.** `assert-build-after.txt`: all 94
standard instances run under `-UNDEBUG`, 0 aborts, 0 other failures. Before
the clamp, 11 aborted. That is the debt's stated value delivered.

**The gate moved exactly where predicted and nowhere else** (`gate-run.txt`):

```
netlib            gate: PASS   94 compared: 92 bit-identical, 2 moved
                  DIGEST bnl1, DIGEST finnis; basis on both; gap on bnl1
netlib-infeas     gate: PASS   29 of 29 bit-identical
netlib-kennington gate: PASS   16 of 16 bit-identical
all three: baseline 0 regressed, 0 improved, 0 new
```

The two that moved are the two the control named before the gate ran. The
basis moves with them and in the right direction: a clamped value equals
`rec->hi` exactly, so the status test publishes `AT_UPPER` where it published
`BASIC` — a variable resting on its bound is nonbasic, which is D137's rule
read the other way round.

## The control, because a repair whose removal changes nothing is not a repair

`run-clamp-control.sh` removes **only** the clamp, keeps the new assert, and
re-runs the assert-enabled build. It expects `bnl1` and `finnis` back, and
that is what it reads (`clamp-control.txt`).

The expectation is two rather than eleven, and getting that wrong the first
time is what produced the count correction at the top of this file. The script
now names the two instances so a later run cannot pass by accident.

## Reproducing it

```
bash bench/measurements/02-61/run-assert-build.sh --parent  # the 11, before
bash bench/measurements/02-61/run-assert-build.sh           # all 94, after
bash bench/measurements/02-61/run-gap-probe.sh              # what the gaps are made of
bash bench/measurements/02-61/run-traffic-probe.sh          # the refuted traffic window
bash bench/measurements/02-61/run-clamp-control.sh          # remove the clamp, expect bnl1 finnis
bash bench/measurements/02-60/run-gate.sh                   # the three sets
```
