# TODO — the current milestone

Rows from `SPECS.md`, and defects found by reading the code and the
readings. A line leaves this file in the commit that lands it. When the file
is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong, what the fix needs and how to verify it. The
reading behind it is in the commit that wrote the row, named here by hash.

Milestones A to E ended on 2026-09-21 and 2026-09-22 (A with the tag
`v0.4.0`). This file was filled on 2026-09-22 by an audit of every document
against the code. Work it in order: F (defects), G (the bindings reach what
Python reaches), H (performance), I (the rest of SPECS). Milestones F and
G ended on 2026-09-23.

## Milestone H: performance

Where JAOS stands (`bench/compare/results/P0.txt`, 2026-09-23, after
e180c91): on LP it takes 2.11x HiGHS's time and 1.71x Clp's over the
instances above the 0.05 s floor; the iteration counts are close (1.15x
HiGHS), so the gap is time per iteration (1.83x). HiGHS itself ran 17.5%
faster than in the reading of 2026-09-22 (`P0-2026-09-22.txt`), which drops
three instances under the floor; on the 19 instances both readings share,
JAOS against HiGHS went from 1.77x to 1.65x, and JAOS's own time fell to
0.78 with every iteration count the same. On MIP (`mip-miplib.txt` and
`mip-miplib2017.txt`, 2026-09-23, tree 12180a6) it solves 22 of MIPLIB 3 in
20 s where HiGHS and SCIP solve 24, at 1.34x HiGHS's shifted mean time and
1.40x SCIP's (1.43x and 1.48x on 2026-09-21, when it solved 23; `bell5`
has grown from 190741 nodes to 327119 since the best-estimate node order),
and 0 of the 2017 set where HiGHS and SCIP solve 8 each.

H1. **The simplex's time per iteration, second pass.** `stocfor3` still
takes 14.5x HiGHS. After e180c91 its callgrind profile is 32.4e9
instructions: the entering column's FTRAN 36%, the pricing row's BTRAN
15%, refactoring 11%, and the two dense copies in `pivot` (`col` from
`raw`, `tau` from `rho`) 8.5%. Those copies stay as they are unless the U
solve stops leaving -0.0 outside the column's pattern, since clearing by
pattern would change signs of zero. Verify with `tools/icount.sh` and
`make compare COMPARE_ARGS='-t P0'` on a quiet machine; the gates
byte-identical or re-based.

H2. **MIP: the tree has too many nodes.** The attribution of 2026-09-23
(`bench/measurements/02-300/`, the log line "branch and bound work")
puts 77% to 98% of the work in node relaxations on 22 of MIPLIB 3's 24
instances and 84% to 100% on 29 of the 2017 set's 30, so the tree loses on
node count: `bell5` takes 327119 nodes where SCIP takes 357. On `bell5` and
`bell3a` SCIP's small trees come from strong branching and bound
propagation, not from presolve or cuts. JAOS's strong branching cuts
`bell5` to 8287 nodes at reliability 2, but its probes cost more than they
save on the set (0.958x work, three instances past 2x), so D293 holds.
Next: a cheaper probe (a dual simplex probe stopped after a few
iterations, its bound read from the dual objective), and a propagation
that tightens as much as SCIP's; each measured against D293's and D324's
reopen conditions.

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
