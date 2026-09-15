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

   **Do not re-measure these.** Five ideas are refused with their reopen
   conditions in `bench/refusals.txt`: `primal-bound-perturbation`,
   `primal-tie-hash`, `primal-cost-perturbation`, `primal-noise-floor`,
   `primal-expand-step`. Steepest edge, Devex and Dantzig are all already
   read over the set; the SPECS row records them. Cost per iteration was
   read and paid 7.6% over the set for nothing here, because the gap is the
   iteration count.

   **The five are two faults, not one**, so a remedy aimed at either reads
   as noise over the set unless it is measured on its own group (d5a43e9).

   - d6cube and degen3 stand still. d6cube holds one vertex for 8508
     consecutive phase-2 bases. They need a remedy that leaves a vertex.
     EXPAND's growing tolerance schedule and its periodic reset are the
     named candidate; the step alone is measured and halves d6cube without
     clearing the overrun (77dcabf), and neither the schedule nor the reset
     is built.
   - seba and fit1d do not stall at all. They leave for a new point at
     nearly every pivot, so degeneracy is not what holds them and the
     target is the entering column. What holds them is still unnamed.

   The walk revisits no basis, so there is no cycle for an anti-cycling
   rule to break, which is why Bland's rule never pays here (f954aee).

4. **Convex QP: the reading on a published set.** SPECS row 22. The push
   that lands the barrier's point on its bounds is in
   (`bench/measurements/02-248/`), and the generated set is clean on both
   sides of the checker. Missing: a published QP set (the Maros-Meszaros
   instances are in QPLIB form, which JAOS reads) against its reference
   values, and the 5 of 6000 generated models where the barrier's dual
   residual jumps to 5.6e5 at its tenth iteration and the model ends
   `NUMERICAL_ERROR` (`push.sh` leaves them as `fail-s*-m*.lp`); the push
   never runs there, so that is the barrier's own lead.

