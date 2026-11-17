# 02-468 — 1-opt on every heuristic point, refused

Taken on 2026-11-17 on 2b85caf with `one-opt.patch`, for TODO row J32.
`read.sh` applies the patch to a copy of HEAD and takes the readings one
solve at a time: `miplib3/one-opt.txt` and `full/one-opt.txt`.
`full/before.txt` is 02-466's record.

## What was built

1-opt, the cheapest improvement heuristic. Every point that passes
`rounded_point` (the dives, the pump, the sub-MIPs, the exchanges, the
starts and the rounding of each node's LP all do) is polished before it is
offered:
each integer column with a cost moves towards a better objective as far as
its bounds and the slack of its rows allow, one column at a time, in column
order, with every other column held. It costs one pass over the matrix and
takes no sub-MIP budget. It is off for a quadratic objective, SOS,
indicator rows and live semi-continuous columns. `J32_ONEOPT=1` turns it
on.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: 0.998x, no instance past 2x.
`bell5` reads 1.23x (21,414 to 26,691 nodes), `blend2` 0.86x and `p0201`
0.87x.

MIPLIB 2017, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-466) | 6.8396 | 6.5210 | 13.3607 |
| 1-opt | 6.8457 | 6.5210 | 13.3667 |

Four of 30 instances change:
- `tr12-30` reaches its optimum, 0.0005 to 0;
- `gen-ip054` loses its optimal incumbent at 1e10, 0 to 0.0025;
- `beasleyC3`, 0.028 to 0.032;
- `gen-ip002` ends at another node count with the same gap.

The points it gets are already tight. A dive, a sub-MIP or a node's LP
ends at a vertex, where each integer column with a cost is held by a row
with less than a step of slack. 1-opt holds the continuous columns,
so it cannot let one of them take up that slack. Where it does move a
column, the new incumbent changes the tree's path, and that is as likely
to lose as to win.

## The verdict

Refused: `bench/refusals.txt`'s `one-opt` line says what reopens it.
