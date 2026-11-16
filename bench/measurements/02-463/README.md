# 02-463 — exchanges between assignment rows find `enigma`'s point; the structures the open instances share

Taken on 2026-11-16 on 3083afc with this change, for TODO row J7.
- `census.py` reads every instance of a set: `census-miplib3.txt` and
  `census-miplib2017.txt`.
- `read.sh` takes, one solve at a time, the MIPLIB 3 and MIPLIB 2017
  readings by default (`miplib3/default.txt`, `full/default.txt`),
  `enigma` under four strong-branching settings (`enigma.txt`), and the
  census.

## Why

02-462 found that `enigma`'s root bound is its optimum, so its tree is the
search for a first feasible point. No heuristic JAOS has finds that point,
the feasibility jump at 1000x its work included. That search moves with
any change to the branching, so `enigma` decided the MIPLIB 3 gate for
strong branching.

`enigma` is a cryptarithm, a permutation of letters over digits under one
equation. From a permutation, the next one is four flips away, and the
jump moves one binary at a time.

## What was built

`swap_for_point` in `src/mip.c` runs at the root of a linear model with no
incumbent once the feasibility jump finds none. It needs three things:
- the model has rows that take exactly one of their binaries
  (partitioning rows);
- at least half of its free columns are cells, binaries in exactly two
  such rows;
- no column is continuous and free. Other free integer columns are held
  at their lower bound.

Each start is a seeded random assignment that covers every partitioning
row once. A move takes two cells at 1 and gives each one's row to the
other: `(a, b)` and `(c, d)` become `(a, c)` and `(b, d)`, or `(a, d)` and
`(b, c)`, when those cells exist. Every partitioning row stays satisfied.

From each start, the search takes the move that most lowers the violation
of the other rows, until none does, then starts again. A point it finds is
fixed and its continuous rest solved, as for the jump. When the objective
is integral over the cells, the search then looks for a point at least 1
better, until the root's bound rules one out.

Its work is at most `MIP_SWAP_WORK` (5000) times (nonzeros + columns +
rows), within the jump's share of the root's work (`MIP_FJ_ROOT`).
`tests/test_mip.c` reads it on a 7x7 cryptarithm whose equation reads the
digits in base 7, so that one permutation satisfies it.

## `enigma`

| | nodes | first incumbent at | work |
|---|---|---|---|
| before | 895 | 895 | 18,684,934 |
| with the exchanges | 1 | 1 | 3,678,186 |

The search reaches the optimum, 0, in 192,697 work units, 0.094 of its
budget. The root's bound is 0, so the tree ends at the root. The result
is the same under every strong-branching setting: `--reliability 4` at
caps 0, 1 and 4, which had read 730 to 9,950 nodes.

## MIPLIB 3 and MIPLIB 2017

In both sets only `enigma` has the shape:

| model | cells | items, binaries in one partitioning row | binaries |
|---|---|---|---|
| `enigma` | 99 | 1 | 100 |
| `l152lav` | 202 | 3 | 1,989 |
| `mod010` | 239 | 2 | 2,655 |
| `air03` | 30 | 116 | 10,757 |

No MIPLIB 2017 model has a cell.

By default, MIPLIB 3 reads `enigma` at 0.20x and the total at 0.9987x
(`miplib3/default.txt`). No other instance moves past 0.8x or 1.25x, and
none regresses.

MIPLIB 2017 by default reads 13.6705, the same gap sum as before
(`full/default.txt`). No model there has the shape. The scan's few work
units move `glass4` by 6 nodes and `neos-3046615-murg` by 5 at the 1e10
limit, and no gap. `bench/results/miplib2017.txt` is this reading.

## The census: what the open instances are

`census.py` classifies every row and counts five structures. With
`bench/results/miplib2017.txt` (02-459's record) beside it, the open 2017
instances fall into seven families. Each has heuristics in the literature
made for its shape. None of those is in JAOS, and none of these families
has a member that JAOS closes.

| family | instances, what JAOS reaches at 1e10 | what marks it | a heuristic made for it |
|---|---|---|---|
| Periodic timetables (PESP) | `ic97_potential`: no incumbent. `timtab1`: incumbent 0.68 of the optimum away | rows of two potentials and an integer period column (`periodic`: 146 on `ic97_potential`) | modulo network simplex (Nachtigall and Opitz, 2008), from an event order fixed by the relaxation |
| Scheduling with one start per job | `csched008`: no incumbent, the root takes the whole budget. `csched007`: 0.35 away | 50 partitioning rows over 1,284 to 1,457 items, precedence rows | list scheduling, the serial schedule generation scheme (Kolisch, 1996), its priorities read from the relaxation |
| Big-M placement and geometry | `glass4`: 1.0 away (4.9e9 against 1.2e9). `graphdraw-domain`: 0.44. `supportcase26`: 0.025. `neos-3754480-nidda`: 0.028 | 200 to 792 big-M rows over binaries that pick a side | fix every disjunction to the side the relaxation's coordinates order, then solve the LP; then local moves over the order |
| Fixed-charge networks | `beasleyC3`: 0.10 away. `tr12-30` and `neos-3627168-kasai`: 0.0005 to 0.017 | flow rows (340 to 816) and variable bounds `x <= u y` | dynamic slope scaling (Kim and Pardalos, 1999), then closing and opening arcs with the LP solved again |
| Partitioning with items | `mad`: 0.30 away. `neos-911970`: 0.015. `assign1-5-8`, `glass4`, `csched*` share the shape | items: binaries in one partitioning row (130 to 1,457) | the exchange of this change carried to items: move one item to another of its row's columns, or trade two items between rows |
| Market split | `markshare2`: incumbent 119 against 1 | a few dense rows of binaries and one slack each | the lattice reformulation of Aardal, Bixby, Hurkens, Lenstra and Smeltink (1999) |
| General integer equality systems | `gen-ip002`, `gen-ip054`; 02-394's 34 models with no point | integer knapsack and equality rows over general integers | the lattice reformulation of Aardal, Hurkens and Lenstra (2000) |

`pk1`, `mas76` and `neos5` already hold the optimum, so a heuristic does
not close them. `mas74`, a knapsack system like `mas76`, is 0.042 away.

## The verdict

The exchanges are landed and on by default. TODO row J32 holds the
families above, each with its heuristic, how it is detected, and how it is
verified.
