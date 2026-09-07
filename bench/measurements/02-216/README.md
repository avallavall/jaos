# 02-216 — what a larger limb budget buys

Closes **D337** as a refusal. One question: D333 left four of the 29
pinned infeasibilities unreached because the a-priori bound refuses them
at 11680 to 16158 bits against a capacity of 4096. What does raising
`JM_EXACT_LIMBS` buy?

## What is here

| file | what it is |
|---|---|
| `sweep.sh` | the run: 128, 256 and 512, each from `make clean`, each timed |
| `sweep.txt` | its output |
| `n128.txt`, `n256.txt`, `n512.txt` | `bench/measurements/02-213/ray.py` at each setting |
| `t128.txt`, `t256.txt`, `t512.txt` | the wall time and peak resident size of each |

`make` does not track a change in `EXTRA_CFLAGS`, so the `make clean`
between settings is the whole point of the script (D154). The canary is
the peak resident size: it moves with the setting even where the counts
do not, which is what says three binaries ran and not one.

## What it says

| `JM_EXACT_LIMBS` | derivations that fit | certificates that hold | wall | peak RSS |
|---|---|---|---|---|
| **128** | 12 of 29 | **25 of 29** | 9.80 s | 44 MB |
| 256 | 12 of 29 | 25 of 29 | 9.78 s | 63 MB |
| 512 | 17 of 29 | 27 of 29 | 48.41 s | 88 MB |

**256 moves no verdict at all.** Not one derivation changes, and it costs
43% more memory. The four refusals need 365 limbs and more, so 8192 bits
clears none of them, and everything else already fitted.

**512 closes two of the four** — `qual` and `refinery` — and leaves
`pang` and `vol1` refused, so even 16384 bits does not close the set. It
costs five times the wall time and twice the memory.

## Why that is a refusal

The capacity is a ceiling on every exact path, not on this one: it is the
size of a `jm_rational`, which holds its magnitude inline, so every array
of them scales with it. `jaos_verify` on an optimum is where most of the
exact work happens and 30 of the 110 gate bases prove at 128 (D274). This
sweep did not measure what 512 does to that number, and two certificates
of 29 for five times the time on one set is not enough to move a constant
that governs all of it.

`bench/refusals.txt` carries the reopen condition: the same sweep over
the optimum proofs.

## Reproducing

```
bash bench/measurements/02-216/sweep.sh
```

About four minutes, most of it the 512 arm. It leaves the tree clean.
