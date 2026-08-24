# 02-89 — what `RSUB_FLOOR` costs, and where the knee is

D177. One constant in `bench/run.c`: `RSUB_FLOOR` from `1e-9` to `1e-16`.
No change to `src/`, so no solve moved and no digest could move.

## The defect

`bench/run.c` has one predicate that watches whether an answer got worse
rather than slower. It reports a regression when `relative_suboptimality`
passes `RSUB_FLOOR` and grows by `RSUB_REGRESSION_FACTOR = 2.0` against the
baseline.

**At `1e-9` it watched 4 solves out of 110.** On Kennington it watched none:
that set's worst value is `4.18e-14`, five decades under the floor, so the
predicate was dead across all sixteen instances. On the standard set it saw
`forplan`, `pilot`, `pilot87` and `wood1p` and nothing else.

An instance sitting at `1e-15` could therefore degrade by six orders of
magnitude and the gate would report `0 regressed`.

## The reason the floor gave was wrong, and the record refutes it

The comment said ratios mean nothing below the floor. `rsub` is deterministic,
so the only thing that moves it is a real change to the solve, and the
question is how far a legitimate one moves it.

**D171 is the sample and it is already committed.** It moved 88 of 94 digests
on the standard set, and `02-81/gate-diff.txt` kept the before and after value
for the 73 instances whose `rsub` moved at all.

| | |
|---|---|
| worst move up | **1.688x**, `scsd1`, `4.3e-17 -> 7.26e-17` |
| worst move down | 0.594x, `sctap1` |
| moved by 2.0x or more, either way | **0 of 73** |
| would have fired at any floor, including no floor | **0** |

## Why 1e-16 and not another decade

The floor's job is to keep the factor of 2 away from values so small that a
ratio between two of them says nothing. So the question is how much headroom
the factor keeps at each candidate, measured on that same change:

| floor | watched | worst legitimate move | headroom under 2.0x |
|---|---|---|---|
| 1e-14 | 10 | 1.006x (`maros`) | 1.99x |
| 1e-15 | 32 | 1.078x (`scagr7`) | 1.86x |
| **1e-16** | **55** | **1.078x** (`scagr7`) | **1.86x** |
| 1e-17 | 71 | **1.688x** (`scsd1`) | **1.18x** |

**The knee is between 1e-16 and 1e-17**, and it is one instance that puts it
there: `scsd1` ends at `7.26e-17`, so 1e-16 excludes it and 1e-17 admits it.
The value agrees with a second argument that owes the data nothing: `rsub`
divides by `1 + |primal_obj|`, so below about eps the numerator is the
rounding of the number underneath it.

Coverage goes from **4 solves to 84** — 75 of 94 standard, 9 of 16 Kennington.

The floor still has a job. Three baselines read exactly 0, and against a zero
baseline every positive value is an infinite ratio.

## The case the predicate has to catch — `predicate-validation.txt`

Three runs, one variable each, both runners built from the same `gcc` line so
that the source is the only difference. `adlittle` publishes `rsub = 1.52e-15`.
Halving its baseline value to `7e-16` makes the ratio 2.2x, and the old floor
cannot see it however large the ratio gets.

| | |
|---|---|
| parent (1e-9), doctored baseline | `0 regressed` — **does not see it** |
| working tree (1e-16), same baseline | `1 regressed`, `7e-16 -> 1.52e-15 (2.2x)` |
| working tree, committed baseline | `0 regressed` — no false positive |

The script aborts if the two sources carry the same `RSUB_FLOOR`, because an
experiment with no variable in it is one binary measured twice (D82).

## The gate at this change

`make test` and `make sanitize` exit 0. All three sets `gate: PASS` with
`0 regressed, 0 improved, 0 new`, and `bench/results/*.txt` came out
**byte-identical to the committed records**. That is what a constant in the
runner's comparison logic predicts: it changes what is compared, never what
is solved.

## What this does NOT do, and it is the important half

**The zero point is still the baseline.** This widens the predicate's reach by
five decades; it does not give it an absolute bar. A suboptimality that was
already there when the baseline was written still reads as permanently fine.
`pilot` is that case and it stays invisible to the gate.

Two candidates for an absolute bar were measured here and both are recorded
so that the next attempt does not repeat them. Ground truth is 02-83's
`refeps`; `pilot` and `pilot87` are the two instances the checker has any
chance of seeing.

| candidate | catches | top clean instance | margin |
|---|---|---|---|
| **`rsub`, what the gate already records** | `pilot`, `pilot87` | 7.4e-09 (`wood1p`) | **343x** |
| `gap_positive` on its own | `pilot`, `pilot87` | 0.002185 (`ken-18`) | **0.35x** |
| `gap_positive / (eps * sum|c_j x_j|)` | `pilot`, `pilot87` | 5.646e+07 (`wood1p`) | 199x |

**`gap_positive` on its own does not even order correctly**: `ken-18` is a
clean answer carrying a larger absolute bound than `pilot87`'s. An absolute
bar on it is refused on that alone.

**Normalising by the objective's own traffic is worse than what exists.** The
new denominator was the session's first idea and it loses to `rsub` by 343x
against 199x. The denominator was never the problem.

**`wood1p` is why none of the three is shippable as a verdict today.** It
publishes the exactly correctly rounded optimum — `refeps = 0` in 02-83 — and
carries the loosest certificate of any clean instance on either set. A bar
placed to catch `pilot87` sits 343x above it, which is a real margin and not a
large one. The quantity measures how loose the certificate is, and a loose
certificate is compatible with a perfect answer.

An absolute bar also turns the gate red on whatever is already over it, which
is `pilot` and `pilot87` today. That is the same judgement `TODO.md` item 1
carries and it is not taken here.

## Reproducing

```
bench/measurements/02-89/run-rsub-coverage.sh            # reads records only
bench/measurements/02-89/run-predicate-validation.sh     # builds two runners
```

`run-rsub-coverage.sh` refuses to take a reading when a set reads zero
instances. That is what a campaign mid-write looks like, and it happened on
the first run of this script: `make netlib-kennington` was in flight, the
Kennington column read 0, and the coverage table printed as if it were
finished.
