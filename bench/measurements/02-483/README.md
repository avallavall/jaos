# 02-483 — assign1-5-8: Gomory rounds past the first on the models with implied integers

Taken on 2026-11-18 on 1235543a, for TODO row J7's `assign1-5-8`.
`read.sh` takes every reading from copies of 1235543a with
`prototype.patch` (its switches are listed in the script), `change.patch`
(the change; `landing.patch` is it with its tests and docs) and
`null.patch` (the change with 02-474's null readings, `JAOS_NULL` 0 to
600 times the model's size added at the root):
- `assign.txt`: `assign1-5-8` at 1e10 under every arm, five null readings
  each;
- `bell5.txt`, `misc07.txt`: the same for the two models the arms grow;
- `census.txt`: the models with implied integers at 1e10; `grid.txt`,
  which of the 54 models have an objective grid and implied integers;
- `roots.txt`: the root round by round, its lift and the nonzeros of its
  cuts;
- `change-1e11.txt`: `assign1-5-8` under the change at 1e11;
- `miplib3/*.txt`, `m3cmp.txt`: MIPLIB 3 under the arms and the change;
- `miplib2017/change.txt`, `m17cmp.txt`, `sum.txt`: the 2017 set at 1e10;
- `lp-gates.txt`: the netlib, infeasible netlib and Kennington sets.

TODO named the step: integral slacks on the all-integer equality rows,
with Gomory rounds past the first, in a form that keeps `bell5`'s tree.
The two pieces were read apart.

## The equality rows' slacks

An equality row's slack is fixed at zero. Its term in a Gomory cut,
whatever its coefficient, multiplies a quantity that is zero at every
point of every node's relaxation, so the integer coefficient, the
continuous one and none at all give the same cut on the relaxation; they
differ only in the cut's written form once the row is substituted back:
its coefficients on the columns, hence its dynamism, its efficacy (a node
keeps its four most efficacious cuts) and which cuts `cut_finish` keeps.
The integral slack of an equality row adds no strength, only a different
draw of cuts.

On `assign1-5-8` (`assign.txt`, the bound at 1e10, five null readings):

| arm | root | bound | nodes |
|---|---|---|---|
| 1235543a | 189.80 | 197.32 to 197.52 | 58,435 to 59,849 |
| the equality rows' slacks integral (`JAOS_GE=1`) | 189.80 | 197.69 to 198.22 | 59,974 to 61,710 |
| the same and the cut rows' (`JAOS_GE=7`) | 189.80 | 197.71 to 198.17 | 57,074 to 62,955 |
| the equality rows' slacks left out of the cut (`JAOS_GE=16`) | 189.80 | 197.13 to 197.55 | 50,176 to 53,948 |
| five rounds (`--cut-rounds 5`) | 192.13 | 197.88 to 198.55 | 56,452 to 58,123 |
| five rounds, the equality slacks integral | 192.13 | 198.89 to 199.37 | 44,748 to 53,507 |
| five rounds, the same and the cut rows' | 192.13 | 199.15 to 199.66 | 43,470 to 54,172 |
| five rounds, the equality slacks left out | 192.14 | 197.67 to 197.92 | 22,937 to 26,003 |
| three rounds, the equality slacks integral | 191.52 | 198.39 to 198.62 | 60,562 to 64,806 |
| ten rounds, the equality slacks integral | 192.30 | 199.11 to 199.57 | 26,044 to 31,970 |

`assign1-5-8`'s equality rows are all integer, so integral slacks on every
equality row (`JAOS_GE=32`) read as `JAOS_GE=1` there. The draw helps
`assign1-5-8` and is a lottery elsewhere (`miplib3/`, `m3cmp.txt`):

| arm | MIPLIB 3 geometric | total | moves past 1.3x |
|---|---|---|---|
| the all-integer equality rows' slacks integral, every model (`JAOS_GE=9`) | 1.121x | 1.171x | `enigma` 46.5x (1 node to 24,663), `misc07` 1.64x |
| every equality row's slack integral (`JAOS_GE=32`) | 1.107x | 1.172x | `enigma` 14.2x, `misc07` 1.64x |
| the equality rows' slacks left out (`JAOS_GE=16`) | 0.957x | 0.935x | `misc03` 1.37x |
| every all-integer row's slack integral (`JAOS_GE=11`, `gomory-integral-slacks` over the implied integers) | 1.091x | 0.846x | `enigma` 46.5x, `gt2` 3.86x |

On the models with implied integers alone, where `JAOS_GE=1` acts,
`misc07` grows from 8,375–9,915 nodes over five null readings to
13,133–15,098 (`misc07.txt`), 1.45 times the work, and `misc03` shrinks
to 0.75x. Refused as `gomory-equality-slacks`.

## Why bell5 changed

At 1235543a the integral slacks do not grow `bell5` (`bell5.txt`): every
all-integer row's slack integral reads 15,313 to 19,455 nodes over the
five nulls (`JAOS_GE=10`, `11`, `15`), against 14,073 to 27,699 for
1235543a. What grows it is the rounds past the first. Its second round
adds 4 Gomory cuts that lift the root by 875 of 8.65e6, its third 2 and
its fourth 1 (`roots.txt`); with two rounds `bell5` takes 194,705 nodes,
7.5 times the work, and with five it is open at 1e10 after 913,461 nodes,
930,717 with node MIR off. Its trees are a lottery over the path: given
the optimum as a cutoff from the start, the head and two rounds both take
500,000 to 525,000 nodes. So the form that keeps `bell5`'s tree is one
that leaves its root alone: `bell5` has no implied integers.

## Rounds past the first

On every model, two rounds read 1.288x on MIPLIB 3 and five 1.966x
(`m3cmp.txt`): `bell5` 7.5x and 68x, `gt2` 5.5x, `p0201` 2.6x and 5.8x,
`misc07` 2.0x and 1.7x. They run here only on the models whose objective
step finds implied integers, the ones 02-480's Gomory form reads, and,
as the MIR rounds past `MIP_MIR_ROUNDS` do, only while the round before
lifted the bound by `MIP_MIR_MORE_STALL`'s 1e-4. `misc03` and `misc07`,
the two such models in MIPLIB 3, and `pk1`, `csched008` and the two
`markshare` lift nothing in their first round, so their roots stay as
they were.

`roots.txt`, under the stall alone (`JAOS_GM=20`):

| model | lift a round, as a share of 1 + \|bound\| | the cuts' nonzeros a round, times the model's |
|---|---|---|
| `assign1-5-8` | 3.5e-2, 5.7e-3, 3.3e-3, 1.3e-3, 1.8e-3, 4.5e-4, 3.1e-4, 8.6e-5 | 4.9, 4.2, 4.2, 3.7, 3.6, 3.5, 3.9, 3.9 |
| `csched007` | 5.8e-2, 9.6e-3, 1.8e-2, 4.5e-3, 2.9e-3, ... 6.1e-5 at the eleventh | 18, 23, 24, 24, 36, 37, 40, 34, 41, 33, 33 |
| `ic97_potential` | 1.2e-4, 5.4e-4, 1.0e-3, 5.5e-4, 0 | 0.6, 2.1, 4.9, 7.0, 4.5 |

`csched007`'s cuts are the heavy ones: with five rounds a node costs 2.8
times as much and its bound at 1e10 falls from 306 to 302, and under the
stall alone its root spends the whole 1e10 and reads 296 (`census.txt`).
A cap on the cuts' total does not fit both: `assign1-5-8`'s rounds sum to
32 times its nonzeros, `csched007`'s first round alone holds 18. A cap on
a round's own cuts does: 10 times the model's nonzeros leaves
`assign1-5-8` to its stall and stops `csched007` after its first round.

The number of rounds is a draw too (`assign.txt`, no integral slacks): a
cap on the total at 10 stops `assign1-5-8` after three rounds and reads
198.26 to 198.36, at 15 after four and reads 196.77 to 196.93, below the
head, and five read 197.88 to 198.55. Run to the stall, eight rounds,
it reads 199.04 to 199.36; with the equality slacks integral as well,
199.11 to 199.57.

## The change

`change.patch`: on a model whose objective step finds implied integers,
unless the rounds are set, the root's Gomory rounds go past
`MIP_CUT_ROUNDS`, up to `MIP_GOMORY_MORE` (20, `MIP_MIR_MORE`'s value),
while the round before lifted the bound by `MIP_GOMORY_MORE_STALL` (1e-4)
of 1 + |bound| and its cuts held at most `MIP_GOMORY_MORE_NZ` (10) times
the model's nonzeros. The summary log says so and gives the rounds.

`assign1-5-8` (`assign.txt`, `census.txt`):

| | 1235543a | the change |
|---|---|---|
| root | 189.80, 149 cuts | 192.30 after 8 rounds, 803 cuts |
| bound at 20,000 nodes | 196.13 to 196.27 | 198.47 to 198.99 |
| bound at 1e10 | 197.32 to 197.52 | 199.04 to 199.36 |
| nodes at 1e10 | 58,435 to 59,849 | 26,342 to 39,812 |

SCIP reads 197.12 at 20,000 nodes and 199.05 at 50,000 (02-477, 02-480),
so the bound per node now leads SCIP's by some 1.6 at 20,000. The root
spends 2.2e9 work units against 3.6e8, its heuristics' share with it.

At 1e11 (`change-1e11.txt`) the bound reads 199.71 at 50,000 nodes,
200.50 at 100,000, 201.41 at 200,000, 202.39 at 400,000 and 202.93 at
570,000, against 197.17, 198.01, 198.93, 199.92 and 200.25 at 500,000 for
02-480's first Gomory form on b53a1ffd: some 2.5 higher at every count,
and still 0.9 a doubling of the nodes, against SCIP's 1.3. Past 211 takes
about 2e8 nodes at that rate, against the 30,000 that 1e10 buys, so
`assign1-5-8` stays out of reach.

The census (`census.txt`, at 1e10):

| model | 1235543a | the change |
|---|---|---|
| `ic97_potential` | root 3868.46, bound 3869 in 22,102 nodes | root 3876.72 after 5 rounds, bound 3877 in 487 nodes |
| `csched007` | bound 306, incumbent 389, 6,832 nodes | bound 306, incumbent 405, 6,569 nodes |
| `pk1` | optimal at 7.42e9, 181,251 nodes | optimal at 7.88e9, 191,637 nodes |
| `misc03` | 307 nodes, 4.40e7 | 335 nodes, 4.32e7 |
| `misc07` | 8,375 nodes, 1.60e9 | 8,169 nodes, 1.58e9 |
| `markshare_4_0` | 617,263 nodes | 613,261 nodes, the same gap |
| `csched008`, `markshare2` | | as the head |

`pk1`, `misc03`, `misc07` and `markshare_4_0` lift nothing in their first
round and `csched007` stops after it; they move through their sub-MIPs,
whose models carry implied integers of their own.

MIPLIB 3 (`miplib3/change.txt`, `m3cmp.txt`, through the change's
`bench/run` against `bench/miplib.baseline` at `-j 2`): every instance
solved, none regressed, geometric 0.999x, total 0.996x; `misc03` 0.982x
and `misc07` 0.986x move, `bell5` reads its baseline node for node.

MIPLIB 2017 at 1e10 (`miplib2017/change.txt`, `m17cmp.txt`, `sum.txt`,
through the change's `bench/run` at `-j 2`):

| | solved | primal | dual | sum |
|---|---|---|---|---|
| the record (`bench/results/miplib2017.txt`) | 9 | 5.4828 | 6.1347 | 11.6175 |
| the change | 9 | 5.5284 | 6.1232 | 11.6517 |

25 of the 30 read as the record node for node; the five that move are
among the seven with implied integers:

| instance | the record | the change |
|---|---|---|
| `assign1-5-8` | bound 198, dual 0.0660, 58,435 nodes | 200, 0.0566, 27,985 |
| `ic97_potential` | bound 3869, dual 0.0185, 22,102 nodes | 3877, 0.0165, 487 |
| `csched007` | incumbent 389, bound 306, 6,832 nodes | 405, 306, 6,569 |
| `pk1` | optimal at 7.42e9, 181,251 nodes | optimal at 7.88e9, 191,637 |
| `markshare_4_0` | incumbent 2, 617,263 nodes | the same, 613,261 |
| `csched008`, `markshare2` | | as the record |

`csched007`'s root is the record's; its tree moves through its sub-MIPs,
and its incumbent reads 405 as in 02-474's five null readings, 02-477 and
02-480, where 02-479 read 398 and the record 389. That is all of the sum's
rise, 0.046 on the primal side against 0.012 gained on the dual, inside
02-474's spread of 10.85 to 11.88.

The LP sets (`lp-gates.txt`, through the change's `bench/run` at `-j 6`,
Kennington at `-j 2`): netlib 94 of 94, infeasible netlib 29 of 29 and
Kennington 16 of 16, each as its baseline, none regressed.

`make test`, `make sanitize` and `make python-test` pass on the change.

## The verdict

Landed, as `landing.patch`. `assign1-5-8` does not close and cannot at
1e10, but its bound per node moves past SCIP's: at 1e10 from 197.32–197.52
to 199.04–199.36 over five null readings, at 20,000 nodes from 196.1–196.3
to 198.5–199.0 against SCIP's 197.12. `ic97_potential`'s root goes from
3868.5 to 3876.7. MIPLIB 3 reads 0.996x in total, `bell5` node for node.

The step TODO named came apart. The integral slacks of the all-integer
equality rows rewrite a cut without strengthening it, help
`assign1-5-8` by a draw and grow `misc07`; refused
(`gomory-equality-slacks`), with `gomory-integral-slacks` and
`gomory-rounds-while-lifting` brought up to date and
`gomory-rounds-implied` retired, its rounds having landed in this form.
Leaving the equality rows' slacks out of the cut read 0.957x on MIPLIB 3
in one reading and nothing on `assign1-5-8`; not taken here. Next for
`assign1-5-8`: reliability branching's probes, 4.2 of SCIP's bound at
50,000 nodes.
