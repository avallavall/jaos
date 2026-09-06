# 02-198 — Knapsack cover cuts at the root, on the MIP set (D300)

One binary under one setting, 2026-09-06, tree a47016d plus the batch's
working copy. Work units are the measure; nodes and cuts are read beside
them.

## What is here

| file | what it is |
|---|---|
| `sweep-cover.sh` | the control, one to three cover rounds beside the default Gomory round, the plain tree, and covers alone; 12 at once, 120 s cap, and the comparisons |
| `sweep-cover-more.sh` | the second half: four, five and eight cover rounds, and two Gomory rounds with three covers |
| `sweep-control.txt` | every default before this decision, one Gomory round and no covers: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-g1c1.txt` .. `sweep-g1c8.txt` | `--cover-rounds N` for N = 1, 2, 3, 4, 5, 8 with the Gomory round at its default of 1 |
| `sweep-g0.txt` | `--cut-rounds 0`: the plain tree, D289's control on today's branching |
| `sweep-g0c1.txt`, `sweep-g0c2.txt` | covers alone, one and two rounds, with the Gomory round off |
| `sweep-g2c3.txt` | `--cut-rounds 2 --cover-rounds 3` |
| `*-against-control.txt`, `g0c*-against-g0.txt` | per instance, the work ratio, the nodes and the cuts; the geometric mean at the end |

## The reading

Work in geometric mean of per-instance ratios over the 17 (D46: never a
sum):

| arm | mean | better | worse | past 2x |
|---|---|---|---|---|
| Gomory 1, covers 1 | 1.001x | 3 | 3 | 0 |
| Gomory 1, covers 2 | 0.961x | 5 | 2 | 0 |
| Gomory 1, covers 3 | 0.749x | 7 | 1 | 0 |
| **Gomory 1, covers 4** | **0.745x** | 7 | 1 | 0 |
| Gomory 1, covers 5 | 0.767x | 5 | 3 | 0 |
| Gomory 1, covers 8 | 0.804x | 4 | 4 | 0 |
| Gomory 2, covers 3 | 0.778x | 6 | 6 | 2 (`misc03` 4.55x, `p0201` 2.00x) |
| no Gomory, no covers (the plain tree) | 1.307x | 7 | 6 | 4 |
| covers 1 alone, against the plain tree | 1.052x | 3 | 3 | 0 |
| covers 2 alone, against the plain tree | 0.912x | 5 | 1 | 0 |

At four rounds: `mod010` 7 nodes to 1 for 0.026x (the covers close the
root), `mod008` 0.659x (25743 to 11675 nodes), `dcmulti` 0.674x, `p0033`
0.741x, `lseu` 0.744x, `p0201` 0.840x, `rgn` 0.876x; `enigma` 1.412x the
one worse, 2888 to 4114 nodes; eight instances have no all-binary row with
a violated cover and are unchanged.

## The verdicts

**Accepted, four rounds by default.** Four is the setting with the best
mean that keeps every instance under 2x, the rule D289 used for the
Gomory round; three is within half a percent of it and five is worse. The
two families want each other: covers alone read 1.052x and 0.912x against
the plain tree, and beside the Gomory round 0.745x. Two Gomory rounds
bring back D289's tails whatever the covers do.

**What it costs to say "no cuts".** `--cut-rounds 0` turns the Gomory
round off and nothing else; the tests that want a branching tree now say
`--cut-rounds 0 --cover-rounds 0`.

**`bench/miplib.baseline` is rewritten** to the four-round trees by
`make miplib-baseline`; the three gate sets are byte-identical.
