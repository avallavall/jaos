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
refuses. Tier 2 was small fixes to the bench tooling and the records, and
is empty since 2026-09-25. Tiers 3 and 4 are the performance gaps, largest
first. Tier 5 is the features SPECS still lists.

Where JAOS stands (`bench/compare/README.md`, 2026-09-24, tree 6c79039).
LP: 1.91x HiGHS's time, 1.54x Clp's and 0.57x SoPlex's over the instances
above the 0.05 s floor; the iteration counts are close (1.15x HiGHS), so
the gap is time per iteration (1.66x). MIP: MIPLIB 3 23 of 24 solved in
20 s at 1.18x HiGHS's shifted mean time and 1.23x SCIP's; the 2017 set 0
of 30 where HiGHS and SCIP solve 8 each (1.44x and 1.58x). QP: Maros-Meszaros 133 of 138 at 0.40x HiGHS's
shifted mean and 0.72x Clp's. Conic: continuous CBLIB 27 of 29 at 0.13x
SCIP's (`bench/compare/README.md`).

## Tier 1: answers the checker refuses

J2. **QPLIB_9002 has no accepted optimum.** Since 02-315 a QP optimum
the checker refuses is not published, and QPLIB_9002 ends
`NUMERICAL_ERROR`. It is a network flow (2890 columns, 1649 equality
rows of coefficients ±1) with a separable quadratic cost whose
curvatures run from 9e-12 to 2. The barrier stops making progress at a
dual residual of about 3e-9, and the push's first full step finds 743 of
1983 pinned variables with the wrong sign; after 5 freeings 734 are
left, the worst 1.5e2 (02-318). Two causes were found. First, the push's
equality-constrained QP does not fix the rows' duals in the directions
no free column touches, and its regularised step moves them by up to
345 there. Keeping the barrier's duals in those directions (a solve with
the rows' residual at zero) took the wrong signs from 760 to 40 under
`JAOS_NEARMODE=6` of the diff below, but the push still did not settle:
freeing the 40 at once gave each a step of 1e-8 the wrong way, and they
were pinned again. Second, the pinned set is
really wrong: at the push's point the LP `min g'x` with `g = c + Qx` over
the same rows reaches 2.3e-4 lower than `g'x`, so duals from that LP do
not rescue it. Freeing one variable a round from those clean duals
(`one-free.diff`) takes the wrong signs from 275 to 251 in 5 rounds, and
then a row stays 1.2e-6 off with none of its variables pinned, so the
push has nothing to release. It needs an active-set iteration that frees
one variable at a time from duals the rows fix, and releases a pin
elsewhere in the row's connected part when such a row stays off; or a
barrier that converges further. The dual-direction solve is in
`bench/measurements/02-318/push-modes.diff` (`JAOS_YCLEAN`). Verify with
the QPLIB reading of 02-318 (`cqp-new.txt`).

J3. **Conic models with no accepted optimum.** Since 02-319 a refused
conic optimum is settled on its active rows (a least move of the columns,
then a least-squares refit of the duals, with two rules for which rows
are active), and 9 of QPLIB's 13 continuous QCQPs end `OPTIMAL` taken by
the checker. QPLIB_2456 and QPLIB_3105 still end `NUMERICAL_ERROR`: their
walks stall near a gap of 1e-9 with 226 and 1196 ball rows refused, and
neither rule settles them. On QPLIB_2456 holding every row whose dual is
over 1e-7 on its side puts 118 rows 2e-5 to 4e-5 off their side, with
duals of 1.1e-7 to 1.7e-7, into the active set; together they are
inconsistent and the move is refused (worst violation 1.1e-1), while the
other rule leaves duals off by 5.6e-7. On QPLIB_3105 the two rules leave
duals off by 5.8e-7 and rows off by 7.1e-6. QPLIB_2468 ends
`NUMERICAL_ERROR` too: the walk stops without progress, and the settle
reads the same way (2026-09-25): under the first rule the projection
passes and the refit stalls at a stationarity residual of 3.6e-7; under
the second the refit reaches 6.7e-10 but the projection is refused
(worst violation 1.1e-1). Keeping in the refit the inactive rows' duals
already under the tolerance changes none of the three. The residual comes
from the rows whose dual is over 1e-7 and whose slack is larger still.
On QPLIB_2456 those are 118 rows with duals up to 1.5e-6. Adding them to
the active set in batches by the size of their dual does not settle it
(2026-09-25, `conic-settle-batches`, `bench/measurements/02-327/`): the
12 largest take the refit to
5.0e-7 and the 30 largest to 3.3e-7, and from 39 the projection is
refused. Keeping every left-out row's own dual in the refit reaches a
stationarity residual of 8.9e-10, but the checker counts a dual over 1e-7
on a row off its side as a violation of that size (1.87e-6), so those
duals have to be 0. The walk's point is not near enough an optimum for its
duals to name the active rows. What is left is a finish that moves the
columns and the duals together while the active set changes, or a walk
that converges further. An infeasibility whose free
column with no curvature needs its coefficient to vanish exactly has no
certificate one multiplier at a time can hold. Verify with `make cblib`
and the QCQP reading of 02-319 (`conread.sh`).

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
rows (one pass finds none on `p200x1188c` and `exp-1-500-5-5`). Flow
covers are on at 5 rounds since 2026-09-24 and lift the root bounds of
`sp150x300d` and `p200x1188c` to 51.0 and 9865; MIR aggregation of 6
steps lifts `exp-1-500-5-5`'s to 59627, and with both on `sp150x300d`
solves (4.7e9 work units), but aggregation costs 1.588x on MIPLIB 3
(`bell5`'s tree 14767 to 2112667 nodes, `gen` 19.6x from its per-step
scan of every column; `bench/measurements/02-317/`). Since 2026-09-24
the MIR rounds touch only the columns of the rows they aggregate, with
the same cuts (MIPLIB 3 writes the same files). `bell5`'s growth is its
tree's sensitivity (`bench/measurements/02-320/`): the aggregated root
cuts alone, one aggregation step already, lift its root bound by 3e-5
of itself and send pseudocost branching down a 143x longer path, and a
cutoff at the optimum changes neither tree. Since 2026-09-25 aggregation
is on at 6 steps, each root round's aggregated cuts tried on a copy of
the root LP and kept only when they lift the bound by
`MIP_MIR_AGG_GAIN` (1e-2) of itself (`bench/measurements/02-321/`):
MIPLIB 3 1.012x, the 2017 gap sum 0.986x, `exp-1-500-5-5`'s bound
49815 to 61197 of 65887. `sp150x300d` drops the cuts and stays unsolved,
and `timtab1`'s bound falls from 441250 to 414914. Next on the networks:
what `sp150x300d` needed from aggregation, since its root bound does not
move. MIR with variable upper bounds (`mir-vub`) and exact cover
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
only (pricing column by column is refused, `price-by-column`). Since
2026-09-25 `price_all` reads such a copy, the nonbasic entries first in
each row, with the same answers everywhere: 0.975x instructions over
seven models, d2q06c 0.949x and dfl001 0.965x, stocfor3 0.997x
(`bench/measurements/02-323/`). Half of
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
reopens on a probe that learns from a stopped child), zero-half and
lifted cover cuts (flow covers went on in 02-317), local branching,
restarts (they need a MIP presolve that can run again), bound propagation,
reduced-cost fixing and probing. Each needs a reading that lands it on, on
the tree J7 leaves. Clique fixing and RINS went on on 2026-09-25, RINS off
for a quadratic objective. The same reading took the others one at a time
on MIPLIB 3 and the 2017 set (`bench/measurements/02-325/`) and none gains
on both: local branching 0.948x in the 2017 gap sum at 2.023x MIPLIB 3's
work, lifted covers two first incumbents on the 2017 set at 1.042x,
propagation 1.033x in the gap sum, zero-half 1.200x on MIPLIB 3,
reduced-cost fixing no change, and probing takes `bell5` past 4 GB.

J12. **Parallel.** A parallel simplex; a round of nodes cheap enough to be
the default where a node takes a few pivots (rounds of 4 cost 1.37x the
work on MIPLIB 3, `bench/measurements/02-290/`). The rest of the barrier
stays on one thread by measurement (`barrier-normal-threads`,
`bench/measurements/02-311/`). JAOT asked for it in issue #10: a MIP that speeds
up with the thread count alone. The batch cannot follow the thread count,
since the answer must not depend on the machine, so the round has to be
cheap enough to be on at a fixed size.

## Tier 5: the features SPECS still lists

J13. **Cones and quadratic rows.** Cuts and warm starts in the conic tree;
a quadratic row over more than `CONIC_QC_DENSE` columns; QPLIB's
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
