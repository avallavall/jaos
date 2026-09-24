# 02-314 — duplicate rows in presolve, built and refused

Taken on 2026-09-24 on the tree of 764fe58, after 02-312 met D101's reopen
condition for duplicate rows.

## The patch

`duplicate-rows.patch` adds a presolve pass that runs when a round changes
nothing else. It hashes each live row by its pattern and its coefficient
ratios, sorts the rows by that key, and compares the rows that share one.
A row `k` whose every coefficient is exactly `lambda` times the one of an
earlier row `i` is removed, and `i` takes the tighter of its bounds and
`k`'s bounds divided by `lambda`. Postsolve gives `k` back basic, or, when
the bound `i` sits on came from `k`, puts `k` on that bound with `i`'s dual
divided by `lambda` and makes `i` basic. The Farkas lift moves a
multiplier the same way, and a start basis maps `k`'s bound status onto
`i`. Rows with an indicator, models with quadratic rows or cones are left
alone. Three tests in `tests/test_presolve.c` cover a transferred bound, a
looser duplicate and an infeasibility certified through a merged row.

## Readings

Work units against the committed files, geometric means:

| set | ratio | notes |
|---|---|---|
| netlib dual (94) | 0.994 | agg3 0.54, nesm 0.80, agg 0.81; fffff800 1.41 |
| Kennington (16) | 1.017 | cre-a 0.97; cre-b 1.08, cre-c 1.06, cre-d 1.18 |
| primal (88) | 1.000 | |
| barrier (94) | 0.988 | agg3 turns overrun because its dual budget fell |
| pdlp (94) | 0.995 | |
| concurrent (94) | 0.994 | |
| warm (92) | 1.059 | the pass costs more than a short warm solve saves |
| MIPLIB 3 (24) | 1.122 | enigma 0.50, l152lav 0.60; p0033 3.79, misc03 2.72, gen 1.79 |

The files are here as `<set>.txt`. Every instance kept its verdict and the
checker accepted every answer; netlib flags two suboptimality bounds at
rounding level (agg2 1.1e-16, d2q06c 5.4e-13).

The five MIPLIB 2017 instances with duplicate rows, at 1e10 work units:

| instance | nodes | incumbent | bound |
|---|---|---|---|
| ic97_potential | 12116 → 48002 | none → none | 3868.46 → 3868.44 |
| supportcase26 | 95201 → 103604 | 1802.2 → 1841.5 | 1474.5 → 1484.0 |
| neos-3046615-murg | 100455 → 95733 | 1697 → 1727 | 437 → 420 |
| timtab1 | 280849 → 278290 | none → none | 441250 → 411710 |
| neos-2657525-crna | 1076 → 3652 | none → none | 0 → 0 |

The node LPs get cheaper, but no gap closes and most incumbents and bounds
get worse.

## Verdict

Refused as `presolve-duplicate-rows` in `bench/refusals.txt`. The pass
lands only if it pays on MIPLIB 3, Kennington and the warm reading as well
as on the models that carry the duplicates.

## One fix kept

The MIP test `test_a_trees_progress_calls_carry_the_running_total` failed
under the patch: the dual simplex reported its progress before it checked
for optimality, so a warm solve that was optimal at once reported the best
infeasibility at its start value, infinity. The report now reads 0 when no
row is violated. That line landed on its own, with its own test in
`tests/test_simplex.c`.
