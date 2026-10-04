# 02-338 — root cuts scanned again at every node

Taken on 2026-10-04 on the tree of 7e37ad1, for TODO row J7
(`binkar10_1`: HiGHS closes it in 4066 nodes with cuts at its nodes).
MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work units a
model, against `bench/results/miplib.txt` and `miplib2017.txt` of 7e37ad1.

## What was built

`node-pool.patch` keeps the root's cuts as a pool. At every node below the
root whose relaxation is fractional, it scans the root cuts the node does
not hold, takes those its point violates by more than 1e-6 (1 + |lo|) and
by an efficacy over `JAOS_POOL_EFF`, the `JAOS_POOL_CAP` (50) most
efficacious, adds them to the node's rows and solves again, for
`JAOS_POOL_ROUNDS` rounds. The node's children inherit them like the
node's Gomory cuts, and drop them when they go slack. `JAOS_POOL_FREQ=K`
scans only at depths that are multiples of K. `JAOS_POOL_CHECK=FILE` reads
a solution file and counts the root cuts that cut it off.

## Readings

| arm | MIPLIB 3, geometric | MIPLIB 3, sum | `bell5` | 2017 gap sum | dual part | `binkar10_1` bound | `p200x1188c` bound |
|---|---|---|---|---|---|---|---|
| base (7e37ad1) | 1 | 1 | 1 | 15.28 | 7.288 | 6713.1 | 10789 |
| `pool1`: 1 round, efficacy 1e-4 | 1.130x | 0.924x | out of memory | 16.51 | 7.296 | 6720.4 | 10285 |
| `pool3`: 3 rounds | 1.166x | 1.015x | out of memory | 15.61 | 7.341 | 6720.4 | 10261 |
| `poolf5`: 1 round at depths 0, 5, 10, ... | 0.975x | 1.035x | 1.031x | 16.44 | 7.287 | 6721.6 | 10320 |
| `poole2`: 1 round, efficacy 1e-2 | 0.993x | 1.085x | out of memory | 16.26 | 7.290 | 6721.0 | 10376 |

The MIPLIB 3 means of the arms where `bell5` ran out of memory are over the
other 23. `bell5` grows past 4 GB in 20 minutes; without the pool it
solves in 20089 nodes. At 1.5e9 work units with one round it holds 9084082
(the optimum is 8966406) after 85976 nodes. `JAOS_POOL_CHECK` with the
optimum JAOS writes for it finds 0 of its 16 root cuts violated, so the
cuts are valid: the tree takes another path and does not find the
optimum.

On the 2017 set the bounds of `binkar10_1`, `mas76` and `neos5` rise a
little, and `p200x1188c`'s falls by about 450 in every arm, since a node
costs more and fewer are solved. The incumbents of `tr12-30`, `beasleyC3`,
`graphdraw-domain` and `mad` get worse in every arm. No arm lowers the
gap sum.

## Cuts separated at every node, for comparison

`binkar10_1` at 1e10 with `--node-mir --cut-depth 100000
--node-cut-cap 50` (a Gomory and a MIR round at every node over the node's
own bounds, local to its subtree) reaches a bound of 6726.6 in 5128 nodes,
against 6713.1 in 15610 without and 6720.9 for HiGHS at node 2460. With
`--node-mir` at the default depth of 3 it reaches 6715.7. Cuts made at the
nodes move this bound more than root cuts scanned again do.

Refused as `node-pool-scan` in `bench/refusals.txt`.
