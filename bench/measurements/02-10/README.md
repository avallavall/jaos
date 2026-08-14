# Why HiGHS presolves the two instances that decide the comparison

Opened by D104. At the P0 rung `maros-r7` is JAOS's worst instance against
every competitor, and `stocfor3` is close behind. These are the readings that
say what is happening there, and one of them refutes the obvious answer.

Nothing here is a plan. It is what the models are and what HiGHS reports, so
that a family is proposed against a count rather than against an impression.

## What each side removes

JAOS's figures are the `presolve=` field of `bench/results/`; HiGHS's are its
own log, run at the P0 settings with the log left on. Reading a solver's own
log output is what the comparison harness already does on every run — D12
forbids writing JAOS's code from someone else's, which is a different thing,
and the same line was drawn for netlib's `emps` (PLAN Q6).

| instance | JAOS removes | HiGHS removes |
|---|---|---|
| `maros-r7` | 0 rows, 0 cols, 0 nonzeros | 984 rows (31%), 2803 cols (30%), **64654 nonzeros (45%)** |
| `stocfor3` | 58 rows, 32 cols, 308 nz | 8416 rows (50%), 8155 cols (52%), 14431 nz (22%) |
| `degen3` | 0, 0, 0 | 98 rows, 96 cols, 507 nz |
| `truss` | 0, 0, 0 | **nothing — "Not reduced"** |

`truss` is the control and it earns its place: it is the same coarse shape as
`maros-r7` and neither solver reduces it, so a structural story that does not
separate the two is not the story.

## The obvious answer, and why it is wrong for `maros-r7`

`shape.c` counts structure only, on the model as loaded. It reads; it changes
nothing. Built against `src/` because the public header does not expose row
degrees.

```
maros-r7   3136 equality rows, 100% of the model, ALL of degree 4 or more
           0 doubleton equalities, 0 rows of degree 2, 0 free columns
truss      1000 equality rows, 100%, ALL of degree 4 or more
           0 doubleton equalities, 0 rows of degree 2, 0 free columns
stocfor3   8829 equality rows (53%), of which 1960 are doubletons
           7846 rows of degree 2 out of 16675
```

So **`maros-r7` and `truss` are indistinguishable on every structural count
taken here, and HiGHS reduces one by 31% of its rows and the other by nothing
at all.** Whatever HiGHS finds in `maros-r7` is not doubleton substitution,
not a free column, and — from `bench/measurements/02-07/`'s counter, which
reads `remrow=0 remcol=0 dualfix=0` there — not a duplicate row, a duplicate
column or a dominated column either. It is none of the eight families JAOS
has ever scoped. **That question is open and this directory does not answer
it.**

## What it does answer: doubleton equalities are worth counting

A doubleton equality is an `==` row with exactly two entries. One variable is
substituted out, the row disappears, and every nonzero the substituted column
had goes with it. It is not among JAOS's five live families and not among the
three D101 deferred.

`shape-all.sh` counts them over both feasible sets, on the model as loaded:

| set | doubleton equality rows | of all rows | instances carrying one |
|---|---|---|---|
| netlib | 6504 | **7.53%** | 67 of 94 |
| Kennington | 72459 | **28.15%** | 12 of 16 |

Worst instances: `stocfor3` 1960, `bnl2` 849, `greenbeb` and `greenbea` 371
each. On Kennington, `ken-18` alone carries 48276 — and `ken-18` is the
slowest instance in that set.

**Against D101's 0.15%, this is fifty to a hundred and ninety times larger.**
D101 deferred three families on a count; this is the count that says a fourth
is a different proposition.

**Upper bound, not an estimate.** These are the rows in the model as the
reader hands it over. JAOS's five families run first and some of these rows
will already be gone when a doubleton pass would see them. The instrument that
measures the residue properly is `bench/measurements/02-07/`'s counter, which
runs at presolve's exit; extending it to this family is the next measurement,
not this one.

## The instrument, and the two faults it caught in itself

`shape.c` was checked against hand-read values before its totals were
believed: `stocfor3` must read 1960 doubletons and `truss` must read 0, and
`shape-all.sh` prints that check on every run.

It earned its place twice. The first aggregation read the literal `2` out of
`of degree 2` instead of the count beside it, so every instance reported
exactly 2 and the set totalled 188 — a number that looks like a reading. The
second matched `rows of degree 2` as well as `of degree 2`, adding 2 per
instance to every total. Both were found by the known-value check and neither
by looking at the totals, which were plausible each time.
