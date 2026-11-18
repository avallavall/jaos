# 02-489 — ZI rounding at the nodes

Taken on 2026-11-18 on the commit of 02-488 (crossover, mutation and DINS),
for TODO row J32. `read.sh` takes the readings from copies of 37720d32
with 02-488's `prototype.patch` (`JAOS_GH=1`, ZI rounding alone, `zi`)
and of the landed commit (`land`, with 02-488's three):

- `miplib2017/*.txt`, `*-who.txt`, `m17cmp.txt`, `sum.txt`: MIPLIB 2017
  at 1e10 through the CLI, as in 02-488;
- `miplib3/*.txt`, `m3cmp.txt`: MIPLIB 3 through `bench/run`;
- `g394/*.txt`: 02-394's 34 general-integer models at 5e8.

## What was built

ZI rounding (Wallace, "ZI round, a MIP rounding heuristic", 2010) at every
node of a linear model's tree, before crossover: for each fractional
integer column of the relaxation, the slack of its rows and its bounds
give how far it can move up and down with every row held; it moves to the
end of the side that leaves it least fractional, the cheaper side on a
tie, and only if that lowers its fractionality. Five passes at most
(`MIP_ZI_PASSES`); the point is offered when every integer column ends
whole. The continuous columns stay at the relaxation's values, so the
point holds every row within half the primal tolerance. The rows'
activities come from the node's solution. ZI stops for good after 1000
nodes that gave it no incumbent (`MIP_ZI_TRIAL`).

## MIPLIB 2017

| | primal | dual | sum |
|---|---|---|---|
| 37720d32 | 5.5284 | 6.1232 | 11.6517 |
| ZI alone (`zi`) | 5.4058 | 6.1203 | 11.5262 |
| 02-488 (`land` there) | 5.1957 | 6.1223 | 11.3180 |
| landed | 5.1419 | 6.1310 | 11.2729 |

ZI finds incumbents on three models in both readings: `beasleyC3` (nine,
all by node 98), `gen-ip002` and `gen-ip054`. It is never the last; on
`beasleyC3` its early points lead the sub-MIPs, 0.0928 to 0.0504 alone and
0.0106 to 0.0080 over 02-488. Elsewhere it stops after its 1000 nodes,
at 1.5e5 to 1.4e7 work units of 1e10. Where it has found one incumbent it
does not stop: on `gen-ip002` in the landed reading it ran at 309,127
nodes for 2 incumbents and 2.4e8 work units.

The primal gaps that move, landed against 02-488: `timtab1` 0.382 to
0.339, `mas74` 0.032 to 0.018, `supportcase26` 0.0327 to 0.0279,
`beasleyC3` 0.0106 to 0.0080, `tr12-30` 0.0007 to 0, `kasai` 0.0022 to
0.0019; `neos-3754480-nidda` 0 to 0.0088 and `csched007` 0.134 to 0.137.
ZI found none of those points itself: the moves come from the trees'
paths. The dual side gives 0.009, from `assign1-5-8` (0.0566 to 0.0613)
and `neos-3046615-murg`.

## MIPLIB 3

ZI alone reads 0.999x geometric and 1.000x total, every instance within
5%. The landed commit reads 1.039x and 1.046x against 02-488's 0.993x and
1.004x: `blend2` goes from 0.443x back to 0.814x and `p0282` from 1.040x
to 1.205x, path changes on models where ZI finds nothing. None regresses.

## 02-394's general-integer models

Their rows are equalities, which leave ZI no slack, and it finds no point
on any of the 34. Its work moves the trees: the landed commit has points
on 29 of 34 against 02-488's 28, better on 11 and worse on 6 of those
both have.

## The verdict

Landed: it holds MIPLIB 3 and MIPLIB 2017's primal side, and gives
`beasleyC3` its first points. The stop after one incumbent is a next step
in TODO J32.
