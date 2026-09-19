# 02-265 — a solve over the directions, when the walk's ray is refused

`TODO.md` row 5 listed 8 numerical errors over the 3000 generated conic
models of 02-253: six rays the projection could not bring inside the ray
checker's tolerance (2e-6 to 9e-2 past a row side or a bound) and two
walks that stop with nothing to answer from.

## What the six were

The walk's unbounded exit hands out its own iterate as the direction, and
`ray_polish` projects it onto the rows it has to keep. The checker is
stricter than the projection: a column with a finite bound may not move
against it at all (`max_col_escape` has no tolerance), a row's activity
may move against a finite side only inside a relative tolerance, a cone
has to hold the direction, and the curvature has to vanish. Six of the
3000 ended with a direction 2.1e-6 to 8.8e-2 outside that.

## The change

When the polished direction still fails, `cm_ray_probe` in `src/conic.c`
solves for one that cannot fail, the conic form of what `qp_ray_probe`
does for a quadratic model: a direction per column, boxed at 1 where the
column's bound in that direction is infinite and held at 0 where it is
finite; every row with a finite side holding its direction's activity on
that side of 0; `Q d = 0` as rows for the objective's quadratic part and
for each quadratic row's, since a convex part has to stay flat along a
ray; the model's cones over the same columns, a cone being its own
recession cone; and the objective the rate `σ c'd`, minimised.

The answer is cleaned the way the walk's ray is (parts below
`CONIC_RAY_ZERO` of the largest go to zero) and clamped into its own box,
because the checker's column test is exact and its row test is relative
to a row's traffic: without the cleaning four of the six still failed on
row activities of 1e-31 to 9e-14 that are nothing but cancellation.

The checker decides as before, so a direction this solve finds is
published only if it passes.

## The reading

02-253's 3000 generated models, seeds 1 to 3:

| | before | after |
|---|---|---|
| numerical errors | 8 | 2 |
| unbounded, ray taken | 241, 245, 227 | 243, 247, 229 |
| checker failures | 0 | 0 |
| work units | 155217614, 152519849, 150016425 | 155398635, 152674327, 150679726 |

The six rays end `UNBOUNDED`, each with a ray the checker takes. What is
left is the two walks that stop with nothing to answer from (seed 1's
model 448 and seed 3's model 831), which `TODO.md` row 5 keeps. The work
rises by 0.1% to 0.4%, the price of a second solve on the six.

02-255's 3000 mixed-integer models: every answer still agrees with brute
force, and the trees move a little where a node's relaxation now ends
unbounded instead of as a numerical error: nodes 2410, 2488, 2486 to
2404, 2482, 2465, and the work 1.1886e9 to 1.1833e9.

CBLIB's 80 mixed-integer instances at 1e10 work units: the same answers
to the bit. `make cblib`, the 29 continuous instances: 0 regressed, 0
improved, 0 new.
