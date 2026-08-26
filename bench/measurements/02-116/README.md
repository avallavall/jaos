# 02-116 — D199's memset-to-scatter clear, in seconds

## The question

D199 replaced `primal_phase1_costs`'s `memset` over all `nvar` doubles with a
scatter over the positions the last call set. It was accepted on a work
geometric mean of **0.9452** with byte-identical digests, and no time ratio was
taken anywhere in the entry.

A review objected with a specific argument. A `memset` moves `8*nvar` bytes at
32–64 B/cycle; the replacement is `cleared` scattered 8-byte stores into an
array that is 0.5–2 MB on the larger instances. Break-even therefore sits near
`cleared` about `nvar/12`, and the sampled density on this set is 11–13% of
`nvar`. That is the one case where work units and seconds can move in opposite
directions, and D45 says a change is judged on three things rather than one.

## What was run

`run-clear-timing.sh`, output in `run-clear-timing.txt`.

Two worktrees, one machine, one session: `4d1ca2d` (the memset) and `f135e8b`
(the scatter). `-j 1`, alternated, minimum over 5 rounds, geometric mean of
per-instance ratios. Each tree uses its own `bench/primal`, which carries the
log-callback timing bias a later commit removed — identically on both sides, so
it cancels in a ratio.

The gate cannot answer this question at all. A cold start is dual feasible, so
`make netlib` never enters phase 1. `bench/primal` is the only campaign that
does.

**Movers** are instances where phase 1 is most of the primal solve, so the
changed code is what the seconds are made of. **Controls** are instances the
change provably cannot reach.

## The reading

| | ganges | fit2d | fit2p | pilot | **geomean** |
|---|---|---|---|---|---|
| movers | 0.9800 | 1.0000 | 1.0122 | 1.0109 | **1.0007** |

| | grow15 | grow22 | **geomean** |
|---|---|---|---|
| controls | 0.9824 | 0.9883 | **0.9853** |

Below 1.0 means the scatter is faster.

## What it says

**No measurable wall-clock effect, in either direction.**

The controls are the reading that matters. They moved 1.5% in the direction of
"the scatter is faster" on solves the scatter never touches, so 1.5% is what
noise looks like in this reading. The movers moved 0.07%. Both are far inside
this host's 6.27% repeatability (D93).

So the review's concern is answered rather than confirmed. D199's 0.9452 is a
real work-unit gain and it does not appear in seconds on this host. It is also
not a regression: the two largest movers sit at +1.1% and +1.2%, which is
smaller than the controls' own excursion.

**No density fallback is warranted.** `price_all`'s `apat`/`anpat` pattern
falls back to a `memset` when `np` exceeds `cap`, and that guard exists for a
CAPACITY overflow — the pattern can be larger than the array kept for it.
`n_c1_at` cannot overflow: at most `nrow` positions are ever set, because only
a basic variable can be infeasible and a basis holds distinct variables, and
`c1_at` is `nrow` long. The density case is the one measured here, and it costs
nothing. Adding a threshold would be fitting a constant with no measurement
behind it.

## Limits of this reading

`pilot-ja` produced no pair and is absent from the table. `bench/primal` prints
seconds only for instances that reach `ok`, and its budget is 10x the dual's
work — so an instance near that edge can finish under one tree and overrun
under the other, which is exactly what a 5.5% work change does to whatever sits
closest to the line. Four of five movers read, and `ganges` at 94% phase-1
share and `fit2d` at 89% are the two that carry the question.

Two controls is thin. They agree with each other to 0.6%, which is why the
1.5% is quoted as a floor rather than a correction.
