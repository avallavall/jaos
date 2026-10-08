# Feature matrix — JAOS against the field

This page compares JAOS with six other solvers. `SPECS.md` holds JAOS's own
target and the status of each row. A JAOS cell at ◐ or ○ is a `SPECS.md`
row that is not done. The rows about JAOS's own tooling (copying a model,
names, statistics, whether the model is a MIP, `diff` and `show`) are left
out. The `SPECS.md` row "Exact rational proof of the final basis" appears
here as two rows of section 6.

**How to read it**

| symbol | meaning |
|---|---|
| ● | present and complete |
| ◐ | present but partial — the gap is named in the notes |
| ○ | absent |
| — | not applicable to that solver's scope |
| ? | the vendor documents nothing either way; the note says what was searched. Treat it as unknown, not as absent |

**The solvers.** Every cell outside JAOS's column comes from public
documentation, checked on 2026-09-22. No cell comes from a run here. The
timings against HiGHS, SoPlex, Clp and SCIP are in
`bench/compare/README.md`. CPLEX, Xpress, COPT and Mosek are in Gurobi's
class and are left out to keep the table readable.

Versions: JAOS 0.5.0 · HiGHS 1.15.1 · SoPlex 8.1.0 · Clp 1.17.11 ·
SCIP 10.1.0 · Gurobi 13.0.3 · Hexaly 15.0.

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

SoPlex and Clp are LP solvers, so their integer rows read "—". JAOS's ○
cells here are rows `SPECS.md` marks out of scope.

- *QP*: the barrier solves a convex quadratic objective, and a push
  finishes the point on its active bounds. The largest of QPLIB's convex
  QPs stop at the work limit, and Maros-Meszaros `values` is refused as not
  convex (`SPECS.md` §1). Clp's FAQ says its barrier solves convex QPs.
- *QCP, SOCP*: second-order cones and convex quadratic rows with one
  finite side go to a conic interior point (`src/conic.c`). An answer is
  published only when the checker takes it. A few QPLIB QCQPs and quadratic
  rows over more than `CONIC_QC_DENSE` columns are missing (`SPECS.md` §1).
- *MIQP, MIQCP*: integer columns run in the linear tree on barrier
  relaxations, or beside cones in `src/conictree.c`. Most of QPLIB's
  mixed-integer instances do not finish within 1e11 work units
  (`SPECS.md` §1).
- *Constraint programming*: SCIP has cumulative and logical constraints, a
  FlatZinc reader and a CP/SAT mode. It has no alldifferent and no other
  global constraint.

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

- *Primal simplex*: steepest-edge pricing, a composite phase 1 and a Harris
  ratio test. Six of the 94 standard instances run past 10x the dual's work
  (`bench/results/primal.txt`).
- *Barrier*: Mehrotra's predictor-corrector over the sparse Cholesky of
  `src/chol.c`. The crossover and the lack of a rule that picks the
  augmented system for an LP keep it from ● (`bench/results/barrier.txt`).
- *Crossover*: a primal push puts the barrier's point on a basis, and the
  primal simplex finishes. On 14 of the 94 standard instances that costs
  more than ten dual solves (`bench/measurements/02-287/`). HiGHS's ● is
  for its interior point solvers; its first-order solvers have no
  crossover.
- *First-order method*: primal-dual hybrid gradient with the restarts of
  Applegate et al. (2021), deterministic, finished by the crossover. No set
  where it beats the dual has been read (`bench/results/pdlp.txt`).
- *GPU*: out of scope for JAOS. HiGHS runs cuPDLP-C and HiPDLP on NVIDIA
  GPUs. Gurobi 13.0 runs PDHG on a GPU (`PDHGGPU=1`), for a MIP at the root
  only.
- *Concurrent solve*: the dual, the primal and the barrier race under a
  growing work budget, so the winner is the same on every machine. No set
  has been found where the barrier wins (`bench/results/concurrent.txt`).
  HiGHS's methods do not race.
- *Hexaly*: it uses "simplex methods when linear" and names no variant, no
  crossover and no race, hence `?`. Its interior point is not offered as an
  LP method, hence ◐ (searched: the docs index, `HxParam`, the 13.0 to
  15.0 release notes).

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

- *Presolve*: empty rows and columns, singleton rows, cost-0 singleton
  columns, fixed columns, forcing and redundant rows, implied free column
  singletons, and an aggregator (`src/aggregate.c`). Duplicate rows and
  columns, dominated columns, bound tightening and dual fixing are missing.
  Each was measured and refused (`bench/refusals.txt`).
- *Hexaly*: its API has no basis. It documents no scaling, LU or
  hyper-sparse solves (searched: the docs index, `HxParam`, the release
  notes).
- *Resume*: JAOS parks its simplex state at a limit, and the next solve
  ends on the uninterrupted answer to the bit. Clp restarts from the basis
  the stop left. SoPlex says nothing of a second `optimize()` after a limit
  (searched: the v8.1.0 CHANGELOG, the FAQ, the Suite papers).

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

- *Strong branching*: `--reliability N` exists and is off, measured worse
  (`bench/refusals.txt`).
- *Node selection*: Gurobi names no rule, and no parameter selects one
  (searched: the parameter reference, the MIP logging page, the 12.0
  slides).
- *MIP restarts*: `--restart` is on in network mode only. A MIP presolve
  that removes what the restart fixed is missing
  (`bench/measurements/02-343/`).
- *Cutting planes*: Gomory, knapsack cover, MIR, clique and flow cover cuts
  run by default. Zero-half and lifted cover cuts are off by measurement
  (`bench/measurements/02-361/`).
- *MIP presolve*: coefficient tightening, implied bounds, a parity step and
  the clique table run by default. Probing (`--probing`) is off by
  measurement (`SPECS.md` §4). HiGHS documents no coefficient tightening.
- *Propagation and reduced-cost fixing*: reduced-cost fixing is on. Node
  propagation is off on a linear MIP by measurement
  (`bench/measurements/02-335/`). Hexaly names "propagation methods" and no
  reduced-cost fixing.
- *Primal heuristics*: rounding, dives, the feasibility pump, lock
  rounding, a feasibility jump, RINS and RENS run by default. Local
  branching is off by measurement (`bench/measurements/02-325/`).
- *MIP start and cutoff*: HiGHS documents no MIP cutoff. Hexaly takes a
  start, and its `objective_threshold` stops the search instead of cutting
  it off.
- *Node limit and callback*: Hexaly has neither.
- *Semi-continuous, SOS, indicators*: HiGHS has no SOS (issue 2148) and no
  indicator constraints. Hexaly writes all three with logical operators,
  and SOS2 only through `piecewise`.
- *Deterministic parallel tree search*: both JAOS trees solve rounds of
  nodes on threads under `--tree-batch N` and take the answers in a fixed
  order, so the tree is the same at any thread count
  (`bench/measurements/02-290/`). HiGHS does not say whether its parallel
  tree depends on the thread count (searched: the parallel page, the
  options, the 1.15 release notes, issues).
- *Hexaly's `?` cells*: Hexaly publishes almost nothing about its tree
  search (searched: the docs index, `HxParam`, the release notes, its
  conference abstracts).

## 5. Parallelism

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Parallel LP solve | ◐ | ● | ○ | ○ | ◐ | ● | ? |
| Parallel MIP solve | ◐ | ◐ | — | — | ● | ● | ● |
| Deterministic under parallelism | ● | ◐ | — | — | ● | ● | ● |

- *Parallel LP solve*: the concurrent solve's arms and the barrier's
  factor run on threads (`bench/measurements/02-288/`). The simplex runs on
  one. SCIP threads the LP only through a threaded LP solver, and SoPlex is
  not one. Hexaly says only that its threads "parallelize the search"
  (searched: `HxParam`, `HxPhase`).
- *Parallel MIP solve*: the tree rounds are off by default, because no
  round is cheap enough where a node takes a few pivots (`SPECS.md` §5).
  HiGHS's threaded tree search is a prototype that runs only when
  `parallel` is "on".
- *Deterministic under parallelism*: no clock decides anything, so the
  answer and the work units are the same at any thread count. HiGHS
  documents this for its parallel interior point only. Hexaly's ● is
  inferred from its promise of equal results under a fixed seed and fixed
  iteration limits.

## 6. Correctness and verification

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| **Bit-identical results across different machines** | ● | ○ | ? | ? | ? | ○ | ? |
| Deterministic on the same machine and version | ● | ● | ● | ● | ● | ● | ● |
| Independent checker shipped with the solver | ● | ○ | ○ | ○ | ○ | ○ | ○ |
| Exact rational LP solutions | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Exact solving with no numerical tolerances | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Machine-checkable certificate of the result | ● | ○ | ◐ | ○ | ● | ○ | ○ |
| Certified bound on suboptimality | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Infeasibility / unboundedness certificate | ● | ◐ | ◐ | ◐ | ● | ● | ○ |
| Irreducible infeasible subsystem (IIS) | ● | ● | ○ | ○ | ● | ● | ● |
| The IIS written out as a model of its own | ● | ● | — | — | ● | ● | ○ |
| Feasibility relaxation of an infeasible model | ◐ | ● | ? | ○ | ◐ | ● | ○ |
| **Prove a basis another solver produced** | ● | ○ | ○ | ○ | ○ | ○ | ○ |
| **Check a point another solver produced** | ● | ○ | ○ | ○ | ○ | ○ | ○ |

- *Bit-identical across machines*: Gurobi and HiGHS say results can differ
  between machines. The others document nothing (searched: the CHANGELOGs,
  FAQs and Suite papers, Clp's guide and issues, Hexaly's `HxPhase`).
- *Independent checker*: `jaos_check_solution` judges an answer from the
  model alone, with tolerances, and shares no algorithm with the solver.
  SCIP's `viprchk` checks certificates SCIP emits.
- *Exact rational LP solutions*: after `jaos_verify` proves a basis, its
  values and duals are exact rationals (`jaos verify --values`). A basis
  whose numbers outgrow the limb budget gets none
  (`bench/measurements/02-358/`).
- *Exact solving*: `jaos solve --exact` proves an LP's answer over the
  rationals and repairs its basis by exact pivots. MIPs, QPs, cones and
  numbers past the limb budget are not covered (`SPECS.md` §6).
- *Machine-checkable certificate*: `jaos_write_proof` writes an exact proof
  of an optimum, an infeasibility or a ray, and `jaos_check_proof` judges
  it from the model alone. SoPlex writes rational values and checks them
  itself, with no certificate format and no separate checker.
- *Certified bound*: the bound is sound, and alone it cannot separate a
  wrong vertex from a right one (`SPECS.md` §6).
- *Infeasibility certificate*: HiGHS gives a ray only from its simplex and
  none for a MIP. SoPlex gives one "if available" unless `bool:ensureray`
  is set. Clp gives none inside branch and bound, after the barrier or on a
  presolve verdict.
- *IIS*: `jaos_iis` names an irreducible set of bound sides of the linear
  relaxation, and `jaos iis --write OUT` writes it as a model
  (`bench/measurements/02-236/`). Hexaly's inconsistency core has no save
  call.
- *Feasibility relaxation*: `jaos relax` moves rows, columns or both by the
  least total amount, integer columns included. A proof that no relaxation
  exists when inequality rows leave no integer point is missing
  (`SPECS.md` §6). SCIP has only a PySCIPOpt recipe, and SoPlex documents
  nothing (searched: the CHANGELOG, the FAQ, the command-line help).
- *Prove a basis*: `jaos_verify_basis` proves or refuses a basis from an
  MPS basis file over the rationals, with no solve.
- *Check a point*: `jaos check FILE --point POINT` runs the checker on a
  point file another program wrote.

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

`docs/format-support.md` gives what each JAOS reader takes and what each
writer guarantees. Hexaly reads and writes no MPS or LP file. Its own
formats are HXB and HXM.

- *Compressed output*: every JAOS writer but the proof writer compresses
  on a `.gz` name. Clp writes gzip only through `CoinMpsIO`. HiGHS, SoPlex
  and SCIP document reading `.gz` and not writing it (searched: their
  guides, help text and CHANGELOGs).
- *Point and duals files*: SoPlex and Clp write them and read no point
  back.
- *Other solvers' solution files*: JAOS reads the point files of Gurobi,
  MIPLIB, SCIP, HiGHS and CPLEX. Gurobi reads only its own formats.
- *`.nl`, OSiL, QPLIB and CBF*: SCIP documents `.nl` and OSiL readers and
  no QPLIB or CBF reader. Hexaly was not checked.
- *Line numbers*: Clp's and Gurobi's reader messages name the line. HiGHS,
  SoPlex, SCIP and Hexaly document none (searched: their guides,
  CHANGELOGs, FAQs and issues).

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

- *Julia*: `julia/JAOS` is a MathOptInterface optimizer, so JuMP uses it
  directly. It is not in Julia's General registry. Hexaly's Julia wrapper
  is third-party and unsupported.
- *Java, .NET and R*: `dotnet/Jaos`, `java/src/org/jaos` and `R/jaos`
  reach every C call the Python package reaches. HiGHS has no Java binding
  and SCIP no .NET one. Their R packages come from outside their teams.
- *Modelling systems*: `jaos STUB -AMPL` serves AMPL, Pyomo and JuMP's
  AmplNLWriter (`bench/measurements/02-257/`). GAMS and `.nl` bodies above
  degree two are missing. Hexaly supports no modelling system.
- *pip*: JAOS installs with `pip install .` and is not on PyPI.
- *`make install` and CMake*: SoPlex and SCIP install a CMake config and no
  `.pc` file. Clp ships no CMake config. Gurobi and Hexaly ship binaries.

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

- *Work limit*: JAOS counts reproducible work units
  (`docs/work-units.md`). HiGHS, SoPlex and Clp limit only iterations.
- *Tolerances and ranging*: Hexaly has no tolerances, no duals and no
  ranges.
- *Progress callback*: SoPlex has an interrupt flag and no callback.
- *Steering callbacks*: `jaos_set_node_callback` adds cuts and lazy rows,
  picks the branching column and hands in solutions. HiGHS's callback can
  only hand in a solution.
- *Options*: Clp and Hexaly take options on the command line and document
  no parameter file.
- *Thread count*: SoPlex and Clp are single-threaded. Hexaly calls its
  thread count "indicative".

## 10. Licence and distribution

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Open source | ● | ● | ● | ● | ● | ○ | ○ |
| Free for commercial use | ● | ● | ● | ● | ● | ○ | ○ |
| No external dependencies | ● | ◐ | ○ | ○ | ○ | — | — |

- *No external dependencies*: JAOS links only libc and libm. HiGHS's zlib
  is optional, and its HiPO solver links Metis and OpenBLAS.

---

## Sources

JAOS's cells come from `SPECS.md` and `bench/`. No solver's source code
was read for this page. The other cells come from these pages:

- HiGHS: <https://ergo-code.github.io/HiGHS/dev/solvers/>,
  <https://ergo-code.github.io/HiGHS/dev/options/definitions/>,
  <https://ergo-code.github.io/HiGHS/dev/parallel/>,
  <https://ergo-code.github.io/HiGHS/dev/callbacks/>,
  <https://github.com/ERGO-Code/HiGHS/releases>,
  <https://github.com/ERGO-Code/HiGHS/issues/2148>,
  <https://arxiv.org/pdf/2508.04370>.
- SoPlex: <https://github.com/scipopt/soplex/tree/v8.1.0>,
  <https://soplex.zib.de/doc/html/FAQ.php>,
  <https://soplex.zib.de/doc/html/EXACT.php>.
- Clp: <https://coin-or.github.io/Clp/faq.html>,
  <https://coin-or.github.io/Clp/Doxygen/classClpModel.html>,
  <https://coin-or.github.io/Clp/messages.html>,
  <https://coin-or.github.io/CoinUtils/Doxygen/classCoinMpsIO.html>.
- SCIP: <https://github.com/scipopt/scip/tree/v10.1.0>,
  <https://arxiv.org/abs/2511.18580>,
  <https://github.com/scipopt/PySCIPOpt/discussions/854>,
  <https://github.com/scipopt/vipr>.
- Gurobi:
  <https://docs.gurobi.com/projects/optimizer/en/current/reference/parameters.html>,
  <https://docs.gurobi.com/projects/optimizer/en/current/reference/fileformats.html>,
  <https://support.gurobi.com/hc/en-us/articles/360031636051-Is-Gurobi-deterministic>,
  <https://www.gurobi.com/resources/reports/what-s-new-in-gurobi-13-0>.
- Hexaly: <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxparam.html>,
  <https://www.hexaly.com/docs/last/pythonapi/optimizer/hxsolution.html>,
  <https://www.hexaly.com/docs/last/java/com/hexaly/optimizer/HxPhase.html>,
  <https://www.hexaly.com/gurobi>.
