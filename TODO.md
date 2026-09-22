# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Milestone A, publishing 0.4.0, ended with the tag `v0.4.0`, and milestone
B, performance, ended on 2026-09-22 with P0 re-taken. A row in a later
milestone waits for the earlier ones
unless it says otherwise. Every row was taken from the reading of
2026-09-21 (the three audits of the docs, the code and the bench record)
and nothing from that reading is left out of this file. Each row says how
to verify it, so a row leaves when its check passes, not before.

## Milestone C: reach and polish

The research rows. They were here before 2026-09-21 and stay as written.

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
   relaxation.
