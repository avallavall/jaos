# The row-activity check finds a real one: pilotnov's published point does not satisfy its own equality row

Taken 2026-08-19, the standing debt `numerics-reviewer` proposed in its own
words. Closed as D153.

## What was open

Two producers in the replay assign `sol_row[i]` outright where every other
producer accumulates: `JM_PS_EMPTY_ROW` writes `0.0` and `JM_PS_SINGLETON_ROW`
writes `rec->coef * xv`. Both are correct today by an argument about arena
order that nothing checks — an empty row had every column dead before it
fired, a singleton row had exactly one live column — and **the class has
already cost one campaign (D106)**.

The proposed enforcement: one pass at the end of `jm_postsolve_expand` that
recomputes every row's activity from `sol_col` and asserts it matches
`sol_row`. That is the checker's own predicate, extended from the instances
that reach the checker to all of them.

**D152 is what made this runnable.** Before it, eleven of the 94 aborted
before reaching any new assert.

## The instrument, validated before it was believed

Two faults injected, each the shape the check exists for
(`activity-check.txt`):

| build | check fires on | with asserts OFF |
|---|---|---|
| `JM_PS_EMPTY_ROW` overwrites a row whose share already arrived | **45 of 94** | 0 of 94 |
| one published activity moved by 1.0 | **86 of 94** | 0 of 94 |

The "asserts OFF" column is the control that matters: it says the injected
fault is silent on its own, so the aborts are the check firing and not the
fault breaking something else first.

## Getting the window wrong twice, and what that cost

The check needs a tolerance and the first two shapes were wrong. Both are
recorded because both are the obvious thing to write.

**First: a fixed multiple of eps times the traffic.** It fired on `osa-30` and
`osa-60` as well. Their rows carry **72554 and 173365 nonzeros**, and a naive
sum of n terms is only bounded by `(n-1)·eps·Σ|t|`, not by a constant times
eps. Taking the n out leaves **0 rows disagreeing on both** — the window's
shape was wrong and the solver was not.

**Second, before that: no OPTIMAL gate.** The check fired on all 29
netlib-infeas instances. Both call sites run the replay whatever the verdict,
because the index mapping is owed even when the answer is a stopping point; on
that path `sol_col` and `sol_row` describe where the method stopped and are
not required to agree. Asking them to finds the convention, not a defect.

A third false alarm was the harness rather than the check: the first run
reported 29 of 29 infeasible instances failing because the script omitted
`-e infeasible`, so every instance looked like a gate failure. The script now
separates an assert abort from a gate failure and reports them in different
columns.

## The finding

With the `(n-1)` window and the OPTIMAL gate, **138 of the 139 instances
pass and `pilotnov` does not** (`disagreeing-rows.txt`).

| | rows disagreeing | worst, fixed window | worst, `(n-1)` window |
|---|---|---|---|
| `osa-30` | 2 | 1.69x | **0 — explained** |
| `osa-60` | 2 | 4.31x | **0 — explained** |
| **`pilotnov`** | **36** | 524x | **18 rows still out, worst 131x** |

The decisive line, because it cannot be arithmetic:

```
row=931  nnz=3  published=0  recomputed=-1.9313301891088486e-07
         diff=1.93e-07  traffic=4.15e+06  rowlo=0  rowhi=0
```

**Row 931 is an equality at zero with three nonzeros.** Its published
activity reads exactly 0.0; the published columns make -1.93e-07. Relative to
the row's traffic that is 4.6e-14, about 100 times what a three-term sum can
accumulate — the bound is 2·eps ≈ 4.4e-16. The worst overshoot in the set,
131x, is on a row of **five** nonzeros.

So the published point does not satisfy a constraint the published row
activity claims it satisfies. No answer is wrong today: the checker judges
relative to scale and passes it, and the gate reads `checker=ok`.

`pilotnov` has form here. D118 saw it publish an objective 29% wrong as
`optimal` under a refused candidate, and D119 found the same reduced model
reaching Koch's optimum to the last bit at a shorter refactorization
interval. Whether this is the same mechanism is not established and is not
claimed.

## Why the check ships opt-in

It is behind `-DJAOS_VERIFY_ACTIVITY` and is **not** on in a plain
assert-enabled build. D152 had just bought the property that every one of the
94 standard instances runs under `-UNDEBUG`; trading that away for a check
that reports one already-open defect is a net loss until the defect is
repaired. The day `pilotnov` is fixed, the guard moves under `#ifndef NDEBUG`
and becomes an invariant.

**It is inert by default, and that is measured rather than argued**: the
compiled objects without the flag have the same md5 as HEAD's, so the gate
campaign at HEAD carries over untouched. `make test`, the
`-DJAOS_NO_PRESOLVE` reference build and `make sanitize` all exit 0.

## Reproducing it

```
bash bench/measurements/02-62/run-activity-check.sh     # validate, then run
bash bench/measurements/02-62/run-disagreeing-rows.sh   # what the rows are made of
```
