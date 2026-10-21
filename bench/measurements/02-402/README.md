# 02-402 — the same answers at 1 and at 4 threads

Taken on 2026-10-21 on the tree of 0a701f1. `read.sh` takes the readings.
No code changed.

## The question

JAOS's answers must not depend on the machine, the thread count included.
The thread count reaches the barrier's parallel Cholesky, the concurrent
solve's arms and the trees' node batches. The gates run at one thread, and
the unit tests cover a few small cases. The parallel Cholesky starts only
on blocks with enough work, so small generated models never reach it.

## The reading

`read.sh` runs each set at `threads=1` and at `threads=4` and
`compare.py` sets the two records side by side, instance by instance, on
the status, the work units and the digest of the answer's bits:

| set | instances | differ |
|---|---|---|
| Maros-Meszaros, as its gate runs it | 138 | 0 |
| CBLIB's continuous set, as its gate runs it | 29 | 0 |
| Netlib under the barrier | 94 | 0 |
| Netlib under the concurrent solve | 94 | 0 |
| MIPLIB 3 with tree batches of 4 nodes | 24 | 0 |

The `.cmp` files count two lines more per set, the record's summary lines.
