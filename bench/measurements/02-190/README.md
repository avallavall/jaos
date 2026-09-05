# 02-190 — The rounding heuristic on the MIP set (D290)

One binary under one switch, 2026-09-05, tree 94d5344 plus the batch's
working copy. Work units are the measure; the node of the first incumbent
is the number that judges a heuristic under a best-bound order, since it
cannot prune there.

## What is here

| file | what it is |
|---|---|
| `sweep-heur.sh` | the two arms over `bench/miplib.manifest`, 12 at once, 120 s cap, and the two comparisons below |
| `sweep-control.txt` | `--no-heuristics`: reproduces `bench/miplib.baseline` (the D289 reading) node for node and unit for unit on all 17. The canary |
| `sweep-heur.txt` | the default: rounding on at every fractional node, the pass billed |
| `heur-against-control.txt` | per instance: the work ratio, the points the rounding found, and the node of the first incumbent off -> on; the geometric mean at the end |

## The reading

| | |
|---|---|
| trees changed | **0 of 17**: every node count identical |
| work | **1.0177x** geometric mean; worst `stein45` 1.058x, `egout` 1.040x, `stein27` 1.038x |
| points found by rounding | on 8 of 17; `stein27` and `stein45` 4 each, `lseu` 3 |
| first incumbent | earlier on **8 of 17, later on 0**: `stein45` 25450 -> 40, `mod008` 1409 -> 2, `lseu` 17479 -> 518, `stein27` 1444 -> 74, `p0033` 857 -> 8, `blend2` 29903 -> 14897, `p0201` 1001 -> 959, `mod010` 4 -> 3 |

## The verdicts

**Why no tree moved, and had to not move.** Under a best-bound order the
optimum is found at a node whose bound is at most the optimum, and every
node with a smaller bound is solved whatever incumbent is held. An
incumbent found earlier prunes only nodes with a bound above its value,
and those are never taken once the optimum is known. So a heuristic
cannot shrink this tree, and the first reading of this sweep, before the
pass was billed, read 1.0000x on all 17 with the same node counts.

**On by default.** 1.8% of the work buys the incumbent a budget stop hands
back, up to three orders of magnitude earlier. `bench/miplib.baseline` is
rewritten with it on.
