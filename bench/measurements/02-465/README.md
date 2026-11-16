# 02-465 — how far J7's seven are from closing

Taken on 2026-11-16 on f6818bc, with strong branching on by default
(02-464), for TODO row J7. `read.sh` takes the readings one solve at a
time:
- `seven-5e10.txt`: the seven at 5e10 work units;
- `node-presolve-off.txt`: `pk1` and `mas76` at 5e10, with
  `MIP_NODE_PRESOLVE_ITERS` at 0, so every node after the first 50 skips
  the LP presolve;
- `profile-pk1.txt`, `profile-mas76.txt`: callgrind's instructions by
  function, at 1e9 work units.

## The seven at five times the budget

| instance | at 1e10 (02-464) | at 5e10 |
|---|---|---|
| `pk1` | dual gap 0.203 | optimal at 1.64e10, 337,705 nodes |
| `mas76` | 0.0057 | optimal at 1.95e10, 632,196 nodes |
| `gen-ip002` | 0.0043 | bound -4795.71 against -4783.73, 1.81M nodes |
| `gen-ip054` | 0.0060 | bound 6813.33 against 6840.97, 3.12M nodes |
| `neos5` | 0.067 | bound 14.375 against 15 |
| `assign1-5-8` | 0.092 | bound 194.69 against 212 |
| `binkar10_1` | 0.0035 | bound 6725.03 against 6742.20 |

`pk1` needs 1.64 times less work to close inside the benchmarks' budget,
and `mas76` 1.95 times. The other five stay open at five times the
budget, so their bound per node, not their node cost, is what is short.

## Where a node of `pk1` and `mas76` goes

The tree's own split at 1e10:

| instance | node relaxations | heuristics | strong branching | propagation | the rest |
|---|---|---|---|---|---|
| `pk1` | 87.5% | 12.2% | 0.1% | 0.0% | 0.1% |
| `mas76` | 80.5% | 16.1% | 0.1% | 1.7% | 1.6% |

Both find the optimum at node 1, so the heuristics' 12% to 16% buys
nothing on them.

A `pk1` node takes 12 dual iterations, about 4,000 work units each. A
`mas76` node takes 6, with node presolve kept: its trial nodes average
under `MIP_NODE_PRESOLVE_ITERS`.

By instructions, `mas76`'s nodes spend about 20% in presolve
(`ps_row_range` 12.5%, `jm_presolve_run` 4.0%, `ps_build_rowwise`,
`ps_bound_shift`) and about 15% in Curtis-Reid scaling of each reduced
model (`scale_curtis_reid` 10.7%, `log2` 4.2%). `pk1`'s nodes spend them
in the simplex: factorisation 9.3%, `price_all` 8.5%, and the solves.

In work units, presolve is the smaller share. With it off after the trial,
`mas76` closes at 1.84e10 in 564,475 nodes, 0.94x. `pk1`, which skips it
already, reads the same.

## The verdict

Nothing changes in the code. For `pk1` and `mas76` the target is the node
relaxation's work: 1.64x and 1.95x less (1.84x with node presolve off),
through fewer iterations per node or cheaper ones, and the heuristics'
share once an incumbent is held. For
the other five it is the bound per node. TODO J7 holds both.
