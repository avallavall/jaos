# 02-331 — MIP: parity rows fixed by elimination mod 2

Taken on 2026-10-04 on the tree of 70446ad, for TODO row J7 ("no
incumbent"). The settings are 02-328's.

## The model

`enlight_hard` of the 2017 set is a lights-out board: 100 binaries `x`,
one per cell, 100 general integers `y`, and one equality row per cell,
`x(cell) + x(its neighbours) - 2 y(cell) = 1`. The `-2 y` term lets the sum
take any odd value, so the relaxation does not see that only the sum's
parity matters. JAOS reached 1e10 work units with a bound of 23 and no
point; HiGHS 1.15.1 solves it in 2303 nodes and SCIP 10 at its root in
0.003 s (`bench/compare/results/mip-miplib2017.txt`).
Node bound propagation (`--propagate 4` and `20`) lifts the bound at 2e9
work units from 21 to 26 and 27 and finds no point.

## The step

At the root, under `jaos_set_mip_tighten`, after the implied fixings: an
equality row with an integer right-hand side, over integer columns with
integer coefficients, at least one of them even, and every odd one on a
binary or a fixed column, holds the sum of its odd binaries mod 2. Gaussian
elimination mod 2 over those rows (64 columns a word, `MIP_PARITY_WORK`
caps its size) fixes every binary whose reduced row holds it alone, and a
row reduced to `0 = 1` ends the solve `INFEASIBLE`.

`scan.sh` runs `parity.py` over both sets. Two models have parity rows:

| model | rows | binaries | rank mod 2 | fixed |
|---|---|---|---|---|
| enlight_hard | 100 | 100 | 100 | 100 |
| enigma | 1 | 15 | 1 | 0 |

With `ALLROWS=1`, which also counts equality rows over binaries with no
even coefficient (set partitioning rows), 17 models have rows and
elimination fixes one binary more, on `enigma`; those rows are left out.

## The reading

| arm | MIPLIB 3 geo | sum | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|
| 70446ad | 1 | 1 | 2 | 24 | 9.064 | 9.117 | 18.181 |
| b4 | 0.9998 | 0.9998 | 3 | 25 | 8.064 | 8.738 | 16.802 |

`enlight_hard` solves at the root in 27464 work units with the reference's
37. The scan of the rows moves every model's work by its nonzeros; on
MIPLIB 3 no model changes by 0.1%, and on the 2017 set two bounds move by
under 1e-6 of themselves (`gen-ip054`, `mas76`).
