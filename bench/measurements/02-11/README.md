# Why presolve makes `grow22` eleven times more expensive

Opened by D103's campaign, which measured presolve's price over the three sets
and found two instances of 94 past 2x:

```
grow22   4.4e7 -> 4.9e8 work  11.16x    2180 -> 16382 iterations  7.51x
grow7    6.4e6 -> 5.5e7 work   8.56x     545 ->  4805 iterations  8.82x
```

Not a regression from anything in D103 — the records either side of it are
bit-identical — so this arrived with presolve and nobody had asked about it.

## One family, twenty firings, and nothing else

`families.c` calls `jm_presolve_run` directly and prints every counter.

```
instance     rows   cols     nz | emptR emptC  sglR  sglC freeC  fixC forcR  redR
grow7.mps     140    281   2592 |     0     0     0    20     0     0     0     0
grow15.mps    300    625   5600 |     0     0     0    20     0     0     0     0
grow22.mps    440    926   8232 |     0     0     0    20     0     0     0     0
```

`sglC` is `JM_PS_SINGLETON_COL`, the cost-0 bounded singleton column (D95).
It is the **only** family that fires on any of the three, so it is the whole
of the difference between the presolved and the un-presolved solve. No further
attribution experiment is needed: `-DJAOS_NO_PRESOLVE` already measured the
other side.

`greenbea` and `25fv47` are in the table as controls — instances where seven
of the families fire together and presolve pays.

## What the twenty firings do

Removing a cost-0 column that lives in exactly one row means that row's bounds
must widen by everything the column could have contributed, or the rest of the
row could be driven somewhere the column can no longer complete. The
relaxation is forced by correctness. `relax.c` prints what it leaves.

Every one of the twenty rows, on all three models, is an **equality row
`== 0`**, and every one becomes a **range**:

```
   row    col |      rlo      rhi |     cmin     cmax |    newlo    newhi
   420    926 |       0       0 |-3.75e+05      -0 |        0 3.75e+05
   421    927 |       0       0 |-2.5e+05       -0 |        0  2.5e+05
   422    928 |       0       0 |  -5e+05       -0 |        0    5e+05
```

Twenty equality constraints become ranges of up to half a million. The rows
are still there and the row count does not move — 440 to 440 — so the
`presolve=` field reports almost nothing happened: 20 columns and 20 nonzeros
of 8252. What actually happened is that **twenty exact pins became twenty
things that constrain nothing in practice**, and a dual simplex steers by
those pins.

None of the sixty records leaves a row unconstrained on both sides, so a check
for "did this row become free" would not catch it. The damage is the width,
not the infinity.

## The part that is not explained

`grow15` gets the same twenty firings on the same rows with the same
magnitudes — the three are one model family at three sizes — and presolve
**halves** its iteration count, 21653 to 11453. So the relaxation is not
uniformly harmful, and no reading here says which way it will go on a model
that has not been run.

That is why nothing is proposed as a fix. What the readings support is a
candidate rule and a reason to measure it: **do not fire when the relaxation
widens the row beyond some multiple of its own scale.** Here an `== 0` row
becomes `[0, 5e5]`, which is an unbounded relative widening, and it is the
extreme case rather than a typical one. Any such threshold needs a sweep on
both sides and a campaign, because the family pays 0.810x over the standard
set as it stands (D103) and a rule that stops it firing pays that back.

## Rebuilding

```
gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc \
       families.c src/*.c -o families -lm
gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc \
       relax.c src/*.c -o relax -lm
```

Both read. Neither writes to the model, and neither is ever compiled into the
shipping build.
