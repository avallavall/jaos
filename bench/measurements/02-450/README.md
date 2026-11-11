# 02-450 — a row that forces on both sides

Taken on 2026-11-11 on the tree of 2d69b47 with this change to
`src/presolve.c`, for TODO row J29. The records are 02-447's harness
(`modes.patch` on 02-440's `mirrorfile.c`) on MIPLIB 3 under 2e9 work
units with seed 1: `miplib-rows.txt` with rows negated, `miplib-cols.txt`
with columns mirrored and `miplib-both.txt` with both.

## Where enigma parted

After 02-447, `enigma`, `l152lav`, `misc03` and `misc07` still took
another tree with their rows negated. On `enigma` the two runs agree
through 39 cuts and through every pivot of the tree's seventieth LP
solve. They part after it, when the tree reads the tableau for its
Gomory cuts: the basis the solve hands back has another basic column.

The cause is in presolve. A node's LP keeps a fixed column that is basic
in its start basis, so a row whose columns are all fixed stays in the
model, and it forces on both sides at once: its smallest and its largest
activity both equal its side. Presolve always took such a row as forcing
on its upper side. Mapped back, the multiplier is then the smallest of
the columns' ratios, and the column that gives it becomes basic. With the
row negated, the upper side is the other side, the multiplier is the
largest ratio, and another column becomes basic.

## The change

`ps_row_reads_up` reads a row in one direction: the finite side above
when only one is finite, the side of larger magnitude above, a positive
right-hand side for an equality, then a positive sum of the entries,
then a positive first entry. A row that forces on both sides is taken on
its upper side in that direction, so on its lower side when the row is
written the other way. A row with a positive right-hand side is treated
as before.

Two neutral rules were tried first and left. A multiplier of 0, which is
valid when every column is fixed, gave `enigma` 18,623 nodes as it is,
against 895. The side with the larger multiplier gave it 3,824, and
`l152lav` 2,272 against 1,438 and `misc07` 20,028 against 13,020 at their
full solves.

`test_a_row_forcing_on_both_sides_reads_the_same_negated` in
`tests/test_presolve.c` solves a two-row LP from a start basis, as a
node's LP, as it is and with its first row negated; the row's multiplier
must change sign and nothing else. It fails on 2d69b47.

## The reading

MIPLIB 3, models whose node count differs:

| what is mirrored | 3a81a08 | 02-447 | tree |
|---|---|---|---|
| rows negated | 7 | 4 | 0 |
| columns mirrored | 22 | 0 | 0 |
| both | 22 | 4 | 0 |

With rows negated, 17 models match in every number and 7 differ in work
units alone, or in iterations (`blend2`: 124,695 against 124,814). With
columns mirrored every model differs in work units, for the solve on the
model as loaded that follows the mirrored tree.

## The gates

`make test`, `make sanitize` and `make python-test` pass. `make netlib
netlib-infeas netlib-kennington miplib miplib2017 primal barrier pdlp
concurrent warm`: 0 regressed, and no line of `bench/results/` moves. The
QPLIB MIQP gate reads as its record apart from work units and QPLIB_4270,
which has differed from that record since 02-415.
