# 02-297 — the relaxation over the columns ends by itself

Taken on 2026-09-22 on the tree of 2e2c7ca, for TODO row D1.

## What ran away

Over the columns the elastic copy frees every column, and a free integer
column gives the tree an unbounded space, so a freed integer column is
held in its own bounds widened by `M`. `M` doubles while the box is too
narrow or holds no integer point. A model whose rows plus integrality
admit no point at all has none in any box, so it doubled for ever:
`tests/data/relax_runaway.mps` (3 rows, 4 columns, 2 of them integer;
its rows ask `3 x1 + 2 x4 = -9.5`, which no pair of integers gives)
ended only on the caller's work limit, and said "the relaxation's solve
answered work limit reached".

## The caps

`M` now grows at most `RELAX_BOX_ROUNDS` times (16), and a round after
the first takes at most `RELAX_ROUND_WORK` (64) times the first round's
work, the first round being the LP that sets `M` plus the first box.
The rounds are what runs away rather than their number: on an infeasible
model a box twice as wide costs about four times the work.

`jaos relax tests/data/relax_runaway.mps --cols`, no work limit given:

    jaos: cannot relax tests/data/relax_runaway.mps: no point in the
    columns' box widened by 896, after 8 rounds and 61986861 work units,
    so there is no smallest violation to report; the rows and the
    integrality may admit no point at all

0.13 s, exit 5. The rows scope of the same model is untouched and ends
`optimal` at 3.1666666666666670 in 8788 work units. With a work limit
below the caps the report is the old one, "the relaxation's solve
answered work limit reached", and `tests/cli.sh` keeps that case.

## The caps cost nothing where an answer exists

`compare.sh OUT TREE_A TREE_B` builds `bench/measurements/02-230/relax.c`
against two checkouts and compares its lines. 02-230's generator plants
an integer point outside the boxes, so every model has an answer once the
columns are freed. Two seeds of 2000 models, three scopes each, 12000
lines a seed: the tree of 2e2c7ca and the tree with the caps print
byte-identical output on both seeds (602 and 563 of each 2000 carry no
integer mark and take the single-solve path).
