# 02-91 — a rule wider than the firing row has a supply on 19 of 24, none at all on two, and still no rank argument

D179. No source change. A public-API probe: three calls per instance, no
instrumented build and no worktree.

## The question

`TODO.md` asks for "a rank argument **wider** than the firing row" after D141
closed every local repair with a count: of the firings that publish a basis one
member too long, **66 of 80 and 86 of 152 have no other basic column of that
row resting on its own bound**, so no rule confined to the row can close the
residue.

Before designing a wider rule, the thing to know is whether a wider rule has
anything to work with. This counts the supply it would draw on.

A candidate is a basic variable — column or logical — whose **published value
rests exactly on one of its own declared bounds**. Demoting such a variable to
`AT_LOWER` or `AT_UPPER` is status-consistent on its own: the status claims the
variable rests on that bound, and it does. A fixed variable is excluded, since
it is nonbasic-eligible on both sides and demoting it says nothing.

**This does not attempt the rank argument.** Whether the remaining set is still
nonsingular is exactly the missing piece, and it is not answered here.

## The count reconciles with 02-48, on an instrument sharing no code with it

**24 netlib instances publish a basis one or more members too long, and 0
Kennington instances do.** 02-48's probe counts solves rather than instances,
and the gate solves each instance twice; 24 × 2 = **48**, which is the figure
D171 left. That is two independent routes to the same number.

## The supply, and where it runs out

| tier | instances whose over-count the model-wide supply covers |
|---|---|
| **exact equality with a bound** | **19 of 24** |
| within one ulp of a bound | 19 of 24 |
| within 1e-9 relative of a bound | 21 of 24 |

The five it does not cover, at exact equality:

| instance | over | supply, exact | ulp | 1e-9 rel |
|---|---|---|---|---|
| `bandm` | 18 | 2 | 2 | 8 |
| `capri` | 6 | 3 | 4 | **6 — covered** |
| **`fit1p`** | 21 | **0** | **0** | **0** |
| `nesm` | 18 | 0 | 4 | **18 — covered** |
| **`share1b`** | 2 | **0** | **0** | **0** |

**`fit1p` and `share1b` have no candidate at any tier.** Not one basic variable
in either model rests within 1e-9 of its own bound. So no demotion rule of this
shape can close them, at any tolerance, and loosening the window is not a
route — which is the same shape of answer D141 gave the within-row rule, one
level out.

`bandm` is the third that no tier reaches: 8 candidates against an over-count
of 18.

## What this says about the design

**A wider rule is worth designing and it does not close the item.** Where the
within-row rule had nothing on 66 of 80 firings, the model-wide supply covers
19 of 24 instances outright. That is a real improvement and it is bounded: 3
instances stay wrong at every tolerance measured, so the residue survives any
rule of this family.

**The rank argument is still the whole of the work.** Having a candidate is
necessary and not sufficient: demoting a variable whose column is the only one
covering some row makes B singular, and nothing here tests that. Postsolve has
no factorization and adding one is what the design would have to justify.

**Which is what the published rules already say to do.** D137 records Galabova
2023: HiGHS **attempts** an assignment and falls back rather than deriving one,
and the bar is a valid starting basis rather than the optimal one. This
measurement puts a number under the "falls back" half: on this population it is
at least 3 of 24, and 5 of 24 without a tolerance.

## Reproducing

```
bench/measurements/02-91/run-demote-supply.sh
```

Both sets. The Kennington section prints a header and no rows, and that is the
result rather than a failure: that set publishes an exact basic count on all
16, which is D139's outcome and unchanged since.
