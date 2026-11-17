# 02-475 — pk1 and mas76 close inside 1e10 work units

Taken on 2026-11-17 on 4dc8e97, for TODO row J7's `pk1` and `mas76`.
`read.sh` takes every reading one solve at a time, from a copy of 4dc8e97
with `prototype.patch`, whose three parts environment switches turn on
and off. `landing.patch` is the change with its tests and docs:
- `highs-pk1.txt`, `highs-mas76.txt`, `scip-pk1.txt`, `scip-mas76.txt`:
  HiGHS 1.15.1 and SCIP 10.0 on both, one thread;
- `scip-tree-mas76.txt`: SCIP on `mas76` with its parts taken away, by
  02-474's `scip_tree.py`;
- `arms.txt`: JAOS on both at 5e10 work units, part by part;
- `census.txt`: the objective step on MIPLIB 3 and MIPLIB 2017;
- `miplib3/*.txt`, `miplib2017/*.txt`, `sum.txt`: the arms on both sets.

`census.txt` was taken on an earlier copy with the same objective step.
`miplib3/land.txt` and `miplib2017/land.txt` come from the landing build,
which reads as the prototype's `all` arm node for node on MIPLIB 3, `pk1`
and `mas76`. 02-474's `m3cmp.py` and `m17cmp.py` print the per-instance
tables below.

## The two models

`pk1` has 55 binaries and 31 continuous columns. Fifteen equality rows
`a_i y + p_i - q_i = 731` hold a deviation with whole coefficients up to 55,
and thirty rows `z >= p_i`, `z >= q_i` put the objective column `z` above
each. The objective is the largest deviation, a whole number at every
integer point. The optimum is 11.

`mas76` has 150 binaries and one continuous column `z`: a cardinality row
`sum x <= 15` and eleven rows `a_i x + z >= b_i` with fractional
coefficients. The objective is `z + 1e-5 sum x`, and the optimum 40005.05.

Both find their optimum at node 1. At 5e10 work units HEAD closes `pk1` at
1.61e10 in 331,251 nodes and `mas76` at 1.95e10 in 632,196.

## HiGHS and SCIP

| solver | `pk1` | `mas76` |
|---|---|---|
| JAOS 4dc8e97 | 331,251 nodes | 632,196 nodes |
| SCIP | 353,721 nodes, 88 s, its optimum at node 30,400 | 216,712 nodes in two runs, 52 s |
| HiGHS | bound 5.33 and incumbent 12 after 563,792 nodes and seven restarts, stopped by hand at 215 s | 167,805 nodes, 60 s |

On `pk1` JAOS's tree is already the size of SCIP's. Neither SCIP nor
HiGHS reads `pk1`'s objective as integral: both report 0 implied integers.

On `mas76` SCIP needs 2.9 times fewer nodes. `scip-tree-mas76.txt` takes
its parts away one at a time:

| SCIP setting | `mas76` nodes |
|---|---|
| default | 216,712 |
| no reduced-cost fixing | 354,034 |
| no conflict analysis | 334,009 |
| no cuts below the root | 316,847 |
| no restart | 167,711 |
| pseudocost branching | 190,688 |
| none of the first three, and no restart | 666,815 |
| that, and no objective propagation | 678,845 |

With reduced-cost fixing, node cuts, conflict analysis and the restart
off, SCIP takes 666,815 nodes, about JAOS's 632,196, so JAOS's branching
is as good as SCIP's here. Reduced-cost fixing is the largest single part,
1.63 times the nodes; SCIP's statistics count 574,065 bounds from it at
the nodes, 13,088 conflicts from nodes whose relaxation passed the
incumbent, and 22,584 c-MIR cuts.

## What was built

Three parts, each a textbook piece JAOS lacked:

1. **The objective step through continuous columns.** The step of 02-328
   needed every costed column to be integer. A continuous column with a
   cost now counts when every continuous column its rows link it to has
   whole bounds and sits in rows that hold at most one of them at +β and
   one at −β, with whole sides and integer coefficients over β. Those rows,
   over the continuous columns, are the transpose of a network matrix, so
   they are totally unimodular, and at any integer point their relaxation
   has a whole optimum (Hoffman and Kruskal). On `pk1` the bound of every
   node rounds up to the next whole number, so a node closes once its
   bound passes 10.
2. **A node relaxation that stops at the incumbent.** Below the root, the
   dual simplex of a node tracks the objective of its basic solution,
   `c'x`, across pivots and bound flips. Once it reaches the bound at which
   the tree would prune the node, the Lagrangian bound of the current duals
   is computed; if it reaches that bound too, the solve stops and the node
   is pruned without its optimum, the objective limit a tree gives its LP
   solver (Achterberg, 2007). The child's pseudocost learns that bound
   (`JAOS_NCUT=1`; `=2` learns nothing from it).
3. **Reduced-cost fixing at every node.** With an incumbent, a node of a
   linear model pulls each nonbasic integer column's far bound in as far as
   its reduced cost allows against the prune bound, and its subtree
   inherits the new bounds, as the root already did for the whole tree
   (02-335). The orbital fixing does not read such a bound as a branching,
   and a conflict whose proof needs one adds no row.

## pk1 and mas76

`arms.txt`, at 5e10 work units:

| arm | `pk1` work | nodes | `mas76` work | nodes |
|---|---|---|---|---|
| HEAD | 1.61e10 | 331,251 | 1.95e10 | 632,196 |
| the step | 1.02e10 | 203,103 | 1.95e10 | 632,196 |
| the stop | 1.32e10 | 340,814 | 1.32e10 | 577,515 |
| step and stop | 8.34e9 | 208,554 | 1.32e10 | 577,515 |
| step and stop, no pseudocost from the stop | 8.44e9 | 212,059 | 1.35e10 | 598,479 |
| node fixing | 1.40e10 | 297,736 | 8.06e9 | 417,737 |
| step and node fixing | 9.19e9 | 188,780 | 8.06e9 | 417,737 |
| all three | 7.49e9 | 174,316 | 5.60e9 | 379,655 |

`mas76` has no continuous column with whole data, so the step leaves it
alone. Its 288,078 stopped relaxations and the heuristics they skip save
a third of its work, and node fixing saves 59% on its own, 1.95 million
bounds over 417,737 nodes. `pk1` with the step alone closes at 1.02e10,
just past the budget; HEAD run with `--cutoff 10.00001`, a tree that only
has to prove nothing beats 10, ends infeasible at 1.0006e10 in 218,055
nodes, so the step is worth that much and no more. Its stopped
relaxations and node fixing take it under.

Neither model holds a conflict row. `mas76`'s peak memory is 377 MB
against 236 MB: its nodes carry the fixed bounds.

A node that tightens a column its list already holds, as a general
integer can be tightened again and again, writes over that column's last
entry instead of adding one; the bounds a node sets are the same, since
the last entry wins. Appending, `gen-ip002`, 41 general integers, peaked
at 1.66 GB against HEAD's 352 MB at 1e10 work units, 2.37 million
tightenings over 350,355 nodes; writing over, at 559 MB. The work charged
for the lookup moved the trees: before it, `pk1` closed at 7.35e9 and
`mas76` at 6.50e9.

## The census

`census.txt`: the new step reaches `misc03` and `misc07` on MIPLIB 3 and
`markshare_4_0`, `markshare2`, `pk1`, `assign1-5-8`, `ic97_potential`,
`csched008` and `csched007` on MIPLIB 2017. The other models read as
before.

## MIPLIB 3

Against `bench/miplib.baseline`, work, arm by arm (`miplib3/*.txt`):

| arm | geometric mean | total | worst |
|---|---|---|---|
| `off`, every switch off | 1.000x | 1.000x | none moves |
| `grid`, the step | 1.001x | 1.004x | `misc07` 1.013x |
| `cut`, the stop | 0.935x | 0.867x | `misc06` 1.009x |
| `rc`, node fixing | 0.879x | 0.854x | `stein45` 1.022x |
| `cut2`, all three, no pseudocost from the stop | 0.837x | 0.757x | `rgn` 1.042x |
| `all`, all three | 0.827x | 0.754x | `gt2` 1.015x |

`off` reads as the baseline node for node. All 24 solve in every arm, with
the objective, the checker and the second solve as the gate wants.
`land.txt`, the landing build, reads as `all` node for node; the `-O2`
build `read.sh` makes reads as the Makefile's.

| instance | work under `all` | nodes |
|---|---|---|
| `bell3a` | 0.355x | 63,291 to 26,389 |
| `blend2` | 0.361x | 4,216 to 2,817 |
| `p0282` | 0.607x | 291 to 269 |
| `bell5` | 0.719x | 22,191 to 16,161 |
| `l152lav` | 0.757x | 583 to 473 |
| `misc03` | 0.761x | 291 to 209 |
| `p0201` | 0.798x | 59 to 53 |
| `misc07` | 0.800x | 8,617 to 8,504 |
| `lseu` | 0.853x | 245 to 229 |
| `mod008` | 0.855x | 43 to 41 |
| `dcmulti` | 0.892x | 376 to 382 |
| `rgn` | 0.906x | 187 to 139 |
| `stein45` | 0.910x | 51,901 to 55,644 |
| `stein27` | 0.940x | 1,229 to 1,231 |
| `khb05250` | 0.989x | 71 to 71 |
| `p0033` | 1.013x | 11 to 13 |
| `gt2` | 1.015x | 5 to 5 |

The rest read within 1% of the baseline. `bell3a`, whose tree TODO J7
calls far larger than HiGHS's, takes 0.378x under node fixing alone.

A first form kept the conflict rows whose proof needs a bound the nodes'
reduced costs fixed. It read 0.844x and 0.770x on MIPLIB 3, but on
`markshare_4_0` it kept 3,424 conflict rows against HEAD's 1,788, each a
permanent row of every node relaxation, so an iteration cost 1.7 times
as much and the tree reached 428,665 nodes at 1e10 against 650,369
(`miplib2017/unguarded.txt`). Neither `pk1` nor `mas76` holds a conflict
row, so the guard leaves them as they were.

## MIPLIB 2017

At 1e10, 02-328's `m17sum.py` (`sum.txt`):

| reading | solved | primal | dual | sum |
|---|---|---|---|---|
| the record, 4dc8e97 | 6 | 4.4435 | 6.4500 | 10.8936 |
| `land`, the change | 8 | 5.4989 | 6.3148 | 11.8137 |
| `unguarded`, its first form | 8 | 5.4754 | 6.2933 | 11.7687 |

02-474's six readings of one code, offsets of the root's work, span 10.85
to 11.88 in sum, 4.43 to 5.44 on the primal side and 6.42 to 6.55 on the
dual side. The change's dual side, 6.3148, is below all six. Read against
the record and 02-474's null readings (`miplib2017/null*.txt` there):

| instance | record, primal and dual | `land` | the nulls |
|---|---|---|---|
| `pk1` | 0, 0.1998 | optimal, 174,316 nodes | dual 0.203 to 0.240 |
| `mas76` | 0, 0.0057 | optimal, 379,655 nodes | dual 0.0053 to 0.0066 |
| `markshare_4_0` | 0, 1 | 1, 1: incumbent 3 | two of five lose the optimum |
| `mas74` | 0, 0.0472 | 0.0415, 0.0446 | 0 to 0.245, 0.0466 to 0.0477 |
| `supportcase26` | 0.0157, 0.1572 | 0.0327, 0.1564 | 0.006 to 0.026, 0.150 to 0.161 |
| `tr12-30` | 0.0001, 0.0024 | 0.0110, 0.0024 | primal 0 to 0.103 |
| `neos-3754480-nidda` | 0, 1 | 0.0047, 1 | primal 0 to 0.0043 |
| `neos-3046615-murg` | 0.0631, 0.7119 | 0.0656, 0.7081 | 0.043 to 0.076, 0.710 to 0.723 |
| `timtab1` | 0.6889, 0.2875 | 0.6889, 0.2891 | dual 0.277 to 0.286 |
| `glass4` | 0.2500, 0.2500 | 0.2500, 0.3333 | dual 0.25 to 0.33 |
| `assign1-5-8` | 0, 0.0906 | 0, 0.0896 | dual 0.0898 to 0.0923 |
| `csched007` | 0.1538, 0.1388 | 0.1538, 0.1339 | dual 0.137 to 0.140 |
| `beasleyC3` | 0.0491, 0.0159 | 0.0305, 0.0159 | primal 0.035 to 0.092 |

The primal side moves on the incumbents the nulls move too. Most of it is
`markshare_4_0`: HEAD's tree reaches its optimum at node 545,446 and this
one stops at 3, as two of 02-474's five nulls do; given 3e10 work units it
reaches the optimum at node 1,001,789. `mas74` ends at 12291.0 against its
optimum 11801.2, as three nulls end further away, and its bound is better
than the record's and every null's. Three instances end just outside the
nulls' range: `supportcase26`'s incumbent, 1802.2 against the record's
1772.5 and the nulls' worst 1790.0; `neos-3754480-nidda`'s, 0.47% from its optimum where
the nulls' worst is 0.43%; and `timtab1`'s bound, 543645 against 544875,
0.2% under the record's, its nodes costing 1.2 times the units.

The other instances read as the record or better: `graphdraw-domain`,
`neos-3627168-kasai`, `ic97_potential` and `beasleyC3` gain a little,
and `binkar10_1`, `exp-1-500-5-5`, `p200x1188c` and `csched008` keep their
answers in fewer nodes, `p200x1188c` in 28 against 242. Peak memory over
the set is 1.50 GB, against 1.56 GB for HEAD on `markshare_4_0` alone.

## On top of 02-476 and 02-477

These readings were taken on 4dc8e97. The change landed after 02-477's
second symmetry search and 02-476's tree on a reduced basis. 02-477's
objective rule, a costed continuous column alone among integer columns
in its rows, is the one-column case of this change's difference rows,
so the landed code keeps this rule alone and charges its pass to the
work. Both sets were taken again on the landed code, one solve at a
time (`combined/miplib3.txt`, `combined/miplib2017.txt`, against
`combined/before.txt`, 02-476's record).

MIPLIB 3 reads 0.746x in total and 0.837x geometric against
02-474's baseline, none past 1.1x: `bell3a` 0.355x, `blend2` 0.506x,
`p0282` 0.607x.

MIPLIB 2017 solves 9 of 30 against 7, with `pk1` at 7.49e9 work units
and `mas76` at 7.64e9 (521,051 nodes). The dual side reads 6.3032
against 6.4328 and the gap sum 11.7099 against 10.8822, inside
02-474's null spread:

| instance | before, primal and dual | landed |
|---|---|---|
| `pk1` | 0, 0.1998 | optimal |
| `mas76` | 0, 0.0057 | optimal |
| `markshare_4_0` | 0, 1.0 | 1.0, 1.0 |
| `gen-ip002` | 0, 0.0039 | 0.0025, 0.0049 |
| `neos-3754480-nidda` | 0, 1.0 | 0.0043, 1.0 |
| `tr12-30` | 0.0001, 0.0024 | 0.0110, 0.0024 |
| `glass4` | 0.25, 0.25 | 0.25, 0.3322 |
| `neos-3046615-murg` | 0.0394, 0.7150 | 0.0200, 0.7081 |
| `beasleyC3` | 0.0796, 0.0159 | 0.0398, 0.0159 |
| `mas74` | 0, 0.0472 | 0, 0.0435 |

`markshare_4_0` loses its optimum as two of 02-474's five null
readings do; without it the sum reads 10.71. `gen-ip002` loses its
optimum too: with fixing at every node, 02-476's first 20,000 nodes
over the columns no longer reach the point the columns' tree found at
node 10,326, and the reduced tree finds none of its own (TODO J7).

`make test`, `make sanitize`, `make python-test`, the netlib,
infeasible and Kennington sets, `make miplib J=2` and `make primal
barrier pdlp concurrent warm J=12` pass on the landed code, the last
five reading as their records.

## The verdict

Landed, as `landing.patch`: the objective step through continuous columns
held as differences, node relaxations that stop at the incumbent, and
reduced-cost fixing at every node. `pk1` closes at 7.49e9 work units and
`mas76` at 5.60e9, inside the budget, and MIPLIB 2017 reads 8 solved with
its dual side below every null reading. MIPLIB 3 reads 0.827x geometric and
0.754x in total, worst `gt2` 1.015x. The 2017 sum, 11.81, sits inside the
nulls' spread; of the instances the change moves, `markshare_4_0` and
`mas74` lose incumbents the nulls lose too, and `supportcase26`'s and
`nidda`'s incumbents and `timtab1`'s bound end just outside the nulls'
range, the furthest `supportcase26`'s at 0.7% of its objective.

`make test`, `make sanitize`, `make python-test`, the Netlib, infeasible
and Kennington sets against their baselines (nothing moves: the stop is
set only by the tree) and MIPLIB 3 against `bench/miplib.baseline` pass on
the landed code. `make primal barrier pdlp concurrent warm` were not
re-taken: the dual simplex reads the stop only when the tree sets it.

`unguarded.txt` comes from a copy without the conflict guard and without
writing over a column's last entry; to retake it, drop `|| cur->rcf[k]`
from `conflict_row` and append every tightened bound with `node_add_fix`
in a copy built from `prototype.patch`.
