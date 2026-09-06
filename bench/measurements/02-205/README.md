# 02-205 — the aggregated MIR cut and the dive heuristic, on the MIP set of 24

What decided D312 and D313. Two features of the day's fourth batch, each
swept as its own arm against a control that is every default of D311, on
the 24 MIPLIB 3 instances of `bench/miplib.manifest` (D302). A 240 s cap,
12 instances at once, `bench/measurements/02-205/sweep-b4.sh` the script
that wrote every file here.

## The control

`sweep-control.txt` is the D311 defaults. `control-against-baseline.txt`
compares it against `bench/miplib.baseline` node for node and unit for
unit: 24 same, 0 different. That is what says the arms below are the
features and not the build, and it is also what says D312's refactor of
D309's single-row round into a dense side moved nothing.

Every file is `name rc status obj nodes cuts heur first work secs`. The
seconds are here because a sweep is not a baseline; nothing in this
directory enters `bench/results/` or a baseline.

## The arms

| file | flags |
|---|---|
| `sweep-ag1.txt` | `--mir-aggregate 1` |
| `sweep-ag2.txt` | `--mir-aggregate 2` |
| `sweep-ag3.txt` | `--mir-aggregate 3` |
| `sweep-ag6.txt` | `--mir-aggregate 6` |
| `sweep-dh10.txt` | `--dive-heuristic 10` |
| `sweep-dh50.txt` | `--dive-heuristic 50` |
| `sweep-dh200.txt` | `--dive-heuristic 200` |
| `sweep-ag2dh50.txt` | both |

Each `<tag>-against-control.txt` holds the per-instance ratio, the node
counts on both sides, and two summary lines: the geometric mean over
every instance both arms finish, then the same split into the 17 the cut
defaults were tuned on and the seven that joined at D302.

## What they say

**The aggregated c-MIR is refused (D312).** 1.185x the work at one
substitution step, 1.165x at two, 1.140x at three, 1.192x at six. Every
arm leaves `bell5` at the 240 s cap, which the control finishes in 13.9 s,
and every arm has `gen` past 2x (8.60x at two steps, 9.84x at three)
against a tree that went from 7 nodes to 5. The split says where the cost
is: 1.030x over the 17 at three steps against 1.523x over the seven. Some
instances do gain, `egout` reading 0.564x with its tree halved, so the
aggregation finds cuts; the rounds cost more than the cuts save.

**The dive heuristic is accepted at 50 solves (D313).** 1.032x the work
over all 24, none past 2x, all 24 finish, and no node count moves — a
heuristic cannot shrink a best-bound tree, which is D290's own finding.
It is judged on the first incumbent, by D290's rule, and that moves
earlier on 6 of the 24 and later on none: `gen` node 7 to 1, `khb05250`
69 to 1, `misc06` 43 to 1, `rgn` 205 to 1, `stein27` 84 to 1, `stein45`
20 to 1. At 10 solves the work is 1.015x and one instance moves; at 200
it is 1.036x and seven move, `mod008` joining. 50 is the setting whose
work stays near the 10-solve arm while it reaches six of the seven
instances the 200-solve arm reaches.

**Together they are worse than either (`ag2dh50`).** 1.209x, 12
instances worse, `bell5` still at the cap. Nothing there argues for the
aggregation.

`bench/miplib.baseline` was rewritten to the dive heuristic's trees after
this reading, and reproduces `sweep-dh50.txt` on all 24.

`retest-mir-aggregate.sh` re-asks D312's question for `make refusals`.
