# 02-334 — the root's re-solves after cuts make their weights exact

Taken on 2026-10-04 on the tree of 4e574b7, for TODO row J7. The settings
are 02-328's; the arm ran from 02-329's `arms.sh` with the switch
`JAOS_CUTEXACT=1`, which the shipped code holds as `DSE_RESOLVE_EXACT`.

The tree solves every relaxation on one copy of the model marked as a node
solve, and a node solve keeps guessed steepest-edge weights
(`DSE_GUESS_RESTARTS`). 02-333 showed that exact weights after one size of
iterations in every node solve fix `csched008`'s root and cost MIPLIB 3
(`node-dse-exact-long`). Here the copy carries a second mark,
`cfg.cut_resolve`, only while the root's cut rounds run: the re-solves
after each round, and the aggregation probes on copies of the root.

`csched008` with heuristics off and `--node-limit 1`: 116568 iterations
and 1.29e10 work units before, 30066 and 3.71e9 with the mark; the bound is
171 both ways. `csched007` does not change (4751 iterations).

| arm | MIPLIB 3 geo | sum | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|
| 4e574b7 | 1 | 1 | 3 | 25 | 8.061 | 8.308 | 16.368 |
| b9 | 1.0000 | 1.0000 | 3 | 25 | 8.147 | 7.323 | 15.470 |

No MIPLIB 3 tree moves: none runs a cut re-solve past its size. On the 2017
set `csched008`'s root ends inside the limit and its bound reads 171 (the
reference 173) where it read none. `mad`'s incumbent goes from 0.2772 to
0.3504 and `neos-911970`'s from 55.6 to 56.3, with its bound from 52.01 to
51.80, and `pk1`'s bound by 1.2e-4 of itself; the other models end as
before.

The LP gates and readings and MIPLIB 3's record are byte-identical.
`miqp-b9.txt` has every status, incumbent, bound and node count of
02-333's `miqp-b8.txt`.
