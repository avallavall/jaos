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
   longer freeing took two more on 2026-09-19. What it leaves, 128 of 138
   clean, 133 `OPTIMAL` and 132 taken by the checker:

   - **dtoc3** answered wrong until the scale exponent was capped at
     `2^±20` (`EXP_LIMIT`, its factors ran to `2^91`); it ends
     `NUMERICAL_ERROR` now, the rows 1.4e-6 off in scaled space against a
     dual step of 1e7, which is the liswet wall below.
   - **5 the barrier cannot settle**: dtoc3 (10000 free columns; the
     primal residual stuck at 1e-8 with mu at 1e-57 was the
     `BARRIER_DELTA` floor on the rows against a large dual step, and
     dropping `delta` by `BARRIER_STALL_DELTA` on a stall, reverting the
     drop when a factorisation loses a pivot, solves the whole liswet
     family but not this one; a refined Newton direction was tried and
     hurt the generated set),
     ksip (1001 rows on 20 free columns, mu of 1e19 from the start),
     ubh1 and boyd2 (200 iterations with the gap at 0.23 and 3.8e-6;
     q25fv47 was here until the push learned to finish a walk stopped
     within `BARRIER_NEAR_TOL` of converged, and the push does not settle
     on these two: boyd2 is at the reference objective to 4e-9 by
     iteration 100 and its push leaves one row 5e-3 off through 40
     rounds and 40 releases, ubh1 loses its interior by iteration 5, mu
     at 1e-22 against a gap of 0.5, and a lift of every complementarity
     product back to 1e-3 of the gap was measured and refused). qgrow22 was here, its LDL replacing 61 pivots at every
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


5. **Cones and quadratic rows, the rest.** SPECS row 23. The conic
   interior point, its Newton finish and every format with CBF landed on
   2026-09-19 (`bench/measurements/02-253/`), and the CBLIB reading the
   same day (`make cblib`, `bench/measurements/02-254/`). Missing:

   - **the conic tree's reach** (`src/conictree.c`, since 2026-09-19,
     `bench/measurements/02-255/`). It branches and plunges with no cuts,
     no heuristic but the plunge, and no warm start, so 44 of CBLIB's 80
     mixed-integer instances end at the work limit of 1e11, 17 of them
     with no incumbent. A rounding of the relaxation's point finds one on
     the pp files at once (`pp.py` of 02-255). SOS sets, semi-continuous
     columns and indicator rows beside cones are refused.
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
   - **QPLIB's convex QCQPs**, a second published set.
   - **the 8 numerical errors of 02-253**: six rays the projection
     cannot bring inside the ray checker's tolerance (2e-6 to 9e-2 past a
     row side or a bound), and two walks that stall away from any answer.
