# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.

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
   handoff that ground the dual simplex. What it leaves, 111 of 138 clean:

   - **dtoc3 answers wrong**: the origin, `OPTIMAL`, rows off by 15. All
     columns are free and two are fixed; the scaled data is enormous
     (`EXP_LIMIT` in `src/scale.c` lets a factor reach 2^512), so the
     relative residual reads 4e-23 at a point that satisfies nothing.
     The checker refuses it. Read what the scale factors are on it first;
     a cap on the exponent changes every reading and needs the four
     baselines rewritten after their diffs are read.
   - **18 the barrier cannot settle**: liswet1 and 7 to 12 (10000 free
     columns, the primal residual stuck at 1e-8 with mu at 1e-57: the
     `BARRIER_DELTA` floor on the rows against a large dual step; a refined
     Newton direction was tried and hurt the generated set), ksip,
     cvxqp1_l, cvxqp3_l, powell20, huestis, qforplan, qpcboei2 (the dual
     iterate grows 1e6-fold within 20 iterations), q25fv47, ubh1, boyd2
     (200 iterations with the last residual just above tolerance),
     qgrow22 (diverges at 41).
   - **the dual simplex on ksip's LP** (1001 rows, 20 free columns) calls a
     feasible system infeasible with a Farkas ray of zeros. The QP probe
     now refuses that verdict; the LP engine's own defect is open.
   - **five the checker refuses with the objective right**: qisrael,
     qpilotno, qsierra (dual violations 3e-5 to 0.1 the push's `tol_d`,
     1e-9 times `1 + |c|`, lets through; an absolute 1e-9 was tried, did
     not cure them and cost 30% more push rounds), boyd1 (a row 0.015 off
     on coefficients of 1e12), qgfrdxpn (the push pins one variable a
     round on a flat face and gives up at 40; the barrier's duals are off
     by 5e5).
   - **values is refused as not convex** at a ridge of 1e-10, 1e-8 and
     1e-6; BPMPD reports 1.3966211. Read whether `Q` is indefinite or the
     test is.
   - **hues-mod and liswet2** end at the checker's optimum, certified, but
     6e-6 and 1e-6 away from BPMPD's value.
   - **aug2dcqp, aug2dqp, aug3dqp** pass the checker but not the runner's
     suboptimality ceiling: `Σ d_j (x_j - l_j)` over columns of 1e6 with
     reduced costs of 1e-9.

