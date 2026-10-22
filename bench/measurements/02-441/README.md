# 02-441 — a zero-cost column starts at the bound nearer zero

Taken on 2026-10-22 on the tree of 9b14bd0 with this change to
`src/simplex.c`. `trace.sh` builds a CLI of 9b14bd0 that prints each work
count with its source line, and reads where `afiro` and its mirrored model
part. 02-440's `read.sh` with `OUT=~/read441` takes the reading; its
records are the `*.txt` files here.

## Where afiro parts

On 9b14bd0, `afiro` and its mirrored model (02-440's mirror, seed 1) count
4,687 and 4,696 work units at iteration 0. `trace.sh` shows the two traces
agree until `compute_primal`, the solve for the basic values of the slack
basis. The mirrored model has two more nonbasic columns away from zero, so
its first FTRAN starts from 10 nonzeros against 7. The two are columns 0
and 14. Each has cost 0 and, as the simplex sees it, the bounds [0, 40]
and [0, 125]. The slack basis put a zero-cost column with two finite bounds
at its lower bound. As it is, that bound is 0. Mirrored, the bounds are
[-40, 0] and [-125, 0], and the columns start at -40 and -125. The
mirrored solve starts from another point.

## The change

`build_initial_basis` puts a zero-cost column with two finite bounds at the
bound of smaller absolute value, and at its lower bound when the two are
equal. A mirror swaps the two bounds and their sizes, so the mirrored
column starts at the mirrored bound. A column with a cost, or with one
finite bound, starts where it did. Both bounds keep the start dual
feasible, so no answer depends on the choice.

The same rule in the basis repair, for the column that leaves a singular
basis, made `bnl2`'s warm re-solve in `make warm` take 612 iterations and
17,651,972 work units against 439 and 12,426,868. The repair keeps its
lower-first rule.

`test_a_mirrored_column_starts_at_the_mirrored_bound` in
`tests/test_simplex.c` solves a four-column LP and its mirror. On 9b14bd0
the two count 4,289 and 4,299 work units; with the change both count the
same.

## The reading

| set | files | same | path | DIFF | 02-440 on 69f0284 |
|---|---|---|---|---|---|
| Netlib | 94 | 93 | 1 | 0 | 48 / 45 / 1 |
| Netlib infeasible | 29 | 26 | 3 | 0 | 21 / 8 / 0 |
| Kennington | 16 | 16 | 0 | 0 | 9 / 7 / 0 |
| Maros-Meszaros | 138 | 116 | 22 | 0 | 116 / 22 / 0 |
| CBLIB continuous | 29 | 29 | 0 | 0 | 29 / 0 / 0 |
| CBLIB mixed-integer | 80 | 31 | 48 | 1 | 31 / 48 / 1 |
| MIPLIB 3 | 24 | 0 | 22 | 2 | 0 / 22 / 2 |

`pilot` takes 5,636 iterations and 888,832,160 work units both ways. On
02-440 the mirrored model stopped at 2e9 work units, and took 31,170
iterations under 2e10. The four sets that do not move hold QPs, which the
barrier solves, conic models, and MIPs, whose trees part for their own
reasons (TODO J29).

## What still parts

`pilot87` of Netlib and `qual`, `refinery` and `vol1` of the infeasible
set part before the first pivot, again in `compute_primal`. Each has
zero-cost columns boxed in [-a, a]: 9 in `pilot87` and 117 in each of the
three others. Both bounds are as near zero, so the column starts at -a
both ways. `pilot87` stops at 2e9 work units both ways. On it the
sign-free sum of the basic values at iteration 0 differs in the twelfth
digit, and the dual ratio test of iteration 933 admits 643 candidates
against 642. TODO J28 carries the tie.

## The gates

`make netlib netlib-infeas netlib-kennington primal barrier pdlp concurrent
warm miplib`, each against its baseline: 0 regressed. Only `vtp-base`
changes. Its dual solve takes 45 iterations and 39,552 work units against
46 and 41,312, its primal 72 against 89, and its cold solve after the
branch in `warm.txt` 49 against 51. `bench/results/` is rewritten by these
runs. `bench/netlib.baseline` stays as it is: `make netlib-baseline` would
also rewrite 21 older lines where it lags the results of 9b14bd0.
