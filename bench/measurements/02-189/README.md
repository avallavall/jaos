# 02-189 — The MIP set, the cut rounds, and the dive (D289)

Three questions, one binary under its switches, 2026-09-05, tree 0e2395b
plus the batch's working copy (the switches and the runner mode, before the
defaults were set from this reading). Work units are the measure; seconds
are printed for the reader and decided nothing.

## What is here

| file | what it is |
|---|---|
| `time-cands.sh` | solves every candidate with a build, 60 s cap each, 12 at once; its readings are the two files below |
| `plain.txt` | the plain tree of D288 (tree 0e2395b, no switch existed) on 38 MIPLIB 3 candidates: 17 finish |
| `new.txt` | the first form of this batch on the same 38: ten rounds of cuts and the dive on. Eight instances much cheaper, ten worse, `fiber` and `pk1` failing numerically — the reading that forced the sweep |
| `sweep-mip.sh` | the sweep: the 17 the plain tree finishes, `bell3a`, `fiber` and `pk1`, 40 s cap, 12 at once, one arm per switch setting. The second run of it (`c2nd`, `c3nd`) is the same script with two arms |
| `sweep-control.txt` | `--cut-rounds 0 --no-dive` (the tree's flags at the time): reproduces `plain.txt` node for node and unit for unit on all 20 (`blend2`'s 7761-unit difference in the later arms is the root tableau, billed even when it yields no cut). The canary: without this, nothing below is a measurement |
| `sweep-dive.txt` | dive on, no cuts |
| `sweep-c1.txt`, `sweep-c2.txt`, `sweep-c5.txt` | 1, 2, 5 rounds with the dive on |
| `sweep-c1nd.txt`, `sweep-c2nd.txt`, `sweep-c3nd.txt` | 1, 2, 3 rounds with the dive off |
| `geomean-sweep.py` | per-instance ratios against the control and the geometric mean over the instances both arms solved; `python3 geomean-sweep.py dive c1nd c2nd c3nd c2 c5` |
| `retest-dive.sh` | re-tests the refusal on the current tree, for `make refusals`; writes `retest-dive.txt` and never the files above |

## The reading

Geometric mean of per-instance work ratios against the control, over the
17 instances both arms solved (D46: never a sum):

| arm | rounds | dive | mean | better | worse | past 2x | notes |
|---|---|---|---|---|---|---|---|
| `dive` | 0 | on | **1.125x** | 1 | 6 | 0 | `blend2` 1.98x, `misc06` 1.68x, `air03` 1.43x; `enigma` 0.80x |
| `c1nd` | 1 | off | **0.660x** | 7 | 8 | 0 | worst `p0201` 1.66x; `mod010` 0.056x, `khb05250` 0.116x, `p0033` 0.180x |
| `c1` | 1 | on | 0.717x | 8 | 9 | 0 | the dive's 1.09x on top of the cuts |
| `c2nd` | 2 | off | 0.609x | 7 | 9 | 3 | `p0201` 4.08x, `misc03` 3.50x, `rgn` 2.31x; `egout` 0.084x |
| `c2` | 2 | on | 0.655x | 7 | 10 | 3 | |
| `c3nd` | 3 | off | 0.706x | 7 | 9 | 5 | `misc03` 13.1x, `p0201` 10.1x; `pk1` fails numerically at the root |
| `c5` | 5 | on | 0.839x | 6 | 10 | 5 | over 16: `stein45` no longer finishes; `pk1` fails at the root |

`bell3a`, which the plain tree does not finish in 40 s, finishes at every
setting from one round. `fiber` and `pk1` finish under no arm.

## The verdicts

**One round of cuts, default.** The best mean that keeps every instance
under the gate's own regression factor of 2. Two rounds has the better mean
and loses on its tails; three already breaks an instance.

**The dive, refused.** 1.125x with the cuts off and 1.09x on top of them,
one instance better and six worse. The mechanism is an early incumbent, and
on this set the incumbent is not what limits the tree. Kept behind
`jaos_set_mip_dive` so `retest-dive.sh` can ask again; reopens on a child
rule or a backtracking dive that reads at or under 0.95x with no instance
past 2x, on this set.

**The set.** The 17 the plain tree finishes; `bench/miplib.baseline` is the
one-round, no-dive reading and matches `sweep-c1nd.txt` node for node.
