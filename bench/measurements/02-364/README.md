# 02-364 — Gomory cuts with integral slacks, and Gomory rounds while they lift, refused

Taken on 2026-10-08 on the tree of 35e652a, for TODO row J7. The survey of
02-351 found Gomory cuts the family JAOS uses least: SCIP's Gomory rounds
alone close most of the root gap on `dcmulti`, `misc06`, `lseu`,
`khb05250`, `p0201` and `misc03`, where JAOS runs one round
(`MIP_CUT_ROUNDS`).

## The two changes

`gomory.diff` holds both, behind environment switches:

- **Integral slacks** (`JAOS_GISL`, on unless set to 0). The Gomory
  round treated every row's slack as continuous. A row whose columns are
  all integer and whose coefficients are all whole numbers has an integer
  activity, so at an integral bound its slack can take the integer
  formula of the mixed-integer Gomory cut. The cuts stay valid: every
  answer below matches the reference.
- **Rounds while they lift** (`JAOS_GSTALL`, `JAOS_GMAX`, `JAOS_GCAP`).
  After `MIP_CUT_ROUNDS`, a further Gomory round runs while the previous
  round lifted the bound by `JAOS_GSTALL` times `1 + |bound|`, up to
  `JAOS_GMAX` (20) rounds and, with `JAOS_GCAP`, while the root holds
  fewer Gomory cuts than that many times the model's rows.

## The roots

To the end of the root (`--node-limit 1 --no-heuristics`), integral slacks
on:

| model | one round | while lifting, 1e-4 | optimum |
|---|---|---|---|
| `khb05250` | 1.0417e8 | 1.0583e8 | 1.0694e8 |
| `dcmulti` | 185216 | 185771 | 188182 |
| `lseu` | 1027.1 (1021.9 with slacks continuous) | 1067.3 | 1120 |
| `p0201` | 7157.4 | 7435.2, at 55x the root's work | 7615 |
| `p0033` | 2963.3 | 3089, the optimum | 3089 |

`misc03` and `misc06` do not move: their first Gomory round lifts nothing.

## The arms

`arms.sh` (the arms of `~/j7/tools/arm.sh`: MIPLIB 3 at J=2 and 4 GB a
solve, the 2017 set at 1e10 work units, J=2 and 5 GB). The base is
`bench/results/miplib.txt` and `bench/results/miplib2017.txt`: MIPLIB 3
all solved, the 2017 set 5 solved, 28 incumbents, gap sum 13.48. `gis1`
ran with no switch set, which is integral slacks alone.

| arm | switches | MIPLIB 3 solved | geometric | sum | 2017 solved, incumbents, gap sum |
|---|---|---|---|---|---|
| `gis1` | integral slacks | 24 | 1.302x | 3.556x | 5, 27, 14.06 |
| `g0s3` | rounds while lifting 1e-3 | 23 (`bell5` out of memory) | 1.345x | 1.032x | 4, 26, 16.64 |
| `g0s3c1` | 1e-3, at most 1 cut a row | 23 (`bell5` out of memory) | 1.272x | 1.030x | 5, 27, 14.49 |
| `g0s4c1` | 1e-4, at most 1 cut a row | 24 | 1.610x | 6.900x | 5, 27, 14.46 |

Integral slacks alone take `bell5` from 20089 to 2214955 nodes (141.5x the
work) and `enigma` to 7.42x, against `gt2` 0.455x, `misc07` 0.732x and
`p0201` 0.789x. The extra rounds lift the roots above and grow the trees:
`mod008` 3.0x to 5.7x, `p0201` 2.1x to 2.9x, `p0033` 1.3x to 5.5x, and
`bell5` runs out of memory or takes 310x. Refused as
`gomory-integral-slacks` and `gomory-rounds-while-lifting`.
