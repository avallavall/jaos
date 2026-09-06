# 02-202 — Four cut switches on the 24: the root stall, the node stall, root cuts that leave, and lifted covers (D304 to D307)

2026-09-06, tree b5a792a plus the batch's working copy, the review's
fixes in (the cover margin, which moves no tree here). The MIP set of 24
(D302) under every default, then each switch as an arm, then the
combinations; 12 at once, 240 s cap. Work units are the measure; nodes
and cuts are read beside them, and the seven instances that joined at
D302 are read apart from the 17 the cuts were tuned on.

## What is here

| file | what it is |
|---|---|
| `sweep-b1b.sh` | the control and the fifteen arms over `bench/miplib.manifest`, and the comparisons; the first run of the same arms before the review's fixes read the same numbers and is not kept |
| `sweep-control.txt` | every default before this batch: reproduces `bench/miplib.baseline` (the D302 reading) node for node and unit for unit on all 24 (`control-against-baseline.txt`). The canary |
| `sweep-s<F>.txt` | `--cut-stall F`: the root's rounds end after one that moved the bound by less than F of (1 + \|bound\|) (D304) |
| `sweep-n<F>.txt` | `--node-cut-stall F`: no round under a node whose round moved its bound by less than F of (1 + \|bound\|) (D305) |
| `sweep-rdrop.txt` | `--root-cut-drop`: a root cut leaves below a node where its slack is basic (D306) |
| `sweep-rn<F>.txt` | the drop and the node stall together |
| `sweep-lift.txt`, `sweep-rlift.txt` | `--cover-lift`, alone and with the drop (D307) |
| `*-against-control.txt` | per instance, the work ratio arm / control, the nodes and the cuts; the geometric mean over the 24, then over the 17 and the 7 |
| `retest-cut-stall.sh`, `retest-node-cut-stall.sh`, `retest-cover-lift.sh` | the three refusals' re-tests, for `make refusals` |

## The reading

Work against the control, geometric mean of per-instance ratios (D46:
never a sum); "23" means `bell5` stopped at the cap with no incumbent
and is not in the mean:

| arm | all 24 | better / worse / past 2x | the 17 | the 7 new |
|---|---|---|---|---|
| root stall 1e-4 | 1.007x | 2 / 3 / 0 | 0.985x | 1.065x |
| root stall 1e-3 | 1.172x | 2 / 5 / 1 | 1.219x | 1.065x |
| root stall 1e-2 | 1.121x | 2 / 5 / 1 | 1.139x | 1.079x |
| node stall 1e-3 | 0.993x over 23 | 5 / 4 / 0 | 1.051x | 0.845x over 6 |
| node stall 1e-2 | 0.833x | 10 / 5 / 0 | 0.988x | 0.550x |
| node stall 2e-2 | **0.816x** | 12 / 4 / 0 | 0.960x | 0.550x |
| node stall 5e-2 | 0.832x | 12 / 4 / 0 | 0.987x | 0.550x |
| node stall 1e-1 | 0.897x | 12 / 7 / 1 | 1.061x | 0.596x |
| **root cuts leave** | **0.799x** | **13 / 2 / 0** | 0.881x | 0.630x |
| leave + stall 1e-2 | 0.832x over 23 | 11 / 7 / 1 | 0.997x | 0.499x over 6 |
| leave + stall 2e-2 | 0.782x over 23 | 14 / 5 / 1 | 0.973x | 0.421x over 6 |
| leave + stall 5e-2 | 0.773x over 23 | 14 / 5 / 1 | 0.958x | 0.421x over 6 |
| leave + stall 1e-1 | 0.768x over 23 | 14 / 6 / 1 | 0.981x | 0.384x over 6 |
| lifted covers | 1.005x | 1 / 2 / 1 | 0.965x | 1.109x |
| leave + lifted | 0.801x | 13 / 2 / 0 | 0.884x | 0.630x |

The tails. Root cuts that leave: `p0282` 0.276x, `bell3a` 0.357x, `lseu`
0.518x, `misc07` 0.593x; `gt2` 1.58x and `egout` 1.39x. The node stall
at 2e-2: `bell5` 0.081x, `p0282` 0.38x, `bell3a` 0.44x; `enigma` 1.88x.
Every combination of the two leaves `bell5` at the cap after 1.2 million
nodes with no incumbent and `enigma` past 2x. The root stall at 1e-4:
`dcmulti` 1.62x and `l152lav` 1.48x against `p0033` 0.52x. The lift:
`l152lav` 2.06x against `p0033` 0.52x.

## The verdicts

**Root cuts leave: accepted, on by default, the baseline rewritten
(D306).** The best mean that keeps every instance under 2x and finishes
all 24.

**The node stall: meets the bar alone at 2e-2 and is refused as a
default beside the drop (D305).** The two do not combine under the bar
on this set, and the drop reads better and finishes everything.
`retest-node-cut-stall.sh` asks the combination.

**The root stall: refused (D304).** Worse than no stall at every
fraction; every round the stall removed was worth its solve.

**Lifted covers: refused (D307).** 1.005x with `l152lav` past 2x; the
lift strengthens a cover only where an item outweighs the cover's two
heaviest together.
