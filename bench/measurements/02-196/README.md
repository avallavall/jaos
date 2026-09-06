# 02-196 — A slack local cut leaves the relaxation, and no row churn (D297)

One binary under one setting, 2026-09-06, tree 733d06c plus the batch's
working copy. Work units are the measure; nodes and cuts are read beside
them. The script also produced `02-197/` from the same control.

## What is here

| file | what it is |
|---|---|
| `sweep-drop-and-probe-depth.sh` | the control, the churn canary, three depth arms with the drop, and the probe-depth arms of `02-197/`; 12 at once, 120 s cap, and the comparisons |
| `sweep-control.txt` | every default: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-d1nd.txt` | `--cut-depth 1 --no-cut-drop`: D296's depth-1 arm with only the churn skipped. `d1nd-against-d1.txt` compares it with `02-195/sweep-d1.txt`: every tree the same and **1.000x** in work, so the skip moves no answer and no unit |
| `sweep-d1.txt`, `sweep-d2.txt`, `sweep-d4.txt` | `--cut-depth D` with the drop on: a local cut whose slack is basic at a node is not carried under it |
| `d*-against-control.txt` | per instance, the work ratio against the control, the nodes and the cuts; the geometric mean at the end |
| `d*-against-d*.txt` | the same arm against `02-195/`'s arm at the same depth without the drop: what the drop alone is worth |

## The reading

Work in geometric mean of per-instance ratios (D46: never a sum):

| depth | against the control | past 2x | against D296's arm, no drop |
|---|---|---|---|
| 1 | **0.896x** (7 better, 7 worse) | 3 (`misc03` 2.06x, `flugpl` 2.01x, `p0201` 2.01x) | 0.849x (10 better, 3 worse) |
| 2 | **0.802x** (7 better, 6 worse) | 1 (`misc03` 2.053x) | 0.637x (12 better, 1 worse) |
| 4 | 0.833x over 16 (5 better, 7 worse) | 1 (`misc03` 2.68x); `misc06` ends in a numerical error | 0.525x (12 better, 0 worse) |

`egout` reads 0.065x at depth 2 (39127 to 2291 nodes), `lseu` 0.573x,
`enigma` 0.593x, `mod010` 0.434x, `p0033` 0.511x; `dcmulti` 1.71x and
`flugpl` 1.45x the other way.

## The verdicts

**The drop is the default behaviour of a local cut**, because at every
depth it reads better than carrying the cut: 0.849x, 0.637x and 0.525x
against D296's arms. `jaos_set_mip_cut_drop` and `--no-cut-drop` keep
D296's form for comparison.

**The cut depth stays 0.** Depth 2 with the drop is 0.802x and misses the
bar by one instance, `misc03` at 2.053x against the gate's factor of 2;
depth 1 has three past it and depth 4 breaks `misc06` numerically. What
would take depth 2 over the bar is `misc03` under 2x: a cap on the cuts a
node may add, or a floor on the violation a cut must have. That is the
reopen condition `bench/refusals.txt` now carries for D296.

**The churn skip is a correction to D296's text, not a saving in units.**
D296 said the work counts the rows added and removed between nodes. It
does not: `jaos_add_rows` and `jaos_delete_rows` bill nothing, and the
canary reads 1.000x with identical trees. The skip stays, since the same
rows in the same order are the same relaxation, but its worth is in
seconds and is not measured here (D45: seconds never enter the record).
