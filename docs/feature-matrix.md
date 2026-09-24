# Feature matrix — JAOS against the field

`SPECS.md` says what JAOS is built to be and how far it has got. It does not say
what a serious solver is expected to have, so it cannot tell you whether the
target is the right target. This page is the other half: the feature list a
mathematical-programming solver is measured against, and where each solver
stands on it.

The list was written from what the field offers, not from what JAOS has. It
carries every `SPECS.md` row the field compares on, and a JAOS cell at ◐ or
○ is a row `SPECS.md` does not mark done. The rows about JAOS's own tooling
(copying a model, names, model and presolve statistics, whether the model
is a MIP, the `diff` and `show` commands) have no counterpart in the other
columns, so they stay out. The `SPECS.md` row "Exact rational proof of the
final basis" is compared through two rows of section 6, "Exact rational LP
solutions" and "Machine-checkable certificate of the result".

**How to read it**

| symbol | meaning |
|---|---|
| ● | present and complete |
| ◐ | present but partial — the gap is named in the notes |
| ○ | absent |
| — | not applicable to that solver's scope |
| ? | the vendor documents nothing either way; the note says what was searched. Treat it as unknown, not as absent |

**The solvers.** The first three are the ones JAOS is timed against on LP:
`make compare-solvers` builds them and `make compare COMPARE_ARGS='-t P0'`
measures their speed. JAOS is also timed against HiGHS and SCIP on MIP.
No run here checks a feature cell. Every cell outside JAOS's column comes
from public documentation, not from measurement here. CPLEX, Xpress, COPT
and Mosek are in the same class as Gurobi and are left out only to keep the
table readable.

*JAOS's column was last checked against `SPECS.md`, `src/` and `cli/` on
2026-09-22. The other columns were last checked against their published
documentation on 2026-09-22. That pass added twenty rows, answered the
`?` cells the vendors document, named the gap of every partial cell, and
left `?` only where a vendor documents nothing, with what was searched in
the note. scipopt.org and soplex.zib.de refused automated requests again
(HTTP 429), so SCIP's and SoPlex's cells rest on their GitHub READMEs,
CHANGELOGs, the FAQ text in their repositories and the Suite papers. The
timing in `bench/compare/` ran SoPlex 8.0.3 and SCIP 10.0, one release
behind this line.
Versions: JAOS 0.5.0 · HiGHS 1.15.1 · SoPlex 8.1.0 · Clp 1.17.11 ·
SCIP 10.1.0 · Gurobi 13.0.3 · Hexaly 15.0. SoPlex 8.1.0 and SCIP 10.1.0
came out on 2026-09-18, and neither release moved a cell.*

---

## 1. Problem classes

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Linear programming (LP) | ● | ● | ● | ● | ● | ● | ● |
| Quadratic programming (QP) | ◐ | ● | ○ | ● | ● | ● | ● |
| Quadratically constrained (QCP, SOCP) | ◐ | ○ | ○ | ○ | ● | ● | ● |
| Mixed-integer linear (MILP) | ● | ● | — | — | ● | ● | ● |
| Mixed-integer quadratic (MIQP, MIQCP) | ◐ | ○ | — | — | ● | ● | ● |
| Nonlinear (NLP) | ○ | ○ | — | — | ● | ● | ● |
| Mixed-integer nonlinear (MINLP) | ○ | ○ | — | — | ● | ● | ● |
| Constraint programming | ○ | ○ | — | — | ◐ | ○ | ● |
| Black-box / simulation optimization | ○ | ○ | — | — | ○ | ○ | ● |

SoPlex and Clp are LP solvers by design; their integer side is SCIP and CBC
respectively, which is why those rows read "—" and not "○".

**Clp's QP cell was ○ and is ●.** Clp's FAQ says "The CLP barrier method
solves convex QPs as well as LPs", and `ClpModel::loadQuadraticObjective`
loads the quadratic part. This repository saw it happen: Clp 1.17.11 with
`-barrier` solved Maros-Meszaros QPs (`bench/measurements/02-293/`).

**SCIP's constraint-programming cell reads ◐.** SCIP has a cumulative
scheduling constraint, logical constraints (and, or, xor, disjunction,
pseudo-Boolean), a FlatZinc reader and a pure CP/SAT mode. It has no
alldifferent constraint and none of the other CP global constraints
(element, table, circuit, no-overlap).

JAOS's QP and QCP rows are explained under section 2. **The MIQP row
reads ◐.** A quadratic objective with integer columns solves in the linear
tree on barrier relaxations. Of QPLIB's 17 convex mixed-integer QPs, 4 end
`OPTIMAL` and 13 reach a work limit of 1e11 (`SPECS.md` §1). The
mixed-integer QCQPs are part of the QCP note.

## 2. LP algorithms

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Dual simplex | ● | ● | ● | ● | ● | ● | ? |
| Primal simplex | ◐ | ● | ● | ● | ● | ● | ? |
| Barrier / interior point | ◐ | ● | ○ | ● | ● | ● | ◐ |
| Crossover to a basic solution | ◐ | ● | — | ● | ● | ● | ? |
| First-order method (PDLP / PDHG) | ◐ | ● | ○ | ○ | ○ | ● | ○ |
| GPU acceleration | ○ | ● | ○ | ○ | ○ | ● | ○ |
| Concurrent solve (race several methods) | ◐ | ○ | ○ | ○ | ● | ● | ? |

**The other columns in this table.** HiGHS's concurrent cell was ◐ and is
○: its `solver` option lists every LP method, and none of them races the
others. The parallel dual simplex variants (SIP, PAMI) run one method on
several threads. Clp's crossover cell was ◐ with no gap named and is ●:
`ClpSimplex::barrier` crosses over to the simplex when asked, and the
`crossover` option gives "a basic solution suitable for ranging". Hexaly
says it uses "simplex methods when linear and interior point methods when
nonlinear". Its barrier cell reads ◐ for that reason: an interior point
method is documented (14.0), but not as an LP method, and the user cannot
choose it. Its dual, primal, crossover and concurrent cells stay `?`:
Hexaly never names a simplex variant, a crossover or a race of LP methods
(searched: the docs index, `HxParam`, the 13.0 to 15.0 release notes, its
conference abstracts, "Hexaly dual simplex", "Hexaly crossover").

JAOS's dual simplex has steepest-edge pricing, a Harris two-pass ratio test with
bound flipping, dual phase 1 by artificial bounds, a cost perturbation on
the first stall and Bland's rule after it. Since 2026-09-21 a steepest-edge
weight that drifts from its exact value hands the pricing to dual Devex for
the rest of the solve, except inside the MIP tree.

**The primal simplex reads ◐ rather than ●** because 6 of the 94 standard
instances still run past 10x the dual's work (`bench/results/primal.txt`),
and on pilot87 the primal ends `NUMERICAL_ERROR` where the dual finds the
optimum (the one disagreement in that file). `jaos_set_algorithm` and `--algorithm primal` select it. It has
steepest-edge pricing since 2026-09-08, with Devex and Dantzig behind
`cfg.primal_devex` and `cfg.primal_dantzig` for measurement, a composite
phase 1 and a Harris ratio test; the same day its phase 2 stopped shifting
costs through the shared pivot, which had made it stop after one pivot and
hand the solve to the dual. Stage 7, the unboundedness verdict, landed on
2026-09-01: the primal declares a ray it meets in phase 2 on the same D19
proof the dual already used, and the shared lent-bound verdict also
proves a ray that needs several columns at once.

**Concurrent reads ◐ since 2026-09-10, and the gap is a set where it
pays.** `--algorithm concurrent` copies the model three
times, sets the dual, the primal and the barrier on the copies and runs
them under a work budget that grows each round; the first to answer wins
and the work billed is the sum over the three. Nothing reads a clock, so
the winner is the same on every machine, which the threaded version in the
other columns cannot promise. `--threads N` above 1 starts the three at
once and stops every arm behind one that has answered, and the answer and
the work units do not move. `bench/results/concurrent.txt`, re-taken on
2026-09-23 after the billing fix: 94 of 94 agree inside 10x the dual's
work, none disagrees, and the work geometric mean is 1.0556x the dual. No
instance costs less than the dual alone: the best is `25fv47` at 1.0000x
and the worst `greenbeb` at 2.5649x. The primal won `grow22` in the 2026-09-10 reading; the dual now
takes 2014 iterations there and answers first.

**The barrier reads ◐ since 2026-09-08.** `--algorithm barrier` and
`JAOS_ALGORITHM_BARRIER` select Mehrotra's predictor-corrector on the
normal equations, factored by a minimum-degree sparse Cholesky written
here (`src/chol.c`, `src/barrier.c`), and a crossover since 2026-09-09: the
interior point ranks the variables by primal against dual slack, the best
`rows` of them are the basis guess, the LU repairs it where singular, and
the dual simplex finishes from there. Since 2026-09-22 a primal push first
puts every nonbasic column on a bound and the primal simplex finishes from
that basis (`bench/measurements/02-287/`). A MIP's relaxations stay on the
dual.
Since 2026-09-20 a model whose quadratic objective is diagonal reads both
the normal matrix's factor and the augmented system's, and keeps the one
that costs fewer operations (`bench/measurements/02-272/`).
An infeasible or unbounded model, which the barrier cannot certify, goes to
the dual simplex from the slack basis once the iterate diverges, so the
verdict and its certificate are the dual's (since 2026-09-09,
`bench/results/barrier-infeas.txt`). **The QP row reads ◐ since 2026-09-09**: a
quadratic objective `c'x + ½ x'Qx`, separable through `jaos_set_col_quadratic`
and with a full `Q` through `jaos_set_quadratic` since 2026-09-10, carried by
MPS `QUADOBJ`/`QMATRIX`, the LP `[ ... ] / 2` block, QPLIB and OSiL, solved by
the barrier with `Q` in its Newton system and judged by the checker with the
gradient `c + Q x`. Since 2026-09-15 the barrier's point is finished by a
push (`qp_push` in `src/barrier.c`): the active set read off the
complementarity is pinned on its bounds and the equality-constrained QP on
the rest is solved through the same factorisation, so the published point
sits exactly on the bounds the optimum sits on and the checker takes both
sides. On the 138 QPs of Maros and Meszaros (`make maros-meszaros`) 137
end `OPTIMAL` and the checker takes all 137; `values` is refused as not
convex (`bench/results/maros-meszaros.txt`). On QPLIB's 19 convex QPs
(`bench/measurements/02-256/`, `02-295/`, `02-318/`) 10 end `OPTIMAL` and
the checker takes all 10 at 1e-7. QPLIB_9002 ends `numerical_error`: its
push leaves pinned variables with the wrong sign. Seven of the eight
largest (10000 to 1003001 columns) stop at a work limit of 1e11, and
QPLIB_9008 runs out of memory. The rest of those two sets is what keeps
the row from ●.
**The QCP/SOCP row reads ◐ since 2026-09-19**: second-order cones,
quadratic and rotated, over columns (`jaos_add_cone`) and convex
quadratic rows `a'x + ½ x'Qx` with one finite side
(`jaos_set_row_quadratic`), read and written in MPS (`QCMATRIX`,
`CSECTION`), LP, QPLIB, OSiL and CBF, solved by a homogeneous self-dual
conic interior point with Nesterov-Todd scaling (`src/conic.c`); a
quadratic row becomes a rotated cone over its Cholesky factor. The
point is finished by Newton's method on the constraints the walk ends
on, and the answer is judged by the checker with the cones' duals
(`jaos_check_conic_solution`); an infeasibility certificate and an
unbounded ray are published only when the checkers confirm them. Over
3000 generated models (`bench/measurements/02-253/`) every optimum
passes the checker at 1e-7 on both sides, the same models rewritten
with explicit rotated cones reach the same objectives within 3e-10,
and none ends as a numerical error since 2026-09-20. A certificate the
checker refuses is re-weighted before it is given up, since the test is
sharp in the proportion between the multipliers and not in their scale,
and 599 of the 600 planted infeasibilities publish one
(`bench/measurements/02-270/`). On the 29
continuous CBLIB 2014 instances under 70 MB (`make cblib`,
`bench/measurements/02-254/`), with cones of up to 99998 members, all 29
end `OPTIMAL` and the checker takes all 29 (since 2026-09-20,
`bench/measurements/02-273/`). Integer columns beside cones or quadratic rows go to a
branch and bound of their own (`src/conictree.c`,
`bench/measurements/02-255/`): over 3000 generated models it agrees with
brute force every time, and on CBLIB's 80 mixed-integer instances at
1e11 work units 43 end `OPTIMAL`, all taken by the checker, and 37 at
the work limit, each with an incumbent found by the tree's rounding, its
root dive or its branching. What keeps the row from ●: of QPLIB's 13
convex continuous QCQPs 9 end `OPTIMAL` taken by the checker, 3 end
`NUMERICAL_ERROR` and 1 reaches the work limit
(`bench/measurements/02-319/`), the
mixed-integer QCQPs reach the work limit, a quadratic row over
`CONIC_QC_DENSE` columns is refused, and the 37 mixed-integer CBLIB
instances do not close (`SPECS.md` §1, the quadratically constrained row). **The first-order row reads ◐ since
2026-09-09**: `--algorithm pdlp` and `JAOS_ALGORITHM_PDLP` run primal-dual
hybrid gradient on the scaled model after Ruiz and Pock-Chambolle
preconditioning (`src/pdlp.c`), with the adaptive step,
the restarts to the running average and the primal-weight rebalancing of
Applegate et al. (2021), single-threaded and deterministic, finished by the
same crossover and handing off the same way. `bench/results/pdlp.txt`,
re-taken on 2026-09-22, is its reading: 24 of the 94 end inside 10x the
dual's work, 70 run past it, none disagrees, and the work geometric mean
is 5.5471x the dual. That reading and the lack of a GPU keep the row
from ●. What keeps the barrier rows from ●: the crossover has a primal push
since 2026-09-22 but no dual push, so on 14 degenerate instances the simplex
that follows still costs more than ten cold dual solves; `bench/results/barrier.txt`
is the reading, and it is what decides whether the barrier ever becomes a
default.

**HiGHS's GPU row was ◐ on the claim that the PDLP work was "in progress
rather than released". It is released.** HiGHS ships cuPDLP-C and a native
first-order solver of its own, HiPDLP; `solver` accepts `"pdlp"` and
`"hipdlp"`, and both run on an NVIDIA GPU under Linux and Windows. The GPU is
the point of the row rather than a bonus: the documentation says that on a CPU
these are "unlikely to be competitive with the HiGHS interior point or simplex
solvers". Neither generates a basic solution and neither has a crossover, so
the crossover row above is about the interior point solvers only.

**Gurobi's GPU row was `?` and is ● .** Gurobi 13.0 added PDHG with GPU
acceleration: `Method` takes `GRB_METHOD_PDHG`, `PDHGGPU=1` selects the GPU,
and there is a documented GPU-enabled build. For a MIP it applies to the root
relaxation only.

## 3. Preparing and handling the model

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Presolve | ◐ | ● | ● | ● | ● | ● | ● |
| Postsolve back to original indices | ● | ● | ● | ● | ● | ● | ● |
| Scaling | ● | ● | ● | ● | ● | ● | ? |
| Sparse LU with update (Forrest-Tomlin or similar) | ● | ● | ● | ● | ● | ● | ? |
| Hyper-sparse triangular solves | ● | ● | ● | ● | ● | ● | ? |
| Modify a loaded model (bounds, costs, coefficients) | ● | ● | ● | ● | ● | ● | ● |
| Add and delete rows and columns | ● | ● | ● | ● | ● | ● | ● |
| Warm start from a previous basis | ● | ● | ● | ● | ● | ● | ○ |
| Read and write a starting basis | ● | ● | ● | ● | ● | ● | ○ |
| Exchange a basis in the MPS basis format | ● | ● | ● | ● | ● | ● | ○ |
| Resume after a work or time limit | ● | ● | ? | ● | ● | ● | ● |

**The other columns in this table.** Hexaly's API has no basis at all: the
method lists of `HxSolution` and `HexalyOptimizer` hold no basis call, a
warm start is a point given through `set_value`, and the only file it reads
back is its own HXB. Its scaling, LU and hyper-sparse cells stay `?`:
nothing in its documentation index, `HxParam` or release notes names them.
Clp's resume cell reads ●: a limit stops the solve with status 3, and the
next `primal()` or `dual()` starts from the basis the stop left, keeping the
factorisation when `startFinishOptions` asks. That is a warm restart from
the basis, where JAOS resumes the exact iterate. SoPlex's stays `?`: its
front page claims hot starts from any regular basis, but nothing says a
second `optimize()` after a limit goes on from the stop (searched: the
v8.1.0 CHANGELOG, the FAQ, INSTALL.md, the Suite 8.0 to 10.0 papers).

JAOS's presolve reads ◐: the reduced-model machinery, the postsolve stack
and seven reduction families have landed, and what is left is counted
rather than guessed. Duplicate rows, duplicate columns and dominated
columns are refused at 0.15% of the 139 gate models, with zero removable
rows and columns on all 15 plato instances. MIPLIB 2017's LP relaxations
hold 7.57% of their rows as duplicates (`bench/measurements/02-312/`);
duplicate rows built on that were refused, since they cost more work on
MIPLIB 3, Kennington and warm re-solves than they saved
(`bench/measurements/02-314/`). The implied free column singleton reaches equality
rows only, a third of what its counter reads. Since 2026-09-21 the
aggregator (`src/aggregate.c`) substitutes an implied free column out of an
equality of up to three entries, which covers most doubleton equalities
without the bound transfer D97 refused (`bench/measurements/02-285/`);
a doubleton whose column the row does not imply free still needs it. Dual fixing was measured and
refused at 0.67% of netlib's and 1.09% of fome's live columns against a 5%
bar. Each has its line and its reopen condition in `bench/refusals.txt`.

**Postsolve moved from ◐ to ● on 2026-09-04, and it was a bookkeeping error
rather than a change.** The cell was ◐ with no gap named anywhere, which the
legend above forbids. It was echoing presolve. The one postsolve defect the
record ever carried was D167's broken row-count promise on 46 of netlib's 188
solves, and D257 closed it: every postsolve status is decided from the
reduction's structure, and 188 of 188 netlib and 32 of 32 Kennington solves
publish exactly `num_row` basics. Postsolve covers every reduction JAOS
performs, so the row is complete for what it has to undo.

Both solves report their pattern, and both compute only their reachable
slots on a sparse right-hand side: BTRAN since D253, FTRAN since 02-31, the
latter behind a density prediction per kind of vector because the reach walk
costs more than it saves once the answer is dense. The row closed on an
instruction count, not on work units, since the full pass's traversal of
every slot was never billed (`FTRAN_HYPER_DEN` in `tolerances.md`).

## 4. Mixed-integer machinery

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Branch and bound | ● | ● | — | — | ● | ● | ● |
| Pseudocost branching | ● | ● | — | — | ● | ● | ? |
| Strong branching | ◐ | ● | — | — | ● | ● | ? |
| Node selection beyond best bound | ● | ● | — | — | ● | ? | ? |
| MIP restarts | ◐ | ● | — | — | ● | ● | ? |
| Cutting planes | ◐ | ● | — | — | ● | ● | ● |
| MIP presolve: probing, clique table, coefficient tightening | ◐ | ◐ | — | — | ● | ● | ? |
| Bound propagation and reduced-cost fixing | ◐ | ● | — | — | ● | ● | ◐ |
| Conflict analysis | ● | ● | — | — | ● | ● | ? |
| Symmetry detection | ● | ● | — | — | ● | ● | ? |
| Primal heuristics | ◐ | ● | — | — | ● | ● | ● |
| MIP start and cutoff | ● | ◐ | — | — | ● | ● | ◐ |
| Node limit and incumbent callback | ● | ● | — | — | ● | ● | ○ |
| Solution pool | ● | ○ | — | — | ● | ● | ● |
| Semi-continuous variables | ● | ● | — | — | ● | ● | ◐ |
| SOS1 and SOS2 constraints | ● | ○ | — | — | ● | ● | ◐ |
| Indicator constraints | ● | ○ | — | — | ● | ● | ◐ |
| Deterministic parallel tree search | ● | ? | — | — | ● | ● | ? |

**JAOS's ◐ rows in this table.**
- *Strong branching*: `--reliability N` solves up to eight candidates'
  children on the spot until a column's pseudocost is trusted. It is off:
  reliability 1 reads 0.971x the work over the MIP set, with mod010 at
  2.84x and enigma at 2.07x.
- *MIP restarts*: `--restart` starts the tree again from the root once the
  root's reduced costs fix `MIP_RESTART_FRAC` of the integer columns. It
  never fired on the MIPLIB 2017 set, so it is off. A MIP presolve for the
  restart to run again is missing (`SPECS.md` §4).
- *Cutting planes*: Gomory, knapsack cover, MIR, clique and (since
  2026-09-24) flow cover cuts run at the root, and Gomory cuts to depth 3.
  Flow covers went on when the tree of d6245e0 read them at 1.000x in work
  on MIPLIB 3 (`bench/measurements/02-317/`). Zero-half and lifted cover
  cuts exist behind switches and are off: read again on the 2017 set on
  2026-09-22, neither reached the pay rule (`bench/measurements/02-298/`).
- *MIP presolve*: coefficient tightening runs at the root (`--tighten`), and
  the clique table feeds the clique cuts. Probing (`--probing`, 1.109x with
  no column fixed on the MIP set) and fixing by clique conflicts at each
  node (`--clique-fix`, 1.026x) are off by measurement.
- *Bound propagation and reduced-cost fixing*: both exist and are off by
  measurement, `--propagate N` at 1.093x the work at one pass and `--rcfix`
  at 1.010x. Node propagation is on for a quadratic objective
  (`MIP_QUAD_PROPAGATE`).
- *Primal heuristics*: rounding, a root dive and the feasibility pump are
  on. RINS and local branching exist behind `--rins` and
  `--local-branching` and are off by measurement, so no improvement
  heuristic runs by default.

**The other columns in the new rows.** SoPlex and Clp read "—" as in
section 1. HiGHS: reliability pseudocosts (`mip_pscost_minreliable`),
strong branching (refactored in 1.12.0), dives followed by the best open
node, restarts (`mip_allow_restart`, on), `mip_max_nodes` and an
improving-solution callback, reduced-cost fixing (1.15.0 notes), symmetry
detection (`mip_detect_symmetry`, on), semi-continuous and semi-integer
types, and conflicts in its cut framework (Turner's 2025 workshop slides).
Its MIP presolve reads ◐: probing and the clique table are documented, and
coefficient tightening is never named. Its start-and-cutoff cell reads ◐:
`setSolution` takes a MIP start, and no MIP objective cutoff is documented
(`objective_bound` is described for the dual simplex only). HiGHS has no
SOS constraints (issue 2148, answered by its lead developer) and no
indicator constraints (`HighsLp` holds neither). Its deterministic tree
search cell was ◐ and is `?`: a parallel tree search exists since 1.15.0,
and no HiGHS page says whether the tree stays the same at any thread count
(searched: the parallel page, the options list, the 1.15.0 and 1.15.1
release notes, issues on "deterministic"). SCIP documents every new row:
`relpscost` by default, full strong branching, the `estimate` node
selector by default since 1.0, restarts at the root and in the tree,
`SCIPaddSol` and `SCIPsetObjlimit`, `limits/nodes` and a best-solution
event, the `redcost` propagators, probing, the clique table and
coefficient tightening, semi-continuous columns read as bound
disjunctions, the `sos1`, `sos2` and `indicator` handlers, symmetry
handling and conflict analysis (Suite 10.0 report, CHANGELOG, FAQ).
Gurobi documents every new row except node selection: `VarBranch` (pseudo
reduced cost, pseudo shadow price, strong branching), restarts (12.0
release slides), `Start` and `Cutoff`, `NodeLimit` and the `MIPSOL`
callback, the `S`, `N` variable types, SOS and indicator constraints, the
`Symmetry` parameter, conflict analysis, and propagation, reduced-cost
fixing, probing, clique merging and coefficient strengthening (its
developers' presolve paper and the fixed-issue lists of 10.0 to 13.0). Its
node-selection cell is `?`: no parameter selects the node order, and two
Gurobi staff answers say users cannot control it, without saying which
rule Gurobi uses (searched: the parameter reference, the MIP logging page,
the parameter guidelines, the 12.0 slides, "node selection", "best
estimate", "plunging"). Hexaly: a start through `set_value` and no
objective cutoff (`objective_threshold` stops the search), no node limit
and no new-solution callback type, and "propagation methods" named with no
word on reduced-cost fixing. It has no native semi-continuous, SOS or
indicator object; each is written through its logical operators (`x == 0
|| x >= l`, `!b || (a*x <= c)`), and SOS2 has only the `piecewise`
operator. Its pseudocost, strong branching, node selection, restart,
presolve, symmetry and conflict cells are `?`: Hexaly publishes almost
nothing about its tree search (searched: the docs index, `HxParam`, the
13.0 to 15.0 release notes, its ISMP 2024, EURO 2025, OR 2026 and NORM
2026 abstracts, "Hexaly branching").

**Deterministic parallel tree search moved from ○ to ◐ on 2026-09-20.**
The conic branch and bound takes its open nodes in rounds under
`--tree-batch N`, solves a round's relaxations on up to `--threads`
threads, and takes their answers in the round's own order, so the tree,
its bound and its work are the same at any thread count and only the time
moves (2.6x to 2.8x on four threads at a round of four,
`bench/measurements/02-264/`). Rounds are off by default: they reach an
optimum with less work but leave a worse incumbent where a limit stops
the tree. **It moved to ● on 2026-09-22**, when the linear tree of
`src/mip.c` took rounds too: each node of a round is solved on its own
copy of the tree's LP and the tree takes the nodes in order from their
copies' final bases, with the same determinism. They are off by default
there as well, since a node of a few pivots costs more in the copy and
the second solve than its thread saves; l152lav, 750 pivots a node, falls
from 93.91 s to 21.28 s at rounds of 8 on eight threads
(`bench/measurements/02-290/`).

**The solution pool moved from ○ to ● on 2026-09-06.**
`jaos_set_mip_pool_size` keeps the best distinct integer points a branch
and bound meets, best first, and `jaos_mip_pool_solution` reads them; the
default of 1 is the incumbent alone, so the search is unchanged. "Distinct"
was the intent and not the behaviour until 2026-09-10: the check compared
the two points by their bytes, so a column at `-0` against `+0` read as a
difference, and 69 pools of 24000 held one point twice
(`bench/measurements/02-227/`). Since 2026-09-15 two points are one entry
when they agree on every integer column, the better kept, which is how
the field's pools count and what the row above means by distinct; 9 of
24000 pools had held one assignment twice as two vertices of one face
(`bench/measurements/02-246/`).

**Resume after a limit moved to an exact resume on 2026-09-15.** A solve
that ends on a work limit, a time limit or a callback parks its whole
simplex state on the model, and the next solve goes on from exactly where
it stopped: an LP stopped and solved on ends on the uninterrupted answer
to the bit, work and iterations included. Before, the next solve started
warm from the basis the stop left, and 15 of 33102 such resumes landed on
another point of the same optimal face (`bench/measurements/02-247/`).

## 5. Parallelism

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Parallel LP solve | ◐ | ● | ○ | ○ | ◐ | ● | ? |
| Parallel MIP solve | ◐ | ◐ | — | — | ● | ● | ● |
| Deterministic under parallelism | ● | ◐ | — | — | ● | ● | ● |

**JAOS reads ◐ on the LP row, ◐ on the MIP row and ● on determinism.**
Three things run on more than one thread under `--threads N`.
`--algorithm concurrent` runs the dual, the primal and the barrier on the
same LP at the same time and publishes the first to answer (since
2026-09-10). The barrier factors its normal equations on N threads (since
2026-09-22, below). Both trees take rounds of open nodes on N threads under
`--tree-batch` (section 4). The LP row stays ◐ because the simplex runs on
one thread. The rest of the barrier (forming the normal matrix, the solves)
runs on one thread too, by measurement: forming on threads gained nothing
and the solves take under 2% of its instructions (`bench/measurements/02-311/`).
The MIP row stays ◐ because the rounds are off by default: no
round is cheap enough where a node takes a few pivots (`SPECS.md` §5). A
race of four setups of the tree was measured and refused (`mip-race` in
`bench/refusals.txt`).

Determinism reads ● because no clock decides anything in the three. The
concurrent solve's winner is the first arm in a fixed order to answer
inside a work budget, and over the standard 94 its work units and objective
are identical at 1 and at 3 threads. The barrier's factor applies its
updates in a fixed order. A round's nodes are taken in the round's own
order. The benchmark runner's `-j N` is something else, process-level
concurrency with one instance per process.

**The barrier's factor on N threads.** Under `--threads N` the normal
equations' Cholesky solves each block of 32 rows on N threads and applies
the updates inside the block in a fixed order, so the factor, the answer
and the work units are the same at any count (`bench/measurements/02-288/`).
dfl001 falls from 67.7 s to 38.8 s on four threads.

SCIP reads ◐ on the LP row because `lp/threads` threads the LP only when
the LP solver it drives is threaded, and its default, SoPlex, is not. It
reads ● on determinism because ConcurrentSCIP is documented as completely
deterministic and reproducible between runs; the unreleased 11.0
changelog makes the opportunistic mode the default, so that cell may move.
Hexaly's ● on determinism is inferred: its documentation promises the same
results over several runs once the seed and every phase's iteration limit
are fixed, and sets no condition on the thread count. That sentence says
nothing about a tree or about one LP solve, so its deterministic tree
search cell (section 4) and its parallel LP cell stay `?`: its threads
"parallelize the search", and no page says more (searched: `HxParam`,
`HxPhase`, "Hexaly deterministic threads reproducible").

HiGHS's parallel MIP cell reads ◐: its multithreaded tree search arrived in
1.15.0 as a first variant that the release notes call a prototype, and it
runs only when `parallel` is "on" (the default is "choose"). Its
determinism cell was `?` and is ◐: the HiPO paper by its team says the
parallel interior point is deterministic and that changing the thread
count does not change the output. Nothing is documented for the parallel
tree search or the parallel dual simplex.

## 6. Correctness and verification

This is the section JAOS was built around, so it is worth reading carefully —
including the row where the field is ahead.

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| **Bit-identical results across different machines** | ● | ○ | ? | ? | ? | ○ | ? |
| Deterministic on the same machine and version | ● | ● | ● | ● | ● | ● | ● |
| Independent checker shipped with the solver | ● | ○ | ○ | ○ | ○ | ○ | ○ |
| Exact rational LP solutions | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Exact solving with no numerical tolerances | ○ | ○ | ● | ○ | ● | ○ | ○ |
| Machine-checkable certificate of the result | ● | ○ | ◐ | ○ | ● | ○ | ○ |
| Certified bound on suboptimality | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Infeasibility / unboundedness certificate | ● | ◐ | ◐ | ◐ | ● | ● | ○ |
| Irreducible infeasible subsystem (IIS) | ● | ● | ○ | ○ | ● | ● | ● |
| The IIS written out as a model of its own | ● | ● | — | — | ● | ● | ○ |
| Feasibility relaxation of an infeasible model | ◐ | ● | ? | ○ | ◐ | ● | ○ |
| **Prove a basis another solver produced** | ● | ○ | ○ | ○ | ○ | ○ | ○ |
| **Check a point another solver produced** | ● | ○ | ○ | ○ | ○ | ○ | ○ |

**The relaxation row reads ◐.** `jaos_feasrelax` and `jaos relax` move
the rows, the columns or both by the least total amount that makes the
model feasible, integer columns included. On a model whose rows and
integrality admit no point at all, no box is ever wide enough. Since
2026-09-22 the search over the columns still ends by itself there: it
widens the box at most `RELAX_BOX_ROUNDS` times and names the widest box it
tried (`bench/measurements/02-297/`). What keeps it from ●: it cannot prove
that the widest box is empty, which is an integer feasibility question over
an unbounded space (`SPECS.md` §6).
HiGHS (`Highs_feasibilityRelaxation`, since 1.8.0) and Gurobi (`feasRelax`)
document the same operation. SCIP documents none; a PySCIPOpt recipe that
adds slack columns is the nearest, hence ◐. Clp and Hexaly read ○: neither
the Clp executable's full command list nor its class member lists hold
such a call, and Hexaly's own advice is to rewrite a constraint by hand as
an objective placed before the real one. SoPlex stays `?`: not documented
by the vendor (searched: the v8.1.0 CHANGELOG, the FAQ, the command-line
help, the Suite papers, "SoPlex feasibility relaxation").

**The IIS written out as a model.** JAOS's is `jaos_iis_model` and
`jaos iis --write OUT`, and all 29 reference infeasibilities have theirs
written and solved again to INFEASIBLE (`bench/measurements/02-218/`).
HiGHS writes its IIS with `writeIisModel`, SCIP 10 with the `write/iis`
dialog, and Gurobi with `GRBwrite` to an `.ilp` file. Hexaly's cell was
`?` and is ○: `compute_inconsistency()` returns a core whose full method
list prints it as text and has no save call, and `save_environment` writes
the whole model. SoPlex and Clp have no IIS at all, which is what the `—`
says.

**The certificate row's partial cells.** HiGHS returns a ray only when its
simplex found one. When presolve found the infeasibility, it solves the LP
again without presolve to compute the ray, and it documents no ray for a
MIP. SoPlex returns a ray only "if available": by default it does not
re-solve the original LP after a presolved or scaled solve ends infeasible
or unbounded (`bool:ensureray` turns that on), and its `INForUNBD` status
ends without saying which. Clp's `infeasibilityRay()` and `unboundedRay()`
return NULL when there is none: inside branch and bound, on a crunched
model, after the barrier or on a verdict from presolve there is none
unless an option asks. Hexaly's cell was `?` and is ○: `HxSolution` has
no ray, its statuses include no unbounded one, and it reports an
infeasibility as an inconsistency core of expressions.

**SoPlex's certificate cell reads ◐.** In exact mode SoPlex writes the
rational primal values and dual multipliers to files (`-X`, `-Y`, since
7.0.0) and can check them itself (`int:checkmode=2`). It defines no
certificate format and ships no checker. The VIPR certificate is SCIP's.

**Checking a point another solver produced is the tolerance-judged half of
proving its basis.** A point file is one `NAME VALUE` line per
column and nothing else, so `jaos check FILE --point POINT` runs the
independent checker on an answer this library did not compute; `--duals`
brings the multipliers for the dual half. The others ship no checker to
point at anything, which is the same `○` the "independent checker" row
carries and for the same reason.

**Proving a basis another solver produced: the six `○`s are meant.**
`jaos_verify_basis` runs the exact proof over a basis the caller hands in,
with no solve at all, so JAOS reads a model, reads the basis another
solver stopped on -- `jaos_read_mps_basis` reads the format the field
writes one in -- and says over the rationals whether that basis is
an optimal basis of that model. None of the others exposes its own
verifier to a basis it did not produce: SoPlex and SCIP solve exactly and
SCIP emits a VIPR certificate its own `viprchk` checks, which is a
different object, and the rest have no exact checker to point at anything.
That is what the row is asking, and this cell is a real difference rather
than a restatement of the checker row above.

Three rows carry most of the meaning.

**Exact rational LP solutions moved from ○ to ◐ on 2026-09-05.**
After `jaos_verify` proves a basis, every column's value, every row's dual
and the objective are on the model as exact rationals, and `jaos verify
--values` prints them. It is ◐ because it is an exact answer for the bases
the proof reaches (31 of the 110 gate bases at the 128-limb budget,
re-counted on 2026-09-23 with `bench/measurements/02-275/verify-count.sh`,
where 02-275 read 30: `pds-06` proves now, since the aggregator of
2026-09-21 changed its basis) and not an exact solver: the simplex still finds the basis in floating
point, and where the proof is refused there are no values. SoPlex and SCIP
solve over the rationals; JAOS proves and reports what a floating-point
basis is, exactly. The row joins two `SPECS.md` rows: "Exact rational
values of a proved basis", which is done, and "Exact solving with no
tolerances", which is missing.

**The certified bound on suboptimality reads ◐.** The bound is sound, but
alone it cannot separate a wrong vertex from a right one (`SPECS.md` §6).

**The machine-checkable certificate reached ● on 2026-09-07.**
`jaos_write_proof` writes the exact rational proof to a file and
`jaos_check_proof` judges one from the model alone, over the rationals,
with no tolerance and **no basis read**: the file carries none, and the
checker re-derives primal feasibility, dual feasibility and complementary
slackness, which together are sufficient. So a file that passes is proved
optimal rather than consistent with somebody else's basis. `jaos verify
FILE --proof PATH` writes one and `jaos check FILE --proof PATH` judges
one. What stops it is the limb budget and nothing else, and that is
reported as "cannot judge" rather than as a verdict.

**Since D333 the infeasibility ray in that file is exact rather than
rounded.** `jaos_exact_certificate` solves the basis's own system over the
rationals, from the basis a refusal stops on, and the file carries what it
derived; that took the reference infeasibilities from 18 of 29 certifying
with no tolerance to 25 of 29, with none lost and every derivation the limb
budget admits certifying.

**Since D328 the file carries all three outcomes**: a Farkas certificate
and an unbounded ray as well, checked the same way and with no tolerance.
Those two need no proof step at all, because the vector the solve
publishes is already exact. When D328 was measured over the reference
sets (`bench/measurements/02-211/`), 18 of the 29 pinned infeasibles
certified exactly; D333 later took that to 25 of 29 (above). Every one of
the 28 optimum proofs that existed passed the file checker, which shares
no code with the prover that made it.

**The IIS, and this row was wrong until 2026-09-04.** It read "among the open
solvers here only Gurobi documents the feature", with HiGHS and SCIP at ○.
Both have it, and both had it before this page was first written. HiGHS ships
`Highs::getIis` with documented options (`iis_strategy`, whose value 8 is
"Find true IIS", and `iis_time_limit`), and HiGHS 1.15.0's release notes fix
bugs in it. SCIP 10.0 ships an IIS Finder, section 3.9 of the Suite 10.0
paper. The honest claim left is narrower: JAOS has one, and it is not
distinctive.

What JAOS's is: `jaos_iis` names, for an INFEASIBLE answer, a set of bound
sides (a row's or a column's lower or upper bound) that is infeasible on its
own and becomes feasible when any one of them is dropped. Chinneck and
Dravnieks's sensitivity filter over the published certificate, then their
deletion filter, one warm re-solve per candidate on a private copy of the
model. The solver itself is the oracle: on **28** of the 29 reference
infeasibles the members alone re-solve INFEASIBLE and each one dropped
re-solves OPTIMAL, and all 29 reproduce. The 29th is `cplex2`, infeasible by
less than the feasibility tolerance, which keeps three of its 232 members a cold
re-solve does not need; the fixpoint pass that would drop them is refused on
cost.

**Cross-machine bit-identity.** Gurobi's own documentation states it is
deterministic on the same machine but not between different machines, and that
an LP with several optima can return a different one on different hardware.
JAOS gives the stronger guarantee: the same bits on every machine and every run.
HiGHS's cell was `?` and is ○: its team's HiPO paper says the same
configuration on different machines "can lead to different results", from
the compiler's optimisations and the BLAS library. The other columns stay
`?`: not documented by the vendor. SoPlex (7.0.1) and SCIP (9.0.1) added
the build flag `-ffp-contract=off` "to enhance reproducibility across
different systems", which states an aim and makes no claim. Hexaly promises
the same results over several runs with a fixed seed and says nothing of
other hardware. Searched: the CHANGELOGs, FAQs, INSTALL files and Suite
papers of SoPlex and SCIP, Clp's README, user guide, FAQ and issues, and
Hexaly's `HxPhase` and export pages, with "deterministic", "reproducib",
"different machines" and "platform".

**Exact arithmetic and certificates.** JAOS ships an independent checker that no
other solver here does, and that is a real difference. But it is a
floating-point checker judging against tolerances. SoPlex has solved LPs exactly
over the rationals since version 2.1 and added precision boosting in 6.0; SCIP
10.0 solves MILPs with no numerical tolerances at all and can emit a VIPR
certificate that an external program verifies in exact rational arithmetic.
JAOS proves a floating-point basis optimal over the rationals and reports
its exact values (`SPECS.md` §6, done), but it does not solve exactly:
"exact solving with no tolerances" is the row `SPECS.md` marks missing, and
it is what JAOS lacks against SoPlex and SCIP here.

## 7. Input and output

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Read MPS (fixed and free) | ● | ● | ● | ● | ● | ● | ○ |
| Read LP format | ● | ● | ● | ● | ● | ● | ○ |
| Read compressed input | ● | ● | ● | ● | ● | ● | ● |
| Write compressed output | ● | ? | ? | ◐ | ? | ● | ● |
| Direct load from arrays | ● | ● | ● | ● | ● | ● | ● |
| Write MPS | ● | ● | ● | ● | ● | ● | ○ |
| Write LP | ● | ● | ● | ● | ● | ● | ○ |
| Write a solution file | ● | ● | ● | ● | ● | ● | ● |
| Point and duals files | ● | ● | ◐ | ◐ | ● | ● | ○ |
| Read other solvers' solution files | ● | ● | ○ | ○ | ● | ○ | ○ |
| Read and write `.nl`, OSiL, QPLIB and CBF | ● | ○ | ○ | ○ | ◐ | ○ | ? |
| Indicator constraints in MPS and LP files | ● | ○ | — | — | ● | ● | ○ |
| Reject unsupported constructs with a line number | ● | ? | ? | ● | ? | ● | ? |

**Other formats.** JAOS reads and writes the text `.nl`, OSiL, QPLIB and
CBF. SCIP documents readers for `.nl` (`reader_nl`) and OSiL
(`reader_osil`); the reader documentation searched shows no QPLIB reader
and did not show a CBF one, hence ◐ (scipopt.org, "File Readers",
`reader_nl.cpp`). HiGHS reads MPS, LP and its own EMS; SoPlex reads MPS
and LP; Clp reads MPS; Gurobi reads MPS, LP and its own formats. None of
the four reads these files. Hexaly: not checked against the vendor's
documentation; its own model files are HXM and HXB.

**Indicators in files.** SCIP's MPS reader documents indicators on linear
constraints, and its LP reader takes the CPLEX extensions; Gurobi's MPS
and LP formats carry indicator constraints. HiGHS has no indicator
constraint. SoPlex and Clp have no integer columns, hence —. Hexaly reads
no MPS or LP file.

**Compressed output.** Every JAOS writer compresses when the path ends in
`.gz`, over an encoder written in this repository for the reason the
decoder was: `gzip -t` accepts all 139 gate instances and `gzip -dc`
returns the plain write byte for byte, at 1.3387x the size of `gzip -9`
(`bench/measurements/02-217/`). Gurobi writes `.gz`, `.bz2`, `.zip`, `.7z`
and `.xz` when the system has the utilities. Clp writes gzip through
CoinUtils' `CoinMpsIO` but not through `ClpModel::writeMps`, hence ◐.
Hexaly compresses its own HXB and HXM files. HiGHS, SoPlex and SCIP
document reading `.gz` and say nothing about writing it, so those stay
`?`: not documented by the vendor (searched: HiGHS's guide, executable
page, options and release notes; SoPlex's INSTALL.md, help text and
CHANGELOG; SCIP's INSTALL.md, CHANGELOG, FAQ and PySCIPOpt docs; "gz",
"compress", "zlib", "write").

**Hexaly reads and writes no MPS or LP file.** Its own documentation says
so: a model is built in its modelling language or its APIs and saved as
HXB or HXM. The ○s in its column of this section are that, and its ● on
compressed input is its own HXB format.

**Reject with a line number.** Clp's documented MPS messages carry the
line (`Bad image at line %d`), and Gurobi's LP reader names the line in
its error. HiGHS, SoPlex, SCIP and Hexaly stay `?`: not documented by the
vendor. HiGHS names the line only for an options file (1.15.0). SoPlex's
CHANGELOG says its readers reject bad input and never shows the message.
SCIP documents no reader message outside its source listings, which were
not read. Hexaly's `HxError` line number is a line of Hexaly's own
source. Searched: each vendor's guide, CHANGELOG or release notes, FAQ and
issues, with "line", "parse" and "syntax error".

**Point, duals and other solvers' solution files.** JAOS writes a point
and a duals file, one `NAME VALUE` line each (`--write-point`,
`--write-duals`), and `jaos_read_point` and `jaos_read_duals` read those
back and the shapes Gurobi, MIPLIB, SCIP, HiGHS and CPLEX write. HiGHS
writes the primal and dual solution (`write_solution_to_file`), reads a
solution back, and reads a MIPLIB solution file since 1.8.1. SoPlex reads
◐: it writes the point and the duals (`-x`, `-y`) and reads no point back.
Clp reads ◐: its text `solution` file is write-only, and the file it reads
back is its own binary `saveSolution` file. SCIP writes and reads `.sol`
files, reads CPLEX's XML solution files and `.mst` starts, and writes duals
for a pure LP only. Gurobi writes the point (SOL) and the duals (ATTR) and
reads both back. Its other-solvers cell is ○: every solution format it
lists as readable is its own, though its SOL shape is the `name value`
line others write. Hexaly saves a solution only inside its HXB file.

JAOS's LP reader covers a CPLEX-style core; the exact subset is in
`docs/format-support.md`.

**What JAOS writes it reads back as the same model**, checked field by
field and name by name. Over the 139 gate instances that holds through
MPS for every file, and through LP for every file, under the model's own
names and under positional ones alike (`bench/measurements/02-274/`,
2026-09-21). A name the LP dialect cannot spell, as Netlib's often cannot
(they start with digits and hold `*` and `-`), is written under `c<j+1>`
or `r<i+1>` with a comment map at the top of the file, and `jaos convert
IN OUT --positional` takes every name off first. A row with no
coefficients is written as a zero term and read back as the empty row it
was.

**Write LP reads ● since 2026-09-19.** A free row is written `>= -inf`,
which HiGHS reads as a free row too, and a ranged row as two rows joined by
a `\ range` comment that JAOS's reader folds back; the two-sided form D239
chose is JAOS's own and HiGHS refuses it or, with a coefficient of 1 in
front, silently reads another model. Read against HiGHS 1.15.1 over 2000
generated models, 4000 MPS and LP files: before the change 38 of 51 LP
files at one seed and 9 of 38 MIPs through MPS disagreed (the MPS one an
integer column with no bound, which HiGHS reads as a binary, now written
with `PL`); after it none does (`bench/measurements/02-252/`).

Write MPS reads ● and still has three refusals, which is not a contradiction:
two of them are shapes the format itself has no syntax for, and the third is a
ranged row whose two bounds no RANGES entry reconstructs exactly. All three
are refused with the row named. Nothing on the gate reaches any of them. The
comparison this row invites is what the other solvers do with the same input,
and this page has not measured that.

## 8. Using it from another language

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Command-line tool | ● | ● | ● | ● | ● | ● | ● |
| C or C++ | ● | ● | ● | ● | ● | ● | ● |
| Python | ● | ● | ● | ● | ● | ● | ● |
| Julia | ● | ● | ● | ● | ● | ● | ◐ |
| Java, .NET | ● | ◐ | ○ | ○ | ◐ | ● | ● |
| R | ● | ◐ | ○ | ○ | ◐ | ● | ○ |
| AMPL, GAMS and similar modelling systems | ◐ | ● | ○ | ● | ● | ● | ○ |
| Python package installable with pip | ● | ● | ● | ● | ● | ● | ● |
| `make install` with a pkg-config file | ● | ● | ◐ | ● | ◐ | — | — |
| CMake package | ● | ● | ● | ○ | ● | — | — |
| Windows and macOS builds | ● | ● | ● | ● | ● | ● | ● |

**The modelling-system row reads ◐ since 2026-09-19**: `jaos STUB -AMPL`
answers AMPL's solver protocol, `STUB.nl` in and `STUB.sol` out, which
AMPL, Pyomo's `asl:` interface and JuMP's AmplNLWriter all use. Pyomo
6.10 and JuMP 1.31 read the answers back
(`bench/measurements/02-257/`). Since 2026-09-22 the `.nl` reader takes a
body of degree two, so a Pyomo quadratic objective reads. What keeps the
row from ●: GAMS needs a link library of its own, and a `.nl` body above
degree two is refused by line. Hexaly reads ○: its own page
says it works with none of Pyomo, PuLP, AMPL, GAMS or AIMMS, and GAMS
dropped it in version 38 (2022).

**The Julia row reads ● since 2026-09-19**: `julia/JAOS` is a Julia
package over `libjaos.so`, with the C calls through `ccall` and
`JAOS.Optimizer`, a MathOptInterface optimizer that takes a whole model
through `copy_to`. JuMP uses it directly (`Model(JAOS.Optimizer)`), with
no `.nl` file in between, so quadratic objectives, quadratic rows and
cones reach it too. `make julia-test` runs MathOptInterface's own
conformance suite on it: 4717 checks pass, and 4 tests are left out
because JAOS refuses what they ask by design (three solve a non-convex
quadratic row, and one needs an IIS that keeps integrality, while
`jaos_iis` explains the linear relaxation). Since 2026-09-23 a loaded
model takes bound, row, cost, sense and start changes in place, so JuMP
re-solves warm; lazy constraints and user cuts run on the node callback;
`ResultCount` reads the solution pool; and the `JAOS.` functions reach
every C call the Python package reaches. The package is not in Julia's
General registry, and it finds the library the way the Python package
does.

**The Java, .NET row and the R row read ● since 2026-09-23**:
`dotnet/Jaos` (.NET 8, P/Invoke), `java/src/org/jaos` (Java 22 and later,
the foreign-function API, so no C glue) and `R/jaos` (an R package over
`.Call`) each have their checks (`make dotnet-test`, `make java-test`,
`make r-test`), and all three reach every C call the Python package
reaches: .NET and Java call all 201, and R calls 143 and reaches the
other 58, the typed option setters and getters, by name.

**The install row.** `make install` puts the header, both library forms,
the tool and a generated `jaos.pc` under `PREFIX`, with `DESTDIR`
staging, and `tests/install.sh` compiles a program against the installed
tree on every `make test`. HiGHS and Clp carry a pkg-config template
(`highs.pc.in`, `clp.pc.in`) at their repositories' roots. SoPlex and SCIP
were `?` and read ◐: both document `make install`, and both install a CMake
package config (`soplex-config.cmake`, `scip-config.cmake`) and no `.pc`
file. Gurobi and Hexaly ship binaries rather than a build, which is what
the `—` says, and neither ships a CMake config (Gurobi's support page hands
the user a `FindGUROBI.cmake` template to copy). Clp builds with autotools
and ships no CMake config.

**The packaging rows.** JAOS's Python package installs with `pip install .`
from the repository; it is not on PyPI. HiGHS (`highspy`), SCIP
(`pyscipopt`), Gurobi (`gurobipy`) and Hexaly (`hexaly`, which needs a
Hexaly installation) are on PyPI. SoPlex's PySoPlex installs with `pip
install .` from its GitHub sources, and its CI builds it against SoPlex
7.0. Clp's cell stands on CyLP, a COIN-OR project on PyPI (`cylp`) that
Clp's own README does not list. JAOS builds on Windows with mingw-w64 and
clang-cl and on macOS with GCC 14 in CI (`docs/build.md`). The other five
ship Windows and macOS binaries; SCIP's macOS build is arm64 only.

**Hexaly's language row.** Hexaly's APIs are Python, Java, C# and C++. Its
Julia cell is ◐ for a third-party wrapper that Hexaly does not support,
and its old R package was archived on CRAN on 2026-04-22.

**HiGHS's and SCIP's partial language cells.** HiGHS has no Java binding;
.NET is covered by its C# NuGet package `Highs.Native`. Its R cell reads
◐ because its team does not maintain the R package `highs` on CRAN, which
is user-developed. SCIP has no .NET binding; Java is covered by JSCIPOpt,
which the SCIP team maintains. SCIP's R cell reads ◐ for the same reason
as HiGHS's: the CRAN package `scip` is written outside the SCIP team.
Clp's R cell reads ○: its CRAN packages `clpAPI` and `ROI.plugin.clp`
were archived in 2021 and 2022.

## 9. Controlling a solve

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Time limit | ● | ● | ● | ● | ● | ● | ● |
| Deterministic work limit | ● | ◐ | ◐ | ◐ | ● | ● | ● |
| Set primal and dual tolerances | ● | ● | ● | ● | ● | ● | ○ |
| Logging with verbosity levels | ● | ● | ● | ● | ● | ● | ● |
| Progress callback that can stop the solve | ● | ● | ◐ | ● | ● | ● | ● |
| Callbacks that steer the search | ● | ◐ | ○ | ○ | ● | ● | ○ |
| Choose the algorithm | ● | ● | ● | ● | ● | ● | — |
| Sensitivity analysis and ranging | ● | ● | ○ | ● | ○ | ● | ○ |
| Options as name-value strings and a parameter file | ● | ● | ● | ◐ | ● | ● | ◐ |
| Thread count | ● | ● | ○ | ○ | ● | ● | ◐ |

**The steering row reached ◐ on 2026-09-08 and ● on 2026-09-24.** One
node callback (`jaos_set_node_callback`) sees every node's point once
solved and cut, and every point a heuristic would make an incumbent; it
adds rows that hold for every solution (user cuts, lazy constraints, the
point rejected when a row cuts it), names the column to branch on, and
since 2026-09-24 hands the tree a solution of the caller's
(`jaos_node_add_solution`). JAOS has no callback at the presolve or inside
the LP, which SCIP and Gurobi also offer.
HiGHS's cell reads ◐ for the opposite gap: its user-solution callback
(`kCallbackMipUserSolution`) hands the tree a solution, and none of its
callback types adds a user cut or a lazy constraint or picks the branching
column.

JAOS's "choose the algorithm" is `jaos_set_algorithm`, `--algorithm` and
the `algorithm` option: `dual` (the default), `primal`, `barrier`, `pdlp`
or `concurrent`.

JAOS's deterministic work limit is worth noting as a ● where most of the field
is weaker: the budget is counted in reproducible work units, so the same model
stops at the same point on any machine. HiGHS, SoPlex and Clp read ◐
because what they limit deterministically is the iteration count, not the
work. Hexaly limits iterations per phase and documents the result as
reproducible with a fixed seed. SoPlex reads ◐ on the progress row: it
takes an interrupt flag and documents no callback.

Hexaly's tolerance and ranging cells were `?` and are ○: its full
parameter list has no feasibility or optimality tolerance (only a gap
limit), and `HxSolution` offers no dual, reduced cost or range. On the
options row, Clp reads ◐: it takes any option by name on its command line,
and 1.17.11 documents no parameter file (commands can only come from
standard input or an environment variable). Hexaly reads ◐: its command
line takes `name=value` for its parameters, and it documents no plain
options file. SoPlex (`--loadset`), SCIP (`.set` files), HiGHS
(`--options_file`) and Gurobi (`.prm`) have both. On the thread row,
SoPlex and Clp are single-threaded: SoPlex's FAQ says there is no parallel
version, and Clp's `setNumberThreads` is documented as "not really being
used". Hexaly reads ◐ because it calls its thread count "indicative" and
may use more. SCIP 10.1.0 added `-t`, which sets the thread count of its
concurrent solve.

## 10. Licence and distribution

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Open source | ● | ● | ● | ● | ● | ○ | ○ |
| Free for commercial use | ● | ● | ● | ● | ● | ○ | ○ |
| No external dependencies | ● | ◐ | ○ | ○ | ○ | — | — |

JAOS is Apache 2.0 with no dependencies at all. That is unusual and it is a
deliberate constraint, not an accident of youth. HiGHS reads ◐: its README
says no third-party dependency is required and zlib is optional, and its
new interior point solver, HiPO, links Metis and OpenBLAS (1.15.0).
SoPlex and SCIP read ● on commercial use since 2026-09-22: both have been
under Apache 2.0 since SoPlex 6.0.3 and SCIP 8.0.3 (November 2022), and
the cells were ◐ with no gap named.

---

## What the matrix says

**JAOS is present in every section of this page.** It solves LP and MILP
completely, and QP, QCP and SOCP, with integer columns or without, in
part; `SPECS.md` §1 names each gap. Parallelism reads ◐ on the LP row (the
concurrent solve and the barrier's factor, with the simplex on one thread),
● on the deterministic tree search (rounds of nodes in both trees), and ◐
on a parallel MIP solve (the rounds are off by default). Exact solving is
the one JAOS ○ on a row `SPECS.md` marks missing. The other JAOS ○ cells
are rows it marks out of scope (nonlinear, constraint programming,
black-box, GPU).

**Three things JAOS has that the field mostly does not.** Bit-identical
results across machines, which Gurobi explicitly does not promise. An
independent checker of the solver's own answer, shipped with the solver,
which none of the others ship: SCIP ships `viprchk`, and that verifies a
certificate SCIP emits, which is a different object. A budget counted in
reproducible work units rather than seconds. All three come from the same
decision, and it is the project's distinguishing feature.

**The checker and the exact verifier also judge answers JAOS did not
compute.** `jaos_verify_basis` proves a basis the caller hands in, over
the rationals with no tolerance and with no solve at all, and
`jaos check --point` runs the floating-point checker on a point file
another program wrote. A basis arrives in the MPS basis format the field
exchanges one in. None of the others exposes its verifier to an answer it
did not produce.

**Where the field is ahead on JAOS's own subject.** SoPlex and SCIP solve
exactly over the rationals, and SCIP emits certificates an external
checker verifies. JAOS proves a floating-point basis exactly and reports
its exact values, and it does not solve exactly. Its checker is a
floating-point checker judging against tolerances, and a check is not a
proof.

**This page counts features, not speed.** The measured time against HiGHS,
SoPlex and Clp on LP, and against HiGHS and SCIP on MIP, is in
`bench/compare/README.md`, and closing that gap moves no cell here. JAOS is
timed against other solvers on LP and MIP only. A QP rung and a conic rung
are a row in `TODO.md`.

---

## Sources

Cells for HiGHS, SoPlex, Clp, SCIP, Gurobi and Hexaly were taken from public
documentation on the date above. JAOS's own cells come from `SPECS.md`
and the measured results in `bench/`.

- HiGHS solver capabilities and parallelism: <https://ergo-code.github.io/HiGHS/dev/solvers/> and <https://ergo-code.github.io/HiGHS/stable/parallel/>
- HiGHS 2026 development, QP and GPU PDLP: <https://highs.dev/assets/HiGHS_Newsletter_26_0.pdf>
- The 2026-09-21 pass, by row:
  - IIS written as a model: HiGHS <https://ergo-code.github.io/HiGHS/stable/guide/advanced/>, SCIP's changelog <https://github.com/scipopt/scip/blob/master/CHANGELOG> and the Suite 10.0 paper §3.9 <https://arxiv.org/pdf/2511.18580>, Gurobi <https://docs.gurobi.com/projects/optimizer/en/current/reference/c/inputoutput.html> and <https://docs.gurobi.com/projects/optimizer/en/current/reference/fileformats.html>
  - Feasibility relaxation: HiGHS <https://ergo-code.github.io/HiGHS/stable/interfaces/c_api/> and <https://github.com/ERGO-Code/HiGHS/releases/tag/v1.8.0>, SCIP (PySCIPOpt recipe) <https://github.com/scipopt/PySCIPOpt/discussions/854>, Gurobi <https://docs.gurobi.com/projects/optimizer/en/current/features/infeasibility.html>
  - Hexaly's formats, IIS, command line, determinism and work limit: <https://www.hexaly.com/gurobi>, <https://www.hexaly.com/docs/last/cppapi/optimizer/hexalyoptimizer.html>, <https://www.hexaly.com/docs/last/features/inconsistency.html>, <https://www.hexaly.com/docs/last/modelerreference/mainmode.html>, <https://www.hexaly.com/docs/last/java/com/hexaly/optimizer/HxPhase.html>, <https://github.com/NexOR-Optimization/Hexaly.jl>, <https://cran.r-project.org/web/packages/localsolver/index.html>, <https://www.gams.com/41/docs/RN_38.html>
  - Clp's compressed output, line numbers and iteration limit: <https://coin-or.github.io/CoinUtils/Doxygen/classCoinMpsIO.html>, <https://coin-or.github.io/Clp/messages.html>, <https://coin-or.github.io/Clp/Doxygen/classClpModel.html>
  - Gurobi's LP reader naming the line: <https://support.gurobi.com/hc/en-us/community/posts/10103119821329-Error-reading-LP-format-file-Unrecognized-constraint-RHS-or-sense>
  - pkg-config templates: <https://github.com/ERGO-Code/HiGHS> and <https://github.com/coin-or/Clp>
  - SCIP's LP threads and determinism: <https://github.com/scipopt/scip/blob/master/CHANGELOG> and the Suite 4.0 report <https://optimization-online.org/wp-content/uploads/2017/03/5895.pdf>
  - SoPlex's iteration limit and interrupt flag: <https://github.com/scipopt/soplex/blob/master/CHANGELOG>
  - Versions: <https://github.com/scipopt/soplex/releases/tag/v8.1.0>, <https://github.com/scipopt/scip/releases/tag/v10.1.0>, <https://pypi.org/project/hexaly/>
- The 2026-09-22 pass, by solver:
  - HiGHS: options <https://ergo-code.github.io/HiGHS/dev/options/definitions/>, parallel <https://ergo-code.github.io/HiGHS/dev/parallel/>, callbacks <https://ergo-code.github.io/HiGHS/dev/callbacks/>, C API <https://ergo-code.github.io/HiGHS/dev/interfaces/c_api/>, C# <https://ergo-code.github.io/HiGHS/dev/interfaces/csharp/>, other interfaces <https://ergo-code.github.io/HiGHS/dev/interfaces/other/>, types <https://ergo-code.github.io/HiGHS/dev/structures/enums/> and <https://ergo-code.github.io/HiGHS/dev/structures/classes/HighsLp/>, CMake <https://github.com/ERGO-Code/HiGHS/blob/master/cmake/README.md>, release notes 1.8.0, 1.8.1, 1.12.0, 1.13.0 and 1.15.0 <https://github.com/ERGO-Code/HiGHS/releases>, SOS <https://github.com/ERGO-Code/HiGHS/issues/2148>, HiPO paper <https://arxiv.org/pdf/2508.04370>, and the HiGHS Workshop 2025 slides by Hall, Turner and Galabova
  - SoPlex: CHANGELOG, INSTALL.md and LICENSE at v8.1.0 <https://github.com/scipopt/soplex/tree/v8.1.0>, the command-line help <https://manpages.debian.org/unstable/soplex/soplex.1.en.html>, PySoPlex <https://github.com/scipopt/PySoPlex>, the Suite 9.0 paper <https://arxiv.org/pdf/2402.17702>
  - Clp: FAQ <https://coin-or.github.io/Clp/faq.html>, user guide <https://www.coin-or.org/Clp/userguide/>, class references <https://coin-or.github.io/Clp/Doxygen/classClpModel.html>, <https://coin-or.github.io/Clp/Doxygen/classClpSimplex.html> and <https://coin-or.github.io/Clp/Doxygen/classClpSolve.html>, the 1.17.11 release <https://github.com/coin-or/Clp/releases/tag/releases%2F1.17.11>, the executable's online help, CyLP <https://pypi.org/project/cylp/>, parameter-file discussion <https://github.com/coin-or/Clp/discussions/219>
  - SCIP: CHANGELOG, INSTALL.md and FAQ text at v10.1.0 <https://github.com/scipopt/scip/tree/v10.1.0>, the 10.1.0 release <https://github.com/scipopt/scip/releases/tag/v10.1.0>, the Suite 10.0 report <https://arxiv.org/abs/2511.18580>, PySCIPOpt <https://pyscipopt.readthedocs.io/en/latest/>, JSCIPOpt <https://github.com/scipopt/JSCIPOpt>
  - Gurobi: parameters <https://docs.gurobi.com/projects/optimizer/en/current/reference/parameters.html>, variable attributes <https://docs.gurobi.com/projects/optimizer/en/current/reference/attributes/variable.html>, constraints <https://docs.gurobi.com/projects/optimizer/en/current/concepts/modeling/constraints.html>, callback codes <https://docs.gurobi.com/projects/optimizer/en/current/reference/numericcodes/callbacks.html>, file formats <https://docs.gurobi.com/projects/optimizer/en/current/reference/fileformats.html>, platforms <https://docs.gurobi.com/projects/optimizer/en/current/reference/releasenotes/platforms.html>, fixed issues <https://docs.gurobi.com/projects/optimizer/en/current/reference/releasenotes/fixedbugs.html>, the 12.0 slides <https://cdn.gurobi.com/wp-content/uploads/Gurobi-12.0_Webinar.pdf>, its developers' presolve paper <https://opus4.kobv.de/opus4-zib/files/6037/Presolve.pdf>, CMake <https://support.gurobi.com/hc/en-us/articles/360039499751-How-do-I-use-CMake-to-build-Gurobi-C-C-projects>, node selection <https://support.gurobi.com/hc/en-us/community/posts/4404024478737-How-to-program-a-pure-Branch-Bound>
  - Hexaly: `HxParam` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxparam.html>, `HxSolution` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxsolution.html>, `HxInconsistency` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxinconsistency.html>, `HxOperator` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxoperator.html>, `HxCallbackType` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxcallbacktype.html>, `HxError` <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxerror.html>, initial solutions <https://www.hexaly.com/docs/last/features/initialsolution.html>, command line <https://www.hexaly.com/docs/last/modelerreference/commandline.html>, 14.0 <https://www.hexaly.com/announcements/hexaly-14-0>, ISMP 2024 abstract <https://www.hexaly.com/events/meet-the-hexaly-team-at-ismp-2024>, installation <https://www.hexaly.com/docs/last/installation/pythonsetup.html>
- Gurobi 13.0 problem classes and new methods: <https://www.gurobi.com/resources/reports/what-s-new-in-gurobi-13-0>
- Gurobi determinism across machines: <https://support.gurobi.com/hc/en-us/articles/360031636051-Is-Gurobi-deterministic> and <https://support.gurobi.com/hc/en-us/articles/360045849232-Why-does-Gurobi-perform-differently-on-different-machines>. The explicit negative is stated by Gurobi staff here — "running Gurobi on different hardware may lead to different optimal solutions being returned", "deterministic behavior is only guaranteed when repeating runs on an identical setup": <https://support.gurobi.com/hc/en-us/community/posts/23910882878993-Deterministic-behaviour-in-different-machines>
- HiGHS IIS options (`iis_strategy`, `iis_time_limit`): <https://ergo-code.github.io/HiGHS/stable/options/definitions/>
- HiGHS first-order solvers and GPU (cuPDLP-C, HiPDLP): <https://ergo-code.github.io/HiGHS/dev/solvers/> and the 1.15.0 release notes <https://github.com/ERGO-Code/HiGHS/releases/tag/v1.15.0>
- Gurobi PDHG and GPU: <https://www.gurobi.com/product/whats-new>, <https://docs.gurobi.com/projects/optimizer/en/current/reference/parameters.html> and <https://support.gurobi.com/hc/en-us/articles/43498824105873-Installing-and-Running-GPU-enabled-Gurobi>
- SoPlex reads and writes LP format: its own description, "a standalone solver reading MPS or LP format files via a command line interface", plus the `--writefile` option — <https://github.com/scipopt/soplex> and <https://soplex.zib.de/doc/html/FAQ.php>. zib.de returned HTTP 429 on 2026-09-04; the claim was taken from SoPlex's own repository description and release notes, and is worth one more pass when the site is reachable
- Release histories used for the version line: <https://github.com/ERGO-Code/HiGHS/releases>, <https://github.com/scipopt/soplex/releases>, <https://github.com/coin-or/Clp/releases>, <https://www.zib.de/news/scip-optimization-suite-1000-released>, <https://support.gurobi.com/hc/en-us/articles/360048138771-Gurobi-release-and-support-history>, <https://www.hexaly.com/announcements/hexaly-optimizer-15-0>
- SoPlex as an exact LP solver: <https://soplex.zib.de/doc/html/EXACT.php>
- SCIP Optimization Suite 10.0, exact solving and certificates: <https://arxiv.org/pdf/2511.18580>
- VIPR certificate format: <https://github.com/scipopt/vipr>
- Hexaly Optimizer model types: <https://www.hexaly.com/hexaly-optimizer> and <https://www.hexaly.com/docs/last/modelingprinciples/index.html>

No solver's source code was read to produce this page. That rule applies to implementation, and it is respected here: everything above comes
from documentation a user can read.
