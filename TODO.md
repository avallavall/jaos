# TODO — the current milestone

Rows from `SPECS.md`, and defects found by reading the code and the
readings. A line leaves this file in the commit that lands it. When the file
is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong, what the fix needs and how to verify it. The
reading behind it is in the commit that wrote the row, named here by hash.

Milestones A to E ended on 2026-09-21 and 2026-09-22 (A with the tag
`v0.4.0`), F and G on 2026-09-23. H and I were folded into this file on
2026-09-24: milestone J holds everything still open, in priority order.
Work it from the top. Tier 1 is defects that publish an answer the checker
refuses. Tier 2 is small fixes to the bench tooling and the records.
Tiers 3 and 4 are the performance gaps, largest first. Tier 5 is the
features SPECS still lists.

Where JAOS stands (`bench/compare/README.md`, 2026-09-24, tree 6c79039).
LP: 1.91x HiGHS's time, 1.54x Clp's and 0.57x SoPlex's over the instances
above the 0.05 s floor; the iteration counts are close (1.15x HiGHS), so
the gap is time per iteration (1.66x). MIP: MIPLIB 3 23 of 24 solved in
20 s at 1.18x HiGHS's shifted mean time and 1.23x SCIP's; the 2017 set 0
of 30 where HiGHS and SCIP solve 8 each (1.44x and 1.58x). QP: Maros-Meszaros 133 of 138 at 0.40x HiGHS's
shifted mean and 0.72x Clp's. Conic: continuous CBLIB 27 of 29 at 0.13x
SCIP's (`bench/compare/README.md`).

## Tier 1: answers the checker refuses

J2. **QP models with no accepted optimum.** Since 02-315 a QP optimum
the checker refuses is not published, so QPLIB_9002 and Maros-Meszaros
`qgrow22` end `NUMERICAL_ERROR` where they ended `OPTIMAL` with duals off
by 2.1e4 and 3e-6. Both need a point the checker takes. On `qgrow22` the
push leaves 28 pinned variables with a reduced cost of the wrong sign
after 3 freeings, the worst 3.2e4; on QPLIB_9002 931, the worst 8.7e9.
Moving the columns the barrier marks as sitting at a bound onto that
bound does not make either pass. QPLIB_8785 ends `OPTIMAL` with rows off
by 3e-15 and duals by 0, and its objective gap of 1.31e-7 fails the
checker at 1e-7 and passes at 1e-6. `aug3dqp` sits over the runner's
suboptimality ceiling (1.03e-3 against 1e-6): its 114 columns with no
quadratic term carry reduced costs of -4e-13 against upper bounds of
8.6e10 that the rows imply. Verify with `make maros-meszaros` (136 solved
and checked today; `values` is refused as not convex by design).

J3. **Conic models with no accepted optimum.** Since 02-316 a conic
optimum the checker refuses is not published, so 8 of QPLIB's continuous
QCQPs end `NUMERICAL_ERROR` where they ended `OPTIMAL` with duals off by
8.5e-7 to 3.6e-5 (primal side feasible to 2e-13). They need duals the
checker takes: the refused ones sit on ball rows with a nonzero multiplier
while the row is off its side, and there the Newton finish diverges.
QPLIB_2676 and QPLIB_2468 end `NUMERICAL_ERROR` too: the walk stops
without progress and the checker refuses its point.
`tests/data/g_cone_badbox.mps` with two of its columns in a row ends at a
certificate the checker refuses. An infeasibility whose free column with
no curvature needs its coefficient to vanish exactly has no certificate
one multiplier at a time can hold. Verify with `make cblib` and the QCQP
reading of 02-256.

## Tier 2: the bench tooling and the records

J6. **One reading was never taken.** `make plato-nug` has never written
`bench/results/plato-nug.txt`: in 02-94 `nug08-3rd` took 2.9e11 work
units and 511 s, and `nug20` and `nug30` were stopped after 68 minutes.
Take it, or record in its README why nug20 and nug30 cannot run.

## Tier 3: the largest performance gaps

J7. **MIP: MIPLIB 2017, 0 of 30.** The attribution of 2026-09-23
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
still D293's reopen condition. First, the cuts: HiGHS and SCIP close three
fixed-charge networks at the root with one node (`sp150x300d` 0.05 s,
`p200x1188c` 0.43 s, `exp-1-500-5-5` 2.3 s) where JAOS stops at 1e10 work
units with bounds of 67.9 against 69, 7395 against 15078 and 46851 against
65887. The bounds the flow rows imply do not reach their `x - u y <= 0`
rows (one pass finds none on `p200x1188c` and `exp-1-500-5-5`), and flow
covers or MIR over aggregated rows lift them only part of the way (10302
and 60879). MIR with variable upper bounds (`mir-vub`) and exact cover
lifting (`cover-exact`) were refused on 2026-09-24. Then MIPLIB 3:
`l152lav` at the 20 s limit, where 113 of 374 node LPs still arrive short
from forcing rows that fix basic columns (keeping those rows is refused as
`node-forcing-keep`; the fix needs a dual postsolve for a kept basic
column), and `bell3a` at 8.8 s against 0.24 s. Verify with `make miplib
J=2`, the 2017 gap sum (`bench/measurements/02-298/gapsum.py`) and
`run-mip.sh`.

J8. **LP: the simplex's time per iteration, and presolve.** `stocfor3`
still takes 11.2x HiGHS (14.5x before 2d6f3dc and 764fe58). After
e180c91 its callgrind profile was 32.4e9
instructions: the entering column's FTRAN 36%, the pricing row's BTRAN
15%, refactoring 11%, and the two dense copies in `pivot` (`col` from
`raw`, `tau` from `rho`) 8.5%. Those copies stay as they are unless the U
solve stops leaving -0.0 outside the column's pattern, since clearing by
pattern would change signs of zero. The dual's choice of leaving row now
reads a violation cached per row (2d6f3dc: `stocfor3` 0.888x, `80bau3b`
0.938x in instructions). The next item in that profile is the dense FTRAN
path (14%). The entering column is not what takes it: its FTRAN averages
3.3% dense and runs hyper-sparse 90% of the time. The steepest-edge `tau`
does: 17% dense on average, split between 7365 solves at 10% or more and
4705 under 1%, and `rho`'s own count does not tell them apart (with `rho`
under 1% dense, 47% of the `tau` still reach 10%). On `d2q06c` and
`dfl001` the largest item is `price_all` (16% and 13%), which also prices
the basic columns; HiGHS keeps a row-wise copy of the nonbasic columns
only (pricing column by column is refused, `price-by-column`). Half of
`stocfor3`'s gap is presolve: HiGHS takes it from 16675 rows to 8259 (its
aggregator 5508, doubleton equations 2054, free column substitution 769)
and needs 6404 iterations, where JAOS's presolve leaves 13305 rows and the
dual needs 12977. JAOS already aggregates every doubleton equation of
`stocfor3`; D97's bound transfer and a column counted implied free by any
of its rows were refused (`bench/measurements/02-302/`). The LU's column
singleton step no longer searches and shifts long columns (764fe58,
`fit2p` 0.447x, `bench/measurements/02-313/`). Verify with
`tools/icount.sh` and `make compare COMPARE_ARGS='-t P0'` on a quiet
machine; the gates byte-identical or re-based.

J9. **The large QPs and MIQPs.** 7 of QPLIB's 8 largest convex QPs reach
1e11 work units. QPLIB_9008 runs out of memory (read 2026-09-24): the
barrier's normal matrix has 989604 rows from 9.6 million nonzeros; the
minimum degree ordering and symbolic factor take 41.6e9 work units and 3
minutes without an iteration, then an allocation beyond 3.7 GB fails at a
6 GB cap while the process holds 2.3 GB. 13 of 17 convex MIQPs do not
finish within 1e11 work units. 37 of CBLIB's 80 mixed-integer instances
stop at the work limit.

## Tier 4: the other performance rows

J10. **The primal's six overruns and the crossover's fourteen.** The
primal runs past 10x the dual's work on d6cube, dfl001, fit1d, fit2d,
pilot and seba (`bench/results/primal.txt`); the crossover overruns 14 of
the 94 (`bench/results/barrier.txt`) and 6 of the infeasible set. Seven
primal remedies are refused; read `bench/refusals.txt` first.

J11. **MIP switches that are off by measurement.** Strong branching (D293
reopens on a probe that learns from a stopped child), flow cover,
zero-half and lifted cover cuts, RINS and local branching, restarts (they
need a MIP presolve that can run again), bound propagation and
reduced-cost fixing, probing and clique fixing. Each needs a reading that
lands it on, on the tree J7 leaves.

J12. **Parallel.** A parallel simplex; a round of nodes cheap enough to be
the default where a node takes a few pivots (rounds of 4 cost 1.37x the
work on MIPLIB 3, `bench/measurements/02-290/`). The rest of the barrier
stays on one thread by measurement (`barrier-normal-threads`,
`bench/measurements/02-311/`).

## Tier 5: the features SPECS still lists

J13. **Cones and quadratic rows.** Cuts and warm starts in the conic tree;
SOS sets, semi-continuous columns and indicator rows beside cones; a
quadratic row over more than `CONIC_QC_DENSE` columns; QPLIB's
mixed-integer QCQPs, 8 of which end with no incumbent and 2 refused for
that dense quadratic row.

J14. **The barrier, PDLP and the concurrent solve.** A rule for an LP to
choose the augmented system, which needs an analysis cheaper than the
symbolic factorisation; a crossover cheaper than a crash. PDLP: a set too
large to factor where it pays (on Netlib it overruns 70 of 94), and
feasibility polishing. The concurrent solve: a set where the barrier is
the arm that wins.

J15. **Presolve.** Duplicate columns, dominated columns, bound tightening,
dual fixing, and the substitution of a column the equality does not imply
free (D97, refused as `agg-doubleton-moves`). Each waits for a set past
its bar: D101 and D246 ask for 5% of a set's rows or columns, and every set
reads under 1% (`bench/measurements/02-312/`). Duplicate rows passed that
bar on MIPLIB 2017 and were built and refused (`presolve-duplicate-rows`,
`bench/measurements/02-314/`).

J16. **Links.** GAMS, which needs a link library of its own; `.nl` bodies
above degree two, refused by line today.

J17. **The feasibility relaxation's proof that the widest box is empty.**
An integer feasibility question over an unbounded space.

J18. **The certified bound on suboptimality alone cannot separate a wrong
vertex from a right one.**

J19. **Exact solving with no tolerances** (missing).
