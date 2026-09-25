# 02-320 — why bell5's tree grows under MIR aggregation

Taken on 2026-09-25 on the tree of f40ab3c, for TODO row J7, which asked
for this reading. `bell5.sh` runs it and `bell5.txt` is its record.

## The root

With `mip_mir_aggregate=6` the root adds 31 aggregated MIR cuts to the 16
it had, and its bound goes from 8653018.03 to 8653263.99. The optimum is
8966406.49, so the cuts close 0.08% of the root gap.

## The tree

| arm | nodes | work units | first incumbent |
|---|---|---|---|
| default | 14767 | 2.04e8 | node 790, by rounding |
| default, cutoff 8966407 | 14767 | 2.04e8 | |
| aggregation 6 | 2112667 | 2.79e10 | node 23627 |
| aggregation 6, cutoff 8966407 | 2112667 | 2.79e10 | |

A cutoff at the optimum changes neither tree, so the late incumbent is not
the cause: no node's bound reaches the optimum early enough for an
incumbent to prune it. The aggregated tree's bound climbs faster at first
(8956308 at node 1000, against about 8949960 at node 1100 by default) and
then crawls.

Capped at 200000 nodes and 2.5 GB:

| arm | status | bound at the cap |
|---|---|---|
| MIR at nodes off | optimal, 14767 nodes | |
| MIR at nodes off, aggregation 6 | node limit | 8961509 |
| aggregation 1 | node limit | 8955091 |
| aggregation 2 | node limit | 8961660 |
| most-fractional branching | node limit | 8727413 |
| most-fractional branching, aggregation 6 | node limit | 8689687 |

The default never generates an MIR cut at a node, and turning node MIR off
leaves it byte-identical. With node MIR off, aggregation still grows the
tree past the cap, so the aggregated cuts of the root alone do it, and one
aggregation step is enough. bell5 needs pseudocost branching: under
most-fractional branching both arms pass the cap.

bell5's tree is sensitive to its LP path. d6245e0 (a node keeps its
parent's basis through presolve) took it from 327119 nodes to 14767
(`bench/measurements/02-304/`). Aggregated cuts that move the root bound by
3e-5 of itself send pseudocost branching down a path 143x longer.

## What follows for J7

Judging aggregation on bell5's node count measures how its tree reacts to
any change of the root LP. A rule that keeps the aggregated cuts only when
they pay at the root would leave bell5's path as it is: aggregate on a copy
of the root LP, and add those cuts only when they lift the bound by more
than a threshold. On the fixed-charge networks aggregation lifts the root
bound of exp-1-500-5-5 from 39074 to 59627 (02-317); on bell5 by 246.
sp150x300d solves with flow covers and aggregation both on while its root
bound moves by nothing under aggregation, so such a rule would lose it.

Two of these runs grew past 3.5 GB in 17 minutes with node cuts off
(`mip_cut_depth=0`) and were stopped; the script caps every run that can
grow.
