# TODO — the current milestone

Rows from `SPECS.md`, and defects found by reading the code and the
readings. A line leaves this file in the commit that lands it. When the file
is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong, what the fix needs and how to verify it. The
reading behind it is in the commit that wrote the row, named here by hash.

Milestones A to E ended on 2026-09-21 and 2026-09-22 (A with the tag
`v0.4.0`). This file was filled on 2026-09-22 by an audit of every document
against the code. Work it in order: F (defects), G (the bindings reach what
Python reaches), H (performance), I (the rest of SPECS). Milestone F, the
defects, ended on 2026-09-23.

## Milestone G: the bindings reach what Python reaches

Read on 2026-09-22 over the 200 C calls, and counted again after G1 on
2026-09-23: Python reaches 199, .NET and Java 48 each, R 35, Julia 46,
and each reaches 54 to 58 more only by option name.

G2. **.NET to parity**, about 97 C calls, in this order: settings and a warm
re-solve in `Problem` (bound, cost, sense, constant); SOS, indicators and
semi-continuous columns with their getters; quadratic rows in `Problem`;
the solution pool; the progress, incumbent and node callbacks; the NL,
QPLIB, CBF, OSiL and AMPL `.sol` writers; the IIS and the feasibility
relaxation; the basis and the solution, basis, point and duals files; the
checkers; ranging; exact verification and proof files; model editing and
getters; names; statistics and the presolve report; option files
(`jaos_read_options`).

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
on 2026-08-17, with `make plato-pds-baseline plato-fome-baseline` after
reading the diff. That also fixes their headers, which name
`make netlib-baseline`. `plato-nug` has no baseline and runs only when
named, since two of its three instances do not finish.

H9. **Aggregate a warm-started solve.** A model with a starting basis is
not aggregated, so a warm re-solve runs on a larger model than a cold one,
and 17 of the 92 warm readings cost more than their cold solve for that
reason (bench/results/warm.txt, 2026-09-23). The starting basis has to be
mapped forward through the aggregator: an aggregated pair drops a column
and its equality row, and the count of basic members only stays balanced
when exactly one of the two was basic.

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
