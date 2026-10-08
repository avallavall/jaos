# 02-368 — a node's presolved model scaled from the tree's LP, refused

Taken on 2026-10-08 on the tree of 26c77af, for TODO row J8.

## The idea

A MIP node that runs the LP presolve gets a new reduced model, and the
simplex computes its Curtis-Reid scale from nothing: up to 30
conjugate-gradient iterations a node, 22% of `bell5`'s instructions
(02-353). `node-scale.diff` gives the reduced model the scale of the
tree's LP instead, row by row and column by column through presolve's
`orig_row` and `orig_col`. The tree's LP is scaled once, and again only
when its matrix changes. `JAOS_NODESCALE` switches it on.

## The reading

`ns.sh`, six MIPLIB 3 models, off and on:

| model | nodes off | nodes on | work on/off | seconds off | seconds on |
|---|---|---|---|---|---|
| `bell5` | 20089 | 106667 | 4.90x | 1.44 | 7.82 |
| `flugpl` | 1937 | 1937 | 0.89x | 0.04 | 0.04 |
| `lseu` | 4005 | 4011 | 1.04x | 0.27 | 0.30 |
| `enigma` | 895 | 12306 | 4.63x | 0.08 | 0.34 |
| `p0201` | 351 | 396 | 1.12x | 0.20 | 0.22 |
| `misc07` | 13020 | 7964 | 0.57x | 9.30 | 5.08 |

The other scale changes each node's vertices, and with them the
branching, so the trees move by up to 14x. The time a node takes does not
fall either: `bell5` runs 72 µs a node off and 73 µs on. Refused as
`node-scale-from-parent`.
