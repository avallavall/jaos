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
   1.23x to 1.10x, work 2.65x to 2.53x, and 2.34x on 2026-09-09 once the steepest-edge weight update stopped building its whole sigma vector to read a handful of it, which left the walk untouched and cleared no overrun). Exact weights in phase 1
   were tried and refused: pilot87 trips `PHASE1_RISE_MAX` under them at
   every switch point (`docs/tolerances.md`, `PSE_CHEAP_RESTARTS`).
   Reading the walk: 70% of the phase-2 pivots on d6cube and degen3 are
   degenerate (step zero); fit1d has 2% degenerate pivots and simply
   takes 15x the dual's count. Bound perturbation and a hashed choice at
   a tie in the ratio test were refused (`bench/refusals.txt`,
   primal-bound-perturbation and primal-tie-hash), and so was a cost
   perturbation on the pricing side after a run of zero steps
   (primal-cost-perturbation). Where the five stand at the tree of
   f1a5390, each run alone without a work limit (`jaos solve --algorithm
   primal`): d6cube optimal in 18131 iterations at 92x the dual's work,
   its best dual infeasibility flat at 868 from iteration 12000 to 16000,
   never under Bland's rule; fit2d 7847 iterations at 23x; seba 344 at
   17x; fit1d 1177 at 16x; dfl001 not finished at 300 s, 398528
   iterations and 35x the dual's work. On dfl001 the progress measure
   was broken: by iteration 39000 the carried reduced costs priced no
   candidate, `dinfeas_best` took the total 0, the verification refresh
   found breaches and the walk went on, but 0 cannot be improved, so
   `last_gain` froze, Bland's rule came on at iteration 214429 and stayed
   on for the remaining 184099 iterations without terminating. Fixed on
   2026-09-09: `run_primal` and the dual's `price_row` record a best
   total only when the pricing found a candidate, so a stale zero never
   enters it; the three gates and the primal campaign are byte-identical,
   because the freeze only showed past the 10x limit. What dfl001 does
   now, at the 10x work limit: 107923 iterations, never under Bland's
   rule, and from iteration 38000 its best total dual infeasibility
   sits at 1.0e-9 against `DUAL_TOL` 1e-9, one reduced cost a fraction
   of a percent past the tolerance on a degenerate vertex, 70000
   pivots without leaving it. That edge, and not Bland's rule, is the
   next lead on dfl001. Bland's rule itself runs with the ratio test's
   exact ties
   (`jm_primal_row_wins`) and a zero-clamped distance, which is where the
   published remedy, EXPAND's growing tolerance schedule (Gill, Murray,
   Saunders, Wright 1989), would go; `PRIMAL_HARRIS_DELTA` in
   `docs/tolerances.md` says JAOS does not carry it. The row in SPECS
   stays partial until the count is zero.

   **Where a fresh session starts.** Not on cost per iteration: that was
   read on 2026-09-09 and paid 7.6% over the set for nothing here, because
   the gap is the iteration count. seba takes 344 primal iterations against
   the dual's 107 and wants 2186880 work units; at 3367369 it would still
   miss the bar with the arithmetic free. Three remedies are already refused
   with their reopen conditions (`bench/refusals.txt`,
   primal-bound-perturbation, primal-tie-hash, primal-cost-perturbation) and
   the last of them closes with the instruction: read the degenerate phase 2
   of d6cube, degen3 and dfl001 pivot by pivot. Two threads are named and
   neither is measured. One is dfl001's edge above, a total dual
   infeasibility of 1.0e-9 against `DUAL_TOL` 1e-9 held for 70000 pivots on
   one vertex, which asks whether the tolerance or the measure is wrong
   rather than the pricing. The other is EXPAND's growing tolerance schedule
   (Gill, Murray, Saunders, Wright 1989), which `PRIMAL_HARRIS_DELTA` in
   `docs/tolerances.md` records that JAOS does not carry. Either may end in a
   fourth refusal line rather than a fix, and that is a fair outcome to plan
   for.

   **The first thread is read, and it ended in the fourth refusal**
   (`bench/refusals.txt`, primal-noise-floor). The measure is the one at
   fault and the tolerance is not: a reduced cost is a difference of sums the
   size of `column_traffic(v)`, dfl001's traffic reaches 2.0e7, and past
   iteration 37000 the pricing walks candidates whose breach is five decades
   under the rounding of those sums. At the iterations logged there, 0 of the
   candidates stood over their own noise and 7 under it, the chosen column's
   breach 1.79e-9 against 4.45e-4 of noise; on one of them the steepest-edge
   score preferred a column under its noise while a real candidate was there.
   The settle path beside it already refuses to move such a column
   (`can_move`, `wants_a_pivot`), and the pricing does not. What the refusal
   settles is that the reading cannot go in the pricing in either form. As a
   filter it decides optimality, and a floor that grows with the column
   discards reduced costs the checker calls real breaches: dfl001 came back
   `DISAGREE`. As an order it is sound and it is idle, because d6cube, fit1d,
   fit2d and seba never produce a candidate under its floor and walk to the
   same iteration and the same work unit either way. So the phenomenon is
   dfl001's alone and the other four are a different fault: **they are not
   chasing noise, and whatever holds them is still unnamed.** What is left of
   this thread is the reading used to end the walk rather than to price it,
   or a floor stated in the model's units. The second thread, EXPAND, is
   still unmeasured.
