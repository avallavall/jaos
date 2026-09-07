# 02-213 — the exact Farkas ray

Closes **D333**. One question, over the 29 pinned infeasible instances of
`bench/instances-infeas/`: does deriving the Farkas multipliers over the
rationals close the eleven certificates D328 could not?

## What is here

| file | what it is |
|---|---|
| `ray.py` | the run: per instance, whether a basis was published, whether the exact derivation fitted, and whether the proof file certifies with the doubles and with what the writer emits |
| `ray.txt` | its output |

## What it says

```
publish a basis                 : 19 of 29
exact derivation fits the budget: 12 of 29
certify on the published doubles: 18 of 29
certify on what the writer emits: 25 of 29
derivations that fit AND certify: 12 of 12
closed by the exact ray         : bgetam, bgindy, bgprtr, forest6,
                                  klein1, klein2, klein3
lost                            : none
```

**25 of 29, up from 18.** The seven closed are exactly the ones whose
`(A'y)_j` sat a rounding away from zero, which is what D328 measured the
eleven failures to be.

**12 of 12 is the sharper reading.** The a-priori bound admits 12 of the
19 instances that have a basis, and every one of those certifies. The
seven it refuses are refused before a limb is allocated — 11680 to 146899
bits against a capacity of 4096 — and they keep the published doubles, so
the derivation has no failure mode on this set: it fits and proves, or it
says it does not fit.

**Nothing is lost, and the two mechanisms are complementary.** The ten
instances presolve settles by itself have no basis to solve against and
were already certifying, because a site-seeded ray is one signed unit
lifted through the reductions and is exact by construction. The nineteen
the simplex settles are the ones the rounding was costing.

## A correction to this script, worth keeping

Its first version wrote the exact file only where the derivation fitted
and read the other 17 as losses, reporting 12 of 29 against 18. That is
not what the library does: `jaos_write_proof` falls back to the published
doubles when there is no derivation, so an instance can only move upward.
The `shipped` column measures what the writer actually emits, and the
number it gives is 25.

## Reproducing

```
make shared
python3 bench/measurements/02-213/ray.py
```

About four minutes; `gosh` and `cplex1` are most of it.
