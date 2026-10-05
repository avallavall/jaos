# 02-353 — the end-of-solve verification on carried factors

Taken on 2026-10-05 on the tree of 0908bf3, for TODO row J8 (the
simplex's cost per solve) and J7 (MIPLIB 3's large trees). MIPLIB 3 at
J=2 and 4 GB a solve.

## Where a node solve's work goes

`warmcost.py` solves each model's LP relaxation cold, then again from its
own optimal basis, so the second solve needs no pivot:

| model | cold | warm, no pivot | a tree node on average (0908bf3) |
|---|---|---|---|
| `bell5` | 58572 | 11609 | 14720 |
| `bell3a` | 81722 | 12471 | 22491 |
| `stein45` | 116552 | 24053 | 47790 |
| `misc07` | 1138360 | 81387 | 296961 |
| `l152lav` | 7341403 | 78850 | 1548304 |

A solve that pivots nowhere still factors the basis twice
(`refreshdbg.py` tags each call): once when the dual loop starts, and once
more when it finds no leaving row and verifies the optimum, because the
loop accepts an optimum only on fresh factors. On `bell5` each
factorization costs about 5300 work units, so two of them are most of a
node.

## The change

When the dual or the primal loop verifies an optimum or an infeasibility,
and the factors carry `VERIFY_UPDATES` updates or fewer since the last
factorization, it no longer factors again: it computes the primal values
and the duals on the same factors, each with one step of iterative
refinement against the basis's own columns. A basis changed by a restore
or a repair still refactorizes. With 0 updates the factors are the ones a
refactorization would build, so the answers stay to the bit.

`verifyskip.py` reads the bound from `JAOS_VSKIP_K` under `JAOS_VSKIP`.
MIPLIB 3 against the reading of 0908bf3 (`m3-vsk*.txt`):

| updates allowed | geometric mean | sum | answers to the bit | past 1.1x |
|---|---|---|---|---|
| 0 | 0.993x | 0.998x | 24 of 24 | none |
| 4 | 0.910x | 0.950x | | `l152lav` 1.109x |
| 8 | 0.844x | 0.865x | | none |
| 16 | 0.884x | 0.925x | | `l152lav` 1.753x, `p0201` 1.164x |
| 32 | 0.944x | 0.919x | | `l152lav` 1.504x |

Every arm solves all 24 with the published optimum and every answer
taken by the checker. At 8: `lseu` 0.512x, `enigma` 0.524x, `bell3a`
0.596x, `flugpl` 0.630x, `gt2` 0.691x, `bell5` 0.696x; the worst is
`blend2` 1.039x.

## The gates at 8

All gates pass. Against the readings of 0908bf3, in the geometric mean of
work: Netlib 0.9995x, the infeasible set 0.991x, Kennington 1.000x,
Maros-Meszaros 1.000x, `primal` 0.997x, `barrier` 0.999x, `pdlp` 0.999x,
`concurrent` 0.999x, `warm` 0.949x and MIPLIB 3 0.844x. No status,
verdict, checker result or overrun changes; three `warm` objective values
move in the last digit. The 2017 gap sum goes from 14.53 to 14.45, 4
solved and 25 incumbents as before.

`tests/test_simplex.c`'s resumed budget stop used half the solve's work
as its stop point; on its three-row model that now falls before the first
pivot. The test now takes its stop point from a first stop at a work limit
of 1, plus one unit, which lands after a pivot.

## Instructions

A factorization is charged a fixed `JM_WORK_FACTOR` (4096 work units) on
top of its operations, so on a small basis the charge is most of it, and
the work units overstate the gain on small models. Callgrind's instruction
count of a full `jaos solve`, the tree of 0908bf3 against 7300529:

| model | instructions | work units | nodes |
|---|---|---|---|
| `bell5` | 0.894x | 0.696x | 20089 both |
| `lseu` | 0.622x | 0.512x | 5670 to 4005 |
| `enigma` | 0.591x | 0.524x | 1983 to 895 |
| `flugpl` | 0.997x | 0.630x | 1937 both |

Where the tree keeps its nodes, a node saves about a tenth of its
instructions (`bell5`, 91 rows); on `flugpl`'s 18-row basis the saving is
not measurable. The larger moves come with trees that change shape.
