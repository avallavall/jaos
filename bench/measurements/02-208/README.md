# 02-208 — the feasibility pump, and what every accepted default since D288 is worth together

What decided D318, on the 24 MIPLIB 3 instances of `bench/miplib.manifest`
(D302). A 240 s cap, 12 instances at once. `sweep-b6.sh` wrote the control
and the first five arms, `sweep-b6b.sh` the two that found the knee.

## The control

`sweep-control.txt` is every default of D316, and
`control-against-baseline.txt` reads 0 DIFFERENT against the
`bench/miplib.baseline` that batch 5 committed: the pump is inert when it
is off, which is what says the arms below are the feature.

Every file is `name rc status obj nodes cuts heur first work secs`. The
seconds are here because a sweep is not a baseline; nothing in this
directory enters `bench/results/` or a baseline.

## The pump, and the guard the first sweep bought

| rounds | work over the 24 | first incumbent moved to node 1 |
|---|---|---|
| 1 | 1.006x | none |
| 3 | 1.011x | 2 |
| 5 | 1.014x | 4 |
| **20** | **1.026x** | **6** |
| 50 | 1.037x | 6 |
| 100 | 1.053x | 6 |

No node count moves at any setting, which is what a heuristic on a
best-bound tree can do (D290), so the reading that decides it is the first
incumbent: at 20 rounds `egout` goes from node 5203 to 1, `l152lav` 295 to
1, `p0033` 99 to 1, `lseu` 47 to 1, `mod008` 29 to 1, `p0282` 28 to 1, and
none moves later. Every instance finishes at every setting and none passes
2x. 50 and 100 rounds reach the same six for more work, so 20 is the knee
and the default.

**`before-the-guard/` holds the first reading, and it is why the guard
exists.** The pump ran at the root whether or not anything had an answer
yet, and read 1.035x, 1.041x, 1.043x and 1.056x at 1, 3, 5 and 20 rounds
against 1.006x, 1.011x, 1.014x and 1.026x with the guard. The cost was
where the pump could not help: `gen` paid 1.855x on a seven-node search
because the dive heuristic had already put an incumbent at node 1 there,
and 8 of the 24 have one at node 1 before the pump runs. This is the plain
pump of the 2005 paper, which looks for a feasible point and not a good
one, so it now runs only while nothing has an answer. All six gains
survive the guard: every instance it helps has no root incumbent without
it.

## The plain tree, and what six years of defaults are worth

`sweep-plain.txt` is the branch and bound of D288 with everything the
record has added since switched off: `--cut-rounds 0 --cover-rounds 0
--mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 0
--branching most-fractional`. `plain-against-control.txt` compares it
against today's defaults.

**6.021x**, geometric mean over the 20 instances both finish, 17 worse and
11 of them past 2x. Four instances the plain tree does not finish inside
the 240 s cap at all -- `bell5`, `gt2`, `l152lav`, `p0282` -- and today's
tree finishes all 24, so the mean understates it. Split by the two groups
of D303: 4.288x over the 17 the defaults were tuned on, and **41.237x**
over the three of the seven newer instances the plain tree can finish.

The largest: `gen` 10461x, its tree 86589 nodes against 7; `mod010` 697x,
152 nodes against 1; `khb05250` 18.7x; `p0033` 12.0x. Two instances are
better without any of it: `rgn` 0.874x and `stein27` 0.944x.

This number had never been measured. The accepted defaults each read
between 0.660x and 0.835x against the tree of their own day (D289, D292,
D300, D301, D306, D309), and multiplying those gives 0.17x, which is 5.9x
the other way round. The measurement says 6.021x, so the arithmetic was
close, and it is a measurement now.
