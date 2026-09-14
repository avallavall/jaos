# 02-233 — the presolve report, read against the log line beside it

SPECS row 57, presolve statistics, said `done` and said nothing else
(`TODO.md` row 6). `jaos_presolve_result` gives back the sizes the
simplex ran on, the round count and how many of each reduction fired;
the CLI's `solve` prints the sizes and the rounds as `presolve_*`, and
Python has `presolve_report()` at both layers. `prestats.c` reads six
properties, holding the report against the presolve log line the same
solve prints and against the model's own sizes.

## The models

2000 per seed, six seeds, the generator of 02-229: four to ten columns,
three to eight rows, and it plants what presolve removes, an empty row, an
empty column, a fixed column, a singleton row, a copy of another row and a
free column; every row holds the zero point. On top of it one column in
three is a cost-0 singleton, so the cost-0 singleton column fires too.
Each model is solved twice, the second time warm from the first.

## The properties

1. the report's reduced sizes are the sizes the presolve log line prints,
   and when the log says nothing fired they are the model's own sizes
   with every count zero
2. the report's counts are the counts the log line prints
3. the accounting closes: the rows left are the rows loaded less every row
   reduction, the columns left are the columns loaded less every column
   reduction, a free column singleton and an implied free column taking
   one of each
4. the second solve reports the same numbers field for field
5. nothing is negative and nothing left exceeds what was loaded
6. before any solve the report reads all zeros and `JAOS_OK`

## Two defects the first run found

The first 300 models broke properties 1 and 3 on 86 of them, all the
models where the log said nothing fired. Presolve had found reductions,
an empty row and a singleton row say, then met an empty column whose
cost runs to an infinite bound. It hands such a model back whole, because
an empty column says unbounded only where a feasible point exists, and
that is the simplex's to decide. The counts of what it had found and
discarded stayed in the report, so a caller read two removed rows on a
model the simplex solved as loaded. The counts are zeroed on that path.

With that fixed, property 3 still failed on three models in 300, every
one with a forcing row. A forcing row fixes the columns it holds and
removes itself, and the columns it fixed were counted nowhere, so the
column accounting could not close from the report. A column a forcing row
fixes now counts as a fixed column, in the report and in the log line,
which is what it is.

## The reading

| seed | models | reduced | solved by presolve | nothing fired | with a cost-0 singleton | broken |
|---|---|---|---|---|---|---|
| 1 | 2000 | 927 | 452 | 621 | 132 | 0 |
| 2 | 2000 | 970 | 412 | 618 | 153 | 0 |
| 3 | 2000 | 968 | 435 | 597 | 146 | 0 |
| 4 | 2000 | 934 | 440 | 626 | 132 | 0 |
| 5 | 2000 | 955 | 426 | 619 | 117 | 0 |
| 6 | 2000 | 912 | 463 | 625 | 139 | 0 |

**No defect left.** 12000 models, every property holds on every one; the
three Netlib gates are byte-identical, because nothing the simplex runs on
changed, only what is said about it.

## The pass is not vacuous

`prestats.sh control` makes two one-line edits to `src/model.c`, one at a
time, at 2000 models and seed 1, and puts the file back from a copy:

| the control | broken |
|---|---|
| as it is | 0 |
| the reported singleton column count pushed by one | P1 on 621, P2 on 1379, P3 and P6 on all 2000 |
| the reported row count pushed by one | P1, P3 and P6 on all 2000, P5 on 621 |

The two edits fail different properties: the first is caught by the log
line and the accounting, the second by the log line's sizes and by the
rows left exceeding the rows loaded on the models where nothing fired.

## How to run

```
make all
bench/measurements/02-233/prestats.sh            # six seeds
bench/measurements/02-233/prestats.sh control    # the two edits, seed 1
```

`control` rebuilds the library twice and leaves the tree built under the
current source.
