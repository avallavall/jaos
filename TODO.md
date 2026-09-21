# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Two milestones, in this order; milestone A, publishing 0.4.0, ended with
the tag `v0.4.0`. A row in a later milestone waits for the earlier ones
unless it says otherwise. Every row was taken from the reading of
2026-09-21 (the three audits of the docs, the code and the bench record)
and nothing from that reading is left out of this file. Each row says how
to verify it, so a row leaves when its check passes, not before.

## Milestone B: performance

The gap against HiGHS is 3.46x per solve (P0, tree 6ae3966, 2026-09-21,
`bench/compare/results/P0.txt`): 2.12x per iteration over the set, and
the iteration count on four instances. Rows B6 to B9 in gain order; B11
has no expected gain on record, and B14 follows from the others, so
they come after. Each is unrefused today; read the named refusal before
starting and stop if its condition is not met. Every row
that changes `simplex.c`, `lu.c`, `presolve.c`, `scale.c` or `mip.c` runs
the gates (`CLAUDE.md`, step 3). Every B row reads its before from that
`P0.txt`.

The bar for a B row, unless it says otherwise: no instance of the four
gates past 2.0x its baseline work, the geometric mean of work under 0.95x
over the standard 94, and no verdict or suboptimality bound regressed.

B6 **The crossover push.** SPECS's crossover row is missing a primal and
    a dual push; from the barrier's point the ranked guess costs more than
    a cold dual solve on 21 of 94 (d2q06c 237232 iterations against 27935,
    pilot87 83342 against 37362). `crossover-primal`, `crossover-dual-slack-key`
    and `crossover-tight-barrier` are refused; the push itself is not.
    Build the push (Bixby and Saltzman's form: move each nonbasic column to
    a bound along a direction that keeps primal feasibility, then the dual
    push), measure `make barrier`. Bar: fewer than 21 overruns and the
    barrier's geometric mean under 2.705x the dual. SPECS row "Crossover"
    changes to done when the count is zero.

B8 **Parallel tree for `src/mip.c`.** Row C7 below has the design. A
    wall-clock reading needs the larger set, which `make miplib2017` now
    is (`bench/measurements/02-284/`).

B9 **A parallel simplex or a parallel barrier.** SPECS's "Parallel LP"
    row: one method on N cores on one factorisation. Absent and not
    refused. The barrier's normal-equation Cholesky is the natural first
    (parallel column blocks in `src/chol.c` with a fixed schedule, so the
    result is bit-identical). Last in B; it needs a design reading first.

B11 **Local branching, MIP restarts, node selection.** SPECS "RINS, local
    branching" is partial: RINS is off by measurement and local branching
    was never built. Build it behind a switch and measure on B7's set. MIP
    restarts (a root restart after enough fixings) and a node selection
    beyond best-bound (best-estimate, or a plunge with a bound gap) have no
    SPECS row and no refusal; SPECS is closed, so the user decides whether
    to add the two rows. Ask once, with B7's numbers, before building.

B14 **Re-take P0 when milestone B ends.** `README.md`'s Results table,
    `bench/compare/README.md` and this milestone's intro quote P0 as
    re-taken after the rows that land before it. Re-take
    `make compare COMPARE_ARGS='-t P0'` on a quiet machine once B3 to B11
    have landed or been refused, and update the three.

## Milestone C: reach and polish

The research rows. They were here before 2026-09-21 and stay as written.
C3 is B's item 9 (the primal's five instances). C7 is B8.

C3 **Primal simplex: 5 of the 94 standard instances run past 10x the dual's
   work** (`bench/results/primal.txt`): d6cube, dfl001, fit1d, fit2d, seba.
   None disagrees. The SPECS row stays partial until the count is zero.

   **Do not re-measure these.** Six ideas are refused with their reopen
   conditions in `bench/refusals.txt`: `primal-bound-perturbation`,
   `primal-tie-hash`, `primal-cost-perturbation`, `primal-noise-floor`,
   `primal-expand-step`, `primal-expand-schedule`. Steepest edge, Devex and Dantzig are all already
   read over the set; the SPECS row records them. Cost per iteration was
   read and paid 7.6% over the set for nothing here, because the gap is the
   iteration count.

   **The five are two faults, not one**, so a remedy aimed at either reads
   as noise over the set unless it is measured on its own group (d5a43e9).

   - d6cube stands still, as degen3 does under the bar at 4.0x. d6cube holds one vertex for 8508
     consecutive phase-2 bases. They need a remedy that leaves a vertex.
     EXPAND is now measured whole (`primal-expand-schedule`, 2026-09-15):
     the step, the growing width, the reset and the hold at the ceiling
     get d6cube to 0.33x at best and 27x the dual's work, and the walk is
     chaotic in the width, so no schedule closes the gap. What is left to
     try on d6cube is the pricing side, which is also what seba and fit1d
     point at.
   - seba and fit1d do not stall at all. They leave for a new point at
     nearly every pivot, so degeneracy is not what holds them and the
     target is the entering column. What holds them is still unnamed.

   The walk revisits no basis, so there is no cycle for an anti-cycling
   rule to break, which is why Bland's rule never pays here (f954aee).

C4 **Convex QP: the Maros-Meszaros set.** SPECS §1, "Convex quadratic (QP)". `make
   maros-meszaros` reads the 138 instances against BPMPD's values; the
   first reading (`bench/measurements/02-250/`) found and 808022a fixed
   the NaN points, the 1e-6 row slips, the four unreadable files and the
   handoff that ground the dual simplex; six more fixes followed from its
   list (02-250, "After the reading"), and the push's stretch and its
   longer freeing took two more on 2026-09-19, the augmented restart took
   boyd2, and the conic interior point after a failed barrier took dtoc3,
   ksip and ubh1 (`bench/measurements/02-258/`; the barrier alone still
   stops on the three). What it leaves, 132 of 138 clean, 137 `OPTIMAL`
   and 136 taken by the checker:

   - **one the checker refuses with the objective right**: qgrow22. Its
     first push step came out NaN, and since 2026-09-19 the push takes
     such a round again on a larger regularisation; it then runs 40
     rounds and ends with 19 pinned variables of the wrong sign, the worst
     2.4e5 against 2.8e-9. The barrier's own duals are off by 3e-6.
     qsierra and qgfrdxpn were here until the push learned to stretch a
     stalled step along the rows' null space (`QP_PUSH_EXTRAPOLATE`) and
     to go on freeing while the wrong signs fall. liswet10 and liswet11
     were here until the walk learned to go on to `BARRIER_TOL_QP` when
     the push does not settle; qsierra and qgrow22 cannot reach it.
     qisrael, qpilotno and boyd1 were the same list until the push
     learned to release a pin in a row it left unsatisfied and the
     checker's row test went relative to the row's traffic.
   - **values is refused as not convex**, and it is not: its `Q` has 60
     eigenvalues below zero, down to -1.27e-5 against a largest of 10.77,
     the six-digit rounding of a covariance. BPMPD's 1.3966211 is a
     stationary point. Nothing to fix unless the contract changes to take
     a `Q` within its data's precision of semi-definite.
   - **hues-mod and liswet2** end at the checker's optimum, certified, but
     6e-6 and 1e-6 away from BPMPD's value.
   - **aug2dcqp, aug2dqp, aug3dqp** pass the checker but not the runner's
     suboptimality ceiling: `Σ d_j (x_j - l_j)` over columns of 1e6 with
     reduced costs of 1e-9.
   - **QPLIB's convex QPs** (`bench/measurements/02-256/`): 10 of 19 end
     `OPTIMAL` within 5.7e-7 of the library's values. QPLIB_9002 ends
     `OPTIMAL` on the barrier's own test with its rows 8.9e-7 off and a
     dual violation of 2.1e4, the push leaving 931 pinned columns with
     the wrong sign. The 8 largest (10000 to 1003001 columns) reach a
     work limit of 1e11, and QPLIB_9008 (1009306 columns) runs out of
     memory. Their normal equations filled badly, and since 2026-09-20
     the barrier reads both factors' operation counts and keeps the
     cheaper (`bench/measurements/02-272/`), so QPLIB_8785 reaches 59
     iterations in the budget where it reached 7, QPLIB_10038 16 where
     it reached 8 and QPLIB_10034 169 where it reached 84. None of them
     finishes: on the augmented system QPLIB_8785 reaches the library's
     objective by iteration 39 and its dual residual then shrinks by a
     quarter per iteration with `mu` at 1e-40, and QPLIB_10034 does not
     converge in 169 iterations. What is left is the walk itself, not
     the system it factors.


C5 **Cones and quadratic rows, the rest.** SPECS §1, "Quadratically constrained, second-order cone". The conic
   interior point, its Newton finish and every format with CBF landed on
   2026-09-19 (`bench/measurements/02-253/`), and the CBLIB reading the
   same day (`make cblib`, `bench/measurements/02-254/`). Missing:

   - **the conic tree's reach** (`src/conictree.c`, since 2026-09-19,
     `bench/measurements/02-255/`). It branches and plunges, and rounds and
     dives at the root, with no cuts and no warm start, so 37 of CBLIB's
     80 mixed-integer instances end at the work limit of 1e11
     (`bench/measurements/02-263/`), each with an incumbent: the
     sssd-*-8, uflquad-nopsc and turbine07_lowb_aniso files 0.14 to 1.7
     above the reference, their bounds 5% to 65% below it. Pseudocost
     branching (02-261) closes most of the sssd bounds and solves 28 of
     the 80 at 1e10 against 26, and costs the robust_50 files 2x to 4x
     the work. The cones the walk now leaves out (02-262) and the
     certificates it trims (02-263) took turbine07, turbine54 and
     turbine07_lowb to `OPTIMAL`. SOS sets, semi-continuous columns and
     indicator rows beside cones are refused.
   - **a certificate whose free column has to vanish exactly.** The
     checker caps a column by its curvature since 2026-09-20
     (`bench/measurements/02-267/`), and a refused certificate is
     re-weighted since the same day (`bench/measurements/02-270/`), so
     313 of 02-253's 314 ball-and-half-space models publish a
     certificate where 3 did. What the re-weighting cannot reach is a
     free column with no curvature: its coefficient `a` has to be 0
     exactly, which is one equation over the multipliers, and a search
     that moves one multiplier at a time cannot hold an equation. The
     answer is a solve over the multipliers, which is a second-order
     cone program: the gap is concave in them, `a²/(-2h) ≤ t` is a
     rotated cone in `(t, -h, a)`, a column with a finite bound gives
     two linear rows, and a free column with no curvature gives `a = 0`.
     An off-diagonal quadratic part is still refused by
     `row_curves_its_way`; it needs the supremum of a concave quadratic
     form, which is `a'H⁺a / 2` where the shift `u` of
     `a'x + ½x'Hx ≤ (a + Hu)'x - ½u'Hu` makes `a + Hu` vanish.
   - **CBLIB's twelve filterdesign instances** (71 to 872 MB) are not
     read at all. The three `sched_*_orig` are done since 2026-09-20
     (`bench/measurements/02-271/` and `02-273/`): the Newton finish's
     point is taken when the walk's own is refused and the finish's
     violation is smaller, and every column that leaves the finish on a
     bound with a reduced cost pushing it the other way leaves the
     active set, so `make cblib` reads 29 of 29 solved and taken by the
     checker where it read 26.
   - **QPLIB's convex QCQPs** (`bench/measurements/02-256/`). Of the 13
     continuous ones, 10 end `OPTIMAL` within 2.7e-7 of the library's
     values, but 8 of those have duals the checker refuses at 1e-7, off
     by 8.5e-7 to 3.6e-5 with the primal side to 2e-13. The Newton finish
     is refused on them: on QPLIB_2482, 1682 active rows over 1806 free
     columns leave directions with no curvature, and one step moves a
     column by 1.44 and breaks row 1759, which the walk left inactive.
     Taking rows within 1e-6 of a side as active and letting Newton run
     six steps reaches a KKT residual of 9e-16 on 1683 constraints, and
     the checker then finds a violation of 5.1e-3, so the missing piece
     is an active-set update after the finish. What the checker refuses
     is the rows, not the columns: on QPLIB_3088 the ball rows q(x) <= 1
     end 2e-6 to 1e-4 from their side with duals of 1e-7 to 2.5e-6, their
     products near 1e-11, while every reduced cost is under 3e-9; fitting
     each row's multiplier to its cone's whole dual and damping the
     finish both change nothing (`conic-qc-dual-refit`,
     `conic-newton-prox`); QPLIB_2676 and
     QPLIB_2468 stop without progress and end `NUMERICAL_ERROR`; QPLIB_3312
     (41406 columns) reaches the work limit. Of the 14 mixed-integer
     ones, the two `LMD` files end within 1e-2 of the reference at the
     work limit, 10 `LMC` files reach it, 8 of them with no incumbent,
     and QPLIB_10006 and 10007 are refused for a quadratic row over
     `CONIC_QC_DENSE` (3000) columns.
   - **a badly scaled box beside a cone that stays**: 15 bound-only
     columns of QPLIB_9002, values of 1e9 against `Q` entries of 4e-11,
     end the walk at a certificate the checker refuses ("columns reach
     inf"). With the box alone the model now leaves the walk, since its
     one cone is idle and nothing conic is left
     (`bench/measurements/02-266/`), and the barrier solves it at
     73622257.83. Beside a cone the walk cannot leave out
     (`tests/data/g_cone_badbox.mps`) the walk still fails, and that is
     the walk's own trouble with a box of that scale.

C6 **Mixed-integer quadratic, the QPLIB reading.** SPECS §1, "Mixed-integer quadratic". Of
   QPLIB's 17 convex mixed-integer QPs (`bench/measurements/02-256/`,
   `02-258/`, `02-259/`), 4 end `OPTIMAL` and 13 reach a work limit of
   1e11: QPLIB_3871, 3698, 3792, 3694 and 3861 with incumbents 27% to
   71% above the reference, QPLIB_3913, 4270 and 3547 with incumbents
   32.6%, 6.4% and 64% above it, and QPLIB_3980, 5577, 5924, 5527 and
   5543 with none. Each node is a cold barrier solve. The conic tree,
   measured on the same set and refused (`miqp-conic-tree`), ends the DML
   files 1.5% to 62% above the reference and finds QPLIB_3980 an
   incumbent at node 96; its root heuristics now run on the MIP tree too
   (02-259) and do not reach those, so the difference is in the search.
   QPLIB_5577, 5924, 5527 and 5543 (6014 to 25700 columns) spend the
   whole budget at the root node, and the last three never finish its
   relaxation. Symmetry detection also stops on a model whose `Q` has an
   off-diagonal pair (`src/symmetry.c`): the graph's colours carry the
   diagonal of `Q` and nothing carries a pair, so it could report a
   generator the model has not got. The pairs as edges between their two
   columns, labelled by value as the row entries are, would let it run.

C7 **Parallel tree search, the rest.** SPECS §4, "Deterministic parallel tree search". The conic tree takes
   its open nodes in rounds since 2026-09-20 (`--tree-batch N`,
   `bench/measurements/02-264/`): a round's relaxations solve on up to
   `--threads` threads and their answers are taken in the round's own
   order, so nothing the solve publishes depends on the thread count.
   Rounds are off by default, because a round of four leaves a worse
   incumbent where a work limit stops the tree. Missing: the tree of
   `src/mip.c`. Its nodes warm start from their parent's basis and share
   a cut pool, so a round there is not the round of cold walks the conic
   tree has; each worker would need its own copy of the basis and the
   pool, and the cuts a round finds would have to be taken in its order.
