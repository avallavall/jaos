# 02-195 — Gomory cuts below the root, on the MIP set (D296)

One binary under one setting, 2026-09-06, tree d5f10a4 plus the batch's
working copy. Work units are the measure; nodes and cuts are read beside
them.

## What is here

| file | what it is |
|---|---|
| `sweep-cut-depth.sh` | the control and five depth arms over `bench/miplib.manifest`, 12 at once, 120 s cap, and the comparisons |
| `sweep-control.txt` | every default, cuts at the root only: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-d1.txt` .. `sweep-d1000.txt` | `--cut-depth D` for D = 1, 2, 4, 8, 1000: one round of Gomory cuts at every node whose depth is at most D, each cut valid in its node's subtree and a row of the relaxation for exactly the nodes under it |
| `d*-against-control.txt` | per instance, the work ratio, the nodes control -> arm and the cuts added; the geometric mean at the end |
| `retest-cut-depth.sh` | re-tests the refusal on the current tree, for `make refusals`: depth 1 against the default; writes `retest-cut-depth.txt` and never the files above |

## The reading

Work against the control, geometric mean of per-instance ratios over the
instances both arms finish (D46: never a sum):

| depth | mean | over | better | worse | past 2x | trees |
|---|---|---|---|---|---|---|
| 1 | **1.056x** | 17 | 6 | 10 | 3 (`p0201` 4.22x, `flugpl` 3.11x, `misc03` 2.64x) | `egout` 39127 to 2715, `p0033` 1609 to 691, `mod010` 7 to 3 |
| 2 | 1.260x | 17 | 4 | 11 | 4 (`p0201` 9.65x) | |
| 4 | 1.508x | 17 | 5 | 11 | 7 (`misc03` 16.3x, `p0201` 13.8x) | `egout` to 1867 |
| 8 | 1.948x | 16 | 6 | 9 | 8 (`p0201` 37.9x, `misc03` 17.2x) | `stein45` no longer finishes in 120 s |
| every node | 2.440x | 16 | 6 | 9 | 8 (`misc03` 120x, `p0201` 46.2x) | `egout` to 1803, `flugpl` 4495 to 197, `lseu` 25947 to 2181 |

## The verdicts

**Refused as a default.** The information is real: at every depth the
trees shrink on most instances, and on `egout` by a factor of 14 at
depth 1 for 0.081x the work. The price is the rows. A node's cuts ride
with every node under it, so the relaxations grow with the depth, and
`p0201` at depth 1 pays 4.22x with its tree unchanged (1357 to 1353
nodes) for 82 rows carried through it; at every node it carries 14892
cuts and pays 46x for a tree of 399. Each arm also adds and removes the
local rows between nodes whose lists differ, which the work counts.

**What reopens it.** A cut that leaves the relaxation once its slack is
basic at a node, so a row that no longer binds is not carried under it;
and no row churn between two nodes that hold the same cuts. Both are
measured on this set against these arms; `bench/refusals.txt` carries
the condition.

**The default stays 0**, the root only; the setting stays behind
`jaos_set_mip_cut_depth` and `--cut-depth`, and `bench/miplib.baseline`
is untouched.
