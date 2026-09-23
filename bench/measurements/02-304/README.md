# 02-304 — a node keeps its parent's basis through presolve

Taken on 2026-09-23 for TODO row H2, on the tree of 16be1ea.

A node LP of the MIP tree starts from its parent's basis. The column the
parent branched on was basic there, fractional, and the child fixes it at
a bound, so presolve removed it as a fixed column. The mapped basis then
arrived one member short and `build_warm_basis` promoted a logical in its
place, which throws away the dual feasibility the warm start was for. In
the first 300 nodes of `l152lav` 328 of 353 warm node LPs arrived short
and 21 more started from the slack basis; `misc06` had 81 of 95 short and
14 slack starts (node LPs logged with the tree's log callback left on).

Since this change presolve keeps a fixed column in the reduced model when
the model is a node solve and its starting basis holds that column basic.
The dual simplex starts from the parent's basis whole and pivots the
column out. The same count reads 113 short and 13 slack starts on
`l152lav`, none short and one slack start on `misc06`.

`make miplib J=2` on this tree against the results of 16be1ea
(`../02-303/cmpmip.py`): 0.631x in work over the 24 instances, none past
2x, every objective within the gap.

| instance | work | nodes |
|---|---|---|
| bell5 | 0.038x | 327119 to 14767 |
| misc06 | 0.073x | 70 to 90 |
| misc03 | 0.221x | 1097 to 207 |
| rgn | 0.243x | 247 to 221 |
| l152lav | 0.444x | 1878 to 1742 |
| enigma | 0.505x | 5221 to 3401 |
| mod008 | 0.535x | 2777 to 2835 |
| bell3a | 0.708x | 85367 to 65901 |
| blend2 | 0.720x | 6763 to 5606 |
| p0282, p0033, gt2, p0201, khb05250, lseu | 0.78x to 0.88x | |
| gen, air03, mod010, flugpl | 0.99x to 1.02x | |
| stein27 | 1.082x | 2853 to 3143 |
| egout | 1.179x | 497 to 589 |
| misc07 | 1.457x | 4712 to 8793 |
| stein45 | 1.507x | 100986 to 105320 |
| dcmulti | 1.854x | 441 to 481 |

The 2017 set (`make miplib2017 J=2`, 1e10 work units, scored by
`../02-298/gapsum.py` against `../02-303/miplib2017-base.txt`) reads a
gap sum of 0.948x, with 17 incumbents on both sides and 8 at the
reference instead of 4: `markshare_4_0`, `neos5`, `gen-ip054` and `mas76`
reach it, `supportcase26`'s primal gap falls from 0.086 to 0.033, and
the one instance whose primal gap rises is `neos-3046615-murg`, 0.053 to
0.061. That meets the pay rule of 0.95x with no fewer incumbents.

Two further steps were measured on top and refused
(`node-forcing-keep` in `bench/refusals.txt`, `node-forcing-keep.py`).
Counting what still removes a basic member at `l152lav`'s nodes names
forcing rows: they fix and remove 1259 basic columns in 360 node
presolves. Leaving such a forcing row in the model takes `l152lav` to
0.102x but `bell5` to 9.42x and `enigma` to 15.5x (1.116x overall);
keeping as well the redundant rows whose logical is nonbasic reads 0.897x
with `bell5` 12.2x and `enigma` 2.09x.

The netlib, infeasible and Kennington gates and the primal, barrier,
PDLP, concurrent and warm readings write the same files, since only a
node solve takes the new branch.
