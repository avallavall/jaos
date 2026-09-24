# 02-318 — the QP push pins by each variable's scale, closes the gap and sets exact duals

Taken on 2026-09-24 on the tree of 327f7e5, for TODO row J2.

## The four models

| model | before | cause |
|---|---|---|
| Maros-Meszaros `qgrow22` | `numerical_error` after the conic walk | the push pins wrongly: its bounds reach 3.2e7, so `QP_PUSH_NEAR (1 + max(|b|, |bounds|))` is 3.2 |
| QPLIB_8785 | `optimal`, refused at 1e-7 on a gap of 1.31e-7 | free reduced costs of 3.3e-10 on columns 6.5e5 from their bounds |
| Maros-Meszaros `aug3dqp` | suboptimality bound 1.03e-3, over the runner's ceiling | reduced costs of ±5e-11 on single-row columns, charged against upper bounds of 8.6e10 the rows imply |
| QPLIB_9002 | `numerical_error` after the conic walk | open, see below |

### qgrow22

The barrier ends with `mu` at 1e-33. The push pinned columns such as
column 0 at its lower bound, 2.7e-4 away, although the barrier gave it a
dual slack of 6e-32: the start pins every variable within
`QP_PUSH_NEAR (1 + max(|b|, |bounds|))` of a bound. After the first
freeing, the step moved 17 of the freed columns back out of their box
and they were pinned again at step 0. A partial step also pinned every
free variable within 3.2 of a bound and moved it there. The push ended
with 28 wrong signs after 3 freeings.

`push-modes.diff` (on 327f7e5) holds the switches read here. Under
`JAOS_NEARMODE`:

| mode | start | partial step | qgrow22 | work |
|---|---|---|---|---|
| 0 | as before | as before | `numerical_error` | 123434269 |
| 1 | `1e-7 (1 + |bound|)` | `1e-7` | `optimal` | 31533564 |
| 2 | `1e-7 (1 + |bound|)` | as before | `numerical_error` | 136220999 |
| 3 | no near test | as before | `numerical_error` | 136220999 |
| 4 | as before | `1e-7` | `numerical_error` | 123434269 |
| 5 | as before | `1e-7 (1 + |bound|)` | `numerical_error` | 123434269 |
| 6 | `1e-7 (1 + |bound|)` | `1e-7 (1 + |bound|)` | `optimal` | 31533564 |
| 7 | as before, unless the dual slack is under `1e-7` times the distance | `1e-7 (1 + |bound|)` | `optimal` | 31533564 |
| 8 | as mode 7 | as before | `numerical_error` | 136220999 |

Both rules have to change. Mode 6 on the whole set
(`maros-meszaros-mode6.txt`) solves qgrow22 and moves `qrecipe`'s
suboptimality bound from 2.7e-16 to 1.8e-10 and `qpcboei2`'s from 2.2e-15
to 8.4e-12. Three of `qrecipe`'s columns sit 4e-5 from their bound with a
dual slack of 6e-6, and the barrier cannot tell which one is zero. The
old start rule pinned them, which was right there. Mode 7 keeps the old
start rule except where the dual slack is under `QP_PUSH_NEAR` times the
distance, which is qgrow22's case, and it reads no regression
(`maros-meszaros-mode7.txt`).

### QPLIB_8785

The push settled in 2 rounds with free reduced costs of up to 3.3e-10,
under `QP_PUSH_TOL`. Those columns have curvatures of 1e-5 and sit 6.5e5
from their bounds, and the checker's gap sums reduced cost times distance:
2.06e-3, which is 1.31e-7 of `1 + |primal| + |dual|`. One more round on
the same factorisation takes it to 3.0e-9. The push now takes more rounds
while that sum is over `QP_PUSH_GAP` (1e-8) of `1 + |objective|` and it
halves each round.

### aug3dqp

858 columns with no quadratic term, one row each, a cost of 0 and no
upper bound sit inside their box with reduced costs down to -5e-11: the
duals of their rows are not exactly 0. The checker charges the negative ones against upper bounds
of 8.6e10 that it derives from the rows. When the push settles it now sets
the dual of a row whose slack it left free to 0, and the dual of a row
that holds a free single-row column with no quadratic term to that
column's cost over its coefficient. It keeps the new duals only when the
sign test still passes with no wrong sign and no loose free reduced cost.
aug3dqp's bound goes from 1.03e-3 to 4.1e-16.

## The change

In `src/barrier.c`: the start rule of mode 7, the partial-step rule
`QP_PUSH_NEAR (1 + |bound|)`, the gap rule on `QP_PUSH_GAP`, and
`push_snap` for the exact duals. `tests/test_barrier.c` has a QP whose
inactive row must publish a dual of exactly 0; it fails on 327f7e5.

## Readings

`make maros-meszaros J=6` against the results of 327f7e5:

- 137 solved and 137 taken by the checker, where it was 136 and 136.
  `values` is still refused as not convex.
- qgrow22: `optimal` at 31547988 work units, where it ended
  `numerical_error` at 123434269.
- 132 instances change their work, geometric mean 0.994x over them. Most
  pay 0.1% to 0.5% for the extra sign test. huestis 0.531x, gouldqp2
  0.689x, boyd1 0.878x and hues-mod 0.913x fall; qshare1b 1.075x, qgrow7
  1.062x and qgrow15 1.048x rise.
- The suboptimality bound changes on 59 instances: 17 fall past 2x and 11
  rise past 2x, geometric mean 0.038x. The runner flags two rises: boyd2
  from 1.54e-8 to 3.7e-8, and qstair from 6.9e-11 to 3.6e-10. Both come
  from the exact duals: zeroing an inactive row's dual moves the reduced
  costs of columns with a curvature near 0, and the checker charges those
  `w² / (2q)`. boyd2's gap is now certified (no dropped terms), where it
  was not. The largest rise stays 27x under the ceiling of 1e-6. The
  baseline is rewritten.

The 6000 generated QPs of 02-248 (`gen-base.txt`, `gen-new.txt`): every
property reads the same on every seed, and so do the rounds each push took.
The work rises 0.15% on each seed.

QPLIB's continuous convex QPs that finish under 1e11 work units
(`cqp-base.txt`, `cqp-new.txt`): QPLIB_8785 is taken by the checker at
1e-7. The other nine optimal ones read the same objectives and are taken
as before, at 1.0000x to 1.0017x the work. QPLIB_9002 ends
`numerical_error` at 3.99e9 work units, where it took 4.20e9.

QPLIB's 17 convex MIQPs at 1e10 (`miqp-base.txt`, `miqp-new.txt`): the
trees move. QPLIB_3708's incumbent improves from -6346.7 to -9216.3 and
QPLIB_3980's bound from -2.6155 to -2.6050. QPLIB_3547's incumbent gets
worse, -0.5600 to -0.5382, and QPLIB_10050's bound drops by 1e-3.
QPLIB_10069 stays optimal at 1.29x the work. QPLIB_5577 now finishes its
root relaxation inside the limit, adds 50 cuts, and the second solve runs
to 1.85e10 work units where the first stopped at 1.16e10; neither has a
bound.

`make test` and `make sanitize` pass.

## QPLIB_9002, still open

A network flow, 2890 columns and 1649 equality rows of coefficients ±1,
with a separable quadratic cost whose curvatures run from 9e-12 to 2. The
barrier stops making progress at a dual residual near 3e-9. With the
landed rules the push's first full step finds 743 of 1983 pinned
variables with the wrong sign, and after 5 freeings 734 are left, the
worst 1.5e2.

Two readings, both with `push-modes.diff`:

1. `JAOS_PUSHDBG` prints the duals the push reaches against the
   barrier's. At the first full step they differ by up to 345, in the
   directions of the rows' duals that no free column touches; with the
   barrier's duals only 26 signs are wrong, and 603 free reduced costs are
   not zero. `JAOS_YCLEAN=k` keeps the barrier's duals in those directions
   and solves for the rest with the rows' residual at zero (k passes of
   refinement). Under `JAOS_NEARMODE=6` that takes the wrong signs from 760
   to 40. The push still does not settle: the 40 freed together each get
   a step of about 1e-8 the wrong way, and they are pinned again.
2. `JAOS_DUMPZN` writes the push's point at each full step, and `lpdual.c`
   solves the LP `min g'x`, `g = c + Qx` at that point, over the same rows
   and bounds, and checks the point with the LP's duals. At rounds 5 and 8
   the LP reaches 1.13779e10 and 1.13936e10 against `g'x` of 1.13962e10,
   so the point is not optimal for its own gradient, and the checker
   refuses the pair (duals off by 0.40 and 0.19). The pinned set is
   really wrong.

What is left: an active-set iteration that frees one variable at a time,
or a barrier that converges further on this model.

## Files

- `push-modes.diff` — the switches read above, on 327f7e5
- `maros-meszaros-mode6.txt`, `maros-meszaros-mode7.txt` — the whole set under modes 6 and 7
- `qpread.sh` — the QPLIB and generated-QP readings, HEAD against the working tree
- `gen-*.txt`, `cqp-*.txt`, `miqp-*.txt` — its records
- `lpdual.c` — the LP-dual probe of QPLIB_9002
