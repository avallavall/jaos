# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and since 2026-09-19 `tests/windows.sh` runs the tool and the Python
   binding natively on the Windows host WSL runs on (02-251). Missing:
   clang-cl, which needs Microsoft's C runtime headers and libraries,
   licensed by Microsoft and not on this machine, and macOS, which needs a
   Mac.

3. **Primal simplex: 5 of the 94 standard instances run past 10x the dual's
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

   - d6cube and degen3 stand still. d6cube holds one vertex for 8508
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

4. **Convex QP: the Maros-Meszaros set.** SPECS row 22. `make
   maros-meszaros` reads the 138 instances against BPMPD's values; the
   first reading (`bench/measurements/02-250/`) found and 808022a fixed
   the NaN points, the 1e-6 row slips, the four unreadable files and the
   handoff that ground the dual simplex; six more fixes followed from its
   list (02-250, "After the reading"), and the push's stretch and its
   longer freeing took two more on 2026-09-19, and the augmented restart
   below took boyd2. What it leaves, 129 of 138 clean, 134 `OPTIMAL` and
   133 taken by the checker:

   - **dtoc3** answered wrong until the scale exponent was capped at
     `2^±20` (`EXP_LIMIT`, its factors ran to `2^91`); it ends
     `NUMERICAL_ERROR` now, the rows 1.4e-6 off in scaled space against a
     dual step of 1e7, which is the liswet wall below.
   - **3 the barrier cannot settle**: dtoc3 (10000 free columns; the
     primal residual stuck at 1e-8 with mu at 1e-57 was the
     `BARRIER_DELTA` floor on the rows against a large dual step, and
     dropping `delta` by `BARRIER_STALL_DELTA` on a stall, reverting the
     drop when a factorisation loses a pivot, solves the whole liswet
     family but not this one; a refined Newton direction was tried and
     hurt the generated set),
     ksip (1001 rows on 20 free columns, mu of 1e19 from the start),
     ubh1 (200 iterations with the gap at 0.23; q25fv47 was here until
     the push learned to finish a walk stopped within
     `BARRIER_NEAR_TOL` of converged, and the push does not settle on it:
     ubh1 loses its interior by iteration 5, mu at 1e-22 against a gap
     of 0.5, and a lift of every complementarity product back to 1e-3 of
     the gap was measured and refused). boyd2 was here, its walk stopped
     at a gap of 3.8e-6 with its two dense columns left out of the normal
     matrix; since 2026-09-19 a quadratic walk that stops with dense
     columns left out starts again on the augmented system, and boyd2
     ends at the reference in 270 iterations (`bench/measurements/02-256/`). qgrow22 was here, its LDL replacing 61 pivots at every
     regularisation up to 1e-4 and the next direction NaN; at
     `BARRIER_REG_MAX` 1e-2 it converges to the reference and the checker
     refuses a dual violation of 3e-6 (the push leaves 1575 rows
     unsatisfied and stands down). cvxqp1_l, cvxqp3_l,
     powell20, huestis, qforplan and qpcboei2 were here until a quadratic
     model got its own divergence limit, `BARRIER_DIVERGE_QP`.
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
     memory. Their normal equations fill badly: one iteration of
     QPLIB_8785 (10399 columns) costs 2.8e10 work units, its factor 9.1
     million nonzeros. The augmented system costs 1.6e9 there and half
     the normal equations' work on QPLIB_10038 and 10034, the same on
     8500, so a choice of system by the symbolic factor's size would pay
     on some. It would not finish them: on the augmented system
     QPLIB_8785 reaches the library's objective by iteration 39 and its
     dual residual then shrinks by a quarter per iteration with `mu` at
     1e-40, and QPLIB_10034 does not converge in 169 iterations.


5. **Cones and quadratic rows, the rest.** SPECS row 23. The conic
   interior point, its Newton finish and every format with CBF landed on
   2026-09-19 (`bench/measurements/02-253/`), and the CBLIB reading the
   same day (`make cblib`, `bench/measurements/02-254/`). Missing:

   - **the conic tree's reach** (`src/conictree.c`, since 2026-09-19,
     `bench/measurements/02-255/`). It branches and plunges, and rounds and
     dives at the root, with no cuts and no warm start, so 43 of CBLIB's 80 mixed-integer instances end at the work
     limit of 1e11, each with an incumbent: the sssd-weak, uflquad-nopsc
     and turbine07_lowb files 0.17 to 1.7 above the reference, their
     bounds 5% to 65% below it. SOS sets, semi-continuous columns and
     indicator rows beside cones are refused.
   - **a certificate for an infeasibility that rests on a quadratic row's
     curvature.** The certificate checker takes a quadratic row linearly,
     so the ball-and-half-space models of 02-253 end `INFEASIBLE` with no
     certificate published (311 of their 314). The checker would need the
     supremum of a concave quadratic over the boxes.
   - **CBLIB's three `sched_*_orig`**, which end `NUMERICAL_ERROR`: the
     walk reaches 3e-8 on the primal residual, then loses it as `mu`
     falls, and neither its best point nor the Newton finish passes the
     checker. The library's own solutions carry a primal error of 2e-6 to
     9e-6 on them. The twelve filterdesign instances (71 to 872 MB) are
     not read at all.
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
     is an active-set update after the finish; QPLIB_2676 and
     QPLIB_2468 stop without progress and end `NUMERICAL_ERROR`; QPLIB_3312
     (41406 columns) reaches the work limit. Of the 14 mixed-integer
     ones, the two `LMD` files end within 1e-2 of the reference at the
     work limit, 10 `LMC` files reach it, 8 of them with no incumbent,
     and QPLIB_10006 and 10007 are refused for a quadratic row over
     `CONIC_QC_DENSE` (3000) columns.
   - **the 8 numerical errors of 02-253**: six rays the projection
     cannot bring inside the ray checker's tolerance (2e-6 to 9e-2 past a
     row side or a bound), and two walks that stall away from any answer.

6. **Mixed-integer quadratic, the QPLIB reading.** SPECS row 24. Of
   QPLIB's 17 convex mixed-integer QPs (`bench/measurements/02-256/`), 3
   end `OPTIMAL` and 13 reach a work limit of 1e11: QPLIB_3871, 3698,
   3792, 3694 and 3861 with incumbents 27% to 71% above the reference,
   QPLIB_3547 with the incumbent 0 against -0.56, and QPLIB_3980, 3913,
   4270, 5577, 5924, 5527 and 5543 with none. QPLIB_3708 ends
   `NUMERICAL_ERROR` at a node after an incumbent 33% above. Each node is
   a cold barrier solve.
