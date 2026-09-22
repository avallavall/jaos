# TODO — the current milestone

Rows from `SPECS.md`, and defects found by reading the code and the
readings. A line leaves this file in the commit that lands it. When the file
is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong, what the fix needs and how to verify it. The
reading behind it is in the commit that wrote the row, named here by hash.

Milestones A to E ended on 2026-09-21 and 2026-09-22 (A with the tag
`v0.4.0`). This file was filled on 2026-09-22 by an audit of every document
against the code. Work it in order: F (defects), G (the bindings reach what
Python reaches), H (performance), I (the rest of SPECS).

## Milestone F: defects

F1. **The warm re-solve of `pilot` loops.** Solve `pilot`, set column 3's
upper bound to 1018, solve again: 952801 iterations, then the iteration
guard, under Bland's rule, 3619 pivots declined on factorization
disagreement. A cold solve of the same model takes 5824. Introduced by 8296fb8 (the dual
perturbs its costs after a plateau of the model's size), found by `git
bisect` on 2026-09-22. Before it, the same warm re-solve took 22033
iterations, which is itself 3.8 times the cold solve. Found by re-taking
`make warm`, which had not run since 2026-09-10. Verify: `make warm` reads 0
errors, and a unit test solves this model warm.

F2. **`dfl001` ends `NUMERICAL_ERROR` from a cold start after one bound
change.** Set column 0's upper bound to 0: the settled point has a reduced
cost 1.5e-9 past its bound. The warm re-solve of the same model ends
optimal. It was fine at 6e925a7 (2026-09-10). Find the commit with `git
bisect start HEAD 6e925a7` in a clone outside the repository's folder, each
step solving `dfl001`, setting that bound and solving again after
`jaos_clear_basis` (the steps of `bench/warm.c`). Verify: `make warm` reads
0 disagreements.

F3. **The primal simplex ends `NUMERICAL_ERROR` on `pilot87`.** Its phase 1
infeasibility rises to 3.17 times its best on an ill-conditioned basis. First
seen in cb74ff5's `bench/results/primal.txt`; the 2026-09-09 reading had
none. A bisect from 6e925a7 with `./build/bench/primal -j 1 pilot87` found
it fine up to 6493362 at least; it was still running when this row was
written (clone in WSL at `~/jaos-bisect`, delete it after). Verify: `make
primal` reads 0 disagreements.

F4. **Some warm starts cost far more than they did.** Against the
2026-09-10 reading, warm work rose 87x on `stocfor3`, 12x on `stocfor2`, 4x
on `wood1p` and `gfrd-pnc`, 2x on `scorpion`, `sc205` and `sierra`. Over
all 90 the warm solves got 0.93x cheaper and the cold ones 0.79x (the
aggregator runs on a cold solve only), so 20 of 90 now take more iterations
warm than cold, where none did. `warm-kennington` went from 0.0082x to
0.0154x of the cold work. Find what the warm path lost (the Devex handoff and
the plateau perturbation of 2026-09-21 are the first suspects). Verify: no
instance of `make warm` takes more work warm than cold without a reason
written down.

F5. **The concurrent solve may bill a resumed arm twice.** `settle_arm`
adds `jaos_work_units` of each arm every round (`src/concurrent.c`), and a
simplex arm that resumes from its parked state reports its whole walk. Solve
an LP whose dual needs more than `CONCURRENT_SLICE` with `--algorithm
concurrent` and compare the work with the arms' own totals. Fix the billing
if it double counts, then re-take `make concurrent`.

F6. **A replay of `jaos options` turns off MIQP propagation.** The tree runs
`MIP_QUAD_PROPAGATE` (4) passes on a MIP with a quadratic objective, while
`jaos_get_option(m, "mip_propagate")` and `jaos options` print 0; a
`--params` file of that output sets 0. Fix: an unset option reads back the
value the solve will use, or the default becomes a sentinel.

F7. **The `.nl` reader refuses a quadratic row with a linear part.** In a
row, `form_put` and `form_mul` (`src/nl.c`) never merge like terms, so
`(x+1)^2 <= 5` gives `x` two linear entries, and a column that the body and
the `J` segment both give a coefficient gets two too. `jaos_load_lp`
refuses the model ("the .nl model failed validation", no line). The
objective is not affected, because its costs are summed. Fix: sum duplicate
(row, column) entries in `nl_build`. Verify: a test with `(x+1)^2 <= 5`, one
with a `J` term on the same column, and one naming a column twice.

F8. **Writer round trips that break.** The LP writer writes a name that
starts with `/`, which the LP reader refuses; it writes a row whose lower
bound is above its upper bound as two rows; it writes a finite lower bound
with an upper bound of `-inf` as `<= -inf`, which the reader refuses. The
CBF writer writes a bound at an infinity of the wrong sign as no bound.
Fix: `jm_lp_name_ok` refuses a leading `/`; both writers refuse those
bounds by name, as the MPS writer does. Verify: a round-trip test per case.

F9. **The conic certificate search passes its cap.** `CONIC_CERT_CALLS`
bounds the coordinate climb only; the tilt ladder adds up to 130 checker
calls per outer round. The cone-block product `mul_h` bills no work. Fix
both, then re-read `make cblib` and 02-253's 3000 models to the bit.

F10. **`make pgo` profiles `libjaos.a` only.** `libjaos.so`, the tool and
the wheels are built without the profile. Either profile them too or say
so where `make pgo` is offered.

F11. **Bench bookkeeping.** `bench/run.c` names the baselines' last column
`dropped`, but the value it writes and reads there is the relative
suboptimality bound. `make plato` runs `plato-nug`, whose nug20 and nug30
do not finish, so `make plato` does not finish either.

F12. **Finish the constants docs** (the 2026-09-22 audit stopped here for
the token budget). `docs/tolerances.md`: `NAME_LEN` is 256
(`JAOS_NAME_MAX + 1`), the buffer for every name the writer copies; the
relaxation has five numbers and its two caps can end the search with no
answer; `RELAX_BOX_ROUNDS` lets `M` grow 15 times (the widest box is 32768
times the first); the conic tree has three numbers; the branch and bound's
heading counts 58 numbers of `src/mip.c` and 2 of `src/symmetry.c`, and
`MIP_PUMP_ALWAYS` and `MIP_RCFIX` are off; the three cut rows cite 02-298;
`MIP_BATCH_MAX` cites 02-290; `JM_EXACT_LIMBS` lives in
`src/jaos_internal.h`, a `jm_bigint` is 528 bytes and the block ceiling is
1007 rows; `src/verify.c` has two constants; `CONIC_CERT_CALLS` bounds only
the coordinate climb; `PRESOLVE_ROUND_ULPS` has four live sites;
`BARRIER_MAX_ITER`'s 57 is stale (pilot at 47 now); the `CR_MAX_ITER`,
`CR_TOL`, `GEO_TOL`, `BIG` and `SPLIT` rows. `docs/work-units.md`: the
crossover's push, the barrier's QP push, augmented system and dense-column
correction, the conic Newton finish (`(CONIC_REFINE + 2) * u` a step) and
the unbilled `mul_h`, the rounds of nodes, the IIS and the relaxation, the
LDL kernels and the threaded factor, the aggregator's conditions, and the
time limit read every iteration outside the simplex and PDLP. `docs/scaling.md`:
the Curtis-Reid stop rule (`CR_MAX_ITER`, `CR_TOL`), `GEO_TOL`, the conic
interior point's own Ruiz scaling, and that only the unit tests choose a
scaling mode. Verify: `tools/docs-check.sh`.

F13. **Small items the audit left open.** Each needs a check or a
decision, then a doc line.
- The comparison harness: `bench/compare/jaos_time.c` sets no tolerance, so
  JAOS runs its dual tolerance of 1e-9 against the competitors' 1e-7; set
  it or say it. `run-mip.sh` writes no WSL tag, no dirty-tree mark and no
  SCIP or pyscipopt version in its header, as `run-compare.sh` does.
- `docs/feature-matrix.md`: "30 of the 110 gate bases" (02-275) predates
  the aggregator; re-count with `bench/measurements/02-275/verify-count.sh`.
  Three SPECS rows have no matrix row yet: the exact proof of the final
  basis, indicator constraints in MPS and LP, and the other formats.
- Numbers with no source in the tree: README's "`make pgo` is worth about
  1.1x", `bench/compare/README.md`'s "repeats to about 1.4%", and the flag
  ratios and 0.987x/1.004x readings in `docs/build.md`. Link a measurement
  or drop them.
- "links nothing but libc and libm" (README, SPECS): the library links
  `-pthread`, which is inside libc from glibc 2.34 on. Say which platforms
  the sentence covers.
- "The checker shares no code with the solver" (SPECS, `docs/api.md`):
  `src/check.c` calls `jm_obj_add`, `jm_two_product_residue`, `jm_round`
  and `jm_model_has_integer`. Reword to "no algorithm", or copy the
  helpers.
- `docs/cli.md`: can `jaos_solve` return `JAOS_OK` with the status still
  `not_run`? If so, list `not_run` among the statuses (exit 4).
- `docs/build.md`: the DLL's imports (`KERNEL32.dll` and `msvcrt.dll`
  only) and "clang-cl 20" are not asserted by CI; read them from the last
  CI logs.
- D101 in `bench/refusals.txt` refuses duplicate rows and columns and
  dominated columns, while the retired decision log calls it "deferred";
  and D97's reopen condition ("a crossover exists at postsolve") may be met
  since the crossover landed. Decide both.
- The PLATO baselines' header says `make netlib-baseline`; the targets are
  `plato-pds-baseline` and `plato-fome-baseline`. Fix it when F11 or H8
  re-takes them.
- `bench/compare/results/`: T0 to T3 and the older P0 files are records
  from before presolve (2026-08-11 to 2026-08-17); mark them in
  `bench/compare/README.md` or delete them.

F14. **`jaos --version` on `main` says 0.4.0.** HEAD is 30 commits past the
tag. Move the version to `0.5.0-dev` on `main` (every version string,
`tools/version-check.sh`) so a report names what it ran.

## Milestone G: the bindings reach what Python reaches

Read on 2026-09-22 over the 200 C calls: Python reaches 199, .NET and Java
46 each, R 32, Julia 46, and each reaches 54 to 59 more only by option name.

G1. **Binding defects.** .NET and Java `Problem.Solve` build a new model and
drop every option, limit, log sink and MIP start set on `p.Model`; `Problem`
has no setter of its own, so a `Problem` user cannot set a time limit, a
gap or threads. Java's `mipIncumbent` drops the objective. R has no log
callback. R's `jaos_solve_lp` returns values only at an optimum. Julia's log
goes to stdout with no user sink. Verify: a check per defect in
`make dotnet-test`, `java-test`, `r-test`, `julia-test`.

G2. **.NET to parity**, about 97 C calls, in this order: settings and a warm
re-solve in `Problem` (bound, cost, sense, constant); SOS, indicators and
semi-continuous columns with their getters; quadratic rows in `Problem`;
the solution pool; the progress, incumbent and node callbacks; the NL,
QPLIB, CBF, OSiL and AMPL `.sol` writers; the IIS and the feasibility
relaxation; the basis and the solution, basis, point and duals files; the
checkers; ranging; exact verification and proof files; model editing and
getters; names; statistics and the presolve report; the option list and
option files.

G3. **Java to parity**, the same list, plus `Expr` adding another `Expr`
and `value(Expr)`.

G4. **R to parity**, about 107 C calls: the log, sparse input to
`jaos_solve_lp`, the MIP start and the incumbent, model edits and a warm
re-solve, SOS, indicators, semi-continuous columns, rotated cones and
quadratic rows in `jaos_solve_lp`, the pool, the counters, then G2's list.

G5. **Julia to parity**: an incremental MOI interface (bounds, costs, sense)
so JuMP re-solves warm; MOI callbacks (lazy constraints, user cuts,
heuristic) on the node callback; `ResultCount` above 1 from the pool; names
passed to C; the solution, basis, point and duals files; `iis_model`,
`feasrelax`, ranging, the checkers and the exact proofs as `JAOS.`
functions.

G6. **Python `Problem` takes a product of two variables** (`x * y`) in the
objective and in rows. Today only `Model.set_quadratic` and
`Model.set_row_quadratic` reach it, and the other bindings' modelling layers
already take it.

Verify for G: each binding has a test that calls every C function it
declares, and `make dotnet-test java-test r-test julia-test python-test`
pass.

## Milestone H: performance

Where JAOS stands (`bench/compare/results/P0.txt`, 2026-09-22): on LP it
takes 2.03x HiGHS's time and 1.93x Clp's, and 0.66x SoPlex's; the iteration
counts are close (1.14x HiGHS), so the gap is time per iteration (1.78x). On
MIP (tree 3086162) it solves 23 of MIPLIB 3 in 20 s where HiGHS and SCIP
solve 24, and 0 of the 2017 set where HiGHS solves 8 and SCIP 7.

H1. **The simplex's time per iteration.** A callgrind profile of `stocfor3`
(18.3x HiGHS) puts 10.6% of the instructions in `memset`, 10.7% in
`malloc`, `free` and `realloc`, 6.2% in `memcpy` and 21% in refactoring the
basis. Remove the per-iteration allocations and the dense clears, then read
the refactor frequency. Verify with `tools/icount.sh` and `make compare
COMPARE_ARGS='-t P0'`; the gates byte-identical or re-based.

H2. **MIP against HiGHS and SCIP.** `bell5` takes 190741 nodes and 18 s
where SCIP takes 357 nodes and 0.18 s; `bell3a` 10 s against 0.3 s to 0.9
s; `l152lav` does not finish in 20 s. Take a fresh attribution of the tree
(node LP, cuts, heuristics, branching) on MIPLIB 3 and the 2017 set before
choosing a remedy; the refused ideas are in `bench/refusals.txt`.

H3. **Re-take the MIP comparison** (`bench/compare/run-mip.sh` on both sets,
with `SCIP_PYTHON`): the files are from tree 3086162, before the
best-estimate node order.

H4. **Time JAOS on QP and conic models against other solvers.** No reading
exists. A QP rung (Maros-Meszaros against HiGHS and Clp) and a conic rung
(CBLIB against SCIP).

H5. **The primal's six overruns** (d6cube, dfl001, fit1d, fit2d, pilot,
seba) and **the crossover's fourteen** (`bench/results/barrier.txt`).
Seven primal remedies are refused; read `bench/refusals.txt` first.

H6. **The large QPs and MIQPs.** 7 of QPLIB's 8 largest convex QPs reach
1e11 work units and QPLIB_9008 runs out of memory; 13 of 17 convex MIQPs
do not finish; 37 of CBLIB's 80 mixed-integer instances stop at the work
limit.

H7. **Parallel.** A parallel simplex; the rest of the barrier (forming the
normal matrix, the solves) on threads; a round of nodes cheap enough to be
the default.

H8. **Re-take the PLATO readings** (`plato-pds`, `plato-fome`), last taken
on 2026-08-17, and fix or drop `plato-nug`, which has no baseline.

## Milestone I: the rest of SPECS

I1. Presolve: duplicate rows and columns, dominated columns, bound
tightening, dual fixing (D101 reopens on a set with 5% removable), and the
bound-moving substitution (D97).

I2. QP: QPLIB_9002's dual violation of 2.1e4; QPLIB_8785 refused by the
checker at 1e-7; Maros-Meszaros qgrow22 (dual side) and aug3dqp
(suboptimality ceiling).

I3. Cones and quadratic rows: `tests/data/g_cone_badbox.mps` with two
columns in a row; the duals of 8 of QPLIB's 10 QCQP optima (8.5e-7 to
3.6e-5 off); QPLIB_2676 and QPLIB_2468; QPLIB's mixed-integer QCQPs; cuts
and warm starts in the conic tree; SOS sets, semi-continuous columns and
indicator rows beside cones; a quadratic row over more than
`CONIC_QC_DENSE` columns; the certificate that needs a coefficient to
vanish exactly.

I4. MIP switches that are off by measurement: strong branching, flow cover,
zero-half and lifted cover cuts, RINS and local branching, restarts, bound
propagation and reduced-cost fixing, probing and clique fixing. Each needs a
reading that lands it on (H2's attribution first).

I5. A steering callback that hands the tree a solution of the caller's.

I6. The feasibility relaxation's proof that the widest box is empty.

I7. The certified bound on suboptimality alone cannot separate a wrong
vertex from a right one.

I8. The barrier: a rule for an LP to choose the augmented system; a
crossover cheaper than a crash. PDLP: a set too large to factor where it
pays; feasibility polishing. The concurrent solve: a set where the barrier
wins.

I9. Exact solving with no tolerances (missing).

I10. Links: GAMS; `.nl` bodies above degree two. MATLAB needs a licence the
maintainer has to provide.
