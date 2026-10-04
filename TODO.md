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

J7. **MIP: MIPLIB 2017, 3 of 30 at 1e10 work units.** Since 2026-10-04
(`bench/measurements/02-328/`) the tree rounds a bound up to the
objective's step when every cost sits on one, the root fixes the integer
columns its rows' implied bounds fix, two heuristics run (lock rounding at
the root, and a sub-MIP: RENS at the root, RINS at the root and every 100
nodes below it), and a model where a third or more of the continuous
columns sit under a binary gets 20 rounds of c-MIR with variable bound
substitution. `sp150x300d` and `exp-1-500-5-5` solve, 20 of the 30 hold an
incumbent (17 before) and the gap sum falls from 26.0 to 21.5; MIPLIB 3
reads 0.957x in the geometric mean of work, `bell3a` 1.515x the worst.
A second batch the same day (`bench/measurements/02-329/`) runs lock
rounding before the root's dive and pump, adds a feasibility jump at a root
with no incumbent, widens the network c-MIR's fraction window and runs both
aggregation rules in network mode: 24 of the 30 hold an incumbent, the gap
sum is 18.2, and MIPLIB 3 reads 1.014x of the first batch. A third
(`bench/measurements/02-331/`) fixes the binaries that equality rows with
an even coefficient determine mod 2: `enlight_hard` solves at the root,
the gap sum is 16.8.
HiGHS and SCIP solve 8 each in 20 s. Left, largest first. First, the
networks' root bound: `p200x1188c` holds its optimum 15078 and a bound of
13767 (10789 before 02-341), where HiGHS's root reaches 11640 and then
restarts several times, each restart fixing columns by reduced cost against
the optimum it already has (JAOS's reduced-cost fixing fixed nothing from
10930); `beasleyC3` holds 831 against 754 with a bound of 740 (HiGHS's
root 733), a node there costing 1e7 work units. Five ideas for these roots were read
and refused on 2026-10-04 (`bench/measurements/02-330/`): longer
aggregation walks reach HiGHS's root on `beasleyC3` (733.5) but the root's
LP solves over 5175 rows cost 5.5e9 work units. A pool that takes slack
cuts out between root rounds keeps that LP near 2500 rows and the 12-step
root at 0.94e9 work units, and lifts `beasleyC3`'s bound at the limit to
741, but alone its incumbent gets worse and the gap sum does not fall
(`bench/measurements/02-339/`). Since 2026-10-04 the network MIR round and
its aggregation read the cuts of earlier rounds as rows
(`bench/measurements/02-341/`), and network mode runs up to 100 rounds with
that pool (`bench/measurements/02-342/`): `p200x1188c`'s root reaches 13137
(HiGHS 11640) and its bound at the limit 13767 against the optimum 15078,
`tr12-30` ends at 132496 over 130131 (optimum 130596), `sp150x300d` solves
at the root, and the gap sum is 14.93. Cuts on node sets were read and
refused there (`net-node-sets`). On `p200x1188c` a stall of 1e-5 takes the
root to 13973 for 2.7e9 work units; HiGHS closes it with restarts that fix
columns by reduced cost against the optimum it holds. A MIP presolve that merges the flows through a node
(`bench/measurements/02-337/`) takes `beasleyC3` to 797 with a bound of
724; HiGHS's presolve removes 597 of its 1750 rows where JAOS's removes
161, so its other reductions come next. Second, flat roots: `neos-911970` and
`neos-3381206-awhea` need 10 to 20 rounds of MIR on simplex tableau rows
before their bound moves (to 51.6 and 446, HiGHS 52.1 and 451.8), and the
restart that ran them was refused (mip-deep-restart). Since 2026-10-04
the root's MIR rounds go on past 6 while each lifts the bound
(`bench/measurements/02-332/`): `neos-911970`'s root reaches 43.3 and its
bound at the limit 45.0, and 52.0 since the node solves perturb on their
first stall (`bench/measurements/02-333/`). `neos-3381206-awhea`'s bound sits flat for seven
rounds, so the rule stops it at 416; 20 rounds of Gomory cuts take its root
to 445.3 for 6.1e9 work units. Third, no incumbent: `glass4`, `timtab1`,
`ic97_potential`, `csched007` and `csched008`, where the feasibility jump
fails within its cap. `csched008`'s root costs 3.71e9 work units since
its re-solves after cuts make their weights exact
(`bench/measurements/02-334/`), and its bound at the limit is 171 (the
reference 173), with no point. Fourth, `binkar10_1` holds the reference's point and
a bound 0.4% short after 16000 nodes, where HiGHS closes in 4066. Its root
reads 6693 against HiGHS's 6701, and at 1e10 work units strong branching
at reliability 4, the best-bound order and propagation at 4 passes end its
bound at 6716.8, 6718.6 and 6713.6 (read 2026-10-04), where HiGHS's tree
reaches 6720.9 by node 2460 with cuts separated at its nodes from a pool;
JAOS cuts below the root only with Gomory cuts to depth 3. Root cuts
scanned again at every node take the bound to 6720.4 but cost MIPLIB 3 and
the 2017 gap sum (`node-pool-scan`, `bench/measurements/02-338/`). A MIR
and a Gomory round at every node reach 6726.6 in 5128 nodes, and on the
whole 2017 set they raise the gap sum to 16.17 (`bench/measurements/02-340/`):
the bounds that come from the count of nodes (`pk1`, `mas74`) fall as each
node costs more. A cut at a node pays only where it saves more nodes than it
costs, which these readings do not find outside `binkar10_1`. On
MIPLIB 3, `l152lav` (113 of 374 node LPs arriving short from forcing rows
that fix basic columns; keeping those rows is refused as
`node-forcing-keep`) and `bell3a` (82261 nodes), where HiGHS closes in 19
and 215 nodes. Better root points do not shrink those trees, and strong
branching does at a price the small trees cannot pay
(`bench/measurements/02-336/`); the next form is a probe that learns from a
child stopped by a work cap or proved infeasible (D293's reopen clause).
Verify with `make miplib J=2`, `make miplib2017` and
`bench/measurements/02-328/m17sum.py`.

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
6 GB cap while the process holds 2.3 GB. The symbolic factor it cannot
allocate holds 7.67e9 nonzeros (123 GB) for 2.8e14 operations, from a
lower triangle of 37.4 million (read 2026-09-25). The model is a
time-dependent control problem on a grid of about 99 by 99 points over
about 100 steps (rows of 19701, 19503 and eight entries of -4900.5), so
the normal matrix has the connections of a three-dimensional grid. On
grid graphs nested dissection gives less fill than minimum degree, and
JAOS has no nested dissection ordering; how much less on this model is not
measured. The other ways are a solve that does not factor the whole
system, or one that uses the time steps' structure. 13 of 17 convex MIQPs do not
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
restarts (they need a MIP presolve that removes what they fix; since
2026-10-04 they fire, at 1.018x on MIPLIB 3, `bench/measurements/02-343/`),
bound propagation
and probing. Each needs a reading that lands it on, on the tree J7 leaves.
Clique fixing and RINS went on on 2026-09-25, RINS off for a quadratic
objective, and reduced-cost fixing on 2026-10-04. Read again one at a time
on the tree of 3dfc0f6 (`bench/measurements/02-335/`), against MIPLIB 3's
geometric mean of work and the 2017 gap sum: zero-half 1.092x and 0.999x,
lifted covers 1.043x and 0.984x (`misc03` 2.534x), local branching 1.983x
and 0.988x with one model fewer solved, propagation 1.070x and 1.047x,
probing 1.325x in the sum with `bell5` out of memory at 4 GB, and reduced-cost
fixing 0.944x and 0.999x, which landed it.

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
