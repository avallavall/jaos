# Six instances publish more basic variables than rows, not three — and D129 said three

Taken 2026-08-18, naming what D129 could only count. **It corrects D129 twice.**
Closed as D130.

## What D129 could not do

D129 measured 23 netlib instances losing their warm start to
`nbasic != s->nrow` in `build_warm_basis`, and printed the dimensions of each
mismatch. It could not name them: the driver forks a child per instance and
twelve share one stderr, so an instance name printed by the driver cannot be
matched to a line printed by the library.

This runs at `-j 1`, where the driver does not fork, and prints the instance
name immediately before the warm solve. Name and outcome are then adjacent by
construction.

## The full tally, netlib's 92 measured instances

| outcome on the warm re-solve | count |
|---|---|
| accepted | 66 |
| short (`nbasic < nrow`) | 17 |
| **over (`nbasic > nrow`)** | **6** |
| nothing was stored at all | 3 |

`17 + 6 = 23`, which is D129's count of mismatches, and `23 + 3 = 26`, which
is D129's count of instances at a work ratio of exactly 1.0000. Both totals
hold. What did not hold is the split.

## Correction one: it is six over, not three

```
OVER   80bau3b    nrow=2022  nbasic=2043  over by 21   (ncol=9077 nvar=11099)
OVER   finnis     nrow=399   nbasic=411   over by 12   (ncol=518  nvar=917)
OVER   standmps   nrow=407   nbasic=418   over by 11   (ncol=1012 nvar=1419)
OVER   standata   nrow=300   nbasic=310   over by 10   (ncol=1013 nvar=1313)
OVER   vtp-base   nrow=52    nbasic=54    over by 2    (ncol=80   nvar=132)
OVER   boeing1    nrow=298   nbasic=299   over by 1    (ncol=369  nvar=667)
```

D129 said three, over by 10, 11 and 2. **The reading was taken from a
truncated terminal output** — `tail -40` on a run that printed 23 mismatch
lines — so the first seven were never seen. The three it names are real and
are the last three of six.

The shape spans **1 to 21**, so it is not a small-model artefact: `80bau3b`
is over by 21 on 2022 rows.

The seventeen short are `degen2`, `degen3`, `dfl001`, `fffff800`, `fit1d`,
`kb2`, `maros`, `pilot4`, `scfxm1`, `scfxm2`, `scfxm3`, `scsd1`, `scsd6`,
`scsd8`, `ship08l`, `wood1p`, `woodw` — sixteen of them by exactly one and
`maros` by five.

## Correction two: `no-basis` does fire, on three, and that explains them

D129 said *"`no-basis` never fires on the warm solve, so nothing is lost for
any other reason"*. It fires on three: **`pilotnov`, `scrs8` and `share1b`**.

Those are exactly the three D129 set aside as *"branches taking zero
iterations on both sides and identical for a legitimate reason"* — and the
reason is now known rather than assumed. **The anchor solve stored no basis at
all**, so there was never anything for the warm solve to start from. Their
work ratio of 1.0000 is not a lost warm start; it is the absence of one.

D129's count of 23 lost to the count defect is unaffected, because these three
were already excluded from it.

## Why the probe was wrong twice before it was right

Both are in the script, and both are the kind of error that reads as a clean
result.

- **A call counter does not survive `-j 1`.** The first version identified the
  warm solve as the second `build_warm_basis` call of the process. At `-j 12`
  the driver forks and the counter resets per instance; at `-j 1` it does not
  fork, so only the first instance in the whole run had a second call. It
  reported `seen=1`.
- **The marker went on the cold solve.** The second version inserted the name
  before the second `st = jaos_solve(m);`, which is the cold one — the anchor
  above them is written `if (jaos_solve(m) != JAOS_OK)` and is not a match. It
  reported 92 instances, all of them `no-basis`, all fields empty, and it read
  as a finding.

The script now carries a proportion canary: every warm solve reporting
`no-basis` means the marker is on the wrong call, while a few of them
reporting it is a different fact about those instances.

## What is left open

Unchanged, in `TODO.md`: the repair. It now has a price (D129) and a shape
this entry adds. **A repair aimed at the missing-one case answers sixteen of
the twenty-three**, and says nothing about `maros`'s five short or the six
that are over.

## Reproducing it

`run-overcount.sh`, beside this file. It runs `-j 1` deliberately; at `-j 12`
the names cannot be matched. `src/` is read and never written.
