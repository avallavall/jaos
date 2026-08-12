# Constraints

Extracted from `SPECS.md` (type SPEC, precedence 1) and — where a lower-precedence
document states a hard constraint rather than background — from the DOC sources.

**Why DOC paths appear here.** The ingest manifest types `bench/README.md`,
`docs/tolerances.md`, `docs/work-units.md`, `docs/format-support.md` and
`docs/scaling.md` as DOC. Their content is not background: `bench/README.md`
defines the acceptance gate, and this project's own rule is that every numeric
threshold carries a measurement and is therefore a constraint rather than a
suggestion. The classifiers for `bench/README.md` and `docs/format-support.md`
both flagged this explicitly. Entries below carry their true source path, so
precedence can still be applied downstream; the full DOC text is in
`context.md`.

Source paths are relative to the repository root.

---

## No external code
- source: SPECS.md
- type: nfr
- content: No external code. Two exceptions, both closed and neither extended: netlib's `emps` as a dev-time instance converter, fetched and checksummed and never redistributed; and Unity for the test suite. Papers, theses and textbooks are the only other input — never another solver's source. (D2, D12, D15)

## Bit-identical results on every machine and every run
- source: SPECS.md
- type: nfr
- content: Bit-identical results on every machine and every run. No clock decides anything, no iteration order depends on an address, no reassociated floating point, no unseeded randomness. (D8, D34)

## Every number needs a measurement on both sides
- source: SPECS.md
- type: nfr
- content: Every number needs a measurement on both sides — a tolerance, a threshold, an interval. "Fitting a constant to one instance is how this project loses weeks." (D17)

## Deterministic work units are the unit of cost, and seconds never enter a baseline
- source: SPECS.md
- type: nfr
- content: Deterministic work units are the unit of cost, and every run also reports wall-clock time. The units make regressions detectable across machines; the seconds say whether the units bought anything. Seconds are development numbers, are labelled as such, and never enter a baseline. (D16, D17, D45, D57)

## Reading a problem
- source: SPECS.md
- type: schema
- content: Fixed MPS **done**; free MPS autodetected **done**; `RANGES` with per-row-type semantics **done**; all `BOUNDS` types, `OBJSENSE` and objective constant **done**; unsupported constructs rejected with a line number **done** (SOS and indicators, never silently skipped); LP format **partial** — CPLEX-style core, the documented subset is `docs/format-support.md`; locale-independent number parsing **done**, own, because `strtod` under a comma-decimal locale corrupts instances; direct load from CSC arrays **done** (`jaos_load_lp`); compressed input (`.gz`) **missing**, handled outside the library today.

## Holding and changing a problem
- source: SPECS.md
- type: api-contract
- content: Dimension and nonzero queries **done**; read a bound or a cost back **done** (`jaos_col_cost`, `jaos_col_bounds`, `jaos_row_bounds`); add or delete rows and columns after load **done** — additions append so existing indices never move, deletion takes a set (D77); change a bound or a cost **done**, each discarding the answer the model was holding (D66); change a coefficient **done** (`jaos_set_coefficient`, D67); re-solve warm from the previous basis **done** and automatic, `jaos_clear_basis` asks for a cold solve (D68); load a starting basis **done** (`jaos_set_basis`) — "a wrong basis costs iterations and never the answer"; resume from where a work or time limit stopped **done**, a numerical failure being the one outcome that leaves no basis (D70).

## Solving — what exists and what does not
- source: SPECS.md
- type: protocol
- content: Dual simplex **done** — steepest-edge pricing [8], Harris two-pass ratio test with bound flipping [7][19], dual phase 1 by artificial bounds [21], Bland fallback on a detected stall (D26, D86). Sparse LU with Markowitz threshold pivoting **done** [4][6][20], Forrest-Tomlin updates [5], singular-basis repair. Stability trigger on the triangular solves **done** — the pivot element computed twice each iteration, past `LU_AGREE_TOL` the pivot declined unbilled and the factorization rebuilt (D86). Scaling **done** — Curtis-Reid [11], geometric-mean equilibration as an option. Hyper-sparsity in the triangular solves **partial**. Presolve **missing**. Primal simplex **missing**. Crash basis **missing** — measured once and refused. Partial and multiple pricing **measured and refused** (D82, D84). Barrier and crossover **missing**. MILP **missing**. Deterministic parallelism **missing**.

## Implementation works from the bibliography and never from another solver's source
- source: SPECS.md
- type: nfr
- content: Citation numbers in SPECS.md §3 are the bibliography in `PLAN.md`. Implementation works from those and their kin only — never another solver's source (D12).

## What a caller may configure is the contract, not the method
- source: SPECS.md
- type: api-contract
- content: A caller sets what depends on their problem and which the solver cannot know: how much precision their data deserves, how long they will wait, where log lines go. How the problem is solved is the solver's to decide — the pricing rule, when a carried weight stops being worth keeping, when to refactorize, whether a sparse or a dense path is cheaper. Work limit and time limit **done**; `jaos_set_primal_tolerance` and `jaos_set_dual_tolerance` **done**, 0 restores the default; `jaos_set_log_callback` / `jaos_set_log_level` **done**, four levels, silent until a callback is installed; `jaos_set_progress_callback` **done** — a watcher may look and may stop a solve, never steer one, asked on a fixed iteration count, a stop is `JAOS_SOLVE_INTERRUPTED` and keeps its basis. Choosing the algorithm and turning scaling off or picking the mode are **out of scope**. (D64, D65, D79)

## Reading an answer
- source: SPECS.md
- type: api-contract
- content: Status, objective, values, activities, duals, reduced costs **done**; `jaos_basis` **done**; iterations and work units **done**; `jaos_solve_time` **done** — the only number JAOS reports that is not reproducible, and the header says so and says not to diff it; independent solution checker **done**; a certified lower bound on suboptimality **partial** (`certified_suboptimality`, sound and never overclaiming, but it reads the same ~1e-25 on answers known to be 1.04e-3 wrong as on correct ones, D73); sensitivity and ranging **missing**; infeasibility and unboundedness certificates **missing**; exact rational verification of a final basis **missing**.

## Writing, and calling it from another language
- source: SPECS.md
- type: api-contract
- content: Write MPS **missing**; write LP **missing**; write a solution file **missing**. C API **done** — `include/jaos.h`, the only header. Python **missing**; anything else **missing**.

## The bars it has to clear
- source: SPECS.md
- type: nfr
- content: Netlib standard set, 94 instances (optimal, objective within tolerance, checker green) **pass**; Kennington subset, 16 instances **pass**; Netlib infeasible subset, 29 instances (refused, no false optima) **pass**; determinism across two solves and across runs, all 139 **pass**, the second solve clearing the basis first (D68); warm re-solve against cold, one branching step per instance **measured: 0.0052 of the iterations, 0.0162 of the work** on 92 of the standard 94, 0.0006 and 0.0041 on 11 of Kennington's 16 (D69 improved by D90), both answers through the independent checker with 0 refused (D92); full suite clean under ASan and UBSan **pass**; reader robustness under fuzzing **pass**; competitive gap at T0 **measured: 3.72x** vs HiGHS 1.15.1, **1.34x** vs SoPlex 8.0.3, **3.77x** vs Clp 1.17.11 (D52, D53, D60, D83); rungs T1-T3 **measured** (D81); MIPLIB 2017 easy and benchmark subsets **not started**.

## Correctness is table stakes, and the per-iteration cost is a property of JAOS
- source: SPECS.md
- type: nfr
- content: "Everything above these is correctness, and correctness is table stakes." At T0 the decomposition is time per solve 3.72x / 1.34x / 3.77x, iterations 1.47x / 0.70x / 1.67x, time per iteration 2.54x / 1.92x / 2.26x against HiGHS, SoPlex and Clp. JAOS takes 30% fewer iterations than SoPlex and is still slower; three separately written dual simplexes agree on the per-iteration cost while disagreeing about everything else, so it is a property of JAOS and not an artefact of one rival (D83). The tail splits in two: a cheaper iteration is `maros-r7`'s problem, fewer iterations is what `pilot`, `pilot87`, `25fv47` and `greenbea` need, and the cause of the second is measured — steepest-edge weights discarded on 80-93% of their iterations, on a threshold bounded on both sides by correctness (D63).

## The acceptance gate — three sets
- source: bench/README.md
- type: protocol
- content: Three instance sets: standard, 94 instances, solved to a verified optimum, `make netlib`; Kennington, 16 instances, the same for correctness only, `make netlib-kennington`; infeasible, 29 instances, classified `INFEASIBLE` with no false optima, `make netlib-infeas`. An instance coming back `optimal` on the infeasible set is flagged `<-- FALSE OPTIMUM` and fails the run. Each set fetches into its own directory because `greenbea` names a feasible model in the standard set and a different, infeasible one in the infeasible set.

## The acceptance gate — four per-instance predicates
- source: bench/README.md
- type: protocol
- content: (1) **Shape** — the file must load with the row and column counts the manifest records (D18). (2) **Objective** — within `1e-6 · max(1, |reference|)` of the reference optimum, compared against reference plus `objconst`. (3) **The independent checker** — primal feasibility, dual sign conditions, complementary slackness and the primal-dual gap, all judged in the original unscaled problem (D18). (4) **Determinism** — the model is solved twice with the basis cleared between them, and the two runs must agree on status, iteration count, work units, and the bits of the objective and of every published value (D8, D68).

## The acceptance gate — all-or-nothing, and the baseline is the regression detector
- source: bench/README.md
- type: protocol
- content: The gate reports `NOT MET` and `make netlib` exits non-zero unless every instance meets every condition. Now that all three sets PASS, the per-instance baseline diff is the only thing that can report a regression at all. Each instance is judged on the four predicates plus its work count; any of them going from holding to not holding is a regression and fails the run on its own, whatever the gate says. Work may grow by up to 2x and is reported past that. `relative_suboptimality` is carried in the baseline and growth past 2x above a floor of 1e-9 is a regression (D88, D91). Improvements are printed as well as regressions. A run that compared against no baseline records `baseline: NOT COMPARED`. **Updating the baseline is a separate command and never a side effect of running the gate.**

## The acceptance gate — no wall-clock figure, and `J` invalidates the seconds
- source: bench/README.md
- type: protocol
- content: No wall-clock figure is produced by the gate; speed is an M2 question and needs a controlled host before any number about it means anything (D17). `J=N` produces a byte-identical record but invalidates the seconds, and the runner says so on every parallel run; the time ratio a change is judged on (D45) must come from `J=1` (D57). Seven record columns judge nothing and are there because "a verdict that only records its own outcome cannot be argued with later": `rowrel`, `Q`, `N`, `drop`, `cert`, `rsub`, `sub`, `rays` (D24, D73, D91).

## The acceptance gate — reference values come from Koch
- source: bench/README.md
- type: protocol
- content: Reference optima are taken from Koch, *The Final NETLIB-LP Results* (ZIB-Report 03-05); every manifest line is sourced `koch`. Netlib's own MINOS 5.3 table disagrees by more than the gate's tolerance on eight instances. `koch-verify.py` reproduces 82 references exactly, double for double, with none in disagreement. Instance files never enter the repository; the manifest stands in for them. netlib's `emps` is used as a dev-time expander, pinned by sha256 and never stored in the repo.

## `make warm` reports a ratio, not a verdict
- source: bench/README.md
- type: protocol
- content: `make warm` / `make warm-kennington` apply one branch-and-bound branching step per instance and solve the perturbed model twice — warm from the anchor basis, cold after `jaos_clear_basis`. It reports geometric means of per-instance ratios (D46), iterations as `(warm+1)/(cold+1)`, and no wall-clock number reaches `bench/results/warm.txt`. It fails when warm and cold disagree on a verdict or an objective, or when the independent checker refuses **either** answer (D92). It is not a gate.

## The comparison harness — rules it enforces
- source: bench/compare/README.md
- type: protocol
- content: A time without a verified answer is discarded — every competitor run is judged the way JAOS is. Tolerances are equalised explicitly, and equal settings were checked to mean equal answers. Competitor versions are pinned by checksum, like the instances. Two times per run: what the solver reports for its own solve, and what the process took. The minimum of N runs, not the mean. Records carry the tier definition they were taken under, and a number taken under WSL is a development number that cannot close a gate (D17). `bench/compare/results/` is a different record from `bench/results/` and carries seconds; the two must never be confused.

## The comparison harness — the tier ladder
- source: bench/compare/README.md
- type: protocol
- content: T0 — JAOS dual simplex with no presolve against competitors with the dual forced, presolve off and no crash basis: the simplex and only the simplex. T1 — competitors free to pick primal or dual. T2 — presolve on. T3 — stock defaults. Each rung's difference from the one below attributes the gap to one feature, and a rung difference is read against the competitor itself rather than through JAOS. The ladder is recalibrated as JAOS grows. Competitors: HiGHS, SoPlex, Clp for M2; SCIP pinned from the start for M3/M4 and not run until then. GLPK is deliberately absent.

## Solver tolerances — frozen
- source: docs/tolerances.md
- type: nfr
- content: Frozen at the Netlib gate's close (D31); a change to any of them is now a changelog entry. `PRIMAL_TOL` 1e-7; `DUAL_TOL` 1e-7; `PIVOT_MIN` 1e-9; `LU_PIVOT_TOL` 0.1; `LU_UPDATE_TOL` 1e-9; `LU_AGREE_TOL` 1e-5 (D86); `IMPLIED_ROUNDS` 64 (D91); `DROP_REL` 1e-14; `TINY` 1e-300; `DSE_MIN` 1e-12; `DSE_DRIFT` 10.0 (D63); `ARTIFICIAL_BOUND` 1e10 (D19). Beside them and not tolerances: `REFACTOR_EVERY` 64 (D39) and `ITER_SANITY_FACTOR` 200. Every one of the eight original checker failures closed as a defect with a mechanism and **not one closed by moving a number**.

## Two spaces, and confusing them misreads every figure
- source: docs/tolerances.md
- type: nfr
- content: The solver runs on a scaled copy of the model, so its tolerances are magnitudes in scaled space. The independent checker runs on the model as loaded, so its tolerance is a magnitude in the units the caller wrote. Neither is converted into the other; they are separate judgements, which is the point of having both. (D27, D89, D92)

## The checker's tolerance and its identities
- source: docs/tolerances.md
- type: protocol
- content: `jaos_check_solution` takes one tolerance from the caller and applies it in original space; there is no default, because "a checker that chose its own would be grading on a curve it set". Primal violation of `v ∈ [lo, hi]` is `max(lo − v, v − hi, 0)` over finite bounds only. Dual: `|w| <= tol` waives the condition; `w > 0` requires `v <= lo + tol · s`; `w < 0` requires `v >= hi - tol · s`; a multiplier pointing at an infinite bound is itself a violation of exactly its own magnitude. `s` is the row's own traffic `max(1, sum_j |A_ij x_j|)` for a row and `max(1, |x_j|)` for a column (D23). Every multiplier contributes `w · bound` to the dual objective including those held to no condition (D22). `gap = |P − D| / (1 + |P| + |D|)`, with `Q` and `N` accumulated separately (D24).

## Work-unit weights and where they are charged
- source: docs/work-units.md
- type: nfr
- content: `JM_WORK_NONZERO` 1, `JM_WORK_ELIMINATED` 2, `JM_WORK_UPDATE` 64, `JM_WORK_FACTOR` 4096. An elimination is charged by what it does, not by which routine runs it. Charged in `src/lu.c` and `src/simplex.c` and nowhere else: factorization, triangular solves, basis update, pricing, ratio test and bookkeeping, pattern ordering, ending a solve, reading the unbounded verdict. Outside the budget: model loading (deliberately), scaling (stated because it is true, not because it was decided), and both pricing forms' own sweep over the variables. There is no per-iteration constant and D32 is the measurement that settled it. The clock is never involved.

## Scaling contract
- source: docs/scaling.md
- type: nfr
- content: Row factors `rho_i` and column factors `gamma_j` such that scaled magnitudes cluster around 1. **The stored matrix is never modified** — the factors live beside it and the original matrix remains the authority the independent checker judges against. Every factor is an exact power of two, so scaling introduces no rounding error of its own; exponents are clamped to ±512. Curtis-Reid is the default, solved by Jacobi-preconditioned conjugate gradients in fixed-order passes so results are bit-identical across runs (D8, D34). Answers come back in the caller's units. Geometric-mean equilibration is kept as an option.

## MPS reader dialect
- source: docs/format-support.md
- type: schema
- content: One reader for both layouts. Names with embedded spaces are rejected loudly. First `N` row is the objective; further `N` rows are kept as free rows, never dropped. **RHS on the objective row sets the objective constant to the negated value**, per CPLEX convention — and the published Netlib reference optima do not include it, so the gate carries it per instance in the manifest. Default RHS is 0. `RANGES`: `G` row `[b, b+|r|]`, `L` row `[b-|r|, b]`, `E` row `[b, b+r]` for `r >= 0` and `[b+r, b]` for `r < 0`; on an objective or `N` row it is an error. `BOUNDS`: `UP LO FX FR MI PL` supported, `BV LI UI SC SI` rejected until M3, and the negative-`UP` wart drops an unset lower bound to -inf. Multiple RHS/RANGES/BOUNDS sets: the first name seen wins. Duplicates are errors. `OBJSENSE` default minimize. Integer markers rejected until MILP lands. Numbers parsed under an explicit "C" locale, Fortran `D` exponents accepted. `ENDATA` required. `OBJNAME` not supported.

## LP reader dialect
- source: docs/format-support.md
- type: schema
- content: CPLEX-style core dialect, token-stream parsed. Sections `Minimize`/`Maximize`, `Subject To`, optional `Bounds`, `End`; keywords case-insensitive and reserved. Comments `\` to end of line. Names start with a letter or `_`. A repeated variable inside one expression **sums**, unlike the MPS reader where a duplicate data entry is an error. Objective label optional, bare constants add to the offset. Constraints: constants inside the expression and ranged constraints are recognised and rejected. Bounds forms listed, later statements override component-wise, reversed forms rejected, bounds on a variable appearing nowhere else are an error. Default bounds `[0, +inf)`. Integer sections, semi-continuous and SOS rejected. Numbers under an explicit "C" locale, no Fortran `D` exponents. `End` required.

## Acceptance criterion for the Netlib gate
- source: docs/tolerances.md
- type: protocol
- content: Separate from the solver tolerances and not one of them: an instance is accepted when `|obj − ref| <= 1e-6 · max(1, |ref|)` against Koch's reference values [22], with the checker green in original space. That criterion is relative, and a test that pins a large objective absolutely is stricter than the project's own gate — "which makes it a test about floating-point luck rather than about the solver".
