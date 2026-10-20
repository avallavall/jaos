# 02-374 — quadratic rows appended in order

Taken on 2026-10-21 on the tree of fd5275e.

## The cost

Under callgrind, reading QPLIB_3312 (41283 rows, 8281 of them quadratic,
each over two columns) took 6.83e9 instructions, and
`jaos_set_row_quadratic` alone 6.37e9. Each call rebuilt the three entry
arrays and walked every row of the model, so a reader that sets the rows
one by one paid rows × calls.

## The change

`jaos_set_row_quadratic` in `src/model.c`. When no row from the given row
onward has quadratic entries, the new entries go at the end: the three
arrays grow by `realloc`, and only the tail of `rq_start` changes. Any
other call rebuilds the arrays as before. Both paths leave the same
arrays.

## The reading

| QPLIB_3312, `jaos stats` | HEAD | this tree |
|---|---|---|
| instructions | 6.83e9 | 5.97e8 |
| seconds | 0.25 | 0.03 |

The solve's work units do not change (4143745365 at a limit of 3e9).
QPLIB's 13 continuous QCQPs (02-316's `qcqp.sh`) write the same lines as
02-373's `qcqp-new.txt`. `make test` and `make sanitize` pass.
