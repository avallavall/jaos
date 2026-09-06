# 02-199 — A cap on a node's cuts, and cuts below the root land (D301)

One binary under one setting, 2026-09-06, tree 5d3eba1 plus the batch's
working copy, on the D300 baseline (four cover rounds at the root). Work
units are the measure; nodes and cuts are read beside them.

## What is here

| file | what it is |
|---|---|
| `sweep-cap.sh` | the control, depth 2 uncapped, depth 2 with caps 2, 4, 8 and 16, depth 1 and depth 3 with a cap of 4; 12 at once, 120 s cap, and the comparisons |
| `sweep-cap-more.sh` | the second half: depths 4, 6, 8 and every node at a cap of 4, and caps 3 and 6 at depth 3 |
| `sweep-control.txt` | every default before this decision: reproduces `bench/miplib.baseline` (the D300 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-d2.txt` | `--cut-depth 2`, no cap: D297's best arm on this tree |
| `sweep-d<D>k<K>.txt` | `--cut-depth D --node-cut-cap K`: the K most efficacious cuts of a node's round, violation over the cut's norm |
| `*-against-control.txt`, `d2k*-against-d2.txt` | per instance, the work ratio, the nodes and the cuts; the geometric mean at the end |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| depth | cap | mean | better | worse | past 2x |
|---|---|---|---|---|---|
| 2 | none | 0.935x | 5 | 8 | 1 (`misc03` 2.05x) |
| 2 | 2 | 1.088x | 4 | 8 | 2 |
| 2 | 4 | 0.920x | 5 | 6 | 0 |
| 2 | 8 | 1.004x | 5 | 8 | 2 |
| 2 | 16 | 0.948x | 4 | 6 | 1 |
| 1 | 4 | 0.955x | 8 | 5 | 0 |
| **3** | **4** | **0.835x** | 7 | 6 | 0 (worst `misc03` 1.58x, `dcmulti` 1.45x) |
| 3 | 3 | 0.836x | 8 | 5 | 0 (worst `dcmulti` 1.95x) |
| 3 | 6 | 0.994x | 6 | 7 | 2 |
| 4 | 4 | 0.854x | 6 | 7 | 1 |
| 6 | 4 | 0.852x | 5 | 7 | 2 |
| 8 | 4 | 0.827x | 5 | 8 | 1 |
| every node | 4 | 0.877x | 5 | 7 | 3 |

At depth 3 with a cap of 4: `egout` 0.124x (39127 to 4271 nodes), `lseu`
0.526x, `enigma` 0.533x, `khb05250` 0.680x, `misc06` 0.732x, `flugpl`
0.832x, `blend2` 0.881x; `misc03` 1.58x, `dcmulti` 1.45x, `mod008` 1.20x,
`p0201` 1.14x, `rgn` 1.09x, `stein27` 1.09x the other way. **Without
`egout` the mean is 0.934x**, still under the bar; with cap 3 it is 0.938x.

## The verdicts

**Accepted: depth 3 with four cuts per node is the default.** It is the
setting with the best mean that keeps every instance under 2x, the rule
D289 and D300 used. Cap 3 at the same depth is within a tenth of a percent
in the mean and has the worse tail, `dcmulti` at 1.95x. The two defaults
move together and the baseline is rewritten.

**The surface is not smooth, and the record says so.** Cap 6 at depth 3
reads 0.994x with two instances past 2x against 0.835x at cap 4; depth 2
reads 0.920x at cap 4 and 1.088x at cap 2. A cut round changes the tree
under it and the trees change shape rather than size. The pair is worth
re-reading on a larger set, which `SPECS.md` lists as not started.

**What this closes.** D296's refusal of cuts below the root expires: a cap
on a node's cuts was its reopen condition after D297, and it is met.
`02-195/retest-cut-depth.sh` compared depth 1 against the default and is
retired with the refusal.
