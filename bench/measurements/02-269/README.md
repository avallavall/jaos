# 02-269 — MIR and cover cuts at the conic tree's root

2026-09-20. Refused. Nothing in `src/` changed.

## What was built

`TODO.md` row 5 says the conic tree runs "with no cuts and no warm start".
The MIR and cover generators of `src/mip.c` need no simplex basis: they read
the model's rows, the relaxation point and the integer bounds. So they can
run on a conic relaxation as they do on an LP one.

The build: `mir_round`, `mir_aggregate_round`, `cover_round` and
`clique_round` skip a row that carries a quadratic part, because dropping
that part is only safe on one side of the row. A new `jm_linear_cuts` in
`src/mip.c` runs one round of cover and one of MIR over the linear rows and
adds what it finds to a model. `src/conictree.c` calls it at the root, up to
`CT_CUT_ROUNDS` rounds, each round on a copy that is solved again, the copy
kept only when it ends `OPTIMAL`; the cut-carrying model then becomes the
one every later node copies.

## The reading

CBLIB's 80 mixed-integer instances, one root each (`--node-limit 1`), with a
counter on every place a cut is dropped.

| | count |
|---|---|
| instances that reached the generators | 78 of 80 |
| instances with at least one cut | 0 |

Three reasons, and each instance has one of them.

- **The integer columns are not in the rows.** On uflquad-nopsc-10-100
  (2111 rows, 3011 columns, 4010 nonzeros, 1000 cones, 10 integer columns)
  all 3212 row sides reach the MIR step with one delta, which is the step
  the rule takes when no integer column in the row is fractional. The rows
  hold 1.9 nonzeros each; the model's structure is in its cones.
- **The rows touch a free column.** robust_50_1 has 102 free columns of 207,
  and 204 row sides stop at the MIR rule that needs one finite bound per
  column. turbine07 stops on 139.
- **The cut is not violated.** sssd-strong-20-4 reaches the efficacy test on
  230 row sides, 8 of them with a fractional integer column and more than
  one delta, and no cut separates the point.

Four instances were also run whole at a work limit of 3e9. The root takes 0
cuts on each, and the work matches the committed runs to 0.3%
(sssd-strong-20-4 903 nodes, robust_50_1 1776697721 against 1776638441,
uflquad-nopsc-10-100 31 nodes, turbine07 22109667 against 22104687).

## What it says about the gap

A conic MIP's integer structure does not sit in its linear rows, so a cut
read off those rows alone cannot separate its relaxation point. A cut for
this set has to read the cones.

## The refusal

`bench/refusals.txt`, line `conic-linear-cuts`.
