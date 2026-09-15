# 02-248 — the push that puts a QP's answer on its bounds

SPECS row 22, convex QP, said what was missing: the barrier's point stops
short of every bound the optimum sits on, by about `mu / z`, so where a
column has a reduced cost that is not zero the checker refuses the dual
side. This is the reading behind the push that lands it (`qp_push` in
`src/barrier.c`).

## The models

1000 per seed, six seeds. A convex QP whose optimum is known by
construction: `x*`, the multipliers `y` and the reduced costs `d` are drawn
first with the signs the KKT conditions ask for, `Q = L L' + eps I` is
drawn with `eps` in {0, 0.25, ..., 1.25}, and the cost is what makes them
fit, `c = d + A'y - Q x*`. A third of the columns sit on a bound with a
reduced cost that is not zero, a sixth on a bound with a reduced cost of
zero, the rest inside. A third of the models are maximised with `-Q` and
`-c`. 4 to 16 columns, 2 to 8 rows.

## The properties

1. the solve ends `OPTIMAL`
2. the objective is `f* = c'x* + ½ x*'Q x*` to 1e-6 relative
3. the checker takes the point and the duals, primal and dual side, at 1e-6
4. a column whose reduced cost is not zero is on its bound to 1e-6
5. with `eps > 0` the optimum is one point and the answer is it to 1e-6

Every 50th model is written as LP and as MPS and the tool solves each with
`--check`.

## Before

The library at 6241190, the commit before the push.

| seed | models | not optimal | wrong value | dual side refused | strict column off its bound | off the one optimum |
|---|---|---|---|---|---|---|
| 1 | 1000 | 1 | 0 | 560 | 13 | 494 |
| 2 | 1000 | 0 | 0 | 567 | 14 | 498 |
| 3 | 1000 | 1 | 0 | 580 | 8 | 501 |
| 4 | 1000 | 1 | 0 | 592 | 11 | 523 |
| 5 | 1000 | 1 | 0 | 599 | 13 | 513 |
| 6 | 1000 | 1 | 0 | 600 | 11 | 531 |

The objective is right everywhere; the point is not. The dual side is
refused on 56% to 60% of the models, by up to 1.0 in the reduced cost of a
column that sits 1e-5 from its bound, and half the models with one optimum
are more than 1e-6 from it, by up to 4e-3. The worst distance of a strict
column from its bound is 1e-5.

## After

| seed | not optimal | wrong value | dual side refused | strict column off its bound | off the one optimum | settled in 1 / 2 / 3 rounds | after a freeing | work against before |
|---|---|---|---|---|---|---|---|---|
| 1 | 1 | 0 | 0 | 0 | 0 | 984 / 13 / 1 | 13 | 1.091x |
| 2 | 0 | 0 | 0 | 0 | 0 | 986 / 13 / 0 | 12 | 1.091x |
| 3 | 1 | 0 | 0 | 0 | 0 | 982 / 16 / 0 | 16 | 1.091x |
| 4 | 1 | 0 | 0 | 0 | 0 | 980 / 18 / 0 | 17 | 1.090x |
| 5 | 1 | 0 | 0 | 0 | 0 | 985 / 12 / 1 | 13 | 1.090x |
| 6 | 1 | 0 | 0 | 0 | 0 | 988 / 10 / 0 | 9 | 1.091x |

Every push settled; the checker takes every answer on both sides; every
strict column is on its bound to 1e-9 and every model with one optimum is
at it, to 2.5e-7 at worst. The push costs 9% of the work, one extra
factorisation and solve on 98.5% of the models. The tool's `--check`
answers `check_ok yes` on all 240 files of the CLI leg, 120 LP and 120
MPS.

The five models that end `NUMERICAL_ERROR` are the same five before and
after: the barrier's dual residual jumps to 5.6e5 at its tenth iteration
and never recovers. That is the barrier's, not the push's, which only runs
after `OPTIMAL`; it is a lead for the reading on a published QP set that
SPECS row 22 still lacks.

## How it got here

Four shapes were tried on the way, each read over the same seeds:

1. **Pin and re-solve, freeing on a wrong sign.** The active set cycled on
   models with a flat objective (`eps = 0`), so 1 in 300 never settled.
2. **Pin, never free.** Settles by construction, but 1.3% of the models
   pin a variable the complementarity misread and end with a reduced cost
   of the wrong sign, up to 6e-3, so the barrier's point stood there.
3. **Pin by a huge diagonal, free at most three times.** The huge diagonal
   against the free variables' 1e-8 gave the quasi-definite LDL a spread of
   1e28, pivots came out with the wrong sign and were floored, and one push
   in 300 published a point breaking a row by 3. Taking the pinned
   variables out of the system altogether (a diagonal of -1, every
   off-diagonal entry zeroed, a row left with no free variable given a
   diagonal of 1) removed the blow-ups; a residual check refused the rest.
4. **The ratio-tested step.** Jumping to the target and pinning whatever
   landed outside its box left rows unsatisfied on 0.3% of the models. The
   step is now cut at the first free variable that reaches a bound, which is
   pinned there, so the point stays inside its box on every round, and a
   free variable whose reduced cost is not zero to `QP_PUSH_TOL` after a full
   step gets another round. With the proximal term raised from 1e-8 to
   1e-6 the factorisation stopped replacing pivots and every push settled.

## The constants

Seeds 1 to 3, everything else at its value. "2 rounds" is the number of
models whose push took a second factorisation.

| constant | value | did not settle | dual side refused | 2 rounds | work against before |
|---|---|---|---|---|---|
| `QP_PUSH_REG` | 1e-4 | 0 | 0 | 1318 | 1.132x |
| | **1e-6** | 0 | 0 | 42 | **1.091x** |
| | 1e-8 | 8 | 3 | 36 | 1.091x |
| `QP_PUSH_TOL` | 1e-7 | 0 | 0 | 35 | 1.090x |
| | **1e-9** | 0 | 0 | 42 | **1.091x** |
| | 1e-11 | 1 | 0 | 1330 | 1.132x |

At 1e-4 the proximal term leaves the free variables a reduced cost of
`1e-4 |dz|`, above `QP_PUSH_TOL` on 44% of the models, so those take a
second round for nothing; at 1e-8 the factorisation loses pivots to the
spread between the term and `BARRIER_DELTA` and 8 pushes never settle. At
a tolerance of 1e-11 the first round's residual is above it on 44% of the
models, again a second round for nothing; 1e-7 buys nothing over 1e-9.
`QP_PUSH_ROUNDS` at 20 and `QP_PUSH_FREEINGS` at 3 are never reached: the
longest push over the six seeds took 3 rounds and 1 freeing.

## The pass is not vacuous

The before table is the control: the same harness against the library
without the push reports the defect on 56% to 60% of the models.

## How to run

```
make all cli
bench/measurements/02-248/push.sh            # six seeds, then the CLI leg
RUNS=200 SEEDS=1 bench/measurements/02-248/push.sh   # one short seed
```
