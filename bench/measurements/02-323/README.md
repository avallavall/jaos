# 02-323 — the pricing row reads the nonbasic columns only

Taken on 2026-09-25 on the tree of 5700138, for TODO row J8.

## The change

`price_all` in `src/simplex.c` builds the pricing row `alpha = rho' A`
row by row, over every column, and then zeroed the basic ones. It now
reads the solver's own copy of the scaled row-wise matrix, in which each
row holds its nonbasic entries first; `pr_nb[i]` says how many. A column
that enters the basis swaps its entries to the back of their rows, and one
that leaves swaps them to the front (`pr_turn`, charged the column's
nonzeros); a new basis, warm or slack or restored, partitions the copy
again (`pr_rebuild`, charged `nnz + nrow`). A logical column is skipped
when it is basic. A debug build checks the partition against the basis
wherever it checks the nonbasic bitmap.

Each nonbasic column still receives its terms in the same row order, so
its `alpha` is the same double as before, and the basic columns stay 0.

## Readings

Instructions under callgrind, `tools/icount.sh -r HEAD` (`icount.txt`):

| model | ratio |
|---|---|
| greenbeb | 0.945 |
| d2q06c | 0.949 |
| dfl001 | 0.965 |
| pilot87 | 0.966 |
| fit2p | 0.996 |
| stocfor3 | 0.997 |
| seba | 1.010 |

The geometric mean is 0.975. seba is small (3.3e7 instructions), and
there the upkeep of the copy costs more than the rows it saves.

Every gate gives the same answers: netlib, netlib-infeas, Kennington and
MIPLIB 3 write the same iterations, objectives, digests and bases, with
work at 0.957x on netlib (greenbeb 0.901x, seba 1.023x), 0.974x on
Kennington and 0.996x on MIPLIB 3. The four baselines are rewritten.

The readings `primal`, `barrier`, `pdlp`, `concurrent` and `warm` give
the same verdicts and answers, except where a run is stopped at 10x the
dual's work: the dual's work fell, so the limit falls with it and such runs
stop a few iterations earlier (d6cube's primal phase 2 at 1079 iterations
instead of 1097). One verdict moves for that reason: PDLP on sc50b goes
from ok to overrun. PDLP itself is unchanged.

`make test` and `make sanitize` pass.
