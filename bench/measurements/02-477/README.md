# 02-477 — neos5 and assign1-5-8: out of reach, and the symmetry the search missed

Taken on 2026-11-17 on 4dc8e97, for TODO row J7's `neos5` and
`assign1-5-8`. `read.sh` takes every reading one solve at a time, from
copies of 4dc8e97 with `sym.patch` (the root's cap raised to 1000, the
first form), `change.patch` (the change; `landing.patch` is it with its
tests and docs) and `symonly.patch` (prints what the symmetry search
spent, then stops):
- `highs-*.txt`, `scip-*.txt`: HiGHS 1.15.1 and SCIP 10.0 on both, one
  thread, 600 s (SCIP's progress lines kept one in two hundred);
  `scip-root-assign1-5-8.txt` and `scip-tree-assign1-5-8.txt`, SCIP's
  separators and components taken away one at a time;
- `jaos-root-*.txt`, `jaos-tree-*.txt`: JAOS's root and its tree at 1e10;
- `symsearch.txt`: the search's spend on both sets under three caps;
- `jaos-arms.txt`: `assign1-5-8` and `neos-911970` under the arms below;
- `miplib3/*.txt`, `miplib2017/*.txt`: both forms on both sets, and
  `m3cmp.txt`, `m17cmp.txt`, the per-instance tables of 02-474's scripts;
- `lp-gates.txt`: the netlib, infeasible netlib and Kennington sets
  under the change.

Both instances hold their optimum as incumbent at 1e10, and neither
closes in 1e10 work units. In 600 s HiGHS closes `neos5` and not
`assign1-5-8`; SCIP closes neither.

## neos5

63 rows `sum x >= b` with every coefficient 1, 53 binaries and 10
continuous columns in [0, 1], every cost 1, optimum 15. Read column `d_j`
as the six bits `y` of 64 - j and row `f` as six bits too: the row holds
the columns with `f.y` odd, every nonzero `y` and `f` appears, each row
has 32 columns, and its side is 9 minus the weight of `f`. The ten continuous
columns break the coordinate permutations; the three solvers find 2
generators. The continuous block is not totally unimodular (968 of its
1023 column sets have no equitable two-colouring), and points at 15.25 and
15.5 exist, so the objective is not integral and a bound of 14.x does not
round up.

| solver | root | bound by node | the end |
|---|---|---|---|
| JAOS | 13.23 | 14.0 at 13,100, the same at 33,482 (1e10) | 14.375 at 253,203 (5e10, `02-465/`) |
| HiGHS | 13.32 | 14.0 at 23,030 | optimal at 206,031 nodes, 69 s |
| SCIP | 13.46 | 14.04 at 65,800, 14.20 at 253,200 | 14.75 at 2.48M nodes, 600 s |

JAOS's tree is no worse per node than SCIP's, and HiGHS's tree is the
smallest. What JAOS lacks is nodes: 1e10 buys 33,482 at 3.0e5 work units
each, 88.6% of it in node relaxations, at 37.8 LP iterations a solve
against HiGHS's 12.4 and SCIP's 9.2 a node. Closing in HiGHS's 206,031
nodes would take 4.9e4 work units a node, a sixth of today's. Out of
reach as one change.

## assign1-5-8

26 points into 5 groups of 5, 5, 5, 5 and 6. Point `i` has a cost column
`z_i` in [0, 20] and, for each group `g`, the row
`z_i >= sum_{k != i} a_ik x_kg - 5 (1 - x_ig)` over symmetric weights 0 to
4; optimum 212.

| solver | root | bound at about 20,000 nodes | at about 45,000 | the end |
|---|---|---|---|---|
| JAOS, 4dc8e97 | 185.26 | 192.79 at 18,426 (1e10) | | |
| JAOS, the change | 185.26 | 194.24 | 196 at 44,794 (1e10) | |
| HiGHS | 193.77, 196.11 after a restart | 196.11 | 196.23 | 200.03 at 427,093, 600 s |
| SCIP | 186.35 | 197.12 | 198.87 | 205.01 at 1.12M, 600 s |

`scip-tree-assign1-5-8.txt`, SCIP's bound at 20,000 nodes:

| SCIP setting | bound |
|---|---|
| default | 197.12 |
| no conflict analysis | 198.00 |
| no restart | 197.12 |
| no cut rounds below the root | 195.29 |
| no symmetry | 191.62 |
| no propagation, the symmetry propagator with it | 191.10 |
| none of those | 189.51 |

SCIP's separators move its root by nothing (186.33 to 186.35 in every arm
of `scip-root-assign1-5-8.txt`). Symmetry is what SCIP's tree needs most:
12 generators, the exchanges of the four groups of five and of twin
points, 15 points in 6 sets of equal weights, handled by orbital and
lexicographic reduction.

JAOS's search found 5 of those generators, exchanges of twins in 3 of the
6 sets and none of the groups. It runs out of
its cap, 250 times the model's size, at 1.01e6 work units, on the deep
levels of its first path, before it reaches the levels where the groups
are exchanged; with 719 times the size it finds all 12 (`symsearch.txt`).
`jaos-arms.txt`, at 1e10:

| arm | nodes | bound |
|---|---|---|
| 4dc8e97 | 18,426 | 192.79 |
| the whole group (`sym.patch`) | 46,613 | 195.57 |
| the same, no orbital branching | 14,598 | 192.08 |
| the same, best-bound node selection | 18,962 | 197.28 |
| the same, Gomory at every node | 6,075 | 193.89 |
| the same, reliability 8 | 47,451 | 195.55 |
| the same, 4 zero-half rounds | 46,402 | 195.50 |
| the same, with `z` declared integer (`zint.py`) | 34,079 | 197, root 189.80 |
| the change | 44,794 | 196 |
| `neos-911970`, 4dc8e97 | 2,645 | 51.927 |
| `neos-911970`, the whole group, 17 generators against 6 | 1,777 | 51.960 |
| `neos-911970`, the change | 1,722 | 51.957 |

HiGHS and SCIP read the 26 `z` as implied integers: each sits in rows
whose other columns are binary with whole coefficients and sides, so an
optimum takes it whole, and the objective, all on such columns, is
integral. JAOS rounded a bound to the objective's grid only when every
costed column is integer.

Even at SCIP's bound per node, `assign1-5-8` needs more than SCIP's
1.12M nodes, 24 times what 1e10 buys JAOS. Out of reach.

## Where the search runs out

`symsearch.txt`: of the 54 models, nine run out at 250. `air03`,
`misc06`, `assign1-5-8` and `neos-911970` finish at 423, 490, 719 and 938
times their size and find 13, 12, 12 and 17 generators against 9, 7, 5
and 6. `neos-3046615-murg`, `sp150x300d`, `neos-3627168-kasai`,
`neos-3381206-awhea` and `beasleyC3` run out at 4000 too; the first path
alone takes the cap.

The first form, `sym.patch`, raises the root's cap to 1000. MIPLIB 3
(`miplib3/sym.txt`): geometric 1.015x, total 1.002x, `air03` 1.31x, which
closes at its root and pays 1.76e7 more for the search, and `misc06`
1.09x, 50 to 46 nodes. MIPLIB 2017 (`miplib2017/sym.txt`) reads 10.9373
against the record's 10.8936; 25 of the 30 read as the record node for
node, and:

| instance | the record, primal and dual | `sym.patch` | 02-474's null readings |
|---|---|---|---|
| `assign1-5-8` | 0, 0.0906 | 0, 0.0775 | bound 192.44 to 192.97 |
| `neos-911970` | 0.0037, 0.0517 | 0.0037, 0.0511 | bound 51.927 to 51.936 |
| `neos-3046615-murg` | 0.0631, 0.7119 | 0.0731, 0.7262 | incumbent 1668 to 1727, bound 443 to 464 |
| `neos-3627168-kasai` | 0.0197, 0.0281 | 0.0192, 0.0286 | inside |
| `beasleyC3` | 0.0491, 0.0159 | 0.0822, 0.0159 | incumbent 780 to 823 |

`murg`'s three new generators move no column, so what moves it is the
work; its bound, 438, sits 5 under the nulls' lowest.

## The change

`change.patch`:
- The root's search keeps its cap, `MIP_SYMMETRY_WORK` 250. When it runs
  out and the tree is about to make its first branch, the search runs
  again under `MIP_SYMMETRY_MORE`, 1000 times the size, and its group
  replaces the first. A model the root closes pays nothing more, as
  `air03`. The second search runs before any branch, so every node of the
  tree reads one group.
- The objective step accepts a costed continuous column with whole bounds
  whose rows hold only integer columns besides it, with every
  coefficient and side a whole multiple of its own (to `MIP_OBJ_FIT`,
  absolutely): given the integers, its interval has whole ends, and an
  optimum takes one. The check's reads are charged as work. The summary
  log says how many such columns the step took.

MIPLIB 3 (`miplib3/land.txt`): geometric 1.009x, total 1.007x, no
instance past 2x. Two trees move: `misc06` 1.225x, the second search on a
tree of the same 50 nodes, and `misc07` 1.021x, 8,617 to 8,663 nodes, now
on a grid; with the check left uncharged it reads 8,631, so its tree moves
with the check's few work units as well as with the grid. `air03` reads
its baseline.

MIPLIB 2017 at 1e10 (`miplib2017/land.txt`), 02-328's `m17sum.py`:

| arm | solved | primal | dual | sum |
|---|---|---|---|---|
| before (the record) | 6 | 4.4435 | 6.4500 | 10.8936 |
| `sym.patch` | 6 | 4.4861 | 6.4512 | 10.9373 |
| the change | 6 | 4.4494 | 6.4380 | 10.8874 |

22 of the 30 read as the record node for node. Of the other eight,
`markshare_4_0`, `markshare2` and `glass4` end at other nodes with the
same gaps, and:

| instance | the record, primal and dual | the change | 02-474's null readings |
|---|---|---|---|
| `assign1-5-8` | 0, 0.0906 | 0, 0.0755 (bound 196) | bound 192.44 to 192.97 |
| `neos-911970` | 0.0037, 0.0517 | 0.0037, 0.0512 (bound 51.957) | bound 51.927 to 51.936 |
| `neos-3046615-murg` | 0.0631, 0.7119 | 0.0394, 0.7150 (1663, 456) | incumbent 1668 to 1727, bound 443 to 464 |
| `neos-3627168-kasai` | 0.0197, 0.0281 | 0.0189, 0.0286 | inside |
| `beasleyC3` | 0.0491, 0.0159 | 0.0796, 0.0159 (814) | incumbent 780 to 823 |

The two targets of the group move past the nulls; `murg`, `kasai` and
`beasleyC3`, whose searches run out at every cap, move with the work, as
the nulls do.

## The verdict

Landed, as `landing.patch`. Neither `neos5` nor `assign1-5-8` closes, and
neither can at 1e10 by one change: `neos5` needs about six times the
nodes 1e10 buys, `assign1-5-8` more than 24 times even at SCIP's bound
per node. What the evidence pointed to on `assign1-5-8`, the group SCIP's
tree leans on, JAOS's search had missed; the change finds it, and with
the cost columns read as whole its dual gap goes from 0.0906 to 0.0755,
and `neos-911970`'s bound from 51.927 to 51.957. MIPLIB 2017's sum reads
10.8874 against 10.8936. MIPLIB 3 reads 1.007x in total, 0.7% worse, from
`misc06`'s second search and `misc07`'s tree at other nodes; no instance
passes 1.23x. The first form, the root's cap raised, is refused as
`symmetry-root-cap`.

`make test` and `make sanitize` pass on the change. The netlib,
infeasible netlib and Kennington sets were read with the change's
`bench/run` against their baselines (`lp-gates.txt`); MIPLIB 3 was read
with the same runner against `bench/miplib.baseline` at `-j 1`, not
through `make miplib`.
