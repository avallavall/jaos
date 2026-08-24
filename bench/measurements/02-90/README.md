# 02-90 — `scsd1` and `degen2` do not lose the same way, so there is one doomed trajectory and not two

D178. No behavioural change: one comment in `src/simplex.c`, proved
object-identical by `comment_only.sh`. The warm records are refreshed because
they were 24 `src/` commits stale.

## The question

`TODO.md` §3 asked for **a predictor of a doomed trajectory** for `scsd1` and
`degen2`, described as losing the same way — "each with warm iterations
exactly equal to cold", which is D148's guard throwing the repaired trajectory
away and charging the attempt plus the whole cold solve. The shortfall was
already refused as the predictor: both are short by 1, the same as the sixteen
that win (D151).

The item named three untested mechanisms and asked for a hypothesis rather
than another sweep. The hypothesis taken here was the fourth and the most
direct: **`build_warm_basis` closes the shortfall with two different loops**,
one promoting the logical of an uncovered row and one walking in index order,
and the second is a free choice where the first is structurally forced. If the
losers use the free branch and the winners the forced one, the repair knows
which it used before it pays for anything.

## The premise is false, and that is the finding

A diagnostic build printed, for every instance whose mapped basis arrives
short by 4 or fewer, what the repair saw and what the guard then did.

| | `degen2` | `scsd1` |
|---|---|---|
| warm iterations / cold | 565 / 565 | **314 / 89** |
| settled dual violation, warm | **12.91** | **0** |
| guard fires | **yes** | **no** |
| work against cold | 3.6751x | 3.7165x |

**Only `degen2` is D148's guard.** `scsd1` reaches a dual feasible point, the
guard accepts it, and the warm solve genuinely runs 314 iterations where the
cold one runs 89. Its warm and cold counts are not equal and have not been for
some time; `degen2`'s are equal because the cold restart resets the counter,
which is the guard's documented behaviour and not a coincidence.

So the item asks for a predictor of a phenomenon that occurs **once** in the
twenty instances that repair. D46 is the entry that says what a rule read off
one instance is worth.

`lotfi` is a third instance costing more warm than cold (1.2753x) and its
guard does not fire either. `25fv47` was a fourth at D151 and now wins.

## Nothing the repair knows before the solve separates them

Eleven quantities, all available inside `build_warm_basis` before a single
pivot. A column separates only if no winner's value falls inside the range the
losers span.

| quantity | losers | result |
|---|---|---|
| shortfall `S` | 1, 1, 2 | 14 winners inside |
| rows the wanted basis leaves uncovered | 0, 0, 0 | 15 winners inside |
| promotions by the uncovered-row loop | 0, 0, 0 | 15 winners inside |
| promotions by the index-order loop | 1, 1, 2 | 14 winners inside |
| `nrow` | 77, 134, 443 | 8 winners inside |
| `ncol` | 292, 532, 759 | 3 winners inside |
| wanted-basic columns | 70, 96, 334 | 7 winners inside |
| wanted-basic logicals | 7, 38, 109 | 12 winners inside |
| `S / nrow` | 0.0023, 0.013, 0.0149 | 9 winners inside |
| wanted logicals / `nrow` | 0.0909, 0.246, 0.2836 | 8 winners inside |
| `ncol / nrow` | 1.2009, 2.1791, 9.8571 | 12 winners inside |

**The branch hypothesis is refuted outright.** 18 of the 20 promote entirely by
index order, including all three losers and 15 winners. The only two instances
that use the uncovered-row loop are `pilot-we` and `ship08l`, and both win
— at 0.0939x and 0.0332x, two of the three best ratios in the table.

`ncol` is the narrowest column and it is not a predictor either: three winners
sit inside a range whose ends are 2.6x apart, in a variable that spans 35 to
3148 over twenty points. Fitting on an interval that is merely sparse is what
D46 warns about.

## The set has moved since D151, measured rather than assumed

D151's per-instance table is 2026-08-19's tree and 24 commits to `src/` have
landed since. The campaign at D177:

| | D151 predicted | now |
|---|---|---|
| netlib work geometric mean | 0.1916 | **0.1910** |
| worst instance | 4.65x, `scsd1` | **3.7165x**, `scsd1` |
| `degen2` | 4.09x | 3.6751x |
| `25fv47` | 1.0349x, WORSE | **0.9854x**, wins |
| instances costing more warm than cold | 4 | **3** |
| Kennington work geometric mean | 0.0070 | **0.0071** |

The mean is where the sweep put it. The two figures the source comment quoted
are not, and the comment is corrected.

## Reproducing

```
bench/measurements/02-90/run-warm-probe.sh
```

It applies four hooks behind `#ifdef JAOS_DIAG`, builds to `build/diag/`, runs
the twenty instances at `-j 1`, and reverts the hooks however it exits. Two
things it does on purpose:

- **`-j 1`.** The `DIAG` lines go to stderr and belong to the instance named
  just before them. At `-j 12` twelve children share one stderr.
- **A canary before the table.** A probe that prints nothing reads exactly like
  a set where the repair never fires, and the table would still print.

The verdict column comes from that run's own work ratios, never from a table
in another measurement directory. Reading 02-60's is what would have carried
`scsd1`'s stale 89-against-89 into this entry.

## What this does not close

**`degen2` is still open and it is now the whole of §3.** Why a repaired warm
basis on that model settles at a point 12.91 outside dual feasibility is not
answered here, and one instance cannot supply a threshold. What would change
that is a second instance of the same mechanism, which means a wider model
population — the fourth instance set `TODO.md` §4 carries.

**`scsd1` is a different question and it is new.** Its warm start is accepted,
correct, and 3.5x longer than starting cold. Nothing here says why.
