# 02-500 — dual proofs from stopped node relaxations close `neos5`

Taken on 2026-11-19 for TODO row J7's three MIPLIB 2017 instances that hold
the optimum and lack only the bound: `gen-ip002`, `neos5`, `assign1-5-8`.
The readings ran on copies of 97541e88 (`head`) and of the change at its
stages; the gates ran on the change rebased on ab51180f. One thread, work
units throughout. The folder:
- `readings-all.txt`: every single-instance reading, one line each, named
  as in the tables below;
- `miplib3/*.txt`, `miplib2017/*.txt`: the sets under the arms, as
  `bench/run` writes them; `miplib3-null/` the four null readings of the
  change and of 87475d8a, compared by `nullcmp.py` in `cmp.txt`;
- `shares.txt`: the share of integer columns in orbits on every model with
  symmetries; `read.sh` takes the main readings again;
- `prototype/`: the edits the arms were built from, applied in order on
  02-482's `prototype.patch` (`colprop.py`, `colprop2.py`, `rules.py`,
  `rules2_tail.py`, `prealways.py`, `nullw.py`), the model edits of the
  objective readings (`n5var.py`, `fixb.py`), the Gomory log (`gomlog.py`,
  `glog-*.txt`) and the covering generator of the tests (`mini.py`).

The builds the reading names point at: `head` is 97541e88; `proof` is
02-482's `prototype.patch` on it (its `JAOS_SC` bits; 4249 adds the column
index), `pcol` adds `colprop.py`, `pcol2` (the `w*` and `null*` arms)
`colprop2.py`, `*pre` `prealways.py`, `prule`/`prule2` (`r*`, `q*`) the
rules; `land` to `land6` are the change at its stages, `land` the first
form, `land2` with the worklist, `land3` with the symmetry share, `land4`
and `land5` with the two cost tests below, `land6` with the proofs' bounds
written ahead of the branching bound; `land7` is that rebased on
87475d8a, `land8` with the integer bounds a proof needs read by its signs,
and `land9` with the share uncharged (below); the change is `land9` rebased
on ab51180f.

`gen-ip002` and `assign1-5-8` stay out of reach (below). `neos5` was the
one with a reachable target: HiGHS closes it in 110,582 to 206,031 nodes
(02-481), and 02-482's dual proofs had already taken JAOS's tree from
2,977,069 nodes to 1,264,193.

## `neos5`'s objective has no step

`neos5` minimises the sum of 63 columns in [0, 1], 53 of them integer, over
63 covering rows `Σ_{T: |S∩T| odd} x_T >= 9 - |S|`, one row and one column
for each nonempty subset of {1..6}. Its optimum 15 needs the ten continuous
columns at fractions: with every column integer (`n5var.py allint`) the tree
holds bound 15 and incumbent 16 at 5e10 work units after 852,689 nodes. So
no objective step closes it at the record's bound 14.25, and an integer
column for the integer part of the objective, which the tree may branch on
(`n5var.py w`), closes it at 1.55e11 in 3,259,341 nodes against the record's
1.91e11 in 2,977,069 (`w-close`).

## The prototype to the close

02-482's `prototype.patch` (`JAOS_SC=153`, dual proofs propagated by
activity) with the column-indexed propagation of `colprop.py` (bit 4096)
and its worklist (`colprop2.py`). Every line closes at 15.

| arm | nodes | work units |
|---|---|---|
| the record | 2,977,069 | 1.91e11 |
| cut depth 0, no proofs (02-481) | 1,496,753 | 6.90e10 |
| 75 proofs (02-482) | 1,264,193 | 7.38e10 |
| 75 proofs, cut depth 0 (`proof-cd0-close`) | 487,875 | 2.10e10 |
| the same and 1000 conflicts over bounds (`both-cd0`) | 347,015 | 1.64e10 |
| 300 proofs, cut depth 0 (`p300-cd0`) | 230,761 | 1.13e10 |
| 1000 proofs, cut depth 0 (`p1000-cd0`) | 200,697 | 1.19e10 |
| 300 / 1000, column-indexed (`c300-cd0`, `c1000-cd0`) | 230,761 / 200,697 | 1.008e10 / 1.011e10 |
| 300 / 1000, with the worklist (`w300-cd0`, `w1000-cd0`) | 230,307 / 205,939 | 9.80e9 / 9.88e9 |
| 300, cut depth 3, without the column index (`p300-cd3`) | 420,405 | 2.47e10 |
| 300, worklist, cut depth 1 / 2 | 442,039 / 436,321 | 2.25e10 / 2.23e10 |
| 300, node presolve on every node, cut depth 3 / 1 | 441,991 / 337,127 | 1.83e10 / 1.41e10 |
| 300, cut depth 0, null 100 / 300 (02-474's work at the root) | 241,949 / 235,117 | 1.04e10 / 1.02e10 |
| 300, cut depth 3, null 100 / 300 | 412,801 / 377,277 | 2.09e10 / 1.91e10 |

The pool size shrinks the tree to about 200,000 nodes, near HiGHS's, and
its propagation then decides the work: over the path's bounds, every node
pays the pool's size times its path (3.5e9 at 1000 proofs, 30% of the
work), the column index pays the path's columns' entries (2.0e9), and the
worklist rereads only the proofs a tightening moved.

The local Gomory cuts hold the rest. Cuts at the two nodes of depth 1 alone
take the tree from 230,307 nodes to 442,039, and node presolve on every node
gives back only part of it. Two things change with them: the node presolve
trial reads 22.7 to 24.7 iterations a node, past `MIP_NODE_PRESOLVE_ITERS`,
so the nodes skip presolve, and orbital branching widens 73 branchings to an
orbit at depth 3 and 113 at depth 1 against 264 at depth 0. Without orbital
branching the cuts still cost (bound 14.06 at 1e10 at depth 3 against 14.19
at depth 0), so the widenings are a reading, not the whole mechanism. Per
node the cuts lift `neos5`'s bound by 0.65 against branching's 1.10 at depth
3 and below, a ratio `misc07` (0.57) and `gt2` (0.74) share, so a pay test
on the lift does not tell them apart (`glog-*.txt`).

## What separates the models that lose their local cuts

Cut depth 0 on every model reads 1.008x geometric and 0.965x in total on
MIPLIB 3 (`miplib3/cd0.txt`), `gt2` 2.25x (5 nodes to 65), past the gate's
2x. On the models whose symmetry search finds generators it gains on six of
ten. Keyed on generators alone, the rule takes `mas76`, with two orbits of
two columns, to 1.47x. The share of the integer columns that lie in orbits
of more than one column (`shares.txt`) separates them:

| set | share 0.5 or more | under 0.5 |
|---|---|---|
| MIPLIB 3 | `misc03` 0.925, `misc07` 0.938, `p0201` 0.965, `rgn` 1, `stein27` 1 | `air03` 0.002, `blend2` 0.034, `enigma` 0.2, `gen` 0.333, `misc06` 0 |
| MIPLIB 2017 | `neos5` 0.792, `assign1-5-8` 0.915, `neos-911970` 1 | `glass4` 0.007, `mas74`, `mas76` 0.027, `neos-2657525-crna`, `neos-3046615-murg`, `timtab1` 0 |

With cut depth 0 the first five of MIPLIB 3 read 0.836x, 0.867x, 0.930x,
0.943x and 1.023x, and `blend2`, below the bar, 1.048x. The landed rule
(`MIP_SYMMETRY_CUTS`): left unset, the cut depth reads 0 when the share is
at least 0.5.

## What landed

`jaos_set_mip_dual_proofs` (`docs/api.md`), 600 proofs by default. A
node's dual simplex that stops once its bound passes the incumbent leaves
its duals, mapped back through the node's presolve; over the model's rows
and the permanent cuts they give a proof `Σ d_j x_j + R <= c'x`, kept when
its least activity over the node's path bounds passes the prune bound, no
local row has a multiplier, and its integer coefficients span at most
`MIP_PROOF_RANGE`. Continuous columns and globally fixed ones go into `R`
at their least term over the global bounds, and so does a reduced cost
under `MIP_PROOF_ROUNDOFF` of its terms. Each proof's activity is kept over
a box the pool moves column by column to the node's path bounds; before a
node's relaxation a proof past the prune bound cuts the node, and one within
reach tightens integer bounds as reduced-cost fixing rounds them. Unlike the
prototype the pool keeps no dense row per proof, so its memory is the
proofs' nonzeros.

**The branching column.** `pseudocost_learn` reads the node's branching
column as the last bound on its path. The prototype appended its tightened
bounds after it, so every node a proof tightened taught the pseudocosts a
gain for a column the node never branched on. The landed pool writes its
bounds ahead of the branching bound. On the same build that one change
reads, to the close:

| | before | after | the record |
|---|---|---|---|
| `neos5` | 222,931 nodes, 9.89e9 | 186,781, 8.45e9 | 2,977,069, 1.91e11 |
| `pk1` | 202,147, 7.86e9 | 159,769, 6.57e9 | 191,552, 8.40e9 |
| `mas76` | 590,513, 9.22e9 | 237,711, 4.04e9 | 360,363, 5.34e9 |

02-482 found that the proofs' tightening grew `pk1`'s and `misc07`'s trees
and asked for a rule that told `neos5`'s tightenings from theirs; what it
read was largely this attribution. Orbital fixing at a node appends its
bounds the same way, and still does.

**The pool's three stops.** After `MIP_PROOF_TRIAL` (100) proofs the pool
stays only if they have cut and tightened more than once each
(`MIP_PROOF_PAYS`): `stein45`'s 100 do not, `neos5`'s, `pk1`'s and
`mas76`'s do. `MIP_PROOF_IDLE` (200) stopped relaxations with no proof stop
it (`bell5`). Every `MIP_PROOF_WINDOW` (1000) nodes a pool that spent more
than half a node relaxation's work a node (`MIP_PROOF_SHARE`) halves,
dropping the proofs unused longest, and under `MIP_PROOF_FLOOR` (25) it
stops: `mas76`'s dense proofs halve it three or four times, to 75 or 37,
`pk1`'s once to 300, `neos5`'s never. A first form set the pool's work against the
relaxations its cuts saved; it stopped `pk1`'s pool, which then closed at
8.50e9 against 7.86e9, because the cuts are not where the proofs pay.

Pool sizes, to the close, on the landed build:

| proofs | `neos5` | `pk1` |
|---|---|---|
| 300 | 209,875 nodes, 8.86e9 | 162,593, 6.37e9 |
| 600 | 186,781, 8.45e9 | 159,769, 6.57e9 |
| 1000 | 187,265, 8.88e9 | 157,457, 6.82e9 |

Null readings (02-474's `JAOS_NULL`, 100 to 400 times the model's size of
work added at the root), the landed build at 600 against `head`:

| | `head` | the change |
|---|---|---|
| `neos5` | open at 1e10 | 8.45e9, 8.20e9, 7.87e9, 8.37e9, 8.70e9 |
| `pk1` | 8.40e9 | 6.57e9, 6.77e9, 6.48e9, 6.45e9, 6.50e9 |
| `mas76` | 5.34e9, 6.63e9, 7.78e9, 7.39e9, 7.08e9 | 4.04e9, 5.47e9, 7.28e9, 6.63e9, 7.51e9 |

`mas76` spreads 1.5x under null readings on both, so its single reading in
a set is a draw; its mean moves from 6.84e9 to 6.19e9.

## MIPLIB 3

On 97541e88 against `head`, one thread (`miplib3/`, 02-482's `m3off.py`):

| arm | geometric | total | worst |
|---|---|---|---|
| cut depth 0 on every model (`cd0`) | 1.008x | 0.965x | `gt2` 2.248x |
| the symmetry-share rule alone (`l6-np`) | 0.982x | 0.959x | `stein27` 1.023x |
| the change (`l6`) | 0.994x | 0.975x | `bell5` 1.176x |

Rebased on 87475d8a the change reads 1.020x and 1.061x against that base
(`miplib3/l8.txt`), `blend2` 1.490x and `misc07` 1.126x: the rebase's ZI
stop had moved `blend2` to 0.675x and `misc07` to 0.821x of their work by
path (02-499), and the share's count, charged as work at the root, moved
them back. The count is no longer charged, as the symmetry search's own
orbit count is not. Four null readings on each side (`miplib3-null/`,
`nullcmp.py`, `JAOS_NULL` 0 to 300), the change against 87475d8a:

| null | geometric | total |
|---|---|---|
| 0 | 1.003x | 1.044x |
| 100 | 0.981x | 0.957x |
| 200 | 0.988x | 0.994x |
| 300 | 0.987x | 0.959x |
| each model's mean over the four | 0.990x | 0.987x |

On ab51180f, the base of the gates, `make miplib J=2` reads 0.976x
geometric and 0.992x in total against the record (`miplib3/change.txt`,
`miplib3/before-ab51180f.txt`): `bell5` 0.59x, whose record had moved to
38,009 nodes, `stein27` 1.115x, `misc03` 0.83x.

Over the four null readings, `misc03` reads 0.813x, `p0201` 0.910x, `misc07` 0.930x and
`rgn` 0.948x, and `bell5` 1.074x, `stein27` 1.072x and `stein45` 1.020x.
`bell5` makes no proof: its 200 stopped relaxations before the pool stops
charge their duals' work, and the shares that read the tree's work move
its path. `stein45`'s first 100 proofs cut and tighten under once each
and its pool stops.

## MIPLIB 2017

`make miplib2017 J=2` on the change rebased on ab51180f, against its record
(`miplib2017/before.txt`, `miplib2017/change.txt`; 02-328's `m17sum.py`):

| | solved | incumbents | primal | dual | sum |
|---|---|---|---|---|---|
| the record | 9 | 29 | 5.2367 | 6.1186 | 11.3552 |
| the change | 11 | 29 | 4.1525 | 5.0942 | 9.2468 |

`markshare_4_0` and `neos5` close. On the closed ones, against the record:

| | the record | the change |
|---|---|---|
| `markshare_4_0` | open, 590,893 nodes | 378,496 nodes, 2.00e9 |
| `neos5` | open, bound 14.25, 110,147 nodes | 165,501 nodes, 7.41e9 |
| `pk1` | 186,963 nodes, 8.11e9 | 152,344, 6.56e9 (0.809x) |
| `mas76` | 417,415, 6.64e9 | 315,175, 5.15e9 (0.776x) |
| `gen-ip054` | 50,913, 1.14e9 | 68,545, 1.48e9 (1.293x) |
| `binkar10_1` | 7,299, 5.50e9 | 7,519, 5.54e9 (1.008x) |

the other five at 1.000x to 1.001x. `gen-ip054`'s pool stops after 200
stopped relaxations with no proof, and their work moves 02-478's check on
its reduced basis. Among the open ones the primal side gains on
`neos-3046615-murg` (0.0338 to 0.0100), `mas74` (0.0415 to 0.0117),
`supportcase26`, `beasleyC3` and `gen-ip002` and loses on none; the dual
side gains on `neos-911970` (0.0341 to 0.0223) and loses on
`neos-3046615-murg` (0.6937 to 0.7188), `graphdraw-domain` (0.2385 to
0.2491) and by under 0.002 on `mas74`, `supportcase26` and `gen-ip002`.
On 87475d8a the change read 11 solved and 9.2634 against 11.3860
(`before.txt`; `l8.txt` is the form that charged the share, 9.2281), and on
97541e88 before the branching-column fix (`l2-300.txt`) 10 solved and 9.27
against 11.26.

## `gen-ip002` and `assign1-5-8`

Neither makes a dual proof. Their first 200 stopped relaxations give none
that passes the checks, `gen-ip002`'s also with no cuts below the root and
no node MIR, and the pool stops (`MIP_PROOF_IDLE`). The stops' duals still
cost work, and on `gen-ip002` that moves 02-478's check, a knife edge: on
87475d8a the tree stays on the reduced basis and ends at 1e10 with bound
-4805.80 and incumbent -4770.72, against the record's -4804.62 and its
optimum -4783.73 from the columns' second tree; on ab51180f, whose record
reads -4804.92 and -4761.72, the change reads -4806.13 and -4774.65.
`assign1-5-8`'s symmetries move 0.915 of its integer columns, so its cuts
stay at the root: bound 200 at 1e10 in 35,814 nodes on ab51180f, whose
record also reads 200, in 31,742, and `head` under
`--reliability 16` reads 200 in 32,762 (`a158-rel16`). Both stay where
02-478 and 02-483 left them: `gen-ip002` needs a relaxation that lifts the
bound per node, `assign1-5-8` reliability branching's probes.

## Gates

On the change rebased on ab51180f:
- `make test`: every suite, `tests/cli.sh`, the docs and version checks and
  the Windows build pass. `test_mip`'s mutation, DINS and ZI-stop tests
  turn the proofs off: they name the heuristic that finds a point or its
  exact work, and the proofs move the tree's path. Python's gap-zero test
  compares the two optima to nine places, not to the bit.
- `make sanitize`, `make python-test`: pass.
- `make netlib netlib-infeas J=12`: `gate: PASS`, `baseline: 0 regressed, 0
  improved, 0 new, 0 at other nodes` on both; `make netlib-kennington J=2`
  the same. `make primal barrier pdlp concurrent warm J=12`: the records
  are unchanged to the bit; the simplex publishes the stopped duals only
  under the tree's request. These LP gates ran on 87475d8a before the
  share's count was uncharged; what changed since is the tree alone.
- `make miplib J=2`: `gate: PASS`, `baseline: 0 regressed, 0 improved, 0
  new, 10 at other nodes`; 0.976x geometric and 0.992x in total against the
  record (above). The baseline is rewritten after reading that diff.
- `make miplib2017 J=2`: 11 of 30, sum 9.2468 against 11.3552 (above).
- `make java-test`, `julia-test`, `dotnet-test` pass on 87475d8a.
