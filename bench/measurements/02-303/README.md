# 02-303 — presolve at the warm nodes of the MIP tree

Taken on 2026-09-23 for TODO row H2, on the tree of e781049. Every node LP
runs the LP presolve again. Skipping it on nodes that start from their
parent's basis read 0.710x on MIPLIB 3 but blew up `enigma` and `bell5`
(`warm-node-no-presolve` in `bench/refusals.txt`, 02-300).

A callgrind profile of `l152lav` stopped at 300 nodes puts 1.6% of the
instructions in presolve itself and 94% in the dual simplex's iterations.
So the gain of skipping comes from the parent's basis kept whole, not from
presolve's own cost.

Each arm is `make miplib J=2` on a copy of the tree with one change, and
`cmpmip.py` sets its result file against the committed
`bench/results/miplib.txt` (work ratio per instance, geometric mean over
the instances optimal in both).

| arm | change | work | past 2x |
|---|---|---|---|
| detect | presolve runs at a warm node, but its reductions are dropped unless it proves the node infeasible or solved | 0.734x (23) | enigma 86.9x, misc07 2.89x; bell5 stopped at 6.5 GB |
| guess | a node's guessed steepest-edge weights are replaced by exact ones after `DSE_GUESS_RESTARTS` drifts, as outside the tree | 0.9995x (24) | misc07 2.21x |
| guessdetect | both | 0.713x (23) | enigma 44.9x, misc07 2.05x; bell5 out of memory |
| keep4 | presolve's reductions kept at a warm node only when they remove at least 1/4 of the nonzeros | 0.722x (23) | misc07 3.17x; bell5 out of memory |
| keep10 | the same at 1/10 | 0.831x (24) | misc07 3.22x |

The detect arm shows what `enigma` loses without presolve. Its node LPs
carry the conflict and cut rows the tree adds, up to 672 rows over 100
columns, and after a node's fixings most of those rows are empty or
singletons, which presolve removes. Solved whole, the same LPs take 166
iterations each instead of 11.8, and their steepest-edge weights drift
and restart 458937 times over 696146 iterations (`enigma-node-lps.txt`),
because inside the tree a drifted weight restarts at 1 and is never made
exact. The guess arm stops that restart loop, but the LPs stay large.
Keeping presolve where it removes a real share of the model and dropping
it where it removes little is what the keep arms test: `enigma` reads
0.959x and 0.910x there, `l152lav` 0.366x and 0.721x, and at 1/10 `bell5`
needs 28265 nodes instead of 327119 (0.088x) and finds the reference
optimum. `misc07` is past 2x in every arm, its tree growing from 4712
nodes to between 8018 and 14194.

## The 2017 set

The refusal `mip-node-devex` asks a node change that one instance
decides to be read on the 2017 set. `node-presolve-keep.patch` is the
1/10 rule as it would ship (a constant `NODE_PRESOLVE_KEEP_DEN`, 0 to
turn it off, and its row for `docs/tolerances.md`). Both arms ran `make
miplib2017 J=2` at 1e10 work units on copies of 2d6f3dc, scored by
`../02-298/gapsum.py`:

| arm | incumbents | at the reference | primal | dual | gap sum |
|---|---|---|---|---|---|
| base | 17 | 4 | 0.5987 | 0.3463 | 1.000x |
| keep 1/10 | 17 | 6 | 0.5945 | 0.3439 | 0.993x |

The trees get through more nodes in the same budget (`neos-3381206-awhea`
75 to 4598, `csched007` 77 to 521, `beasleyC3` 6547 to 11776,
`p200x1188c` 14510 to 22261), `mas76` and `markshare_4_0` reach the
reference, `mad` finds an incumbent and `binkar10_1` loses its own. The
gap sum reads 0.993x against a pay rule of 0.95x, and MIPLIB 3 keeps
`misc07` at 3.22x, so the rule is refused (`node-presolve-keep` in
`bench/refusals.txt`). With the patch applied the netlib, infeasible and
Kennington gates and the five readings wrote the same files and `make
test` passed.
