# 02-306 — bound propagation at the nodes read again after d6245e0

Taken on 2026-09-23 for TODO row H2, on the tree of 2a57cd8. D324 refused
propagation at the nodes because it moved the branching, not because of
its own cost. Since d6245e0 the node LPs are cheaper, so it is read again.

`propagate-sweep.sh` is 02-305's script with `--propagate 0`, `1` and `2`
in place of the reliability: every MIPLIB 3 instance at 1e11 work units, a
4 GB cap on each solve, two at a time; `propagate-sweep.txt` holds the
status, work and nodes. Work against no propagation:

| passes | work over 24 | past 2x | worst |
|---|---|---|---|
| 1 | 1.038x | bell5 7.77x | bell5 14767 to 119753 nodes, bell3a 1.93x, misc03 1.24x |
| 2 | 1.171x | bell5 145.7x | bell5 to 2232277 nodes, bell3a 1.86x |

`p0033` 0.70x, `flugpl` 0.61x and `gen` 0.60x gain at one pass. The
bounds it moves still turn `bell5` and `bell3a` into much larger trees, so
D324 holds; its line in `bench/refusals.txt` carries this reading.
