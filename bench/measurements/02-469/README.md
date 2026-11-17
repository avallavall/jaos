# 02-469 — threshold accepting over the items of partitioning rows

Taken on 2026-11-17 on a573cc7 with this change, for TODO row J32.
`read.sh` takes every reading from a copy of HEAD, one solve at a time:
- `miplib3/landed.txt`;
- `full/landed.txt` and `full/sum.txt`;
- `budget.txt`, from a second copy with `trace.py`'s prints;
- `prototype.txt`, from `prototype.sh`.

`full/before.txt` is 02-466's record.

## The shape

02-463's census puts `mad`, `neos-911970`, `glass4`, `csched007`,
`assign1-5-8` and `graphdraw-domain` in one family: almost every binary is
an item, that is, it sits in exactly one partitioning row. In `mad` and
`neos-911970` every free binary is an item and every continuous column
sits in one row:
- `mad` assigns 20 items to 10 groups with capacities, and its objective
  is the sum of 10 absolute deviations;
- `neos-911970` assigns 35 items to 24 machines, and pays for each
  machine's overflow.

Given the items, each row's continuous columns then cost what covering the
rest of its bounds costs, in closed form. Each item also has a slot: the
set of its other rows. The slot names the group or the machine, and in
both models every partitioning row has exactly one item per slot.

## The prototype

`prototype.py` searches the items' assignment with 100,000 moves, from a
greedy start in a seeded order. A move does one of two things:
- gives one partitioning row another of its items;
- trades the slots of two rows' items, the generalized assignment
  exchange.

The rows without continuous columns are hard, and a move that raises
their violation is refused. The first threshold is the median rise among
2,000 sampled moves. `prototype.txt`, seeds 1 to 3:

| rule | `neos-911970` (optimum 54.76) | `mad` (optimum 0.0268) |
|---|---|---|
| annealing, geometric cooling | 55.26, 54.76, 55.26 | 0.122, 0.172, 0.203 |
| threshold accepting, linear | 55.72, 56.24, 57.43 | 0.155, 0.158, 0.180 |
| threshold accepting, geometric | 54.76, 55.26, 55.26 | 0.177, 0.119, 0.125 |
| the same without trades | 74.62, 59.62, 107.31 | 0.247, 0.458, 0.357 |

Threshold accepting with a geometric fall matches annealing, and needs no
`exp`, whose rounding differs between libraries. The trades are what reach
the optimum. On `mad` no rule comes near the tree's 0.038 at 1e10. An
iterated descent with random kicks, tried first, stopped at 0.30.

## What was built

`anneal_for_point` in `src/mip.c` runs once at the root of a linear model
of this shape, after the sub-MIPs.
- **Detector:** every free integer column is a binary item of one
  partitioning row, and every continuous column sits in at most one row,
  priced so that it cannot run to an unbounded cost.
- **Pricing:** each row sorts its continuous columns by cost per unit of
  activity, upward and downward from the cheapest point.
- **Start:** the incumbent, or else a greedy assignment.
- **Search:** the prototype's two moves, drawn by a seeded generator.
  - A move that lowers the hard rows' violation is taken.
  - One that keeps it is taken while its rise stays under the threshold.
- **Threshold:** the median rise of `MIP_ANNEAL_SAMPLE` moves at first. It
  falls in `MIP_ANNEAL_LEVELS` geometric steps to `MIP_ANNEAL_COOL` of
  that.
- **Budget:** `MIP_ANNEAL_WORK` times the model's size.
- **Result:** the best feasible assignment, if it beats the start, has
  its integers fixed and the rest solved.

A sub-MIP of such a model has the same shape, so the search also runs at
each sub-MIP's root.

`budget.txt` traces both runs, the first inside the root's sub-MIP:

| instance | sub-MIP's run | root's run | share of the root's work |
|---|---|---|---|
| `mad` | 0.748 | 0.748 to 0.115, last point at 0.43 of the budget | 22% |
| `neos-911970` | 57.80 | 57.80 to 54.96, last point at 0.58 of the budget | 4% |

The log credits the sub-MIP's points to the sub-MIP.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: 0.985x, no instance changes
status, and none has the shape. Five end at other nodes:

| instance | work | nodes |
|---|---|---|
| `blend2` | 0.90x | 5,105 to 4,337 |
| `misc07` | 0.97x | 9,781 to 9,485 |
| `lseu` | 0.99x | 5,019 to 4,967 |
| `mod008` | 1.01x | 1,451 to 1,507 |
| `bell5` | 1.00x | 21,414 to 21,034 |

They move because the detector's pass adds its work to the root, and later
choices are measured against the work.

MIPLIB 2017 at 1e10, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-466) | 6.8396 | 6.5210 | 13.3607 |
| this change | 6.5397 | 6.5166 | 13.0562 |

The two models of the shape:
- `mad`: incumbent 0.297 to 0.088 of the optimum away;
- `neos-911970`: 0.030 to 0.0037, and its dual gap 0.054 to 0.052.

The other instances move only by the work the detector adds:
- better: `csched007` 0.405 to 0.316, `mas74` 0.081 to 0.047,
  `neos-3754480-nidda` 0.056 to 0.053;
- worse: `neos-3046615-murg` 0.047 to 0.077, `beasleyC3` 0.028 to 0.054,
  `supportcase26` 0.006 to 0.011.

Those moves net about -0.07. The two models of the shape give -0.24.

## The verdict

Landed, on by default with the rounding heuristics. `--no-heuristics`
turns it off. TODO row J32 keeps the models whose continuous columns link
rows.
