# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.
2. **Primal simplex: the 5 of 94 standard instances that run past 10x the
   dual's work** (`bench/results/primal.txt`): d6cube, dfl001, fit1d,
   fit2d, seba. Down from 17 once phase 2 stopped shifting costs, from 9
   with steepest-edge pricing (02-31: iterations primal/dual 1.43x to
   1.23x, work 2.75x to 2.65x, pilot87 solves), and from 6 on 2026-09-09
   when a phase-2 weight restart, after `PSE_CHEAP_RESTARTS` cheap ones,
   started recomputing the exact steepest-edge weights for the current
   basis instead of the slack basis's: degen3 had restarted on 10723 of
   its 15291 iterations and priced on the wrong weights throughout, and
   its phase 2 went from 12322 pivots to 772 (over the set: iterations
   1.23x to 1.10x, work 2.65x to 2.53x). Exact weights in phase 1
   were tried and refused: pilot87 trips `PHASE1_RISE_MAX` under them at
   every switch point (`docs/tolerances.md`, `PSE_CHEAP_RESTARTS`).
   Reading the walk: 70% of the phase-2 pivots on d6cube and degen3 are
   degenerate (step zero), and d6cube without a work limit runs 259012
   iterations, switches to Bland's rule and trips the iteration guard a
   million iterations later; fit1d has 2% degenerate pivots and simply
   takes 15x the dual's count. Bound perturbation and a hashed choice at
   a tie in the ratio test were refused (`bench/refusals.txt`,
   primal-bound-perturbation and primal-tie-hash), and so was a cost
   perturbation on the pricing side after a run of zero steps
   (primal-cost-perturbation). What is left: why Bland's rule does not
   terminate on d6cube, and the dense-column iteration counts. The row in
   SPECS stays partial until the count is zero.

## Milestone: symmetry

## Milestone: barrier
