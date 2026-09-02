# 02-163 — BTRAN's L' pass walks its pattern, not every slot

D253. A row structure of L (indices only, built once per full-rank
factorization) feeds the same reachability DFS the U' pass has had since
D36, reusing its stamped workspace. A computed slot runs the identical
dot product over the identical column in the identical order, so the
change is judged first on identity: **every digest and every iteration
count on all 139 instances must be byte-identical**, and is. Work is the
only thing allowed to move.

## What is here

| file | what it is |
|---|---|
| `record-diffs.txt` | `record_diff.py` and `geomean.py` on all four records, candidate vs `b7d93b5`, as read for the verdict |

## Re-deriving

The before is every `bench/results/*.txt` and `bench/*.baseline` at
`b7d93b5`; the after is the same files in D253's own commit. The unit
test that drives the new pass through real sub-patterns is
`test_btran_unit_vectors_solve_exactly` in `tests/test_lu.c`; the
zero-dimension edge (a zero-row model under the reference build, where
`l_start[0]` was never written) is the defect `make configs` caught
mid-development, fixed by building nothing at dimension zero.
