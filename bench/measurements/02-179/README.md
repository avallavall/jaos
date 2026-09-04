# 02-179 — what `jaos_verify` proves on the gate (D274)

D273 said the bound would admit 36 of the 110 gate instances that have a basis
to read, and said in the same breath that Hadamard is an upper bound and a
loose one, so 36 was a floor. This runs the verifier that was built against
that measurement and reports what it did.

## What was run

```
bash bench/measurements/02-179/run-proofs.sh          # all three sets
bash bench/measurements/02-179/run-proofs.sh netlib   # the first only
```

## The answer

**30 proved, 74 refused, 6 broken.**

| | |
|---|---|
| bases with an optimum | 110 |
| the bound admitted | **36** |
| of those, proved optimal exactly | **30** |
| of those, disproved | **6** |
| refused a priori, nothing allocated | 74 |

**The bound admitted exactly the 36 D273 predicted.** Two instruments written
separately, one measuring a budget and one spending it, land on the same
population instance for instance. That is the strongest cross-check either of
them gets.

The 30 proved: `adlittle`, `afiro`, `beaconfd`, `blend`, `d6cube`, `degen2`,
`fit1d`, `fit1p`, `fit2d`, `fit2p`, `kb2`, `lotfi`, `recipe`, `sc105`,
`sc205`, `sc50a`, `sc50b`, `scsd1`, `sctap1`, `sctap2`, `sctap3`, `seba`,
`share2b`, `shell`, `ship08l`, `ship08s`, `standata`, `vtp-base`, `ken-07`,
`pds-02`.

## The six that are broken, and why that is a result rather than a defect

| instance | stage | how far out |
|---|---|---|
| `degen3` | dual | 5.638e-16 |
| `ship12s` | dual | 7.105e-15 |
| `ship12l` | dual | 1.421e-14 |
| `ship04l` | dual | 5.684e-14 |
| `ship04s` | dual | 5.684e-14 |
| `sierra` | primal | 7.550e-14 |

Every one is smaller than `PRIMAL_TOL` at 1e-7 and than `DUAL_TOL` at 1e-9 by
five orders or more. **The published answers are not wrong**: five of the six
have one nonbasic reduced cost of exactly the wrong sign and `sierra` has one
basic value exactly outside its bound, in each case by an amount the simplex
is built not to notice. A basis is optimal to a tolerance, which is what a
floating-point simplex promises, and this is the first instrument here that
can tell that apart from optimal.

`jaos_check_solution` accepts all six, correctly, against its own bar.

## The cost

| instance | rows | blocks | largest | held | products | proof |
|---|---|---|---|---|---|---|
| `sc205` | 205 | 22 | 184 | 17.1 MiB | 7276112 | 7.34 s |
| `d6cube` | 415 | 193 | 223 | 25.2 MiB | 13205202 | 4.62 s |
| `fit2d` | 25 | 6 | 20 | 0.2 MiB | 143645 | 0.71 s |
| `sctap3` | 1480 | 1475 | 5 | 0.0 MiB | 11964 | 0.02 s |
| `pds-02` | 2953 | 2931 | 23 | 0.0 MiB | 25907 | 0.02 s |

Two instances take more than a second and the rest are under a tenth. The
cost follows the largest BLOCK and not the number of rows: `pds-02` has 2953
rows in 2931 blocks and costs 0.02 s, while `sc205` has 205 rows in 22 blocks
with one of 184 and costs 7.34 s. That is what D272's block measurement buys.

## The seconds here are not the gate's seconds

**`run-proofs.sh` links `build/dev/*.o`, which is `-Og`.** The gate links
`build/release`, which is `-O3 -flto -march=native -DNDEBUG`. On the same
tree the gate solves `ken-13` in 12.97 s and the `solve_s` column here reads
52.76 s for the same solve: **4.6x, and it is the link and not the instance.**

So the absolute figures above are about 4.6x what the same work costs in a
shipping build. What is comparable is the ratio inside one line, because both
halves run in the same binary. Sixteen other instruments under
`bench/measurements/` link the same way; three of them
(`02-145`, `02-146`, `02-147`) must, because they exercise asserts and
release compiles those out.

## Files

- `proofs.c` — the instrument
- `run-proofs.sh` — builds and runs it; `netlib` for the first set alone
- `proofs.txt` — all three sets
- `proofs-netlib.txt` — the first set, from an earlier run of the same tool
