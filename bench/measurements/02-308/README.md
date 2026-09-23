# 02-308 — MIR with variable upper bound substitution

Taken on 2026-09-24 for TODO row H2, on the tree of f848994.

HiGHS and SCIP close three fixed-charge networks of the 2017 set at the
root (`sp150x300d`, `p200x1188c`, `exp-1-500-5-5`; see TODO H2). Their
rows `x - u y <= 0` are what Marchand and Wolsey's c-MIR reads through a
variable bound substitution: a continuous `x` that sits nearer `u y` than
0 is replaced by `u y - s` with `s >= 0` before the rounding, so its
weight moves onto the binary `y`, and the cut is mapped back to `x` and `y`
after it.

`mir-vub.patch` adds that to `mir_side` behind `MIP_MIR_VUB` (off in the
patch; the arms build it with `-DJAOS_MIP_MIR_VUB_VALUE=1`). Each MIR round
builds the table of variable upper bounds the flow cover separator already
reads (two-entry rows at 0 with a binary partner).

At 3e9 work units, bound against the same run without it:

| instance | default MIR | with `--mir-aggregate 6` |
|---|---|---|
| sp150x300d (optimum 69) | 67.89 to 68.40 | 67.01 to 58.33, 6409 nodes and 3561 cuts instead of 15296 |
| p200x1188c (15078) | 7395 to 7178 | 7914 to 6904 |
| exp-1-500-5-5 (65887) | 46851 to 48507 | 60879 to 63180 |

MIPLIB 3 (`make miplib J=2`) reads 1.020x in work, every objective at the
reference: `blend2` 1.535x, `egout` 1.086x, `dcmulti` 0.956x, the others
within 1%. Refused as `mir-vub` in `bench/refusals.txt`. The substitution
alone does not give the root what the rivals get; lifting, the choice of
rows to aggregate along the flow balance, and many more rounds are what
the papers pair it with.
