# 02-241 — `diff` on one model in two files, and on two models; `show` on one row

SPECS row 124, the `diff` and `show` commands, said `done` and said
nothing else (`TODO.md` row 6). `diff A B` says whether two files
describe one model, comparing what a model is, exactly, and prints one
line per difference then `differences N`, exit 0 when they are one model
and 1 otherwise; `show FILE --row NAME` and `--col NAME` print one row or
one column with its bounds, its cost and integrality for a column, and
every term named by the other side, exit 5 when the name is not the
model's. `diffshow.c` writes the files; `diffshow.sh` runs the tool.

## The models

300 per seed, six seeds, 02-237's generator with column `X1` binary in
every model, so an indicator can go on a row with one edit, and a third
of the models carrying more integer marks. Every model is written as
`A.mps` and as `A.lp`; the LP writer took all 1800. A copy of the model
gets exactly one edit through the API and is written as `B.mps`.

The fourteen edits, one per model, and how often each was drawn: a cost
(117), a column bound (134), a row bound (128), a coefficient that
exists (136), a coefficient that did not (138), an integer mark (105),
the sense (143), the offset (122), a column name (125), a row name
(126), a semi-continuous mark (135), a quadratic diagonal (117), an
indicator (128), an SOS set (146).

## The properties

1. `diff A.mps A.mps` and `diff A.mps A.lp` print `differences 0` and
   exit 0: one model in two formats is one model
2. `diff A.mps B.mps` exits 1, prints the edit's kind and name first
   (`cost X7`, `row_bounds R3`, `entry X12`, `nonzeros`, `sense`,
   `offset`, `col_name 6`, `indicator R2`, `sos`, ...) and a
   `differences` count of at least 1
3. `show --row` and `show --col` print, byte for byte, the text the
   harness formatted from the model it built: name, index, bounds, for a
   column its cost and `integer`, the entry count and every term; and
   `--row NOSUCH` exits 5

## The reading

| seed | self | LP conversion | one edit | show |
|---|---|---|---|---|
| 1 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |
| 2 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |
| 3 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |
| 4 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |
| 5 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |
| 6 | 300 of 300 | 300 of 300 | 300 of 300 | 300 of 300 |

**No defect.** 1800 models: every file is one model with itself and
with its LP conversion, every edit is reported first by its kind and
name, every row and column is printed as built.

A first run reported 27 edits wrong, all of one kind: the harness added
1 to a coefficient of -1 and so removed a nonzero, and `diff` rightly
said `nonzeros`. The harness now steps a -1 to 1. The tool was right
every time.

## The pass is not vacuous

Two one-line breaks in the tool were measured on 60 models of seed 1,
then reverted: `diff` ignoring a cost that differs, and `show` printing
the upper bound on the `lower` line. The first let through 4 of 60
edits, every cost edit drawn; the second broke all 60 shows. The full
table of this batch's five controls is in 02-242's README.

## How to run

```
make all cli
bench/measurements/02-241/diffshow.sh            # six seeds
RUNS=60 SEEDS=1 bench/measurements/02-241/diffshow.sh   # one short seed
```
