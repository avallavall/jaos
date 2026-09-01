# 02-157 — half the forced primal's failures were a backstop that binds

D245. `SETTLE_ROUNDS` 32 → 128. The forced primal goes from 61 of 94
agreeing with the dual to 75, and the gate does not move.

## What is here

| file | what it does |
|---|---|
| `run-settle-rounds.sh` | sweeps the constant, running the primal campaign AND all three gate sets at each setting |
| `settle-rounds.txt` | the sweep, as run |
| `runs/` | the per-instance campaign record and the gate output at each setting |

Derives the repository root and runs from anywhere (D217). Restores
`src/simplex.c` and `bench/results/` itself. About twenty minutes.

## What was being looked at, and why nobody had

`TODO.md` recorded the forced primal as 61 ok, 30 DISAGREE, 3 overrun, and
noted that nothing in the file tracked it. The 30 were assumed numerical.
They were not looked at, because `make primal` is not a gate and prints a
ratio rather than a verdict.

A census at the moment of refusal, on the three worst, says what the
offending columns are:

| instance | columns with a wrong-signed reduced cost | can move | want a pivot |
|---|---|---|---|
| 25fv47 | 188 | **0** | 188 |
| 80bau3b | 746 | 12 | 734 |
| agg | 44 | 8 | 36 |

Almost all of them want a pivot, which is `primal_cleanup`'s job, and the
re-entry loop is what is meant to give it to them.

## The trace that decided it

One line per round of `reenter_after_settling` on `25fv47`:

| round | violation | objective |
|---|---|---|
| 0 | 784.898 | 10311.88 |
| 4 | 1470.55 | 6858.22 |
| 13 | 23.99 | 5748.54 |
| 24 | 9.97 | 5516.06 |
| 31 | 10.84 | 5458.90 |

**All 32 rounds are used and the violation is still falling.** The solve is
not stuck; it is cut off. The objective descends monotonically the whole
way. That is a round budget binding, not a numerical failure.

And the constant says so itself. Its comment reads "a backstop, not a limit
meant to bind (D30)", and on the dual path that is true — D89 measured the
rounds the oscillation wastes on `pilot87` at 278 of 116071 iterations,
0.24%. **The sentence had never been checked on the primal path**, where the
re-entry arrives with a whole solve's worth of dual infeasibility rather
than a handful of columns.

## The sweep

Every setting ran the primal campaign and all three gate sets.

| SETTLE_ROUNDS | binary | ok | disagree | overrun | gate |
|---|---|---|---|---|---|
| 32 (shipped) | 82813b8b | 61 | 30 | 3 | 3 of 3 PASS, rc 0 |
| 64 | 2bbfa206 | 69 | 22 | 3 | 3 of 3 PASS, rc 0 |
| **128** | 63551eaf | **75** | **16** | 3 | 3 of 3 PASS, rc 0 |
| 256 | 4ba232e6 | 76 | 15 | 3 | 3 of 3 PASS, rc 0 |

Four distinct binaries, so four builds were measured and not one build four
times.

**Bounded on both sides.** Below 128 it costs agreements — 64 gives back
only 8 of the 14. Above it, 256 buys one further instance. 128 is the knee
and a power of two.

## The gate column is not what it looks like, and it cost this a rewrite

The sweep reads `0 regressed, 0 improved, 0 new` on all three sets at every
setting, and the first version of this file concluded from that line that
the dual path never reaches the old bound, so one constant would do.

**A full run at 128 then left `bench/results/netlib.txt` modified.**

```
-wood1p  iters=560  work=42078864  digest=514493ffbde8a088 basis=f265e5843393cb5e
+wood1p  iters=656  work=62770176  digest=514493ffbde8a088 basis=f265e5843393cb5e
```

**1.49x work for a bit-identical answer.** Same digest, same basis, same
objective, 49% more work to reach it. The gate said nothing because its
regression bar is 2.0x, and `0 regressed` only ever meant that no predicate
flipped and nothing crossed that bar.

So the dual path does reach 32. Raising the bound for everyone would charge
the shipped path half as much work again on a gate instance in order to help
a switch that is not public API. **The constant is two constants now**,
`SETTLE_ROUNDS = 32` and `SETTLE_ROUNDS_PRIMAL = 128`, chosen once per solve
on `cfg.force_primal`, and the shipped path is byte-identical by
construction rather than by hope.

## What this does not fix

**Sixteen instances still disagree, and one of them is a different failure
entirely.** On `agg` the re-entry's own dual run returns
`JAOS_SOLVE_INFEASIBLE` at round 0, from a point `arm_reentry` had just
built, on a model the dual solves normally. That is not a budget and more
rounds will never reach it. It is written into `TODO.md` as its own item.

**And the agreement count still does not mean what it reads as.** Most of
the campaign's iterations belong to the dual's settling re-entry rather than
to the primal, which `SPECS.md` already says. This entry moves how often the
forced primal produces an answer at all, not how good its own pricing is.
