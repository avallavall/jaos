# 02-175 — what compensating the checker's primal walk bought

D270. `jaos_check_solution` accumulated its row activities, their scale and
the primal objective in `long double`, uncompensated. It accumulates in
`double` now, with a Neumaier compensation and Dekker's exact product
residue. Two questions: does it move the numbers the gate reads, and by how
much does it close the gap to exact arithmetic.

The judge is `jm_exact_evaluate` (D267), which does not round at all and did
not change. That is the negative control: **the exact column must read the
same in both halves, and it does, on all 110.**

## What is here

| file | what it does |
|---|---|
| `exact-cover.c` / `run-exact-cover.sh` | the probe, copied from 02-173. One line per instance: rows, products formed, the ulp distance between the checker's objective and the exact one, both worst-row violations, and seconds |
| `exact-cover.txt` | the **after**: the compensated `double` checker |
| `run-before.sh` | puts HEAD's `src/check.c` back, runs the same probe, restores. Checks both the source and the after file back at the end rather than assuming |
| `before-exact-cover.txt` | the **before**: HEAD's `long double` checker, same day, same machine, same flags |
| `compare-before-after.sh` / `.txt` | joins the two by instance |
| `validate-d270.sh` / `.txt` | puts the review's first finding back and watches its test fail |

**Why the before was re-measured rather than quoted.**
`bench/measurements/02-173/exact-cover.txt` is D267's evidence and the
machine went down while it was being written: it stops at `pds-06`, holds 108
of the 110 instances, and has no summary line. Its entry's counts are right —
this run reproduces "75 worst-row violations differ" exactly — but they could
not be read out of what survived, so the before half is taken here.

## The reading

Both halves, from the probe's own summary line:

| | before, `long double` | after, compensated `double` |
|---|---|---|
| evaluated exactly | 110 | 110 |
| refused for limbs | 0 | 0 |
| objectives differing from exact | 0 | 0 |
| **worst-row violations differing** | **75** | **37** |

**The disagreement halves and nothing regresses.** 38 instances now agree
with exact arithmetic to the last bit that did not, and **0 instances
disagree that agreed before**. At the three significant digits the file
prints, the count goes 33 to 24 and nine instances move: `agg3`, `blend`,
`fit1d`, `fit2d`, `greenbeb`, `pds-10`, `pds-20`, `scfxm1`, `vtp-base`.

## What it does not fix, and this is the part worth reading

**The instances with the largest disagreements did not move at all.**

| instance | before | after | ratio |
|---|---|---|---|
| `cre-c` | 1.6e-14 vs 1.14e-13 | unchanged | 7.12x |
| `adlittle` | 4.58e-14 vs 2.16e-13 | unchanged | 4.72x |
| `cre-a` | 2.84e-14 vs 1.14e-13 | unchanged | 4.01x |
| `fffff800` | 4.41e-12 vs 1.44e-11 | unchanged | 3.27x |
| `degen3` | 1.67e-16 vs 1.11e-15 | unchanged | 6.65x |

Their accumulation was never the problem. The figure the checker publishes is
a **subtraction** — `interval_violation` computes `lo - act` or `act - hi` —
and where the activity sits within an ulp of its bound that subtraction
cancels almost everything it is given. A perfectly accumulated `act` does not
help: the loss is in the last operation, after the sum is finished.
`jm_exact_evaluate` does not have the problem because it subtracts exactly
too.

So the compensation fixes what an accumulator can fix and no more. What is
left is not an accumulation error and cannot be closed by a better sum.

## The gate, read line by line

All five configurations pass. The three sets read `0 regressed, 0 improved,
0 new`, 94/94 and 16/16 and 29/29, `gate: PASS`.

`bench/results/` moves, and it is supposed to: these are the checker's own
figures. **Everything else is byte-identical.** Stripping the
`checker=ok (...)` parenthetical out of every line and diffing what is left
— status, dimensions, shape, iterations, work units, presolve counts,
objective, reference, both verdicts, determinism, solution digest and basis
digest — gives an empty diff on all three files. The solver did not move one
bit.

| file | lines whose checker figures moved |
|---|---|
| `netlib.txt` | 46 of 94 |
| `netlib-kennington.txt` | 4 of 16 |
| `netlib-infeas.txt` | 0 of 29 |

Which of the ten figures moved, over the netlib set:

| field | instances |
|---|---|
| `sub`, the certified suboptimality | 30 |
| `N` | 10 |
| `row` | 9 |
| `gap` | 6 |
| `rowrel` | 3 |
| `Q` | 2 |
| `rsub` | 1 |
| `col`, `dual`, `drop` | 0 |

`sub` moving most is the review's sixth finding showing up where it said it
would: `certified_step` is the one place that consumed `act[i]` at more than
double precision, and it now gets a double. No verdict reads it. `col` not
moving at all is the other half of the same point — a column violation is a
comparison against the value itself, with no sum in it.

`25fv47` is the shape of a typical move, and it is one field by one digit:

```
BEFORE  ... drop=1.6e-16 cert=no sub=4.09e-30 rays=0 rsub=2.09e-14)
AFTER   ... drop=1.6e-16 cert=no sub=4.08e-30 rays=0 rsub=2.09e-14)
```

The baselines were rewritten by their own targets afterwards, deliberately,
because one of the figures they store is `rsub`.

## What no verdict does

Nothing here moves a verdict. Every difference is at 1e-11 or below against a
checker bar of 1e-7, which is what D267 already established and this run does
not change. The gate reads the same words either way; what moves is the
figure beside them.

## The cost

Not measured in seconds here, and deliberately: the probe builds at `-O1`
against `-Og` objects with asserts on, so its seconds say nothing about the
shipping build. What can be said structurally is that the walk trades one
`long double` multiply and two `long double` adds per nonzero for one
`double` multiply, a Dekker split and two to four Neumaier steps. The gate's
own work units do not bill the checker at all.
