# 02-407 — a linear tree with no incumbent propagates from node 1000

Taken on 2026-10-21 on the tree of eb88d70. The scripts in `variants/`
took the readings, each on a copy with an environment switch; the change
is the same rule with no switch.

## The idea

TODO row J7 holds 02-394's general-integer models with equality rows,
where JAOS finds a first point late or never. On the 34 models it left at
2e9 work units, at 5e8: `--propagate 4` gives a point on 30 against the
default's 23, `--probing` changes nothing, and `--restart` never fires
(`02-406/`). Propagation is off for a linear MIP because its moved bounds
change the branching and grow the trees (D324). The question is where it
can run without that cost.

## The variants

Each variant propagates 4 passes at a node while the tree has no
incumbent, on a linear model whose `--propagate` is not set, from a
different node (`variants/*-34.txt`, `*-miplib.txt`, against
`bench/results/miplib.txt`):

| from | 34 models with a point (default 23) | MIPLIB 3 work, geometric mean |
|---|---|---|
| the root | 30 | 1.501x: `bell5` 409x, `enigma` 8.7x, `gt2` 2.0x, `bell3a` 1.9x |
| node 2 | 30 | 1.103x: `enigma` 8.7x, `misc07` 1.15x |
| node 1000 | 27 | 1.000x, every instance the same |

From the root, the propagated bounds become the tree's integer bounds and
change every cut after them; `bell5` has its point at node 1 and still
pays. From node 2, `enigma`'s first point, its optimum at node 895, comes
at node 22616. Every MIPLIB 3 instance finds its first point before node
1000, the threshold the no-incumbent dive already uses
(`MIP_NOINC_DIVE_AFTER`), so from there MIPLIB 3 does not move. On the 34
models the incumbents at 5e8 differ both ways from the default's (13
better, 11 worse, 3 the same, 7 with no point either way), as two
different trees give.

On MIPLIB 2017 at 1e10 (`variants/node1000-m17.txt`, against
`bench/results/miplib2017.txt`), 25 of the 30 instances read the same.
The five that move are the ones whose first point comes late or never:
`glass4`'s incumbent falls from 5.50e9 to 4.92e9, `csched007`'s bound
moves down by 1e-4 of its size and `timtab1`'s by 1e-5, and `ic97_potential`
and `neos-3046615-murg` keep their incumbent and bound. On
`ic97_potential` propagation tightens nothing. The gap sum of
`02-328/m17sum.py` reads 13.4804 before and 13.4805 after.

## The change

`src/mip.c` runs `MIP_NOINC_PROPAGATE` (4) passes of propagation at each
node of a linear tree from node `MIP_NOINC_DIVE_AFTER` while it has no
incumbent, unless `--propagate` is set; a value set, 0 included, replaces
it. `tests/data/mip_noinc_propagate.mps` is `biggeneral` seed 482: with
the rule its first point comes at node 2097, without it at 14914, so at a
node limit of 3000 only the default has one. 02-397's option sweep reads
no broken variant, HEAD or the tree, and `make miplib J=2` is
byte-identical. Taken after the commit, on 8dcb600: `make netlib
netlib-infeas J=12` and `make netlib-kennington J=2` leave their results
files byte-identical too, and those cover 02-397's change to `mip.c` as
well.
