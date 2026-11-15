# 02-455 — the trees the mirror did not reach

Taken on 2026-11-15 on the tree of f4cf5c6 with this change, for TODO row
J29. `read.sh` takes the mirror reading on two sets of generated models
(`mip`, `cone`); `resume-warm.sh` reads a stop inside the settling
re-entry of a warm-started solve (`resume-warm/`).

## The sets

`mip` is 02-224's `perm.c` with `dumpall.patch`: seeds 1 to 4, 2500
models a seed, each a MILP with one of an SOS1 or SOS2 set, a
semi-continuous column, an indicator row or none. `cone` is 02-431's
`permcone.c` with `dumpall-cone.patch` in its mixed mode, the same shapes
beside cones or quadratic rows. 02-440's `mirrorfile.c` with 02-447's
`modes.patch` solves each file as it is and mirrored under 2e9 work
units, in three modes: rows negated and columns mirrored (`both`), rows
only and columns only. `sos.patch` lets it mirror an SOS member too. It
never mirrors a cone's head, a semi-continuous column or an indicator
row's column: a head is at least 0 by its cone, a semi-continuous
column's gap needs a lower bound above 0, and an indicator's column is a
binary, so none of the three can be written in the other sign.

Files whose nodes or iterations differ, SOS members not mirrored, on
f4cf5c6 for `mip` and with f4cf5c6's `src/conictree.c` for `cone`. That
cone reading counts 8,403 of the 10,000 files: one model outgrew memory,
the kernel killed its batch of 100, and `xargs` launched no batch after
it (TODO row J31). `read.sh` now caps each solve's memory.

| set | both | rows | columns |
|---|---|---|---|
| mip, 10,000 | 48 | 0 | 48 |
| cone, 8,403 | 328, 27 of them at another objective | 41, 1 | 299, 25 |

The 48 MILPs are 27 with an indicator row, 11 with a semi-continuous
column, 3 with an SOS set and 7 plain: the MIP presolve, which mirrors a
column whose lower bound lies farther from zero than its upper bound
(02-447), did not run on a shaped model, and on a plain one it left a
column with equally far bounds in its written sign. The conic tree
mirrored only integer columns, and no row.

## The change

`src/mip.c`. The MIP presolve runs on a model with SOS sets, indicator
rows or semi-continuous columns. It only mirrors there, as on a quadratic
objective, and carries the sets, the indicators and the semi-continuous
marks to the tree's copy. An SOS member is mirrored like any column; a
semi-continuous column and an indicator's column keep their sign. A
column whose bounds lie equally far from zero is mirrored when it is
fixed at a negative value, else when its cost is negative, else by the
sum of its entries each read in its row's sign (`jm_row_sign`: 1 for a
row with only its upper side, -1 for one with only its lower side, an
equality row by the sign of its right-hand side, a ranged row by its side
farther from zero), else by the first such entry. Under an incumbent or
node callback the presolve runs too: the callback reads the point in the
model's own columns, and the rows, branches and points it hands back are
moved into the copy's. When no column was merged, an infeasible or
unbounded end of the copy's tree is the model's, with an infeasible one's
certificate checked on the model, where it used to run the tree again on
the model as loaded. The copy's tree gets the whole work limit, so a
model takes the same tree whether or not the presolve had a column to
mirror; the presolve's own pass is billed on top.

The cut separators read the two sides of a row in one order whatever
sign the row was written in: the lower side first when the row's sign is
-1, or is 0 and its first entry is negative. Zero-half candidates of one
row and slack sort by that order. On QPLIB_5924 the flow cover round
reached its cap of 50 cuts (`MIP_FLOW_COVER_CUT_CAP`) six units of work
apart in the two signs.

`src/conictree.c`. The conic tree's copy mirrors every column by the MIP
presolve's rule but a cone's head, a semi-continuous column and an
indicator's column, and negates a row whose sign is -1, or is 0 and the
sum of its entries after the mirror is negative, or is 0 and its first
entry is. The duals, the row statuses and a certificate come back
negated on those rows. The conic interior point is not sign-invariant
(TODO row J27), so the copy can break a walk the model as written does
not: 02-326's model 620 of seed 2 ended `numerical_error` at node 5,
whose fixed solve the checker refused, where f4cf5c6 ended optimal. When
the copy's tree ends `numerical_error` or fails, the tree runs again on
the model as written under the work left, as the MIP presolve does since
02-447. A model the tree refuses as input is then named as it is
written: QPLIB's non-convex mixed-integer QCQPs said "not convex on its
upper side" of a row the copy had negated.
`test_a_mirrored_copy_that_fails_gives_way_to_the_model` solves that
model (`tests/data/cone_mirror_fallback.mps`, its semi-continuous column's
upper bound written as 1e20, since the MPS SC bound cannot express none,
and lifted on reading) to the optimum the tree reaches without the
mirror.

`src/simplex.c`. A fixed variable's reduced cost has no sign to keep, so
it is no dual breach (`dual_breach`, `published_breach`), takes no cost
shift (`shift_to_feasible`) and gets no solve in
`repair_dual_infeasibility`. When an optimum settles, a fixed variable
whose reduced cost reads the wrong way for its status takes the other
status (`settle_fixed`), at no cost, since its value does not move.
Before, the slack of an equality row whose reduced cost read the wrong
way counted as a breach: the settle re-entered the dual simplex for
nothing, a refactorisation and a pass, and the cost shift left on it was
repaid later with a solve for the duals. Negating the row flips that
sign, so `blend2`'s walk billed 571 units more at node 1123 in one sign
than in the other. The status the re-entry left is kept: a node's basis
carries it to the next node, where presolve can make the equality row
ranged and put it on the side that status names. Without `settle_fixed`,
`enigma` takes 7,658 nodes against 895, and `misc07` 16,948 against
13,020. Of 02-424's 1000 small LPs of seed 1, 6 of the first 25 primal
solves re-entered on f4cf5c6, each for about 5,000 of 20,000 to 48,000
units; with the change none of 36,000 does.

A variable that leaves a singular basis takes the bound nearer zero
instead of its lower bound when that one is finite. With both bounds
equally far from zero it takes, in order, the side of a fixed nonzero
value (lower when positive), the upper bound for a column of negative
cost, and for a slack the lower bound when the row's first entry is
positive (`leaving_status`). `blend2`'s row 23, an equality with a
right-hand side of 0 and a dual of exactly 0 in every node, left a
singular basis at its lower bound in both signs in the fourth solve of
the tree, and kept it. At node 5043 presolve made it ranged, and the two
signs started that node from different points. A rule that settles
every fixed variable whose reduced cost is 0 by its row's sign closes
that too, but whichever way its four sign choices point, `enigma` takes
2,454 to 23,682 nodes, `misc03` 225 to 741 and `misc07` 10,601 to
17,341: those statuses carry what the walk did.

`src/presolve.c`. An equality row whose implied free column or free
column singleton the presolve substituted out comes back from the
postsolve at the side its dual's sign names, upper for a negative dual in
a minimisation, and with a dual of 0 at the side the sign of that
column's coefficient names, upper for a negative one; it came back at its
lower side whatever the sign. The status of a fixed slack matters
downstream: Gomory cuts read a nonbasic slack from the side its status
names, where both sides are valid but give different cuts, and presolve
puts an equality row it makes ranged at that side. In `gen` with rows
negated, row 632 came back at its lower side in both signs from the
31st LP solve on, and the 35th solve's LP carried one cut that differed
between the signs: the two took the same nodes and iterations, 60 work
units apart. In 02-455's MILP `s2-g00698`, whose first LP presolve solves
whole through a free column singleton, the equality row R1 came back at
its lower side with a dual of 0 in both signs, and the next LP's Gomory
cut read `3 C4 >= -12` in one sign and that cut less 1.5 times R1 in the
other. With the change, 8 MILPs of 02-455's set that read `path` with
rows negated, 1 to 4,242 units apart, read `same`.

## The reading

`read.sh` on the change, files whose nodes or iterations differ
(`*-summary.txt`; f4cf5c6's are `*-summary-head.txt`):

| set | both | rows | columns |
|---|---|---|---|
| mip, 10,000 | 0 | 0 | 0 |
| cone, 10,000 | 16 | 0 | 16 |

With rows negated all 10,000 MILPs read `same`, work included. With
columns mirrored 7,188 read `path`, at the same nodes and iterations and
the work of the presolve's copy apart.

The 16 cone files are the same in both modes that mirror columns, and on
each the change ends as f4cf5c6 does, status, objective and incumbent,
in each sign. 15 are trees that end `numerical_error` in the sign the
copy reads them; 14 of them run again on each file as written, and
`s3-c01172` as written has nothing to mirror. `s4-c02483` has no tree:
its conic walk ends 1.7e-7 off in one sign (J27). With rows negated 35
cone files read `path` at the same nodes and iterations.

02-447's MIPLIB 3 modes, every model of `bench/instances-miplib` as it is
and mirrored at 2e9 work units (`miplib-*.txt`). On 02-450's records of
f4cf5c6's tree, 7
of the 24 read `path` with rows negated, `blend2` at other iterations.
On the change all 24 read `same` with rows negated, work included, and
with columns mirrored 23 read `path` at the same nodes and iterations,
the work of the presolve's copy apart, as in 02-447.

`blend2` with rows negated took 124,814 iterations against 124,695 in the
same 6,972 nodes on f4cf5c6. On the change both signs take 125,074
iterations, 6,972 nodes and 720,608,448 work units, against 727,044,590.

QPLIB_5924 with rows negated read `path` on f4cf5c6, 2,529,294,983 work
units against 2,529,294,989; it reads `same` on the change.

## The resume test

`test_a_stop_inside_the_settling_reentry_resumes_its_round` stopped
02-424's unbounded LP inside its re-entry. With the change that LP no
longer re-enters: it was the slack of an equality row that sent it
there. Its file is now a node LP of `tests/data/mip_noinc_propagate.mps`
with the basis the tree started it from (`lp_resume_reentry.bas`), which
re-enters from 6,638 to 13,673 units and is stopped at 10,155. With the
round's counts not taken back at the resume, the test reads 23,400 work
units against 13,952.

## A stop inside the re-entry of a warm-started solve

Of the first 60 node LPs of `mip_noinc_propagate.mps` whose solve
re-entered, each written with its start basis and stopped in the middle
of the dual pass of its re-entry, 52 resume to the straight solve and 8
do not (`resume-warm.sh`, files in `resume-warm/`):

| file | straight | resumed |
|---|---|---|
| node-000 | 17 iterations, 31,629 units | 17, 31,634 |
| node-003 | 11, 23,403 | 12, 24,507 |
| node-004 | 31, 49,657 | 31, 49,556 |
| node-033 | 15, 29,759 | 15, 29,753 |
| node-034 | 16, 25,515 | 10, 21,557 |
| node-045 | 8, 23,098 | 8, 23,091 |
| node-053 | 17, 30,408 | 17, 30,157 |
| node-058 | 15, 27,641 | 7, 18,612 |

f4cf5c6 breaks the same 8, so the change did not bring it. 02-424's
harness reads cold solves only. TODO row J30.

## The gates

`make test`, `make sanitize` and `make python-test` pass. Against
f4cf5c6's records:

| reading | regressed | what moved |
|---|---|---|
| `netlib`, `netlib-infeas` | 0 | the same iterations on all 94; work down on 68, -0.43% in all; `scagr25`'s duals move in their last bits |
| `netlib-kennington` | 0 | the same iterations; work down on 11, -3.7% |
| `miplib` | 0 | the same nodes on all 24 (`p0033`'s 21 against the baseline's 13 predates f4cf5c6); work down on 23, up 0.6% on `rgn`; -1.2% |
| `primal` | | the same iterations; work down on 68 |
| `barrier` | | work down on 74; fewer crossover iterations on `cycle`, `d6cube` and `standmps` |
| `pdlp` | | `pilot87` agrees, where it overran; `pilotnov`'s crossover takes 8,919 iterations and 594,234,206 units against 5,503 and 465,464,082, from fixed variables no longer counting as dual breaches; -5.8% |
| `concurrent` | | work down on 68 |
| `warm` | | `bnl2`'s warm solve takes 612 iterations and 17,303,096 units against 439 and 12,426,868, from the bound nearer zero for a variable leaving a singular basis; work down on 27 others, -0.4% |

02-432's `gate.sh` against f4cf5c6's library: 02-255's mixed-integer
conic models take, seed by seed, within 6 nodes of f4cf5c6's, the one
`UNSURE` line f4cf5c6 has and no other, and a worst objective gap against
brute force of at most 7.6e-11 where f4cf5c6's was 1e-16, the answer now
coming back from the copy. 02-326's shaped models: the same counts of
optima, infeasible models, tree errors and failed models, model 620 of
seed 2 included, and within 10 nodes of f4cf5c6's seed by seed. CBLIB's
80 mixed-integer instances: the same
status, incumbent, bound and nodes, 6 at other work. QPLIB's
mixed-integer QCQPs: every line the same. 02-425's `warmcone.c`: P1, P2
and P3 all 0. 02-427's `warmqc.c`: the 24 optima without duals that
02-452 records under P2, and nothing else. `make cblib`: 29 of 29, 0
regressed.

02-454's `read.sh`: no file's nodes or iterations differ in any of the
five modes, and with rows negated all 17 read `same`, QPLIB_5924
included. `miqp.sh` at 1e10: the same status, incumbent, bound and nodes
on all 17; QPLIB_3708 now mirrors 135 columns whose bounds lie equally
far from zero. 02-424's `read.sh` reads clean.
