# 02-457 — p0033's 13 nodes became 21 in e368c84, and the MIPLIB 3 baseline is read again

Taken on 2026-11-15 on the tree of 1183f03. `bisect.sh` finds the first
commit after the baseline at which an instance's nodes, iterations or work
units move (`bisect-*.txt`); `ties.sh` reads where the start rule of
e368c84 acts (`ties.txt`).

## What the record said

`bench/miplib.baseline`, last written on 2026-10-08 at 5c51216, has
`p0033` at 13 nodes, 171 iterations and 1,365,132 work units. Every
`make miplib` since e368c84 has read 21 nodes, 227 iterations and
2,571,379, 1.88 times the work, and `gen` 568 iterations against 541.
The gate counts a work regression past 2.0x (`WORK_REGRESSION_FACTOR`)
and listed the node change on its own line, so its summary read
`0 regressed`. 02-442, the change's own measurement, wrote both numbers
in its table and said nothing of why.

## The bisection

On the 127 commits from 5c51216 to 1183f03, each built from `git
archive`:

| instance | moves at | what moves |
|---|---|---|
| `p0033` | e368c84 | 13 nodes, 171 iterations to 21, 227 |
| `gen` | e368c84 | 541 iterations to 568 |
| `misc03` | 00e975c | 6,550 iterations to 6,640, the same 248 nodes |
| `p0201` | 00e975c | 8,614 iterations to 8,613, the same 351 nodes |

00e975c's own reading (02-455) says MIPLIB 3 keeps its nodes there and
moves in work. No other line of the baseline moves in nodes or
iterations, and every other work count is within 3.1% of the baseline.

## Why p0033 moves

e368c84 places a zero-cost column boxed in [-a, a] at the bound its rows
favour, where the slack basis had put it at -a. No column of a MIPLIB
node LP is boxed that way. The rule acts in the LPs of the hull
separator: for a short row over integer columns, `mip.c` lists the row's
integer points and solves, over a cut's coefficients π boxed in [-1, 1]
and its right-hand side, max π·x* - π0 subject to π·p <= π0 at every
listed point p. A coefficient whose column sits at 0 in the LP point x*
costs 0, so each such LP starts with ties (`ties.txt`: 52 placements on
`p0033`, every one at +1). That LP has many optima, cuts that are equally
violated, and the other start lands on another of them. The tree differs
from that cut on.

`ties.sh` skips the rule on HEAD (`JAOS_NOTIES=1`):

| instance | with the rule | without it |
|---|---|---|
| `p0033` | 21 nodes, 227 iterations, 2,571,379 | 13 nodes, 171 iterations, 1,364,573 |
| `gen` | 568 iterations, 4,213,737 | 541 iterations, 3,906,211 |
| `misc03` | 6,640 iterations, 42,810,322 | 6,640 iterations, 43,118,425 |

With the rule, MIPLIB 3's total work fell (02-442: `misc03` and
`misc07` down, `p0033` and `gen` up). The rule picks one cut of equal
violation over another; neither is a defect, so the code stays.

## What changes

`make miplib-baseline` rewrites `bench/miplib.baseline` on 1183f03, after
reading the diff above: every line's work, the iterations of `blend2`
(02-455), `gen`, `misc03`, `p0201` and `p0033`, and `p0033`'s nodes.
`miplib-5c51216d.baseline` is the file it replaces.

`bench/run.c`'s summary line now counts the instances at other nodes
(`baseline: 0 regressed, 0 improved, 0 new, 1 at other nodes`), so a node
change shows in the line every gate reading quotes. `make miplib` before
the rewrite (`miplib-before.txt`) and after it (`miplib-after.txt`).

The other baselines, read the same way: `netlib-infeas`'s `refinery`
takes 1.17x its work, from e368c84, which 02-442 lists, and
`maros-meszaros`'s `powell20` 1.26x, from 44e294d, which 02-367
explains. Nothing else moves past 1.10x.
