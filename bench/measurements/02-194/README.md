# 02-194 — The dive's child rule, on the MIP set (D295)

One binary under one setting, 2026-09-06, tree d5f10a4 plus the batch's
working copy. Work units are the measure; nodes are read beside them. The
script is `02-193/sweep-cap-and-dive.sh`, which produced both directories
from one control.

## What is here

| file | what it is |
|---|---|
| `sweep-control.txt` | every default, dive off: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-dive-nearer.txt` | `--dive --dive-child nearer`: D289's refused form on today's tree |
| `sweep-dive-up.txt`, `sweep-dive-down.txt` | a fixed side first |
| `sweep-dive-pseudocost.txt` | the direction whose expected objective loss is the smaller, the nearer side on a tie |
| `dive-*-against-control.txt` | per instance, the work ratio and the nodes control -> arm; the geometric mean at the end |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| first child | mean | better | worse | past 2x |
|---|---|---|---|---|
| nearer | 1.053x | 7 | 7 | 1 (`enigma` 2.23x) |
| up | 0.999x | 7 | 7 | 1 (`blend2` 2.38x) |
| down | 1.316x | 5 | 9 | 3 (`blend2` 11.0x, 6827 to 77842 nodes) |
| pseudocost | 0.991x | 5 | 7 | 1 (`blend2` 2.53x) |

## The verdicts

**No rule reopens D289.** The bar is 0.95x with no instance past 2x; the
best rule reads 0.991x and every rule has one past 2x. The nearer rule
reads 1.053x here against 1.125x in D289 because the tree under it is
pseudocost-branched with root cuts now; the direction of the verdict is
the same. `blend2` is the instance the three new rules lose on: the
dive's early incumbent prunes nothing there, and the order defers the
nodes that would have closed it.

**The dive stays off**, the rule stays behind `jaos_set_mip_dive_child`
and `--dive-child`, and `bench/miplib.baseline` is untouched. What could
reopen D289 now is a backtracking dive; `bench/refusals.txt` says so.
