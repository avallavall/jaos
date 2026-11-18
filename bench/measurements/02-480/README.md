# 02-480 — assign1-5-8: where SCIP's bound comes from, and implied integers in the Gomory cuts

Taken on 2026-11-18 on b53a1ffd, for TODO row J7's `assign1-5-8`.
`read.sh` takes every reading one solve at a time, from copies of b53a1ffd
with `prototype.patch` (switches, below), `change.patch` (the change;
`landing.patch` is it with its tests and docs), `null.patch` (the change
with 02-474's null readings) and `cutrows.patch` (the change with one more
switch):
- `scip-root.txt`, `scip-root-arms.txt`, `scip-tree.txt`: SCIP 10.0, one
  thread, its components taken away one at a time, at its root and at
  50,000 nodes, on the model and on `zint.mps`, the model with its 26 cost
  columns declared integer (02-477's `zint.py`); `scip_tree.py`,
  `scip_root.py`, `scip_prio.py`;
- `relaxations.txt`: HiGHS 1.15.1 on the relaxations `rlt.py` builds and on
  `zonly.py`'s model;
- `jaos-arms.txt`, `spread.txt`, `jaos-tree-head.txt`,
  `jaos-impl1-1e11.txt`: JAOS's arms on `assign1-5-8` at 1e10, five null
  readings each, and one tree at 1e11;
- `census.txt`: the prototype's arms on the nine models where the
  objective step finds implied integers;
- `miplib3/*.txt`, `miplib2017/*.txt`, `m3cmp.txt`, `m17cmp.txt`,
  `sum.txt`: the first switch and the change on both sets; `lp-gates.txt`:
  the netlib, infeasible netlib and Kennington sets under the change.

`prototype.patch`'s `JAOS_IMPL` reads, as integer, the columns the
objective step finds whole at an optimum: bit 1 in the Gomory rounds (root
and nodes), bit 2 in the root's other separators, bit 4 in node MIR and its
list of all-integer rows. Its `JAOS_GISL` makes a row's slack integer in
the Gomory rounds: 1 on every all-integer row, 3 on the ones that hold an
implied column. The form 3 reads the original integrality through a static
pointer that a sub-MIP leaves dangling and charges its scan on every model,
so its readings were thrown away; `spread.txt`'s `impl-allrows` rows use
form 1, which does not read the pointer.

## The model and its relaxations

26 points into 5 groups of 5, 5, 5, 5 and 6 (02-477). Point `i`'s cost `z_i`
in [0, 20] sits in one row per group,
`z_i >= sum_k a_ik x_kg - 5 (1 - x_ig)`, so it is the largest of five whole
numbers; optimum 212. `relaxations.txt`:

| relaxation | bound |
|---|---|
| the LP | 183.36 |
| `rlt.py same`: products `x_ig x_kg` within each group, `z_i x_ig`, point `i`'s row for `g` times `x_ig` and `1 - x_ig`, the size row of `g` times `x_ig` | 183.93 |
| `rlt.py full`: every product `x_ig x_kh`, every point row times each `x_ig` and `1 - x_ig`, the assignment and size rows times each `x_ig` | 184.29 |
| `rlt.py tri`: that, with the triangle rows over `y_ik = sum_g x_ig x_kg` | 184.53 |
| `zonly.py`: the `z` integer and the `x` continuous, solved at HiGHS's root | 193 |
| the optimum | 212 |

The products of the columns, the usual strengthening of a partitioning
model, move the LP by 1.2 of its gap of 28.6. The integrality of the 26
costs alone moves it by 9.6: each `z_i` is a small whole number above a
fractional row. The 19 left need the `x` whole as well, which is the tree.

## SCIP

`scip-root.txt`, `scip-root-arms.txt`: SCIP's root reads 186.35 with the
`z` as implied integers and as continuous alike. With them declared integer
it reads 194.33: 192.88 without any separator, 191.85 without root strong
branching, 191.24 without presolve, 191.88 without strong CG cuts, 192.97
without Gomory. SCIP's presolve adds three times each group's size row to
its point rows (coefficients `3 - a_ik`, side 10) and tightens the `z` to
bounds like [1, 18], in both forms. JAOS on SCIP's presolved model
(`sparse.mps`) takes cheaper nodes, 58,857 at 1e10 against 37,202, and reads
196.35 at 20,000 nodes against 196.16.

`scip-tree.txt`, the bound at 50,000 nodes:

| SCIP setting | bound |
|---|---|
| default | 199.05 |
| best-bound node selection | 202.84 |
| the `z` declared integer (`zint.mps`) | 202.03 |
| the same, branching on the `x` first | 201.25 |
| no implied integers (linear dual presolve off) | 191.36 |
| no symmetry | 192.97 |
| neither | 191.25 |
| orbital reduction and lexicographic reduction only | 198.68 |
| symmetry constraints only (orbitopes, symresacks) | 199.08 |
| no orbitopes | 199.05 |
| no node cuts | 196.79 |
| no Gomory | 196.99 |
| no zero-half | 198.23 |
| no c-MIR | 198.98 |
| no propagation, symmetry's with it | 192.51 |
| no reduced-cost or pseudo-objective propagation | 199.38, 199.72 |
| no conflict analysis | 199.42 |
| pseudocost branching in place of reliability | 194.80 |
| an objective limit of 211.5, no implied integers | 191.48 |

Without implied integers SCIP's symmetry gives nothing (191.36 against
191.25), and an objective limit standing in for the integral objective
gives nothing either, so that arm measures the lost symmetry more than the
integers. What SCIP's 199.05 rests on is its symmetry (6.1), reliability
branching (4.2) and its node cuts (2.3, Gomory 2.1). Declared integer, the
costs give SCIP 3.0 more at 50,000 nodes, and not by branching on them:
with the `x` first it reads 201.25.

## JAOS

`jaos-arms.txt`: reading the 26 `z` as integer in the Gomory rounds (bit 1)
takes the root from 185.26 to 189.80, the same as with them declared
integer; in the root's other separators (bit 2) it changes nothing; in node
MIR (bit 4) it lifts the bound per node a little and stops `pk1` from
closing (`census.txt`: bound 10 after 232,430 nodes at 1e10). With the `z`
declared integer JAOS reads 196.24 at 20,000 nodes against 196.16 with bit
1: branching on them gives JAOS nothing either.

`spread.txt`, the open bound at 1e10 over 02-474's five null readings
(`JAOS_NULL` 0 to 600 times the model's size added at the root):

| arm | bound | nodes |
|---|---|---|
| b53a1ffd | 195.90 to 196.33 | 48,390 to 52,235 |
| the Gomory rounds with the implied integers (bit 1) | 196.84 to 197.20 | 30,456 to 37,202 |
| bit 1 and five root rounds | 197.19 to 198.04 | 30,952 to 33,902 |
| the change: bit 1 and integral slacks on the model rows that hold an implied column | 197.61 to 198.07 | 33,637 to 42,551 |
| the change and five root rounds | 197.38 to 197.52 | 38,551 to 42,551 |
| the change, the slacks of cut rows too (`JAOS_CUTROWS`) | 197.70 to 198.35 | 33,202 to 47,642 |
| the same and five root rounds | 197.47 to 197.53 | 41,723 to 42,846 |
| bit 1, integral slacks on every all-integer row (`JAOS_GISL=1`) | 197.03 to 197.62 | 42,877 to 52,915 |
| the same and five root rounds | 198.66 to 199.04 | 37,602 to 41,677 |

The change moves the mean of the five by 1.8, and no reading of it falls
inside the nulls' range. The strongest arm, every all-integer row's slack
integral and five rounds, is 02-364's two refused ideas
(`gomory-integral-slacks`, `gomory-rounds-while-lifting`) over the implied
integers; its gain comes from rows that hold no implied column, the
assignment and size rows and the cuts with whole coefficients. At about
40,000 nodes the change sits some 0.6 under SCIP's default tree read at the
same count between its 20,000 and 50,000; best-bound selection reads 197.44
with bit 1 in 16,911 nodes. With `--no-orbital` bit 1 reads 193.96 in 6,363
nodes.

## Out of reach

`jaos-impl1-1e11.txt`, bit 1 at 1e11: 197.17 at 50,000 nodes, 198.01 at
100,000, 198.93 at 200,000, 199.92 at 400,000, 200.25 at 500,000, 580,000
nodes in all. That is 0.9 a doubling of the nodes. SCIP's default gains 1.3
a doubling from 199.05 at 50,000 to 205.0 at 1.12M (02-477). Closing needs
a bound past 211: about 2e9 nodes at JAOS's rate and 2.5e7 at SCIP's,
against the 40,000 that 1e10 buys. No lever measured here lifts the bound
at 1e10 by more than 3.1, the products of the columns gain 1.2 at the root
and the costs' integrality 9.6; the rest is the tree's. Out of reach by a
factor of 600 or more.

## The change

`change.patch`:
- `integral_continuous` marks the columns it counts, the continuous columns
  that take whole values at an optimum given the integers (02-475's network
  transpose with whole sides and bounds);
- the tree passes to its Gomory rounds, at the root and at the nodes, an
  integrality that holds those columns, and the rows of the model that
  hold one of them and only integer columns besides, with whole
  coefficients, whose slack is then integer at any such point;
- the scan costs the model's size in work, once.

A cut may then cut off a point where such a column is fractional; it never
cuts off every optimum of a node, since at fixed integers the column's
rows give a whole optimum (Hoffman and Kruskal). The summary log says so
after the objective step's line.

Two of `tests/test_mip.c`'s heuristic tests compared the tree with and
without the heuristic: `time_indexed_jobs` now closes at its root with no
heuristic and `one_machine` takes 2,969 nodes without its moves against
3,011 with them, both from the stronger cuts, so the two node comparisons
are gone; `one_machine` keeps its check of the first incumbent's node.

MIPLIB 3 (`miplib3/land.txt`, `m3cmp.txt`): geometric 0.999x, total 0.990x;
only `misc07` moves, 8,228 to 7,615 nodes, 0.965x.

MIPLIB 2017 at 1e10 (`miplib2017/land.txt`, `m17cmp.txt`, `sum.txt`):

| arm | solved | primal | dual | sum |
|---|---|---|---|---|
| before (`bench/results/miplib2017.txt`) | 9 | 5.4067 | 6.3032 | 11.7099 |
| bit 1 | 9 | 4.4067 | 6.2985 | 10.7051 |
| the change | 9 | 5.4067 | 6.2880 | 11.6947 |

23 of the 30 read as the record node for node; the seven that move are the
ones with implied integers:

| instance | the record | the change |
|---|---|---|
| `assign1-5-8` | bound 196, dual 0.0755 | 198, 0.0660 |
| `csched007` | bound 304 | 306 |
| `pk1` | optimal, 7.49e9, 174,316 nodes | optimal, 7.71e9, 177,914 nodes |
| `csched008` | 175 and 171 | the same, 92 nodes against 42 |
| `ic97_potential` | no incumbent, 3869 | the same |
| `markshare_4_0` | incumbent 3 | the same |
| `markshare2` | incumbent 80 | 137, inside the readings of 02-474 and 02-475 (85 to 376) |

Bit 1 alone found `markshare_4_0`'s optimum, as three of 02-474's nulls do,
hence its primal side. `pk1`, `mas76`, `binkar10_1` and `gen-ip054` still
close.

## On top of 02-479

These readings were taken on b53a1ff. The change landed after 02-479's
dual Devex at the nodes, so both sets were taken again on the landed
code, one solve at a time (`combined/miplib3.txt`,
`combined/miplib2017.txt`, against `combined/before.txt`, 02-479's
record). MIPLIB 3 reads its baseline but `misc07`. MIPLIB 2017 solves the
same 9 of 30 and its gap sum goes from 11.6464 to 11.6179: `pk1` closes
at 7.42e9 work units against 8.95e9, `csched007`'s incumbent goes from
398 to 389 and its bound from 305 to 306, and `assign1-5-8` reads 198,
as Devex alone reads it.

## The verdict

Landed, as `landing.patch`. `assign1-5-8` does not close, and cannot at
1e10 by any lever measured: its gap is the tree's, not a relaxation's, and
the tree would need some 600 times the nodes even at SCIP's rate. The lever
TODO named, the implied integers in the cuts, is the largest JAOS had:
with the slacks of their rows, the bound at 1e10 goes from 195.9–196.3 to
197.6–198.1 and the dual gap from 0.0755 to 0.0660; `csched007`'s bound
from 304 to 306. Nothing else moves outside the implied-integer models.
MIPLIB 3 reads 0.990x in total.

Refused: node MIR over the implied rows (`node-mir-implied-rows`, `pk1`)
and Gomory rounds past the first over the change (`gomory-rounds-implied`).
Next for `assign1-5-8`: integral slacks on the all-integer equality rows
with rounds past the first, the 198.7–199.0 arm, in a form that keeps
`bell5`'s tree; then the probes of reliability branching, 4.2 of SCIP's
bound at 50,000 nodes. Neither closes it.

`make test` and `make sanitize` pass on the change. MIPLIB 3 was read with
the change's `bench/run` against `bench/miplib.baseline` at `-j 1`, not
through `make miplib`; the LP sets (`lp-gates.txt`, every instance as its
baseline) likewise, at `-j 2`. HiGHS was not run again: its 600 s reading
is 02-477's.
