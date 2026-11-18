# 02-490 — five dives in the tree, refused

Taken on 2026-11-18 for TODO row J32. `read.sh` takes the readings from
copies of 37720d32 with 02-488's `prototype.patch` and of 02-489's commit
with `dives.patch`; `dives-tests.patch` holds that attempt's tests. No
code landed.

- `miplib2017/*.txt`, `*-who.txt`, `m17cmp.txt`, `sum.txt`: MIPLIB 2017
  at 1e10 through the CLI, as in 02-488: `dives` is the five dives alone
  (`JAOS_GH=62`), `base` 02-489's landed reading, `attempt` the same with
  `dives.patch`.
- `miplib3/*.txt`, `m3cmp.txt`: MIPLIB 3 through `bench/run`.
- `g394/*.txt`: 02-394's 34 general-integer models at 5e8: the five, the
  coefficient dive alone (`coef`), the line-search dive alone (`line`),
  the five at a share of 1% (`share001`) and every 20 nodes (`every20`).

## What was built

One dive engine and five rules for its column and side (Berthold,
"Primal heuristics for mixed integer programs", 2006; Hendel for the
vector length). A dive runs on a copy of the node's LP: it bounds one
fractional column at a time on one side, solves, tries the other side
once if that solve is infeasible or cannot beat the incumbent, and
stops at 50 solves or when the point rounds to a better incumbent.

- coefficient: the column whose rounding side has the fewest locking
  rows, columns that some side leaves unlocked last;
- pseudocost: the side by the distance to the root's relaxation, the
  fractionality, then the cheaper pseudocost, and the column with the
  largest ratio of the other side's cost to this side's, binaries first;
- guided: toward the incumbent, nearest first;
- vector length: the side that raises the objective, the column with
  the least rise per row it sits in plus one, where the model has
  set-covering or partitioning rows of binaries;
- line search: along the line from the root's relaxation, the column
  that reaches an integer first.

Every 10 nodes, while the dives have spent under 5% of the tree's work,
the scheduler of 02-488 picks one rule by its incumbents per run.

## The five alone

| | primal | dual | sum |
|---|---|---|---|
| 37720d32 | 5.5284 | 6.1232 | 11.6517 |
| the five dives | 5.3176 | 6.0477 | 11.3654 |

The gain is not theirs. Over the 30 models they find 7 incumbents
(`dives-who.txt`): the coefficient dive 2 (`gen-ip002`, `murg`), the
pseudocost dive 1 (`gen-ip054`), the guided dive 3 (`gen-ip054`, `murg`,
`pk1`) and the line-search dive 1 (`mas74`); the last incumbent is a
dive's on `mas74` and `murg` among the open models. The vector-length
dive runs on 9 models for none, at about 1e8 work units each and 3.6e8
for two dives on `csched008`. The models whose gaps close most, `timtab1` (0.59 to 0.44),
`beasleyC3` (0.093 to 0.054), `nidda` and `kasai`, get their points from
RINS and the tree on paths the dives' 5% moved.

MIPLIB 3 reads 1.011x geometric and 1.022x total, `dcmulti` 1.33x.

## On top of 02-489

`dives.patch` keeps the four that found a point. Against 02-489's landed
reading:

| | primal | dual | sum |
|---|---|---|---|
| 02-489 | 5.1419 | 6.1310 | 11.2729 |
| with the four dives | 5.4372 | 6.0618 | 11.4990 |

The primal side loses 0.30: `timtab1` 0.339 to 0.598, `supportcase26`
0.028 to 0.047, `kasai` 0.0019 to 0.0108, `csched007` 0.137 to 0.154,
`gen-ip002` 0 to 0.003. `mas74` gains (0.018 to 0.0065, the coefficient
dive's point), `nidda` (0.0088 to 0) and `neos-911970` (0.0037 to
0.0013); the dual side gains 0.07, `glass4`'s 0.33 to 0.25. The dives find
2 incumbents over the 30 (`attempt-who.txt`): on `timtab1` they run 4,762
times each for none, 5e8 work units, and its mutation finds 4 incumbents
in 674 runs against 6 in 704. MIPLIB 3
reads 1.035x and 1.045x against 1.039x and 1.046x.

## 02-394's general-integer models

The five give 32 of the 34 a point at 5e8 against 28, and find none of
those points themselves (no dive incumbent on the 7 models that gain one).
The count moves with the work they take: 32 at a share of 1%, 28 every
20 nodes, 30 for the coefficient dive alone, 31 for the line search
alone.

## The verdict

Refused as `tree-dives`: on top of the sub-MIPs the four take 5% of the
tree for 2 incumbents and the primal side loses 0.30; the vector-length
dive found nothing on the 9 models with covering or partitioning rows.
