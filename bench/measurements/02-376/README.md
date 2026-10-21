# 02-376 — a quadratic row's spare columns leave its factor

Taken on 2026-10-21 on the tree of 299c543, for TODO row J13.

## The failure

QPLIB has 14 mixed-integer QCQPs (`miqcqp.sh`). QPLIB_10006 and
QPLIB_10007 ended with an error: their one quadratic row touches 3125 and
4375 columns, and `CONIC_QC_DENSE` is 3000. 1250 and 1750 of those
columns are fixed at 0 and appear nowhere else. The other 12 stop at the
work limit. On QPLIB_10005 the root relaxation took 1.27e10 work units for
3 iterations: its quadratic row's dense factor covers 2000 columns, 1000
of them fixed at 0 and appearing nowhere else.

## The change

`jm_row_quadratic_factor` in `src/conic.c` leaves out the columns that
`conic_solve` marks as spare: fixed at 0, with no linear entry and in no
cone. Their terms in the row are exactly zero, and nothing else in the
model reads them.

A first version left out every column fixed at 0. One of 02-255's
brute-force solves on seed 2 then ended `numerical_error`: its column C7
is fixed at 0 but is also a member of a rotated cone whose tip holds the
optimum. Both trees fail the first dual check there by 3.9e-7. HEAD's
settle repairs the point, and the first version's settle did not. With
the narrower rule that model is optimal again.

## The reading

`read.sh`: HEAD against the tree.

- 02-253's 3000 generated conic models (`gen-*.txt`), 02-255's 3000
  mixed-integer ones (`mip-*.txt`) and QPLIB's 13 continuous QCQPs
  (`qcqp-*.txt`): the same lines, digests and work.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.
- QPLIB's 14 mixed-integer QCQPs at 1e10 (`miqcqp-*.txt`): QPLIB_10006
  and QPLIB_10007 reach the work limit where they ended with an error.
  The other 12 reach it as before. At 1e11 (`miqcqp-new-1e11.txt`) all 14
  stop at the work limit. That file prints no incumbent for a work-limit
  status; 02-379 reads them, and 5 of the 14 have one.
- QPLIB_10005's continuous relaxation alone: 9.90e10 work units in 22
  iterations on HEAD, 1.38e10 in 23 on the tree.

`make test` and `make sanitize` pass.

## Found on the way

`unsure.patch` makes 02-255's `misocp.c` print each brute-force solve
that ends with neither an optimum, an infeasibility nor a ray, and write
it as MPS under `/home/vall/uns/`. On seed 2, on HEAD and on the tree
alike, 50 solves in 5 models end so:

- 46 end `numerical_error` at an infeasibility certificate the checker
  refuses (columns reach infinity), on models with no quadratic row;
- 4 stop after 206 iterations with no direction the ray checker takes.

That is TODO row J20.

## What is left

No incumbent on 9 of the 14 at 1e11 (02-379). The tree's first point comes from
rounding the root relaxation and a dive of up to 50 solves. On
QPLIB_10005 one solve costs about 1.4e10.
