# 02-197 — Strong branching down to a depth, on the MIP set (D298)

One binary under one setting, 2026-09-06, tree 733d06c plus the batch's
working copy. Work units are the measure; nodes are read beside them. The
script is `02-196/sweep-drop-and-probe-depth.sh`, which produced both
directories from one control.

## What is here

| file | what it is |
|---|---|
| `sweep-control.txt` | every default: reproduces `bench/miplib.baseline` (the D292 reading) node for node and unit for unit on all 17 (`control-against-baseline.txt`). The canary |
| `sweep-r1p0.txt` .. `sweep-r8p0.txt` | `--reliability R --probe-depth 0`: strong branching at the root only, at reliability 1, 2, 4 and 8 |
| `sweep-r4p1.txt`, `sweep-r4p2.txt` | reliability 4, probing one and two levels down |
| `r*-against-control.txt` | per instance, the work ratio and the nodes control -> arm; the geometric mean at the end |

## The reading

Work against the control, geometric mean of per-instance ratios over the
17 (D46: never a sum):

| probes at | reliability | mean | better | worse | past 2x |
|---|---|---|---|---|---|
| the root only | 1, 2, 4, 8 | **0.987x**, one reading | 9 | 5 | 2 (`mod010` 2.84x, `enigma` 2.58x) |
| depth 0 to 1 | 4 | 1.010x | 7 | 6 | 3 (`mod010` 2.84x, `enigma` 2.34x, `dcmulti` 2.04x) |
| depth 0 to 2 | 4 | 1.024x | 5 | 9 | 2 |

The four root-only arms are one tree: at the root no column has a
history, so every candidate is unreliable at any reliability from 1 up,
and the eight best by score are probed once either way.

## The verdicts

**Refused as a default.** The best reading is 0.987x against a bar of
0.95x, with two instances past 2x, and going deeper reads worse. `blend2`
at 0.464x and `p0033` at 0.551x say again what the information is worth;
`mod010` at 2.84x with its tree at 3 nodes says what a probe costs on an
instance whose root solve is the whole tree.

**What this closes.** D293's reopen condition had two clauses, a cheaper
probe by a work cap (D294, `02-193/`) and probing at the root only (this
directory). Both are measured and neither reaches the bar. What could
still reopen it is a probe that learns from an unfinished child solve,
the dual bound at a work-limit stop, which the solver's stop path does not
publish today; `bench/refusals.txt` says so.

**The default stays every depth**, the setting stays behind
`jaos_set_mip_probe_depth` and `--probe-depth`, and `bench/miplib.baseline`
is untouched.
