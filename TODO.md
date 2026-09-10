# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

0. **`relax --cols` does not finish on a model with no integer point.**
   Since 2026-09-10 `relax`, `iis`, `verify` and `ranging` take
   `--work-limit N`, so the runaway can be stopped and says `work limit
   reached` when it is. That is an escape hatch and not the fix; the rest
   of this row still stands.
   The elastic copy frees every column so each one can leave its box at a
   price, and an integer column freed that way gives the tree an unbounded
   space. Where the rows plus the integrality admit no point at all, the
   search has to exhaust that space to say so. Measured 2026-09-10 on the
   copy of a 4-column, 3-row model: 27 nodes with the integer columns held
   to ±10, 507 at ±100, 5307 at ±1000, no answer at ±100000 or free. The
   model is in the sweep at `SWEEP_ONLY=475 ./sweep_relax 600 1`
   (`bench/measurements/` is not carrying it; it is four rows of MPS and
   the SPECS row states them). Three ways out, none measured: propagate the
   rows onto the freed columns to get finite bounds where they exist; give
   `jaos_feasrelax` a work limit of its own and report `work_limit` in
   `jaos_relax_report.status`, which the struct already carries; or refuse
   the shape by name. The first is the only one that answers the question.
   Reaching it needs a reading over the MIP set, because the same
   propagation would run on every node.

0b. **A resume does not follow the path the uninterrupted run took.**
   Measured 2026-09-10 (the SPECS row): of 33102 runs stopped at a work
   limit and then finished, 15 reach a different answer. 9 of them keep the
   objective bit for bit and stand on another point of the same optimal
   face; the other 6 move the objective by about 2e-15. 10 on the dual, 5
   on the primal, none on the barrier. Two runs at the same limit still
   agree exactly, which is what `docs/cli.md` promises, so nothing
   documented is broken. What is missing is the stronger property a caller
   would expect: a stop should not change the answer.

   The cause is the re-entry. `sx_init` builds the factorisation again and
   the pricing weights with it, so the walk after the stop is not the walk
   that would have happened. Carrying that state across the stop is the
   fix, and it is a real piece of work: the LU, its update chain, the
   steepest-edge weights and the Harris pass's state all have to survive.
   It changes `simplex.c`, so it needs the four gates. Measure the cost of
   holding the state as well as the benefit, because a solve that never
   stops would pay for it too.

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
   or a floor stated in the model's units.

   **The second thread is measured and refused** (`bench/refusals.txt`,
   primal-expand-step), on the second attempt. The first changed the `step`
   that `primal_ratio_test` returns and refused the method on what came back;
   nothing moves the point by that value, which the caller reads only to ask
   whether the entering column reaches its own opposite bound first, so it
   measured the bound-flip threshold. Widening it to 1e-2 leaves d6cube's
   walk byte-identical at 18131 iterations and 2117056964 work units, which
   is what a step that does nothing looks like. The step the pivot takes is
   the leaving row's own distance, `theta_primal = (xb[r] - bound) / alpha_q`
   in `pivot`, and EXPAND's has to be handed to it there.

   Handed there, with the plumbing checked first by passing the ratio test's
   own step and reproducing the committed reading over the whole set: **it is
   the largest move any idea has made on d6cube and it is still not enough.**
   d6cube 18131 iterations and 2117056964 work units to 8906 and 953362179,
   0.45x; seba 0.978x; fit1d 0.993x; degen3 1.016x; fit2d 1.087x; every
   objective unchanged. Over the standard 94 the work geometric mean goes
   2.3410x to 2.3508x, nothing disagrees, and **the overrun count stays 5**,
   because 0.45x lands d6cube at 37.9x the dual's work against a bar of 10x,
   down from 84.2x. The set pays 0.4% for a trip that crosses nothing.

   The width it uses is `PRIMAL_HARRIS_DELTA * primal_tol`, the overshoot
   pass one already permits, so nothing about the feasibility contract had to
   move for this. **What is left on d6cube is a factor of 3.8**, and the step
   is already paid for, so the growing schedule and the periodic reset that
   are the rest of EXPAND are the first place to look. Neither is built.

   **Both named threads are now measured and both are refused, so the row
   needs a new lead rather than a fresh session on the old ones.** What the
   two readings leave standing: the five are not chasing noise (the first
   thread) and they are not short of step length (the second). d6cube spends
   69% of its phase-2 ratio tests on a zero step and still walks 18131
   iterations whatever the step is offered, which points at the entering
   choice and not the leaving one, and steepest edge, Devex and Dantzig are
   all already measured on the set (the SPECS row).

   **The walk does not revisit a basis**, read on 2026-09-10, and that is the
   third thing measured away. Hashing every phase-2 basis, the set of basic
   columns together with the bound each nonbasic sits at, and comparing it
   against every earlier one: d6cube visits 16761 of them and repeats none,
   degen3 772 and repeats none, fit1d 775 and seba 134 the same. So there is
   no cycle for an anti-cycling rule to break, which is why Bland's rule
   never pays here, and the 69% of steps that are zero are the walk crossing
   between distinct bases of the same degenerate points rather than returning
   to one.

   **What those bases sit on, read the same day, and it splits the five in
   two.** The key is the active set, which variables sit at a bound and at
   which one, since every basis of one degenerate vertex carries the same and
   it does not wobble when a leaving column snaps; the objective was tried
   first and thrown away, because a snap moves it in its last bits while the
   point stands still and the count came out above the number of non-zero
   steps, which cannot happen. Phase-2 bases against the points under them:
   **d6cube 16761 over 732, with 8508 of them in a row on one point**;
   degen3 772 over 57, longest run 121; seba 134 over 124, longest run 3;
   fit1d 775 over 762, longest run 2.

   So d6cube spends more than half of its phase 2 standing on a single
   vertex, degen3 does the same in miniature, and **seba and fit1d are not
   stalling at all** -- they leave for a new point at nearly every pivot, and
   whatever makes them slow is not degeneracy. The five are two faults, not
   one, and a remedy aimed at either will read as noise over the set unless
   it is measured on its own group.

   For seba and fit1d the target is the entering column, and the three
   pricing rules on offer are all already measured on the set (the SPECS
   row), so a fresh session should not re-measure them there. For d6cube and
   degen3 it is a remedy that leaves a vertex, and EXPAND is still the named
   candidate and still unmeasured, for the reason above.
