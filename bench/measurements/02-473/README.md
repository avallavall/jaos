# 02-473 — pricing weights carried from parent to child, refused

Taken on 2026-11-17 on ec95542 with `carry.patch`, for TODO row J7.
`read.sh` applies the patch to a copy of ec95542 and takes every reading
one solve at a time:
- `nodes.txt`;
- `miplib3/carry.txt`;
- `full/carry.txt` and `full/sum.txt`.

`full/before.txt` is 02-469's record.

## What was built

02-465 found that a child solved from its parent's whole state takes 0.78x
the work of the tree's solve on `pk1` and `mas76`, and 0.52x on
`binkar10_1`, much of it in iterations. It named the pricing weights as
the part a node can keep, since few nodes follow their parent.

A node solve starts its dual steepest-edge weights at 1. With the patch:
- After a node's optimal solve on its unreduced model, the simplex keeps
  the final weights by basic variable.
- Both children copy them.
- When a child's basis is built on a model of the same size, each basic
  variable takes its parent's weight.

A node whose model was presolved, or whose rows changed with cuts, starts
at 1 as before. `J7_DSE=1` turns the carry on; without it the binary
reads as ec95542.

## The readings

`nodes.txt`, five instances to 20,000 nodes, without and with the carry:

| instance | nodes with carried weights | work | iterations |
|---|---|---|---|
| `pk1` | 19,687 | 0.887x | 0.850x |
| `binkar10_1` | 9,703 | 0.926x | 0.934x |
| `assign1-5-8` | 17,399 | 1.083x | 1.060x |
| `gen-ip002` | 30 | 1.000x | 1.000x |
| `mas76` | 0 | 1.000x | 1.000x |

`mas76`'s and `gen-ip002`'s nodes solve presolved models, which the carry
does not reach. At 20,000 nodes the trees already differ, so the bound
reached says little.

MIPLIB 3, against `bench/miplib.baseline`: 1.026x, no instance past 2x.
Seven end at other nodes:

| instance | work | nodes |
|---|---|---|
| `misc07` | 1.08x | 9,485 to 10,162 |
| `misc03` | 1.06x | 253 to 277 |
| `stein45` | 1.01x | 51,901 to 54,004 |
| `dcmulti` | 1.01x | 376 to 396 |
| `l152lav` | 0.99x | 476 to 468 |
| `p0201` | 0.98x | 102 to 104 |
| `stein27` | 0.98x | 1,229 to 1,224 |

MIPLIB 2017 at 1e10, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-469) | 6.5397 | 6.5166 | 13.0562 |
| carried weights | 6.7817 | 6.4885 | 13.2701 |

The dual side gains 0.028, on the instances J7 aims at:

| instance | dual gap | nodes |
|---|---|---|
| `pk1` | 0.203 to 0.176 | 206,742 to 235,665 |
| `neos5` | 0.067 to 0.060 | 33,482 to 63,265 |
| `timtab1` | 0.285 to 0.277 | 219,111 to 249,093 |

`assign1-5-8` loses 0.003 and `graphdraw-domain` 0.009.

The primal side loses 0.24 where the trees take other paths:
- `neos-3754480-nidda` 0.053 to 0.121;
- `csched007` 0.316 to 0.402;
- `tr12-30` 0.0004 to 0.046;
- `timtab1` 0.429 to 0.464;
- `beasleyC3` 0.054 to 0.064.

`binkar10_1` reaches its optimum, 0.0007 to 0.

None of the seven J7 instances closes.

## The verdict

Refused: `bench/refusals.txt`'s `node-dse-carry` line says what reopens
it.
