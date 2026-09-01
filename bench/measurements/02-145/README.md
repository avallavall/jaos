# 02-145 — the ten `simplex.c` and `presolve.c` contracts that became asserts

D232. Fourteen arms and a census, over the last two files of the comment
purge's prose-contract debt.

## What is here

| file | what it does |
|---|---|
| `run-assert-controls.sh` | one arm per assert: a one-line source edit that violates the contract, and the check that the right assert fires |
| `census-forcing.sh` | counts the population the one unfireable assert guards, over all 139 gate instances |
| `run-assert-population.sh` | the other half of the question: all 139 instances solved with `-UNDEBUG`, to show the asserts stay quiet where they should |
| `probe.c` | solves instances with asserts enabled, in one method or both; linked by the controls and the census |
| `controls.txt` | the arms, as run |
| `census.txt` | the census, as run |
| `population.txt` | the 139-instance assert-enabled run, as run |

Both scripts derive the repository root and run from anywhere (D217).
Neither is a gate tool.

## The pass criterion, and why it is not the one 02-139 to 02-142 used

Those campaigns passed an arm when the suite failed to come back clean. Here
an arm passes only when the log carries the **exact expression glibc prints**
for the assert it is for, and the record names the binary that printed it.

The `cost-left-lent` arm is the receipt. Leaving the phase-1 objective lent
makes `cost` and `c1` the same pointer at teardown, so `test_simplex` dies on
`free(): double free detected in tcache 2` — exit 134, no assert message, the
assert never reached. The old rule passes that arm and credits
`assert(s->cost != s->c1)` with a catch it did not make. The assert does fire,
on the probe.

## Why a probe and not only the unit suites

Two of the ten asserts are out of the suites' reach. The primal phase 1 runs
only under `cfg.force_primal`, which no test sets, and presolve's FORCING and
singleton-column families need a real model. `probe.c` solves the 32 smallest
standard instances plus `beaconfd`, dual and primal, in well under a second.
It runs in an arm only when the suites did not already fire that arm's assert.

## The one assert no arm can fire

`assert(v == m->col_lower[j] || v == m->col_upper[j])` in the FORCING fix
loop. Removing the guard on that shape leaves every suite green; removing the
second guard as well leaves them green too. `census-forcing.sh` says why:

| set | seen | rejected as pending | rejected as derived | applied | columns pinned |
|---|---|---|---|---|---|
| 94 standard | 1854 | 18 | 0 | 1836 | 10278 |
| 16 Kennington | 5233 | 97 | 0 | 5136 | 87225 |
| 29 infeasible | 149 | 50 | 0 | 99 | 912 |

No pinned column sits at a bound the caller's model did not carry, and the
`col_pending_dual` test takes every rejection — the bound comparison after it
takes none, because the one site that writes a derived column bound also sets
that flag. The census stops each solve as soon as presolve returns, which is
what makes 139 instances cost a minute rather than an hour.

## They also have to stay quiet

`run-assert-population.sh` builds `bench/run` with `-UNDEBUG` in a throwaway
worktree and solves every gate instance: 94 standard, 29 infeasible, 16
Kennington, **139 records and zero assertion lines**. No baseline is read —
the question is whether anything aborts, and the gate answers the other one.

About fifty minutes, and Kennington is all of it: `dual_ratio_test`'s debug
block runs a second full `admit_candidate` scan over every variable on every
iteration. The infeasible set needs `-e infeasible`; without it the runner
judges every instance against OPTIMAL, exits 1 and writes no record, which
says nothing about the asserts.

## The instrument that had to be repaired

The bound-flip arm first deleted both breaks from `bfrt_walk`. Every candidate
then retired, `live` reached zero, and `dual_ratio_test` returns at
`live == 0` **before** it calls `apply_flips`. Forty-seven tests went red and
the assert was never reached. The breaker that works removes only the width
test and reads an infinite width as zero in the spend test: exactly one
candidate retires and the rest stay live.

## Reproducing

```
bash bench/measurements/02-145/run-assert-controls.sh     # ~10 min, exit 0
bash bench/measurements/02-145/census-forcing.sh          # ~1 min
bash bench/measurements/02-145/run-assert-population.sh   # ~25 min
```

Both build in throwaway git worktrees under `mktemp -d`, carry the working
tree's `src/simplex.c` and `src/presolve.c` into them, and symlink the
gitignored instance directories. The main tree is not touched.
