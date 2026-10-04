# 02-337 — a MIP presolve: flows through a node merged

Taken on 2026-10-04 on the tree of 841e840, for TODO row J7 (the networks'
root bound). The settings are 02-328's; the arms ran from 02-329's
`arms.sh`.

## Why

HiGHS 1.15.1's presolve takes `beasleyC3` from 1750 rows and 2500 columns
to 1153 and 1704 before its root (02-328's `highs-root.txt`); JAOS ran its
tree on the model as read. `contract.py` rewrites an MPS file the way the
presolve now does: an equality row whose only entries are two continuous
columns `x_p - x_q = 0` (in the files, a node that passes all its flow on)
goes, and `x_q` is merged into `x_p`. Run on the rewritten files, JAOS read:

| model | rows merged | root bound | at 1e10 work units |
|---|---|---|---|
| beasleyC3 | 134 | 699.6 to 718.9 | bound 702 to 723, incumbent 889 to 848 |
| sp150x300d | 61 | 66.89 to 67.52 | solved at 2.7e7 work units, was 3.07e8 |
| binkar10_1 | 180 | | bound 6715.38 to 6713.13 |
| tr12-30 | 4 | | bound 118133 to 118170, incumbent 145619 to 143000 |
| csched008 | 10 | | 25 nodes, the bound at 171 |
| egout | 10 | | 0.895x the work |

The substitution leaves the relaxation's value as it was. What changes is
the cuts: an aggregation step of the network c-MIR covers more of the
network, and a merged arc carries both its variable bounds.

## The presolve

`jaos_set_mip_presolve`, on by default. Before the tree, passes over the
model merge every such pair whose columns no other merge of the pass has
touched, until a pass merges nothing (at most `MIP_PRESOLVE_PASSES`). The
tree runs on a copy of the smaller model, which keeps the order of the
columns left. The answer comes back to every column: a merged column takes
the value of the column it went into, and an optimum is solved again on the
whole model with the integer columns fixed, which gives its duals and
basis. A model with a quadratic objective, cones, quadratic rows, SOS
sets, indicator rows or semi-continuous columns, or with an incumbent or
node callback, is solved whole. A sub-MIP inside the tree does not presolve
again: the first reading (`b11`) let it, and the scan's work moved the
caps of models with nothing to merge (`lseu` 5670 to 5424 nodes,
`markshare2`'s incumbent).

The first form also merged rows with a right-hand side other than 0 and
rows with equal coefficients (`x_q = shift + ratio x_p`). It merged 84
rows of `sp150x300d` and 199 of `beasleyC3`, and `sp150x300d` took 3.4e9
work units in 19731 nodes: a substitution with a shift turns the variable
bound row `x - u y <= 0` into one with a nonzero right-hand side, which the
network c-MIR no longer reads as a variable bound.

## The reading

Against `bench/results/miplib.txt` and `bench/results/miplib2017.txt` of
841e840:

| arm | MIPLIB 3 geo | sum | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|
| 841e840 | 1 | 1 | 3 | 25 | 8.129 | 7.322 | 15.452 |
| b11, sub-MIPs presolve too | 0.994 | 1.000 | 3 | 25 | 7.946 | 7.288 | 15.234 |
| b12 | 0.996 | 1.000 | 3 | 25 | 7.987 | 7.288 | 15.275 |

`b12` is the shipped code. On MIPLIB 3 only `egout` moves (0.896x). On the
2017 set `sp150x300d` solves at 2.68e7 work units in 95 nodes (3.07e8 in
1189 before), `beasleyC3` holds 797 with a bound of 724 (889 and 702),
`tr12-30` 143000 and 118170 (145619 and 118133), `csched007`'s bound goes
from 291.90 to 293.75, and `binkar10_1`'s bound falls from 6715.38 to
6713.13. The models with nothing to merge end as before; their work grows
by the scan, two passes over the matrix.

`miqp-b13.txt`, QPLIB's 17 convex MIQPs as in 02-332, has every status,
incumbent, bound and node count of 02-335's `miqp-rc.txt`: a quadratic
objective is solved whole.
