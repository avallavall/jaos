# 02-93 — the fourth set does not reopen §3, and it says what §2 costs on a modern model

D181. No source change. Two public-API probes and one throwaway diagnostic
build, all reverted.

## What was asked

D178 left `degen2` as the only instance in twenty where D148's guard throws a
repaired warm trajectory away, and one instance cannot supply a threshold.
`TODO.md` §3's reopen condition is a second instance and §4 says a fourth
instance set is the executable form of that condition.

**The set was already here and the warm campaign had never been run on it.**
`plato-fome` and `plato-pds` came in with D115 (`bench/measurements/02-23/`),
from Mittelmann's LPopt. `plato-pds` is 6.4 hours of wall clock and was not
attempted; `plato-fome` is four instances and `fome11 → fome12 → fome13`
doubles exactly in both dimensions, which is the one family in this repository
that can say whether a cost grows linearly or worse with nothing else about the
model changing.

## §3 is not reopened: the repair never runs

Four instances, **0 repairs fired**. The block §3 is about never executes, so
the set says nothing about a doomed trajectory. Only `fome21` starts warm at
all, and its guard does not fire.

## Why, and the first probe could not tell

`build_warm_basis` refuses a short count past the cap and a long count at the
same line, and **neither printed anything**. From outside the two read
identically, and they are different questions: the cap is D151's and refused a
change, while a long map is refused because none had been measured. The probe
names every exit now — `why=no-stored-basis`, `why=short-past-cap`,
`why=long-map`.

| instance | `nrow` | mapped basis short by | past the cap of 4 by | as a fraction of `nrow` |
|---|---|---|---|---|
| `fome11` | 12142 | **681** | 677 | **5.609%** |
| `fome12` | 24284 | **1357** | 1353 | **5.588%** |
| `fome13` | 48568 | **2720** | 2716 | **5.600%** |
| `fome21` | 64574 | **0** | — | — |

**The shortfall is a constant 5.6% of rows and it doubles exactly when the
model does**: 1357/681 = 1.993 and 2720/1357 = 2.004, on a family that doubles
exactly. netlib's worst is 596 (`dfl001`).

**No cap of either shape reaches these.** The absolute cap would have to go
from 4 to 2720, and D149 measured the blanket repair at `dfl001` 172x work for
a 596-short repair the guard then threw away. A relative cap is no better:
D151 swept `S <= r*nrow` and its best mean was at r = 0.0036, and 5.6% is
**15 times** that. So the refusal holds on this set for the reason it already
had, which is worth having as an answer rather than as an assumption.

## What the set does say, and it is §2's price

The published basic count, from 02-91's probe, one solve per instance:

| instance | published count over by | warm work against cold |
|---|---|---|
| `fome11` | **8** | **1.0000** |
| `fome12` | **21** | **1.0000** |
| `fome13` | **53** | **1.0000** |
| `fome21` | **0** | **0.5258** |

**Three against three, one against one.** The three that publish a wrong count
are exactly the three whose warm re-solve does bit-identical work to the cold
one, and the one that publishes an exact count saves **47% of the work**. That
is the same attribution D129 and D130 made on netlib, reproduced on a set
netlib's conclusions were never taken on.

**And the two counts grow at different rates on the same family.** The mapped
shortfall doubles exactly with the model; the published over-count goes
8 → 21 → 53, which is 2.63x then 2.52x. They are different objects with
different mechanisms — the mapping drops stored-basic members presolve removes
again, and postsolve's `SINGLETON_COL` adds one per firing — and this family is
the only place in the repository where the two rates can be read apart.

`fome13`'s 53 is larger than netlib's worst, which is `fit1p` at 21 (D179).

## Reproducing

```
bench/measurements/02-93/run-plato-basis.sh  fome     # published count, 1 solve each
bench/measurements/02-93/run-plato-mapped.sh          # mapped count and the refusal
bench/measurements/02-93/run-plato-warm.sh   fome     # the whole warm campaign, 26 min
```

`run-plato-mapped.sh` opens with `degen2` as its control: a netlib instance
whose map arrives short by 1, so the repair fires and the guard rejects it at
12.91. If the control prints nothing the harness is broken and every reading
under it is worthless.

Two things that cost time here and are written down so they do not cost it
again. **Stop a running probe before editing its patch script**: the trap
reverts with the anchors it finds at exit, so editing mid-run leaves the tree
instrumented. And **the first version of this reading inferred the map arrived
long** from the published over-count — the published basis and the mapped basis
are different objects, and the map arrives short.
