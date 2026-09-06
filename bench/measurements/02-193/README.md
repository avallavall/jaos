# 02-193 — A work cap on each strong-branching probe, on the MIP set (D294)

One binary under one setting, 2026-09-06, tree d5f10a4 plus the batch's
working copy. Work units are the measure; nodes are read beside them.

## What is here

| file | what it is |
|---|---|
| `sweep-cap-and-dive.sh` | the batch's one script: the control, eight probe-cap arms (this directory) and four dive-child arms (`02-194/`), 12 at once, 120 s cap, and the comparisons |
| `sweep-control.txt` | every default: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-r1c0.txt`, `sweep-r2c0.txt` | reliability 1 and 2 with no cap: D293's arms, node for node |
| `sweep-r1c0.5.txt` .. `sweep-r2c2.txt` | the same two reliabilities with each probe's solve stopped at 0.5, 1 and 2 times the work the node's own relaxation took; a probe that reaches the cap teaches nothing |
| `r*-against-control.txt` | per instance, the work ratio and the nodes control -> arm; the geometric mean at the end |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| reliability | cap | mean | better | worse | past 2x |
|---|---|---|---|---|---|
| 1 | none | 0.971x | 7 | 9 | 2 (`mod010` 2.84x, `enigma` 2.07x) |
| 1 | 0.5 | **1.228x** | 5 | 9 | 2 (`misc06` 4.53x, `mod010` 3.55x) |
| 1 | 1 | 0.998x | 6 | 8 | 1 (`mod010` 4.51x) |
| 1 | 2 | 1.018x | 7 | 8 | 2 |
| 2 | none | 1.064x | 7 | 9 | 3 |
| 2 | 0.5 | 1.307x | 4 | 11 | 4 |
| 2 | 1 | 1.081x | 7 | 7 | 2 |
| 2 | 2 | 1.091x | 7 | 9 | 3 |

## The verdicts

**Refused as a default, and the cap is refused too.** Every capped arm
reads worse than the uncapped one at the same reliability. The node
counts say why: where the cap bites, the tree does not shrink, so the
probe's work is paid and nothing is bought. `mod010` at reliability 1 is
7 nodes uncapped and 7 nodes capped at 1, for 2.84x and 4.51x the
control's work; `misc06` at cap 0.5 is 109 nodes against 117 for 4.53x.
Where the cap does not bite, the arm is D293's.

**What this closes.** The first clause of D293's reopen condition, a
probe capped at a small multiple of the node's own solve, is measured and
false on this set. What remains of that condition is probing at the root
only; `bench/refusals.txt` says so.

**The default stays 0**, no cap, the setting stays behind
`jaos_set_mip_probe_cap` and `--probe-cap`, and `bench/miplib.baseline`
is untouched.
