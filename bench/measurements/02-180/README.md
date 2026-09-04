# 02-180 — how much of the verifier's refusal is the constant (D275)

D274 proves 30 of the 110 gate bases and refuses 74, on a Hadamard bound that
is an upper bound and a loose one. Nobody had measured how loose.
`docs/tolerances.md` said `JM_EXACT_LIMBS` was not swept and that the sweep
which means something is over the models a verifier can prove. This is that
sweep.

## What was run

```
bash bench/measurements/02-180/run-limbs-sweep.sh
```

D274's instrument over the standard 94, at four settings, with **`make clean`
between them** — without it `make` does not notice a change in `EXTRA_CFLAGS`
and the sweep measures one binary four times (D154, and the same shape at
D-10). The canary is free: the instrument prints its capacity, which is
`32 * JM_EXACT_LIMBS`, so a setting whose capacity did not move did not
rebuild and the run stops and says so.

## The answer

| limbs | capacity | proved | refused | broken | seconds |
|---|---|---|---|---|---|
| 128 | 4096 | 28 | 60 | 6 | 79 |
| 256 | 8192 | **40** | 48 | 6 | 68 |
| 512 | 16384 | **51** | 34 | 9 | 142 |
| 1024 | 32768 | **57** | 24 | 13 | 2500 |

**The default is what refuses, not the mathematics.** Four doublings take the
proved count from 28 to 57 of 94, and the refused count from 60 to 24. At
32768 bits, **70 of the 94 bases get a verdict** rather than a shrug.

**And the cost is what stops it.** 79 s, 68 s, 142 s, then **2500 s** — the
last doubling costs 18x the one before it, because a multiply is quadratic in
limbs and more instances get far enough to do real work. A verifier at 1024
limbs is a diagnostic that runs overnight, not a call.

## The two constants pull against each other

`degen3` is the clearest case and it is not a defect:

| limbs | verdict | table held | products |
|---|---|---|---|
| 128 | BROKEN, dual, 5.638e-16 | 272.4 MiB | 410037476 |
| 256 | refused | 0.1 MiB | 3618 |
| 512 | refused | 0.2 MiB | 3618 |

Its largest block is 735 rows. At 128 limbs a `jm_bigint` is 520 bytes and
the table is 268 MiB, under `VERIFY_BLOCK_BYTES`. At 256 it is 1032 bytes and
the table is 532 MiB, over it, so the call refuses before allocating. **Width
is bought at the price of block size**, and the trade is invisible in any
single run.

## Fourteen percent of the netlib bases are exactly infeasible

At 1024 limbs, thirteen of 94 are disproved, up from six:

| | stage | how far out |
|---|---|---|
| `boeing1` | dual | 1.735e-18 |
| `boeing2` | dual | 2.397e-18 |
| `bnl1` | primal | 6.971e-18 |
| `maros` | primal | 1.035e-17 |
| `scorpion` | primal | 1.281e-17 |
| `ship12s` | dual | 7.105e-15 |
| `ship12l` | dual | 1.421e-14 |
| `wood1p` | primal | 1.555e-14 |
| `ship04l`, `ship04s` | dual | 5.684e-14 |
| `ganges` | primal | 6.514e-14 |
| `sierra` | primal | 7.550e-14 |
| `modszk1` | primal | 1.080e-13 |

Every figure is four orders or more below `PRIMAL_TOL` at 1e-7 and
`DUAL_TOL` at 1e-9. **The published answers are right.** What the wider
settings buy is the ability to say so about more of them: a basis that is
optimal to a tolerance and not exactly is the normal case, not the strange
one, and it stays hidden while the arithmetic refuses to look.

## What this does not say

The sweep is over the standard 94 only, not all 139. The 16 Kennington bases
carry the widest bounds on the gate (`osa-60` at 161809 bits) and no setting
here comes near them. And nothing about a wider setting is free: at 1024 a
`jm_rational` is 8200 bytes, and `VERIFY_BLOCK_BYTES` then admits a block of
about 350 rows where at 128 it admits 990.

## Files

- `run-limbs-sweep.sh` — the sweep, with the clean and the canary
- `limbs.txt` — the four rows above
- `raw-128.txt`, `raw-256.txt`, `raw-512.txt`, `raw-1024.txt` — per instance
