# 02-203 — A backtracking dive, and MIR cuts on the model's rows (D308, D309)

2026-09-06, tree 5e9b3df plus the batch's working copy, the review's fixes
in. The MIP set of 24 (D302) under the D306 defaults, then the dive with
each backtrack budget and the MIR cuts at each round count; 12 at once,
240 s cap. Work units are the measure; nodes and cuts are read beside
them, and the seven instances that joined at D302 are read apart from the
17 the cuts were tuned on.

## What is here

| file | what it is |
|---|---|
| `sweep-b2.sh` | the control, the dive arms and the MIR arms at 1, 2 and 4 rounds, and the comparisons |
| `sweep-b2b.sh` | the MIR arms at 3, 5, 6, 8 and 12 rounds, on the same tree; its control agreed with the first but for the seconds and is not kept twice |
| `sweep-control.txt` | every default after D306: reproduces `bench/miplib.baseline` (the D306 reading) node for node and unit for unit on all 24 (`control-against-baseline.txt`). The canary |
| `sweep-bt<N>.txt` | `--dive --dive-backtrack N`: the dive resumes from the deepest waiting sibling up to N times per dive (D308); `bt0` is D289's dive on this tree |
| `sweep-pbt<N>.txt` | the same with `--dive-child pseudocost`, D295's best child rule |
| `sweep-mir<N>.txt` | `--mir-rounds N`: N rounds of MIR cuts on the model's rows at the root, beside the other families (D309) |
| `*-against-control.txt` | per instance, the work ratio arm / control, the nodes and the cuts; the geometric mean over the 24, then over the 17 and the 7 |

## The reading

Work against the control, geometric mean of per-instance ratios (D46:
never a sum); "23" means `bell5` stopped at the cap with no incumbent
and is not in the mean:

| arm | all 24 | better / worse / past 2x | the 17 | the 7 new |
|---|---|---|---|---|
| dive, no backtrack | 0.835x over 23 | 11 / 8 / 2 | 1.024x | 0.467x over 6 |
| dive, 1 resume | 1.026x over 23 | 11 / 9 / 3 | 1.122x | 0.795x over 6 |
| dive, 2 | 1.134x over 23 | 8 / 9 / 4 | 1.252x | 0.856x over 6 |
| dive, 4 | 1.007x over 23 | 9 / 10 / 2 | 1.145x | 0.700x over 6 |
| dive, 16 | 0.992x | 12 / 9 / 2 | 1.242x | 0.575x |
| dive, unbounded | 1.170x over 23 | 9 / 9 / 5 | 1.408x | 0.692x over 6 |
| pseudocost side, 4 | 1.011x over 23 | 9 / 11 / 2 | 1.197x | 0.627x over 6 |
| pseudocost side, unbounded | 1.142x over 23 | 9 / 9 / 4 | 1.323x | 0.752x over 6 |
| MIR 1 round | 1.017x | 4 / 3 / 0 | 1.023x | 1.004x |
| MIR 2 | 0.933x | 6 / 2 / 0 | 0.967x | 0.856x |
| MIR 3 | 0.790x | 6 / 2 / 0 | 0.891x | 0.591x |
| MIR 4 | 0.752x | 7 / 0 / 0 | 0.837x | 0.580x |
| MIR 5 | 0.773x | 7 / 1 / 0 | 0.879x | 0.567x |
| **MIR 6** | **0.719x** | **6 / 1 / 0** | 0.847x | 0.482x |
| MIR 8 | 0.725x | 6 / 2 / 0 | 0.828x | 0.526x |
| MIR 12 | 0.737x | 6 / 2 / 0 | 0.842x | 0.532x |

The tails. The dive at sixteen resumes: `p0282` 0.155x and `bell5`
0.540x against `enigma` 5.24x and `lseu` 2.19x; unbounded, `enigma`
9.28x and `gt2` 8.18x. Six MIR rounds: `gen` 0.025x (589 to 7 nodes),
`mod008` 0.198x, `gt2` 0.246x, `p0033` 0.374x, `p0201` 0.683x; `lseu`
1.26x the one worse; twelve of the 24 are touched and the rest are
byte-identical to the control.

## The verdicts

**MIR cuts: accepted, six rounds by default, the baseline rewritten
(D309).** The best mean that keeps every instance under 2x, and the curve
is flat past it (0.725x at eight, 0.737x at twelve).

**The backtracking dive: refused (D308).** No budget reads under the bar
on the 24, and every one reads over 1.0x on the 17 and under 0.80x on
the seven. D289's refusal holds; its reopen condition named the child
rules (D295) and the backtracking, and both are measured now.
