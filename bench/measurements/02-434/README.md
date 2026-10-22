# 02-434 — the linear and quadratic solvers against the negated sense

Taken on 2026-10-22 on the tree of 6822270. `read.sh` takes the reading
with the library given (`*-head.txt` on 6822270, `*-new.txt` on the
change).

## The harness

`flip.c` is 02-224's `perm.c` with the second model in the same order,
its objective negated and its sense the other one, as 02-433's
`flipcone.c` does for cones and quadratic rows. Four modes: `lp` takes
the discrete shapes off, `mixed` keeps 02-224's integer marks, SOS set,
semi-continuous column or indicator row, and `qp` and `miqp` add to each
a diagonal quadratic objective over 1 to 3 columns, convex in the model's
sense. The statuses must match and the objectives must sum to within
1e-6 of max(1, |a|). Seeds 1 to 10, 20,000 models a seed.

## The reading on 6822270

| mode | compared | disagree |
|---|---|---|
| lp | 200,000 | 0 |
| mixed | 199,994 | 0 |
| qp | 200,000 | 0 |
| miqp | 199,813 | 1 |

The one is seed 7's model 7087, a minimisation with a convex quadratic
objective, two integer columns and an indicator row. The minimisation
ends at -1.499998412980458 and its negated twin at 1.5. The logs part at
the root: the minimisation's feasibility pump finds a point and the
twin's finds none.

The pump copies the root relaxation, sets its sense to minimise, and
replaces its linear costs with the distance to the rounded point plus a
share of the objective times the sense sign. It kept the quadratic
objective as it was. On a maximisation that term is concave, so
`jaos_solve` refuses the pump's first program ("makes a minimised
objective non-convex") and the pump stops: the twin's tree makes 8
solves against the minimisation's 26.

## The change

On a maximisation with a quadratic objective, the pump's copy takes the
quadratic term negated (`pump_negate_quadratic`), so it reads the
objective times the sense sign as the linear part already did. A
minimisation, and every linear model, takes the same path as before.
`tests/data/miqp_max_pump.mps` is model 7087's twin; it and its negated
minimisation now end at the same point, bit for bit.

## The reading on the change

`lp`, `mixed` and `qp` give 6822270's lines. In `miqp`, 199,814 pairs
are compared and none disagree; one pair that stopped at the work limit
on 6822270 is now compared.

## The gate

`make test` and `make sanitize` pass. `make netlib netlib-infeas J=12`
(94 of 94 and 29 of 29), `make netlib-kennington J=2` (16 of 16) and
`make miplib J=2` (24 of 24) show 0 regressed against their baselines,
and `bench/results/` is byte for byte the same.
