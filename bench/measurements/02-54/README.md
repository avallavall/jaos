# A hostile basis makes HEAD publish a wrong optimum through the public API alone

Taken 2026-08-19, the probe D145 said was one step away. Closed as D146.

## The question

`jaos.h` promises that a caller-supplied basis "costs time and cannot
produce a wrong verdict". D145 manufactured count-valid bases inside a
candidate and got eight wrong optima; this asks whether a caller can do the
same at HEAD with nothing but `jaos_read_mps`, `jaos_set_basis` and
`jaos_solve`.

## The measurement

Five instances, sixteen deterministic hostile bases each — exactly
`num_row` columns `BASIC` at a shifting stride, everything else
`AT_LOWER` — every `optimal` compared against the same binary's cold
reference and handed to `jaos_check_solution`:

| instance | wrong optimum published | correct but checker-refused | clean |
|---|---|---|---|
| `degen2` | **16 of 16** | 0 | 0 |
| `scsd1` | **10 of 16** | 5 | 1 |
| `cycle` | 0 | 0 | 16 |
| `modszk1` | 0 | 0 | 16 |
| `woodw` | 0 | 0 | 16 |

80 trials: **26 wrong optima, 5 refused points, 0 errors.** The worst
`degen2` line publishes −1352.64 as `optimal` where the truth is −1435.178,
a 5.7% error, in 1836 iterations, `checker` refusing the point the caller
is never shown a signal about.

## What it decides

**The promise is refuted at HEAD by construction.** No candidate code is
involved: the probe binary is `src/*.c` at HEAD plus a 150-line main. The
mechanism is the one §5a has carried since D119 — the termination never
re-reads dual feasibility, so a solve that starts badly can end wrongly —
and it now has deterministic, seconds-cheap reproductions on a 444-row
instance instead of a 278003-iteration warm campaign.

This is the largest open correctness item in the repository: a public-API
caller gets `JAOS_SOLVE_OPTIMAL` and a wrong objective with nothing to say
so. It blocks D145's warm retry and it is what `TODO.md`'s head now points
at. `jaos.h`'s comment carries the defect reference until the fix lands.

## Reproducing it

`run-hostile.sh` beside this file builds `probe-hostile.c` against HEAD's
`src/` (read, never written) and reruns all 80 trials; `hostile.txt` is the
filtered result and `hostile.txt.raw` every line.
