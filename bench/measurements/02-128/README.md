# 02-128 — `can_move`'s units: three arms, and what the reading does not cover

`TODO.md` §0 stage 6. **Measured, not landed.** The source change was reverted
before this was committed; the gate never ran. Read "What is still missing"
before doing anything with it.

## The question

`can_move`'s last line is

    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;

`wrong_way` is a reduced cost, a rate. The second factor is a distance in the
units of `x`. Their product is an objective change. `DUAL_TOL` bounds a rate:
`dual_breach`, `published_breach`, `settled_dual_violation` and
`points_outwards` all read it as one, and `docs/tolerances.md` says so. The
comparison is therefore wrong in both directions. A genuine breach inside a
narrow box reads as immovable; a reduced cost that is pure noise inside a wide
box reads as movable.

D184 refused the correction on a measurement, with the reopen condition that
the primal simplex land. It landed (D188) and 02-118 found the units LIVE. That
tree was `56be130` and four commits to the ratio test have landed since (D207,
D209, D212, D213), so this is the third reading.

## Three arms

`run-units.sh`, from HEAD `e5bfe3d`, three worktrees outside the repository.

- `base` — HEAD, the product against a rate
- `rate` — D184's one-liner, `wrong_way > s->dual_tol`
- `dist` — the same, plus a guard that the flip must move the column

Two campaigns: the standard gate set (the dual path) and the forced-primal
campaign, whose settling re-entry runs `can_move` on 60.5% of its iterations
(D204). Verbatim output: `run-units.txt`.

| | netlib (the dual) | forced-primal | ok / DISAGREE / overrun / ERROR |
|---|---|---|---|
| `base` | — | — | 61 / 29 / 5 / 0 |
| `rate` vs `base` | **byte-identical** | 20 lines moved | 61 / 29 / 5 / 0 |
| `dist` vs `base` | **byte-identical** | the same 20 lines | 61 / 29 / 5 / 0 |
| `rate` vs `dist` | byte-identical | **byte-identical** | identical |

**No verdict moves**, which is the question `TODO.md` asked: the units change a
trajectory and not an answer.

**The trajectory.** `grow22` drops from 36856 primal iterations to 22200 and
from 865799070 work units to 642728681, a quarter of its cost. `nesm` drops 359
iterations and 7.1% of its work. `boeing2` and `pilot4` move by one and two
iterations. Over the campaign the iteration geometric mean goes 2.0266 to
2.0078 and work 3.7626 to 3.7398. Two instances that DISAGREE at both settings
move their residual breach in opposite directions: `pilot-ja` from 1473.32 to
**0.99105**, and `pilotnov` from 0.221786 to 6.56892.

## The third arm is dropped, and by reasoning rather than by measurement

`dist` guards against a fixed column, where `lo == up` makes the distance zero.
It fires on no instance in either campaign, so the two arms are byte-identical
and the measurement cannot separate them.

The argument separates them, against the guard. At the other bound the same
reduced-cost sign is dual feasible by definition, so the flip repairs the
breach, `dual_breach` then reads zero, and the point does not move because
there is nowhere to move to. HEAD's product is zero there, so HEAD calls the
column immovable and `arm_reentry` shifts its cost instead, which perturbs the
problem where the flip would not. The guard would have blocked the better
repair.

## D27 chose the product on purpose

D27's argument is that `publish` divides `d` by the same scale factor it
multiplies the distance by, so the product reads the same number in scaled
space and in published space. A rate has to pick a space, and D92 says the two
readings of a breach may not replace one another. **A rate test gives that up**,
and reads scaled space, which is the space `dual_breach` reads and the space
the re-entry acts in.

What answers D27's own cautionary case is D27's other half. D27 measured
`pds-20` running all 32 settle rounds without converging, on columns whose
reduced costs were 2.2e-11 to 1.7e-10 and which cleared the product threshold
only because their boxes were 900 to 4955 wide. The repair D27 landed was not
the product; it was the relative noise test beside it, `wrong_way <=
NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v)`. That test runs first and is
untouched by any arm here.

## What is still missing

1. **`pds-20` is not in either campaign here.** It is a Kennington instance and
   this ran `netlib` and `primal` only. D27's whole cautionary case is
   therefore untested by this record. **`make netlib-kennington` on the `rate`
   arm is the reading that decides whether the change is admissible at all**,
   and it was not taken: the session ended before the gate ran.
2. `netlib-infeas` is untested for the same reason.
3. The published-space reading of the same rate is unmeasured. D27 avoided the
   space choice; a rate test forces one.

## How to resume

    bash bench/measurements/02-128/run-units.sh          # re-take the three arms
    # then, for the candidate alone:
    #   src/simplex.c, can_move's last line -> return wrong_way > s->dual_tol;
    #   make test && make sanitize
    #   numerics-reviewer on the diff
    #   make netlib netlib-infeas netlib-kennington J=12   <-- pds-20 lives here

A decision entry was drafted and not written, because its verdict depends on
the reading in point 1.
