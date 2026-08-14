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
will already be gone when a doubleton pass would see them.

## The residue, and it is where the family stops being simple

`diag_doubleton.inc` is the same instrument at presolve's own exit, on the
model presolve publishes. `run-doubleton.sh` builds it into a copy of the tree
and runs it; the repository is not modified. It refuses to report until
`validate_doubleton.c` reproduces a six-column hand answer,
`eqrows=3 dbl=3 dblfree=1 subnz=6`.

| set | surviving | share of live rows | **with a free endpoint** |
|---|---|---|---|
| netlib | 6153 | **8.55%** | **19** |
| Kennington | 60382 | **29.36%** | **0** |

The five live families barely touch them: 6504 as loaded to 6153 surviving on
netlib, 72459 to 60382 on Kennington. So the population is real and it is
still there when a sixth family would run.

**The last column is the finding.** A doubleton `a*x_p + b*x_q == c` is
substituted by eliminating one endpoint. Where that endpoint is free — box
`(-inf, +inf)` — the substitution needs nothing else: the variable had no
bound to violate. Where it is not, its bounds have to be transferred onto the
survivor, and that is bound tightening, which **D97 refused in six designs,
every one of which returned INFEASIBLE on models that have an optimum**.

19 of 6153, and 0 of 60382. **99.7% of this family is behind D97**, and the
part that is not is six instances of netlib: `capri` 6, `pilot-we` 9, and one
each on `greenbeb`, `perold`, `pilot-ja` and `stair`.

So the reading does not say "build doubletons next". It says the prize behind
D97 is far larger than D97 knew when it refused — D97 weighed bound tightening
on its own, and this is a second family that cannot exist without it. That is
a reason to re-derive the over-tightening D97 recorded, which is already its
stated reopen condition, and not a reason to reopen it by assertion.

`subnz`, the nonzeros one non-interacting pass could remove, reads 20686 of
781125 on netlib (2.65%) and 130419 of 2544048 on Kennington (5.13%). Rows
matter more than nonzeros for an iteration count, but the figure is here
because it is the one that would be quoted otherwise, and it is smaller than
the row share by a factor of three.

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
