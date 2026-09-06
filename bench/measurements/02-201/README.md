# 02-201 — Tonight's two cut defaults re-read on the 24, the seven new instances apart (D303)

2026-09-06, tree 05d6ce0, no source change. The MIP set of 24 (D302) under
every default, then with the covers off, the node cuts off, and both off;
12 at once, 240 s cap. Work units are the measure.

## What is here

| file | what it is |
|---|---|
| `sweep-reread.sh` | the control and the three arms over `bench/miplib.manifest`, and the comparisons |
| `sweep-control.txt` | every default: reproduces `bench/miplib.baseline` (the D302 reading) node for node and unit for unit on all 24 (`control-against-baseline.txt`). The canary |
| `sweep-c0.txt` | `--cover-rounds 0`: the D301 tree without D300's covers |
| `sweep-d0.txt` | `--cut-depth 0`: the D300 tree without D301's node cuts |
| `sweep-c0d0.txt` | both off: the D292 tree with pseudocost branching and one Gomory round |
| `*-against-control.txt` | per instance, the work ratio arm / control, the nodes and the cuts |
| `split-17-and-7.txt` | each arm's geometric mean over all 24, over the 17 the cuts were tuned on, and over the seven that joined at D302 |

## The reading

Each arm against the control, work in geometric mean of per-instance
ratios (D46: never a sum); a value above 1 means the default is the
cheaper of the two:

| arm | all 24 | the 17 | the 7 new |
|---|---|---|---|
| covers off | **1.125x** (3 better, 6 worse, 2 past 2x) | 1.273x | **0.818x** (`p0282` 0.117x, `l152lav` 1.93x) |
| node cuts off | **1.059x** (11 better, 10 worse, 2 past 2x) | 1.190x | **0.784x** (`bell5` 0.081x, `bell3a` 0.442x, `gt2` 5.28x) |
| both off | **1.231x** (5 better, 14 worse, 4 past 2x) | 1.585x | **0.642x** |

The tails: with the node cuts off `egout` pays 8.07x and `gt2` 5.28x,
`bell5` pays 0.081x, so the node cuts cost `bell5` twelve times its work;
with the covers off `mod010` pays 16.5x and `p0282` 0.117x, so the covers
cost `p0282` eight and a half times.

## The verdicts

**The defaults hold over the set and do not hold over its new part.**
By the rule every default here was set by, the geometric mean over the
set with no instance past 2x, both defaults stand: turning either off
reads worse over the 24. Over the seven instances the cuts were not tuned
on, both read the other way, and the mean of the seven with both off is
0.642x. Seven is too few to move a default on and the record does not,
but it is enough to say what the defaults rest on: the 17, and on
`egout` and `mod010` most of all.

**What it points at.** The tails are not noise, they are the same
mechanism both ways: a cut round that closes a gap saves a tree (`egout`,
`gt2`, `mod010`) and one that closes nothing costs its rows and its
re-solve on every node under it (`bell5`, `p0282`). A cut round that stops
when it stops moving the bound is what would keep the wins and drop the
losses, and it is the next thing to measure on this set.
