# 02-312 — D101 and D246 read on the sets 02-154 did not count

Taken on 2026-09-24 on the tree of 1a826dc for TODO row I1.
`run-families-new.sh` is 02-154's script with other sets: Kennington, the
infeasible set, and MIPLIB 3 and the MIPLIB 2017 set as LP relaxations
(integrality dropped, because a MIP does not reach the presolve the counter
sits in). Maros-Meszaros is left out, because a quadratic objective skips
presolve. The counter is 02-07's `diag_families.inc`, unchanged.
`families-new.txt` is the run and `<set>.txt` has one line per instance.

## The control

netlib reads 151 removable rows and 1438 removable columns, the figures
02-154 read, over the same 77405 live rows and 157499 live columns.

## What the counter reports

| set | live rows | live cols | removable rows | removable cols | dual-fix candidates |
|---|---|---|---|---|---|
| netlib (94), control | 77405 | 157499 | 151 (0.195%) | 1438 (0.913%) | 1054 (0.669%) |
| kennington (16) | 205651 | 844890 | 298 (0.145%) | 4 (0.000%) | 0 |
| infeasible (29) | 12750 | 25312 | 22 (0.173%) | 21 (0.083%) | 31 (0.122%) |
| MIPLIB 3 (24) | 3837 | 21522 | 22 (0.573%) | 43 (0.200%) | 117 (0.544%) |
| MIPLIB 2017 (30) | 13870 | 17231 | **1050 (7.570%)** | 4 (0.023%) | 2 (0.012%) |

MIPLIB 2017 passes D101's bar of 5% on its rows. Three instances carry
almost all of it, and each count is the same at all four of the counter's
tolerances, so these rows are exact multiples of other rows:

| instance | live rows | removable rows |
|---|---|---|
| ic97_potential | 1046 | 523 |
| supportcase26 | 830 | 396 |
| neos-3046615-murg | 498 | 120 |
| timtab1 | 158 | 9 |
| neos-2657525-crna | 330 | 2 |

None of the three is solved in 20 s by JAOS, HiGHS or SCIP
(`bench/compare/results/mip-miplib2017.txt`).

## Verdict

D101 is reopened for duplicate rows: a set in the tree now holds more than
5% of its live rows as multiples of other rows. Duplicate columns and
dominated columns stay under the bar in every set (0.913% at most, netlib's
control figure). D246 still holds: the largest dual-fixing share is
netlib's 0.669%.
