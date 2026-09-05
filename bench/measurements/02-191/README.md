# 02-191 — Pseudocost branching on the MIP set (D292)

One binary under one switch, 2026-09-05, tree 94a8a4c plus the batch's
working copy. Work units are the measure; nodes are read beside them.

## What is here

| file | what it is |
|---|---|
| `sweep-branch.sh` | the two arms over `bench/miplib.manifest`, 12 at once, 120 s cap, and the two comparisons |
| `sweep-control.txt` | `--branching most-fractional`: reproduces `bench/miplib.baseline` (the D290 reading) node for node and unit for unit on all 17. The canary |
| `sweep-pc-notie.txt`, `pc-notie-against-control.txt` | the first form of the rule: the product score with the lowest index on a tie |
| `sweep-pc.txt`, `pc-against-control.txt` | the shipped form: the same score, the fraction breaking a tie, then the lowest index |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| arm | mean | better | worse | past 2x | notes |
|---|---|---|---|---|---|
| lowest index on a tie | 0.787x | 9 | 2 | 1 | `enigma` **2.90x**, 3239 to 9520 nodes; `p0033` 1.59x |
| fraction on a tie (shipped) | **0.722x** | 10 | 2 | 0 | `enigma` 0.958x; `p0033` 1.59x, `rgn` 1.08x |

The largest moves under the shipped form: `blend2` 0.217x (32325 to
6827 nodes), `flugpl` 0.303x (12949 to 4495), `misc03` 0.491x, `dcmulti`
0.521x, `p0201` 0.521x, `khb05250` 0.629x, `misc06` 0.681x, `stein45`
0.902x (145699 to 107329).

## The verdicts

**Pseudocost, default.** 0.722x with no instance past the gate's own
regression factor.

**Why `enigma` needed the tie-break.** Its objective is zero, so no child
ever shows a gain over its parent, every pseudocost stays at zero, every
score is the floor squared, and the first form took the lowest fractional
index every time: 2.90x. Breaking the tie by the fraction is the old rule
where the new one has nothing to say, and it reads 0.958x there. A tie
is not rare -- binaries at one half tie exactly -- so the tie-break moves
9 of the 17 trees: 6 better (`enigma` 2.90x to 0.958x, `p0201` 0.745x to
0.521x, `stein27` 1.022x to 0.935x, `lseu` 0.831x to 0.805x, `misc03`,
`dcmulti` within 1%) and 3 worse (`stein45` 0.829x to 0.902x, `rgn`
1.046x to 1.077x, `dcmulti` 0.520x to 0.521x); the other 8 are
byte-identical. The mean goes from 0.787x to 0.722x and the worst
instance from 2.90x to 1.59x, which is what decides it.

**The baseline** is rewritten to the shipped form's trees.
