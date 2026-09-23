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
and 0 of the 2017 set where HiGHS and SCIP solve 8 each. On QP
(`qp-maros-meszaros.txt`) it solves 133 of 138 at 0.40x HiGHS's shifted
mean and 0.72x Clp's, and on continuous CBLIB (`conic-cblib.txt`) 27 of
29 at 0.13x SCIP's, which solves 2 (`bench/compare/README.md`).

H1. **The simplex's time per iteration, second pass.** `stocfor3` still
takes 14.5x HiGHS. After e180c91 its callgrind profile is 32.4e9
instructions: the entering column's FTRAN 36%, the pricing row's BTRAN
15%, refactoring 11%, and the two dense copies in `pivot` (`col` from
`raw`, `tau` from `rho`) 8.5%. Those copies stay as they are unless the U
solve stops leaving -0.0 outside the column's pattern, since clearing by
pattern would change signs of zero. The dual's choice of leaving row
read every basic variable's bounds on every iteration, 17% of
`stocfor3`'s instructions; it now reads a violation cached per row and
updated where the pivot and the bound flips move `xb` (`stocfor3`
0.888x, `80bau3b` 0.938x, `d2q06c` 0.998x, `pilot87` 1.003x in
instructions, every file the same). The next item in that profile is the
dense FTRAN path (14%). The entering column is not what takes it: its
FTRAN averages 3.3% dense and runs hyper-sparse 90% of the time. The
steepest-edge `tau` does: 17% dense on average, split between 7365 solves
at 10% or more and 4705 under 1%, and `rho`'s own count does not tell
them apart (with `rho` under 1% dense, 47% of the `tau` still reach 10%).
`fit2p` (4.6x HiGHS's time at 0.95x its iterations) spends 55% of its
instructions refactoring: `compact_pivot_row` searches each column of the
pivot row from its start for the pivot row's entry (31%), and the column
singleton step moves each column's tail to drop that entry (20%), both
long on the basis's few 3000-entry columns. A fix must keep each column's
order, since that order decides the pivot ties. On `d2q06c` and `dfl001`
the largest item is `price_all` (16% and 13%), which also prices the
basic columns; HiGHS keeps a row-wise copy of the nonbasic columns only.
Half of `stocfor3`'s gap is presolve:
HiGHS takes it from 16675 rows to 8259 (its aggregator 5508, doubleton
equations 2054, free column substitution 769) and needs 6404 iterations,
where JAOS's presolve leaves 13305 rows and the dual needs 12977. JAOS
already aggregates every doubleton equation of `stocfor3`. Two ways to
reach more were built and refused on 2026-09-23
(`bench/measurements/02-302/`): D97's bound transfer leaves `stocfor3`
as it is and breaks the duals of `standata`; a column counted implied
free by any of its rows takes `stocfor3` to 12089 rows and 0.865x work,
but reads 1.003x over the set.
On `fit2p` the factor took 79% of the instructions; f43d9e1 halved that
without changing a bit. Verify with `tools/icount.sh` and `make compare
COMPARE_ARGS='-t P0'` on a quiet machine; the gates byte-identical or
re-based.

H2. **MIP: the tree has too many nodes.** The attribution of 2026-09-23
(`bench/measurements/02-300/`) puts most of the work in node
relaxations. Since d6245e0 a node keeps its parent's basis through
presolve (a fixed column the start basis holds basic stays in the reduced
model): MIPLIB 3 0.631x in work, `bell5` 14767 nodes instead of 327119,
the 2017 set 0.948x in gap sum (`bench/measurements/02-304/`). The root's
coefficient tightening then read a pulling column's slack at 1, as
Savelsbergh's rule does: 0.928x on MIPLIB 3
(`bench/measurements/02-307/`). Strong branching (D293, 02-305) and
node propagation (D324, 02-306) were read again on that tree and still
cost more than they save; a probe that learns from a stopped child is
still D293's reopen condition. Behind HiGHS and SCIP on MIPLIB 3:
`l152lav` at the 20 s limit, where 113 of 374 node LPs still arrive short
from forcing rows that fix basic columns (keeping those rows is refused
as `node-forcing-keep`), and `bell3a` at 8.8 s against 0.24 s. On the
2017 set HiGHS and SCIP close three fixed-charge networks at the root
with one node (`sp150x300d` 0.05 s, `p200x1188c` 0.43 s,
`exp-1-500-5-5` 2.3 s) where JAOS stops at 1e10 work units with bounds
of 67.9 against 69, 7395 against 15078 and 46851 against 65887. The
bounds the flow rows imply do not reach their `x - u y <= 0` rows (one
pass finds none on `p200x1188c` and `exp-1-500-5-5`), and flow covers or
MIR over aggregated rows lift them only part of the way (10302 and
60879), so the gap is in the cuts.

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
