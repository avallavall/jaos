# 02-286 — local branching, the node order and a root restart

Taken on 2026-09-21 and 2026-09-22 on the tree of 2e04b47 with TODO rows
B11 and B17 applied. Row B11 asked for local branching, a best-estimate
node order and a restart of the root, each behind a switch, read on the
MIPLIB 2017 set, and for the plunge bounded by the gap to get its 2017
reading.

## The rule

The rule is 02-284's. On the 2017 set (`bench/miplib2017.manifest`, 30
instances, 1e10 work units each) a switch pays when it solves more instances
than the default, or when its mean primal gap plus its mean dual gap reads
at or under 0.95x the default's 1.0564, with no fewer instances solved and
no fewer incumbents. `../02-284/gap.sh` prints the means. A switch that pays
lands only when MIPLIB 3 also keeps every instance within 2x of
`bench/miplib.baseline`.

MIPLIB 3 runs in two parts. bell5 runs alone at 6.3e9 work units, twice its
baseline 3144587459, because an unfinished bell5 tree grew to 2.9 GB. The
other 23 run at 1.2e11 against the baseline.

## The 2017 set

Each run is `make miplib2017 J=4 MIPLIB2017_ARGS='-O <option>'`, and its
result file is here.

| switch | option | solved | incumbents | at the reference | primal gap | dual gap | gap sum |
|---|---|---|---|---|---|---|---|
| none (the default before) | | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |
| local branching, 10 flips | `mip_local_branching=10` | 0 | 15 | 1 | 0.6847 | 0.3458 | 0.975x |
| local branching, 20 flips | `mip_local_branching=20` | 0 | 15 | 1 | 0.7032 | 0.3444 | 0.992x |
| dive resumed within 1e-2 | `mip_dive=1 mip_dive_gap=1e-2` | 1 | 20 | 4 | 0.5490 | 0.3540 | 0.855x |
| dive resumed within 1e-4 | `mip_dive=1 mip_dive_gap=1e-4` | 1 | 20 | 5 | 0.5157 | 0.3540 | 0.823x |
| estimate, bound every 10th pick | `mip_node_select=1` | 0 | 17 | 7 | 0.5580 | 0.3484 | 0.858x |
| estimate, bound every 5th pick | `mip_node_select=1` | 0 | 17 | 4 | 0.6000 | 0.3464 | 0.896x |
| restart at 0.2 | `mip_restart=1` | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |
| restart at 0.05 | `mip_restart=1` | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |

The bound pick and the restart share are build constants
(`JAOS_MIP_ESTIMATE_BOUND_EVERY_VALUE`, `JAOS_MIP_RESTART_FRAC_VALUE`), so
those rows come from builds with `EXTRA_CFLAGS=-D...`.

- Local branching improves 10 of the 15 incumbents at 10 flips (pk1 196 to
  45, exp-1-500-5-5 102167 to 77608, tr12-30 195766 to 167155). The primal
  gap is capped at 1, and half of the set has no incumbent to start from, so
  the sum moves little.
- The restart fired on none of the 30 at either share. It needs an incumbent
  at the root and that many reduced-cost fixings. The check itself bills
  `nc` work units at a root with an incumbent, so sp150x300d ends one
  iteration apart from the default; every other line is the default's.
- The dive and the estimate order both pay. The plain dive of 02-284 read
  0.855x.

## MIPLIB 3

| switch | the other 23 | past 2x | bell5 |
|---|---|---|---|
| dive resumed within 1e-4 | 0.9167x | enigma 2.622x | stops at 6.3e9 with the optimum 8966406.49 and bound 8960667.32 |
| estimate, bound every 10th pick | 0.8702x | none | stops at 6.3e9 with no incumbent |
| estimate, bound every 5th pick | 0.9221x | none | optimal at 5327074571, 1.694x |
| estimate, bound every 3rd pick | | | stops at 6.3e9 with the optimum and bound 8961306.76 |
| estimate, bound every 2nd pick | | | stops at 6.3e9 with no incumbent |

The dive fails the bar twice. The estimate order with the bound every 5th
pick is the one arm that pays on the 2017 set and keeps MIPLIB 3 inside 2x,
so it lands on, with `MIP_ESTIMATE_BOUND_EVERY` at 5.

## A failed node

Under the estimate order the relaxation of one neos-3754480-nidda node
fails: node 7887 at every 10th pick, node 5719 at every 5th. The settled
point is not dual feasible: a reduced cost sits 1.1e-9 to 1.2e-9 past its
bound, from a cold start. The tree stopped there `numerical_error` with an
incumbent in hand
(`miplib2017-estimate5.txt`). Row B17 sets such a node aside with its bound,
as the conic tree does, and the tree goes on.

## The default after

`bench/results/miplib2017.txt`, taken with the estimate order on and the
failed node set aside: 0 of 30 finish, 17 have an incumbent and 4 are at
the reference. The mean primal gap is 0.5988 and the mean dual gap 0.3464,
0.895x the default before. Every line but one is the line of
`miplib2017-estimate5.txt`: neos-3754480-nidda now runs to the work limit,
27579 nodes, with the incumbent 14284.61 against 14748.31 when the tree
stopped at node 5719.

`bench/results/miplib.txt`: 24 of 24 solved, 0.946x the work of the old
baseline, none past 2x, bell5 1.694x. Its 24 lines are byte-identical to
the two estimate runs above, which were taken without the failed node set
aside, so setting it aside changes nothing on MIPLIB 3. The baseline is
rewritten from this run.
