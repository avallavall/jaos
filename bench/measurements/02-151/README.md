# 02-151 — two singleton-row replay contracts

D238. Two contracts from `bench/measurements/02-121/presolve.c.md`.

## What is here

| file | what it does |
|---|---|
| `run-replay-controls.sh` | six arms, 94 standard and 29 infeasible each |
| `controls.txt` | the arms, as run |

Derives the repository root and runs from anywhere (D217). Not a gate tool.

## These cost a whole solve each

They are **postsolve** asserts, so there is no early return to lean on the way
02-148 and 02-150 could — the simplex has to finish before the replay runs.
Kennington is left out: six arms of it under `-UNDEBUG` is about five hours,
and the standard set carries every presolve family.

## The two

| the contract | the check |
|---|---|
| the row's coefficient is the divisor | `rec->coef != 0.0` before the branch that divides by it |
| the owned bound is one the ROW induced | `dc > 0.0 ? v0 > col_lower[j] : v0 < col_upper[j]` |

**One contract on the list is a tautology and was not taken.** It proposes
asserting `v0 == rec->lo || v0 == rec->hi` inside the `this_row_owns` branch,
and that branch is *defined* by that equality. The divisor guard replaces it:
a singleton row has exactly one live entry, and a zero recorded there makes
every dual the row publishes infinite.

## The interior property is guarded twice

`break-interior` first tried removing only `zero_works`'s two bound tests, and
fired nothing. The reason is structural: `this_row_owns` requires
`row_tightens_lo`, and a row that tightened put `rec->lo` strictly above the
caller's bound — so `v0 == rec->lo` is already strictly interior whatever
`zero_works` says.

The breaker that works also lets a record claim a bound it did not induce.
Either guard alone covers the property, which is the same shape as D232's
FORCING assert and D233's reenter path.

## The arms

| arm | firings | what it says |
|---|---|---|
| `intact` | 0 | both hold |
| `canary-coef` | 70 | the divisor line is reached on 70 instances |
| `canary-interior` | 61 | the owned branch is reached on 61 |
| `break-coef` | 70 | a zero coefficient in the record |
| `break-interior` | 48 | a record owning a bound it did not induce |
| `restored` | 0 | — |

## Reproducing

```
bash bench/measurements/02-151/run-replay-controls.sh   # ~20 min
```
