# 02-192 — Strong branching until reliable, on the MIP set (D293)

One binary under one setting, 2026-09-05, tree 7161315 plus the batch's
working copy. Work units are the measure; nodes are read beside them.

## What is here

| file | what it is |
|---|---|
| `sweep-rel.sh` | five arms over `bench/miplib.manifest`, 12 at once, 120 s cap, and the comparisons |
| `sweep-control.txt` | `--reliability 0`, pure pseudocost branching: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17. The canary |
| `sweep-r1.txt` .. `sweep-r8.txt` | reliability 1, 2, 4, 8: a column with fewer branches per direction has its untrusted children solved on the spot, at most eight columns per node |
| `r1-against-control.txt` .. `r8-against-control.txt` | per instance, the work ratio and the nodes off -> on; the geometric mean at the end |
| `retest-reliability.sh` | re-tests the refusal on the current tree, for `make refusals`: reliability 1 against 0; writes `retest-reliability.txt` and never the files above |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| reliability | mean | better | worse | past 2x | trees |
|---|---|---|---|---|---|
| 1 | **0.971x** | 7 | 9 | 2 (`mod010` 2.84x, `enigma` 2.07x) | `p0033` 1609 to 391, `p0201` 1357 to 329, `blend2` 6827 to 2877 |
| 2 | 1.064x | 7 | 9 | 3 | |
| 4 | 1.173x | 4 | 11 | 3 (`mod010` 2.84x, `misc03` 2.70x, `dcmulti` 2.34x) | `dcmulti` 585 to 135, `misc06` 117 to 43, `mod010` 7 to 3 |
| 8 | 1.437x | 3 | 13 | 5 | |

## The verdicts

**Refused as a default.** The trees shrink at every setting and the work
rises at every setting but the first, which is under 1 only in the mean
and past 2x on two instances. Each probe is a full child solve from the
node's basis, and on models this small a child solve costs about what a
node costs, so a probe pays only where it removes more than one node.
`blend2` at 0.383x and `flugpl` at 0.436x under reliability 1 are where
it does.

**What reopens it.** A cheaper probe: a work cap per child at a small
multiple of the node's own solve, or probing at the root only. The
information is worth having; its price is what this measured.

**The default stays 0**, the setting stays behind `jaos_set_mip_reliability`
and `--reliability`, and `bench/miplib.baseline` is untouched.
