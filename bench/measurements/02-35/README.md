# The fabricated zero is load-bearing: removing it makes the breach compound

Taken 2026-08-18, following `TODO.md` §5a. One probe, one candidate, one file.
The repair D125 pointed at is **refused, by measurement**. Closed as D126.

## What was tried

D125 measured that `shift_to_feasible` writes `s->d[v] = 0.0` on 167816 of
netlib's 1006960 lends where the cost does not move at all, and called that a
fabrication: `d` is `cost[v] - y·M_v` by definition, so zeroing it claims the
cost moved by exactly `need`.

The candidate is `candidate.diff` beside this file. It reads the move back off
the cost and stops when there was none:

```c
const double before = s->cost[v];
s->cost[v] += need;
const double moved = s->cost[v] - before;
if (moved == 0.0)
    return;
s->shift[v] += moved;
s->d[v] += moved;
```

The argument for it was that the residue left behind is bounded: the cost did
not move, so `|d|` is below half an ulp of `cost[v]`, and `admit_candidate`
already clamps the ratio-test numerator with
`rnum[k] = dist > 0.0 ? dist : 0.0`.

## Where the argument leaks

The clamp covers the **choice**. `bfrt_walk`, `jm_harris_pick` and
`jm_bland_pick` read only `rnum`, `rden` and `rrange`, so no wrong-signed `d`
reaches any of them.

It does not cover the **step**. Both exits of `dual_ratio_test` compute

```c
*theta_out = s->d[best] / s->alpha[best];
```

from the raw `d`, and `admit_candidate` only requires `|alpha| > PIVOT_MIN`,
which is `1e-9`. The division is an amplifier of up to a billion.

## What the probe measured

`run-theta-sign.sh` counts, at both exits, how often the winner's distance to
infeasibility is negative, and what the division makes of it. Two binaries,
one run, md5s printed.

| | picks | wrong-signed | worst \|dist\| past zero | worst \|theta\| from one |
|---|---|---|---|---|
| netlib, HEAD | 477562 | 248 (0.052%) | 4.81e-10 | 8.37e-09 |
| netlib, candidate | 712218 | **8840 (1.24%)** | **5.84e-05** | **2.21e-03** |
| Kennington, HEAD | 435418 | 170 (0.039%) | 5.22e-10 | 5.35e-10 |
| Kennington, candidate | 448132 | **5588 (1.25%)** | 4.59e-10 | 4.31e-10 |

**The half-ulp bound is false.** On netlib the worst breach reaching the
division grows by five orders of magnitude and the worst dual step by six.
2.21e-03 is not a rounding artefact arriving at the step; it is a number.

**Why it grows.** The bound assumed the breach is whatever one lend could not
repair. It is not. `update_dual` runs `d[v] -= theta_dual * alpha[v]` every
iteration and then calls `shift_to_feasible`; today's zero resets the breach
each time, so it cannot accumulate. Remove the zero and the next iteration
pushes the same variable further the same way, and the one after that again.
That is the hazard `shift_to_feasible`'s own header names: *"a reduced cost
pushed a tolerance past zero stayed there, and the next iteration could push
it further."*

The trajectory says the same thing from the other side: the candidate takes
**712218 ratio-test picks against HEAD's 477562** on netlib, 49% more, on the
same 188 solves.

## What it leaves standing

**`d[v] = 0.0` is not a fabrication.** It rounds a quantity to zero when that
quantity is below the resolution of the cost that produced it, which is what
`fp-numerics` says to do with a sum below the noise of its terms. D125
described it correctly and this entry corrects what D125 implied about it.

**Two smaller things survive, and neither is repaired here.**

- `shift[v] += need` records a loan that was never made, on the same 16.7% of
  lends. Recording `moved` instead is not a no-op: `repay_shifts` returns
  whether anything moved, and `settle_shifts` skips `compute_duals` and
  `repair_dual_infeasibility` when nothing did — so a phantom loan currently
  forces a re-pricing that would otherwise be skipped. It needs its own
  measurement.
- **At HEAD, 248 netlib picks and 170 Kennington picks already compute the
  dual step from a wrong-signed reduced cost.** Worst step 8.37e-09, which is
  small, and it is a standing fact rather than a proposal. The clamp at
  `admit_candidate` does not reach the division and nothing else does either.

## Reproducing it

`run-theta-sign.sh`, beside this file. It builds HEAD and the candidate in one
run and prints both md5s. `src/` is read and never written; the candidate is
copied into a throwaway worktree. Each record is one `write(2, …)` because
twelve forked children share one stderr (D125).
