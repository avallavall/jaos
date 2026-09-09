# Tolerances

Every number a solve compares against, where it acts, and what it decides.
They were drafts until the Netlib gate closed. It has, so they
are frozen at the values below. A change to any of them goes here with
its measurement.

Three spaces are involved and confusing them is the way to misread every
figure below. The solver runs on a **scaled copy** of the model, so its
tolerances are magnitudes in scaled space (see `docs/scaling.md`). The
independent checker runs on the **model as loaded**, so its tolerance is a
magnitude in the units the caller wrote. Presolve also runs on the model as
loaded, and runs there *before the scaling exists* — so its constants are
magnitudes in the caller's own units too, but they are not the checker's
either: the checker's is a caller's diagnostic choice for judging a finished
answer, and presolve's decide what the solver is handed in the first place.
None of the three is converted into another; they are separate judgements,
which is the point of having them apart.

## The solver's tolerances

Defined in `src/simplex.c`, `src/lu.c` and `src/chol.c`; `LU_PIVOT_TOL` in `src/jaos_internal.h`, because ranging factors the published basis with the same threshold.

| Name | Value | What it decides |
|---|---|---|
| `PRIMAL_TOL` | 1e-7 | How far a basic variable may sit outside its bound before it counts as violated — so it decides which rows the dual simplex tries to repair, and when there are none left |
| `DUAL_TOL` | 1e-9 | The width of the Harris window: how far a reduced cost may be pushed past feasible in exchange for a larger pivot — **and, more consequentially, what the solve calls zero for a reduced cost at all**. `dual_breach`, `published_breach`, `settled_dual_violation` and `held_by_an_invented_bound` all read it, so it decides when there is nothing left to price and the solve stops. A reduced cost is a rate: what a column is still worth is that rate times the distance it would travel, and this bounds the rate alone. Swept over all three sets through `jaos_set_dual_tolerance`, which reaches the same number. Loosening one decade to 1e-6 makes `pilot` and `pilot87` fail outright. Tightening to 1e-9 repairs the four netlib instances that publish a point which is not the optimum — `pilot` 2.31e-05 → 5.27e-09, `pilot87` and `scsd6` to the reference exactly, `etamacro` to one ulp — three of them for less work, at a netlib work geomean of 1.0339x and **six instances past the gate's 2.0x bar**. 1e-8 does not reach `pilot` at all; 1e-10 fails `dfl001` and 1e-11 fails `wood1p` too, so the value is bounded on both sides. **Changed from 1e-7 to 1e-9 on 2026-08-25, on the maintainer's decision.** The campaign: `pilot` 2.312e-05 → 5.266e-09, `pilot87` and `scsd6` publish Koch exactly, `etamacro` 1.315e-08 → 1.137e-13; work geometric mean **1.0339x on netlib** and **1.0976x on Kennington**, which D174 did not measure — `pds-20` pays 4.815x and `d2q06c` 5.319x. `gate: PASS` on all three sets, no answer worse. **Every site that reads it now bounds a rate.** `can_move` tested a rate-times-distance product against it until 2026-08-28; it reads `breached` now, the same bound taken in both spaces. That sweep was taken against the product, and the two versions separate only as this constant loosens, so it describes the code at 1e-9 and not the shape of the curve away from it |
| `PIVOT_MIN` | 1e-9 | Smallest \|alpha\| the ratio test will accept as a pivot at all. Below this a candidate is not eligible, whatever its ratio. **It is a stability floor and not a noise floor**, and the distinction is measured rather than argued: `pivot` and `theta_dual` both divide by this number, and 1e-10 is as dangerous to divide by when it is exact as when it is not. Over the standard 94, every one of the thirteen calls where it rejects an `alpha[q]` has that value equal to its own traffic to all seventeen digits — a dot product with one term and no cancellation, which is the best determined a number gets. Telling a pivot from the rounding of its own arithmetic is a different question and `PIVOT_MARGIN` is what asks it |
| `PIVOT_MARGIN` | 1.0 | The noise floor, in ulps of a quantity's own terms, at **two** places. **(a)** In the two primal ratio tests, on an entry of `B^-1 M_q`, against that column's largest entry. **(b)** On the pricing row's `alpha[q]` at the three sites that judge a pivot element, against `sum_i \|rho_i * a_iq\|` — its own traffic, which is not the column's, so the constant carries over but the quantity does not. On the second, the census puts 1.0 in the middle of a window five orders wide, `(0.352, 20740)`, and 32874 times below anything the gate reaches; it rejects exactly one call on the standard 94, `scsd1`'s, where `alpha[q]` stands at a third of one ulp of its own terms. Its walk costs a work geometric mean of **1.000001x on the dual solve and 1.000496x on the forced primal**, because the stability test runs first and the walk is skipped on every call already rejected. Reading (a) follows. Dimensionless, so it belongs to neither space; the threshold it builds is in scaled space with `s->col`. Since D212 a row below it is dropped from the candidate list, so it neither pivots nor blocks; it moves by at most the step times the floor, which is below the column's own rounding. An absolute floor alone is simultaneously too strict and too lax depending on the column's scale: on `pilot87` it accepted an FTRAN residue of 1.59e-07 on a row whose true entry is exactly zero, on a column reaching 2.1e+14, and the solve refused calling itself defective. Swept over the forced-primal campaign at 0, 3e-1, 1 and 2. **Bounded on both sides by measurement.** Below 3.3457e-06 it decides nothing at all — that is the smallest ratio any of the 94 instances ever reaches, and it is `pilot87`'s own refusal. At 3e-1 and at 1 the campaign reads 56 ok / 30 disagree / 8 overrun / 0 errors against 55 / 31 / 7 / 1 at 0, so the value sits on a plateau three times wide rather than on a spike. Above 5.4855 it would start to reach the gate, which is `wood1p`'s ratio and the only thing near it: `primal_ratio_test` is reached by 3 of the 94 standard instances and by none at all on `netlib-infeas` or `netlib-kennington`. 1.0 is also stricter than `DROP_REL`, which already calls anything below about 45 ulps of the basis matrix's largest magnitude structurally absent. Costs 1.000000x work on the dual solve, byte-identical on all 94, and 0.995321x on the forced primal |
| `PRIMAL_HARRIS_DELTA` | 0.5 | The width of the Harris window in the **two primal** ratio tests, as a multiple of `s->primal_tol` — 5e-8 at the default `PRIMAL_TOL`, and the per-model override scales with it, which is why the field is multiplied and not the default (`jaos_set_primal_tolerance`). In the space `xb` lives in. The dual's equivalent is `DUAL_TOL` used directly. **Bounded above by the same tolerance**, by the phase-1 argument in `docs/research/harris-primal.md`: pass one lets a relaxed basic end up to `delta` outside its bound, and that still counts as feasible only while `delta <= primal_tol`. Multiplying the field keeps that true for any override; a hardcoded 5e-8 would not. An `assert` in `primal_pick` pins the bound on the widened value itself, because nothing else did: before it, 1e9 passed `make test`, and no test reaches that call with a width that matters. It does not pass now. A `static_assert` cannot carry the bound, because a comparison of floating constants is not an integer constant expression and `-Wpedantic -Werror` rejects one. The product underflows to zero for a subnormal `primal_tol`, which `jaos_set_primal_tolerance` accepts; zero is the no-relaxation width and the assert admits it. The ratio one half is MINOS's and SNOPT's, where `delta_f / 2` is where EXPAND **starts** a tolerance that then grows toward `delta_f`; JAOS holds its width fixed and carries none of that schedule, so this is the ratio borrowed and not the method. 1.0 met the bound exactly and is what D212 shipped. **0.5 is also a power of two, so the product is exact**: no contraction can round it differently, which a value like 0.1 could not claim. **Swept over seven settings** on the forced-primal campaign — 0, 0.01, 0.1, 0.3, 0.5, 1, 10. Agreement with the dual reads 59, 61, 61, 61, 61, 60, 58, and from 0.01 to 0.5 it is the **same 61 instances name for name**: a plateau four settings wide, not a spike. What the plateau buys over 1.0 is one instance, `wood1p`. **The gate does not move anywhere in 0 to 10** — all three sets byte-identical at 0, 0.1, 0.5, 1 and 10 — and at 1e9 it breaks, `pilot87` failing the checker, which is what proves the probe reached the code rather than measuring nothing. `pilot87`'s phase-1 divergence does not follow the width: its worst relative rise runs 7.2e+11, 8.3e+11, 1.4e+03, 3.3e+16, 8.1e+11, 8.3e+13, 4.6e+11 across the seven, with no order, so no setting here can be justified by it. **One reading does separate 0.5**: the share of phase-1 pivots below 1e-3 over all 94 instances is lowest there of the seven widths, which refutes the objection that a narrower window can only reach a smaller pivot |
| `PHASE1_RISE_MAX` | 1.0 | How far the primal phase 1's total infeasibility may rise above its own running minimum before the basis is called unrepairable and the solve publishes `NUMERICAL_ERROR`, as a fraction of that minimum. 1.0 is "it may double". In the scaled space `xb` lives in, and not dimensionless: the ratio is invariant only while the violated SET is unchanged, and each violated variable carries its own scale factor, so a set that changes between the minimum and the rise changes the ratio without anything moving in the model the caller handed in. The census below was taken with Curtis-Reid scaling on, which is the default; `JM_SCALE_NONE` is reachable and no reading covers an unscaled forced-primal solve. The quantity is a sum of bound violations and cannot rise at all under an exact pivot; it rises when the basis has gone near singular and `refresh` recomputes `xb` from it. **Bounded on both sides by a census of every rise on all 110 forced-primal solves**. Below, the largest rise on a solve that ends `ok` is `pilot`'s 9.36752e-10 and the largest on any instance but `pilot87` is `woodw`'s 4.26896e-08; above, `pilot87` reaches 8.06882e+11. Every threshold from 1e-7 to 1e+11 stops `pilot87` and nothing else, and from **1e-5 to 1e+2 it stops it at the same iteration**, 19532, because its rise jumps from 2.04558e-06 straight to 633.034 with nothing between. So this sits in the middle of a plateau eight decades wide rather than on a spike, and the measurement does not choose inside it — the same shape as `PRIMAL_HARRIS_DELTA`'s. What settles the value is not the window but what stopping costs: `pilot87`'s running minimum last improved at phase-1 iteration 19532 of 381886, so the 362354 iterations the rule removes lowered it by nothing. `dfl001` is the control and is not touched at any threshold above 3.30714e-10: it grinds 125807 phase-1 iterations and is still improving at the last one. Swept as a campaign as well as a census, at 1e-12, 1.0 and 1e+12, each its own tree and its own binary — and at 1e+12, above every rise measured, the record is identical to the tree before the rule existed |
| `LU_PIVOT_TOL` | 0.1 | Markowitz threshold: a pivot must be at least this fraction of the largest magnitude in its column. Sparsity is traded for stability here and nowhere else |
| `LU_UPDATE_TOL` | 1e-9 | Floor on the new diagonal in a Forrest-Tomlin update, relative to the spike's largest magnitude. Deliberately far looser than the Markowitz threshold: after elimination a legitimate pivot can be orders of magnitude below the spike |
| `FTRAN_HYPER_DEN` | 10 | the density below which FTRAN solves only the slots its right-hand side reaches: a walk over the L columns from the input nonzeros, then over the U columns, marks the reachable slots, the pattern is sorted so the arithmetic runs in the same slot order as the full pass, and every answer stays bit-identical (all four gates, 0 changed). Density is predicted per kind of vector (the entering column and the flips, which ask for a pattern, against the dense solves) from an exponential average of the last answers' nonzero counts, `FTRAN_DENSITY_KEEP`; a prediction at or above 1/`FTRAN_HYPER_DEN` takes the full pass, and so does a solve whose L reach already crossed that line. The work counter bills the reach walk per edge examined and the sort per word and per entry, so in work units the change reads worse where it is faster on the machine (ken-11 +7%, cre-a +20%) and the row closed on instructions: **swept at 5, 10, 20 and 40** under callgrind inside `jm_dual_simplex` on afiro, 25fv47, maros-r7, greenbea, ken-11, pds-06, osa-07 and cre-a against the full pass, geometric mean 0.953x, **0.951x**, 0.954x, 0.959x; at 10 ken-11 0.828x, pds-06 0.865x, cre-a 0.927x, osa-07 0.995x, and the four dense ones within 1.2% (afiro, 1.09M instructions) and 0.3%. Over the four gates no status, iteration count or objective moved on 163 instances and work read 1.037x on netlib, 1.016x infeas, 1.067x Kennington, 1.010x the MIP set; the widest gap between the two measures is ganges, 1.549x in work and 0.987x in instructions (02-31) |
| `FTRAN_DENSITY_KEEP` | 0.9 | the weight the previous prediction keeps against the density the last solve of its kind produced. The estimate survives a refactorization. Not swept: held |
| `LU_AGREE_TOL` | 1e-5 | How far the two computations of the pivot element may disagree before the factorization they came through is rebuilt instead of pivoted on. `alpha_q` arrives by BTRAN with the pricing row, `col[r]` by FTRAN for the basis update; they are one number in exact arithmetic, so this is the factorization contradicting itself and not a guess about conditioning. Over all 139 gate instances no pivot reaches 1e-7 and the worst is 7.83e-08; on `pilot87` at a refactorization interval of 128, where the solve grinds 1.38M iterations, it reaches 1.99. The first pivot to cross 1e-7, 1e-6, 1e-5, 1e-4 and 1e-3 is the same one, so this sits in the middle of a four-decade plateau |
| `IMPLIED_ROUNDS` | 64 | Cap on the checker's bound-propagation rounds — a safety stop and not a quality knob, since the loop exits as soon as a round bounds nothing new. Set where the propagation reaches its fixed point: swept over the standard set, certified answers go 17, 23, 32, 38, 46, 47, 48, 48 at 1, 2, 4, 8, 16, 32, 64, 128 rounds. The cost is flat across the whole sweep — 119 s to 128 s against a gate of about 120 s — so there is nothing to trade against |
| `DROP_REL` | 1e-14 | A value below this fraction of the basis matrix's largest magnitude is structurally absent. Relative, because an absolute floor would call a uniformly small basis singular |
| `TINY` | 1e-300 | The same floor where no scale is available to compare against |
| `CHOL_PIVOT_REL` | 1e-14 | In the sparse Cholesky, a pivot at or below this fraction of its own row's diagonal in the input is called zero and replaced by `CHOL_PIVOT_HUGE`. Relative to the row's own diagonal and not to the largest one, because the normal equations `A D A^T` carry the spread of `D`, which is many decades wide near the end of a barrier run, and a small diagonal is legitimate there while a pivot that has cancelled to rounding noise of its own row is not. Absolute floor `TINY` when the diagonal is zero. **A draft.** What set it is the test in `tests/test_chol.c` where two identical rows give an exact zero pivot and the six random normal-equation systems where no pivot is replaced; the measurement that fixes it is the barrier's own, on the standard 94, when that lands |
| `CHOL_PIVOT_HUGE` | 1e128 | What a replaced pivot becomes. Its square root is 1e64, so the column below it is scaled to nothing and the solve returns a component near zero for that row, which is the usual barrier treatment of a dependent row. Large enough to make that component negligible and small enough that its square and its product with any entry stay finite in double |
| `BARRIER_TOL` | 1e-8 | Where the barrier stops: the largest of the primal residual over `1 + max(|b|, |bounds|)`, the dual residual over `1 + |c|`, and `|primal - dual objective|` over `1 + |primal objective|`, all in scaled space, at or below this. **Swept 2026-09-08 at 1e-6, 1e-8 and 1e-10 over the 19 hard instances** with everything else at its value: all three agree with the dual on 19 of 19, at work 1.188x, **1.255x** and 1.331x, and the checker at 1e-6 accepts 2, 8 and 14 of the interior points. The tighter setting buys checker acceptance at 6% more work and a longer walk into the region where the normal matrix is worst conditioned; the crossover is the designed route to a point the checker accepts, so this stays at the customary 1e-8 until that lands and the reading is retaken |
| `BARRIER_STEP` | 0.99995 | The fraction of the step to the boundary that is taken, separately for the primal and the dual, after Mehrotra's corrector. Swept at 0.9, 0.99, 0.999 and 0.99995 over the same 19: 18, 19, 19, 19 agree at 1.437x, 1.480x, 1.352x, **1.255x**, and the checker accepts 1, 2, 5, 8. Monotone in both readings up to the value Mehrotra published, which is where it stays |
| `BARRIER_REG` | 1e-9 | The primal regularisation of the barrier's Newton system: what is added to every bounded variable's `Θ^{-1}` (`zl/w + zu/v`) before the normal matrix is formed, **times the worst of the three relative measures capped at 1**, so it is full strength while the point is far and vanishes as it converges. In scaled space. It bounds `Θ` from above, which is what keeps the normal matrix's conditioning inside what the Cholesky can factor once `mu` is small; a flat 1e-8 that does not shrink leaves a dual residual of `reg` times the step that never closes on `greenbea`, whose degenerate directions move the point by 1e7 a step. **Swept 2026-09-08 over the 19 hard Netlib instances** (scfxm1-3, brandy, modszk1, stair, greenbea, greenbeb, capri, cycle, pilot, pilot87, d2q06c, bnl1, 25fv47, degen3, nesm, perold, fit2d), agreement with the dual and work geometric mean: off 11 of 19 at 1.367x, 1e-12 17 at 1.273x, 1e-11 17 at 1.253x, 1e-10 18 at 1.327x, **1e-9 19 at 1.255x**, 1e-8 17 at 1.350x, 1e-6 17 at 1.387x, flat 1e-8 15 at 1.648x. Bounded on both sides by an instance: below 1e-9 `greenbea` and one of `pilot87` or `brandy` stall, above it `pilot87` fails and `pilot` takes 73 iterations |
| `BARRIER_FREE_REG` | 1e-8 | The same term for a free variable, where `Θ^{-1}` would otherwise be zero, not scaled down, because a free column has nothing else bounding its step. Swept at 1e-6, 1e-8 and 1e-10 over the same 19 with `BARRIER_REG` at 1e-10: 18, 18, 18 agree at 1.302x, 1.327x, 1.314x; the three instances with free columns (`capri` 14, `cycle` 7, `greenbeb` 4) do not separate the three values. Held at the middle |
| `BARRIER_DELTA` | 1e-10 | The dual regularisation: what is added to the diagonal of the normal matrix `A Θ A^T` before it is factored, so a dependent row gives a pivot of `delta` and not one the Cholesky replaces. Swept at 0, 1e-10, 1e-8, 1e-6 over the same 19 with `BARRIER_REG` at 1e-10: 18, 19, 18, 19 agree at 1.336x, 1.318x, 1.327x, 1.309x, which reads flat. **It is not flat on the pilot family.** With `BARRIER_REG` at its 1e-9, 1e-8 loses `pilot-we` (fails at 133 iterations) and `pilotnov` (stalls at 110), both of which converge in 26 and 20 without it; over the eleven instances pilot-we, pilotnov, pilot, pilot87, pilot-ja, pilot4, brandy, greenbea, greenbeb, scfxm3 and stair, 0 and 1e-12 agree on 10 (greenbea stalls) at 0.951x and 0.940x, **1e-10 on all 11 at 0.947x**, 1e-8 on 8. Bounded on both sides by an instance: below it `greenbea`, above it the two pilots |
| `BARRIER_MAX_ITER` | 200 | Iterations after which the barrier stops and hands the model to the dual simplex, which starts from the slack basis and gives the verdict. Every converging run on the standard 94 takes at most 57 (`greenbea`, `bench/results/barrier.txt`); a run past 100 has stalled, and the rest of the cap is there so that a stall is handed over and not a limit. Not swept |
| `BARRIER_DIVERGE` | 1e6 | The multiple of the data past which the barrier's iterate is called divergent and the model goes to the dual simplex for its verdict: the primal iterate's infinity norm over `1 + |b| + |bounds|`, or the largest of the dual iterate's `y`, `zl` and `zu` over `1 + |c|`, each in the scaled space the barrier works in. The barrier certifies neither infeasibility nor unboundedness, so the hand-off is what turns a run that cannot converge into a certified verdict. **Measured on every iteration of the standard 94 and the infeasible 29** (`bench/measurements/02-220/`): the largest ratio a converging run reaches is 3.8e4 (`recipe`, dual side, converging three iterations later), the next 1.4e3 (`pilot`), so 1e4 is refused. Of the 19 infeasible instances that reach the barrier, 18 pass 1e6 between iteration 3 and 95; at 1e8 four of them take 10 to 87 iterations longer to the same verdict and at 1e10 five run to `BARRIER_MAX_ITER`. `cplex2` stays under 1.5e2 and ends at the cap under any setting. Two decades above the converging floor, measured on the standard and infeasible sets only |
| `BARRIER_DENSE_FACTOR` | 10 | A column is left out of the normal matrix when its nonzero count exceeds this times the average column count (and `BARRIER_DENSE_MIN`); the left-out columns come back through a Sherman-Morrison-Woodbury correction on every solve, `k` extra solves of the sparse factor per factorisation and a dense `k × k` Cholesky, every pass billed. When that small system is not positive definite the columns rejoin the normal matrix and the solve continues on the full form. On the standard 94 it fires on fit1p (23 columns of 627 rows), fit2p (25 of 3000) and seba (14 of 231); fit2p goes from past 10x the dual's work to 1.64x, fit1p to 12.2x, seba to 23.8x. **Swept at 5 and 20** (`bench/measurements/02-223/`): 20 is the same file as 10, no column on the set sitting between them; 5 reads 74 against 73 but both gains are hand-offs to the dual simplex (d2q06c after 97 barrier iterations, fit1p at the cap) and it makes the barrier worse where it newly fires, fffff800 31 to 89 iterations and past the limit, pilot-ja 29 to 68, israel 4 to 17 |
| `BARRIER_DENSE_MIN` | 30 | The count a column must exceed to be dense at all, so a small model with a short average never treats a column of a dozen entries as dense. Not swept |
| `BARRIER_DENSE_MAX` | 100 | Above this many dense columns none is left out: the correction costs `k` solves per factorisation and `k × rows` doubles, and past a hundred the normal matrix is dense enough to factor as it is. Not swept |
| `PDLP_TOL` | 1e-4 | Where the first-order method stops: the primal residual `|Ez|` over `1 + |bounds|`, the dual residual over `1 + |c|` (the part of `c - E^T y` outside the cone the bounds allow) and the gap over `1 + |primal| + |dual|`, all 2-norms in the scaled space, each under this. The reference's default; its high-accuracy setting is 1e-8. The crossover that follows starts the dual simplex from the point's basis guess, so the tolerance decides how good that guess is against how many first-order iterations buy it. **Swept at 1e-4 against 1e-6** (`bench/measurements/02-221/`): at 10x the dual's work the two agree on every verdict of the standard 94 and only truss moves, 3.30x against 3.99x with a crossover of 8540 against 8903 dual iterations; without a limit 1e-4 converges on ten small instances in 448 to 123773 iterations where 1e-6 takes 768 to 148096 and leaves israel and scagr7 past the cap. The published answer is the crossover's vertex at either setting |
| `PDLP_MAX_ITER` | 200000 | Iterations after which the first-order method stops and hands the model to the dual simplex from the slack basis. At `PDLP_TOL` without a work limit afiro converges at 448, adlittle 3648, kb2 15168, share2b 104320 and israel 123773 (`bench/measurements/02-221/`), so the cap sits above the slowest of the small set; under the campaign's 10x work limit the limit stops every run first except pilot, pilot87 and d2q06c, which reach the cap and are handed off. Not swept |
| `PDLP_CHECK_EVERY` | 64 | How many iterations pass between evaluations of the KKT error on the current iterate and on the running average, which is where termination and the restart decision are read; each evaluation costs two matrix passes for the average. The reference evaluates every 64 as well. Not swept |
| `PDLP_RESTART_SUFFICIENT` | 0.2 | A restart fires when the better of the current iterate and the average has a weighted KKT error under this fraction of the error at the last restart; the reference's β_sufficient. Not swept |
| `PDLP_RESTART_NECESSARY` | 0.8 | A restart also fires when the error is under this fraction of the last restart's and has stopped improving since the previous check; the reference's β_necessary. Not swept |
| `PDLP_RESTART_ARTIFICIAL` | 0.36 | A restart also fires when the current epoch holds at least this fraction of all iterations so far; the reference's β_artificial. Not swept |
| `PDLP_RUIZ_ROUNDS` | 10 | Rounds of Ruiz equilibration on the constraint matrix with its slack columns before the first-order iteration, each round dividing every row and column by the square root of its largest entry, followed by one Pock-Chambolle pass (α = 1: the square root of the absolute row and column sums); on top of the Curtis-Reid scaling the model already carries, undone at publication and at the crossover. The reference's count. Ten small instances without a work limit, iterations before against after (`bench/measurements/02-222/`): afiro 448 to 262, adlittle 3648 to 1041, sc50a 896 to 450, sc105 4672 to 2064, blend 20032 to 5059, kb2 15168 to 2757, stocfor1 46976 to 21729, scagr7 74176 to 11986, share2b 104320 to 78773, israel 123773 to 1857. **Swept at 0, 5 and 20** over the standard 94 at 10x the dual's work: 18, 17 and 20 instances inside the limit against 22 at 10, none disagreeing at any count, and 3 with no preconditioning at all (3fc772f); the sets overlap but move, and the count is what the campaign measures |
| `PDLP_STEP_TRIES` | 64 | How many times the adaptive step may shrink in one iteration before the step is taken as it is. The reference loops without a cap; the cap only bounds the work billed to one iteration and was never reached on the standard 94. Not swept |
| `DSE_MIN` | 1e-12 | Floor on a steepest-edge weight. Every weight is a squared norm and so positive by construction; the recurrence subtracts, and subtraction can cancel a small true value to zero. A guard against dividing by zero, not a tuning knob |
| `DEVEX_RESET` | 3.0 | Devex pricing in the primal simplex, behind `cfg.primal_devex` since 02-31 with steepest edge the default: the reference-framework weights are reset to 1 when the entering column's true weight over the framework and its carried estimate differ by more than this factor, in either direction. Weights only grow under the update, so the estimate drifts upward; the reset is what keeps the pricing from decaying to Dantzig's rule. Measured on `make primal` against Dantzig pricing (`cfg.primal_dantzig`, `build/bench/primal --dantzig`) at 3.0: 77 of 94 standard instances reach the dual's answer against 73, overruns 16 to 14, disagreements 5 to 3; over the 73 instances both finish, iterations 1.0146x (fewer on 40, more on 21) and work 1.0395x, the weight updates being the cost. Phase 1 iterations over the set 292901 to 280441. Swept at 2.0, 3.0 and 10.0: 73, 77 and 75 instances reach the dual's answer, with 6, 3 and 6 disagreeing, so 3.0 is bounded on both sides |
| `DSE_DRIFT` | 10.0 | How far a carried weight may sit from the exact one before the whole set is discarded and restarted. Well outside what rounding produces, well inside what one badly conditioned pivot can. Since 02-31 the same factor guards the primal's steepest-edge weights: the entering column's exact weight, 1 + the squared norm of its solved column, is compared with the carried one at every pivot, and a drift restarts the set at 1 + each column's squared norm, exact for the slack basis |
| `PSE_CHEAP_RESTARTS` | 64 | How many times the primal's steepest-edge weights, once the entering column's carried weight has drifted past `DSE_DRIFT`, are reset to the slack basis's weights (cheap, `nnz + rows`, exact for no other basis) before a drift restart in phase 2 rebuilds them exactly for the current basis (one FTRAN per variable). Phase 1 always takes the cheap reset. **Set 2026-09-09 by a sweep over the 13 instances the change touched** (pilot87, maros-r7, pilot, sctap3, ship04l, standata, wood1p, degen3, d6cube, dfl001, fit1d, fit2d, seba), with the exact rebuild in both phases: at 64 and 256 degen3 is fixed but pilot87 trips `PHASE1_RISE_MAX` and maros-r7 overruns; at 1024 pilot fails too; at 4096 maros-r7 and pilot recover and pilot87 still fails; never (the old behaviour) has no failure and degen3 slow. Every failure was a phase-1 walk under exact weights, so the exact rebuild was confined to phase 2 and the sweep's smallest value kept: 8 of the 13 finish inside the bound, degen3 in 3741 iterations against 15291, pilot87 in 53336 against 67339, and the five that remain are the five in `TODO.md` |
| `DSE_GUESS_RESTARTS` | 64 | how many times a warm start's guessed steepest-edge weights may drift past `DSE_DRIFT` before they are replaced by exact ones, one BTRAN of a unit vector per row. A warm basis starts every weight at 1.0, which is exact only for the slack basis, and each drift restarts the whole set at 1.0 again, so a walk that keeps drifting learns nothing about its rows: `klein2` warm from its own infeasible basis priced by violation alone for 106201 iterations, tripped the guard and restarted cold. A cold start never counts, its weights being exact from the first pivot, and neither does a node solve of the tree (`cfg.node_solve`), because the vertex a node lands on decides its children and exact weights at every node measured l152lav 0.435x against gt2 4.0x, bell3a 2.7x and bell5 unfinished (`bench/refusals.txt`, warm-weights-eager). **Swept at 1, 2, 4, 8, 16, 32, 64, 128** on the warm bench of 91 against the same tree without it, geometric mean of work: 1.346x, 1.054x, 1.043x, 1.017x, 1.004x, 0.990x, **0.985x**, 0.987x. At 1 a re-solve that needs one iteration pays the whole computation for nothing, stocfor3 13.0x, stair 11.8x, truss 11.5x; at 32 lotfi still pays 1.49x; at 64 the only instance that moves is 25fv47 at 0.25x, and 128 gives the same 25fv47 back 0.31x. `klein2` answers warm in 247 iterations at 64 (02-31) |
| `DUAL_PERTURB` | 1e-6 | the size of the cost perturbation the dual simplex applies the first time it stalls, before it falls back to Bland's rule: every nonbasic column between two different bounds has its cost moved away from dual infeasibility by this fraction of (1 + \|its cost\|) times a fixed function of its index (a 64-bit mixing hash: no clock, no seed, the same on every machine), and the move is written into `shift`, so the settling that repays every shift at the end removes it and the re-entry cleans what that leaves. Ties in the ratio test are what a dual-degenerate walk cycles on, and a step past a strictly positive reduced cost is what breaks them. Set by the defect that showed the cycle: grow15 with `x0 <= 575295`, cold, made no progress for 9261 iterations, switched to Bland at 10558 and tripped the guard at 185201; with the perturbation it ends optimal at 10679, primal and dual feasible. Over the four gates only the instances that stall move: grow22 from 52901 to 14176 iterations (0.257x work) and l152lav's tree 0.77x, the other 161 byte-identical, and the warm bench reads 92 ok of 92 for the first time. Not swept: held at the size the primal's refused perturbation used, one decade above `DUAL_TOL` times the largest reduced costs seen (02-31) |
| `ARTIFICIAL_BOUND` | 1e10 | The bound dual phase 1 lends a column whose cost points at a bound it does not have. No verdict depends on it: unboundedness is proven against a ray, and a model this bound cuts off is refused rather than answered. So it decides how often the method has to give up, not whether an answer is true |

Three more numbers in `src/simplex.c` are not tolerances but sit beside them:

| Name | Value | What it decides |
|---|---|---|
| `REFACTOR_EVERY` | 64 | Basis updates before a refactorization. Alongside it: the reactive fallback on a failed update, and one more refactorization at the end of every solve, because optimality is not accepted on carried values. The trigger PLAN 2.5.5 also calls for — watching an FTRAN/BTRAN residual *during* the solve — still does not exist. **Swept 2026-08-24 and it never had been**, six settings each its own tree and its own binary, all three gate sets at every one. Work as a geometric mean of per-instance ratios against 64: **8** 1.0318, **16** 0.9484, **32** 0.9143, **64** 1.000, **128** 1.1873, **256** 1.5663; worst single instance 2.267 (`grow22`), 4.430 (`d2q06c`), 2.819 (`grow15`), —, 5.881 (`d2q06c`), 9.125 (`nesm`). **64 is not the minimum**: 32 is 8.6% better on the mean. It stays for the worst case and for accuracy — at 32 `pilot87` goes from 1.044e-07 to 5.329e-05 against Koch, three orders worse, and still clears the gate's 3.017e-04 window with 5.7x to spare. **No answer changes verdict at any setting**, 94 netlib and 29 infeasible at six intervals. The control: the record at 64 is identical to `bench/results/netlib.txt` on all 94 instance lines |
| `ITER_SANITY_FACTOR` | 200 | Times `rows + columns + 1`, an iteration ceiling that is not a limit but a guard against a non-terminating loop. Hitting it is a defect in JAOS and is reported as a library error, never as a solve outcome |
| `SETTLE_ROUNDS` | 32 | How many times a settled point may be handed back to the dual simplex. A backstop, and a dimensionless count rather than a tolerance. **It does bind**, which its comment denied until D245: raising it to 128 costs `wood1p` 1.49x work for a bit-identical answer — same digest, same basis — and the gate reports `0 regressed` throughout, because its bar is 2.0x. So this stays 32 and the primal gets its own |
| `SETTLE_ROUNDS_PRIMAL` | 256 | The same backstop for a solve that set `cfg.force_primal`, which arrives at the re-entry with a whole solve's worth of dual infeasibility rather than a handful of columns. It was binding at 32 on 14 of the standard 94: on `25fv47` all 32 rounds ran with the violation still falling, 784.9 to 10.8, and the objective descending throughout. **Swept on both sides**: the forced primal agrees with the dual on **61** instances at 32, **69** at 64, **75** at 128, and at 256 one more converts (`woodw`) with another moving to an honest overrun; **512 is byte-identical to 256** — the plateau — because three of the remaining trajectories are stuck flat, not slow. Chosen once per solve, so the shipped path never reads it. Since 02-31 the primal's phase 2 no longer shifts costs (that was the dual's tolerance mechanism reached through the shared pivot, and it erased the reduced costs the primal prices on, so phase 2 ended after one pivot and the re-entry did the whole solve: 522814 re-entry iterations over the set), so the forced primal arrives with no shift outstanding and the re-entry made 7 iterations over the 94; the rounds stay as the backstop |
| `POLISH_ROUNDS` | 4 | how many times an optimum may be refined in the model's own units before it is published. The simplex refines the basic solution once in scaled units, and a row scaled down hard can hide a residue that is invisible there and past `PRIMAL_TOL` in the model's units: pilotnov with `x2 <= 4` published column values that left the equality POPL01 by 5.43e-5 on a row whose terms sum to 1.26e6 in magnitude, 4e-11 of that mass, while the row's logical said 0. Each round recomputes every row's residue from the published column values with compensated sums in the model's units, stops when the worst is at or under `PRIMAL_TOL`, and otherwise scales the residue back, solves it through the factorization and corrects the basics; one round took that row to 2.2e-10. A safety stop and not a quality knob: the loop exits on the first clean round, and none of the 110 cold optima of netlib and Kennington needs any (their worst residue is under 1e-7 already), so the four gates stayed bit-identical but for the check's own work. Not swept: held (02-31) |
| `WARM_REPAIR_MAX_SHORT` | 4 | How many basic members a mapped starting basis may be missing and still be repaired by promoting logicals rather than refused. It decides cost and never an answer: past the cap `build_warm_basis` returns false and the solve starts cold, which is always correct. Swept on both sides over every distinct shortfall in the set, from "never repair" to "always" — netlib work geometric mean 0.2553, 0.2089, 0.2047, **0.1916**, 0.1886, 0.1895, 0.1874, 0.1938 … 0.2605 at caps 0, 1, 2, **4**, 5, 6, 7, 8 … 596, with the worst per-instance ratio holding at 4.65 through cap 4 and then stepping to 15.48 at 7 (`greenbea`) and 172.03 at 345 (`dfl001`). The mean is flat across 1..7 and the worst case is not, so the value sits at the end of a plateau rather than at the minimum: 7 is 2.2% better on the mean for a worst case 3.3x larger. Kennington does not vote — all five of its short solves are short by exactly 1, so every cap at or above 1 gives it the whole gain, 0.0572 → 0.0070. **The relative shape was swept too and is worse**: capping `S/nrow` reaches only 0.2081 and meets the 15.48 cliff with 8 instances admitted, where the absolute cap admits 31 before reaching it, because `greenbea` is 7 short of 1954 rows — the smallest relative shortfall in the set and one of the two worst outcomes |

## The checker's tolerance

`jaos_check_solution` takes one tolerance from the caller and applies it in
original space. There is no default: a checker that chose its own would be
grading on a curve it set.

Given a claimed `x` and row duals `y`, with activities `a_i = A_i · x`
accumulated as a Neumaier sum over Dekker's exact products in `double`, and
everything canonicalised to minimisation (for a maximisation model the costs
and duals are negated internally). No walk in the file uses `long double`:
that type is 64 mantissa bits on x86-64 and 113 on aarch64, and what the
checker computes reaches `bench/results/`, so it would not be the same
figure on two machines:

**Primal.** For each column and each row, the violation of `v ∈ [lo, hi]`
is `max(lo − v, v − hi, 0)`, counting only bounds that are finite. The
report carries the largest column violation and the largest row violation
separately, and `primal_feasible` is both being within tolerance.

**Dual.** For a multiplier `w` attached to a value `v` with bounds
`[lo, hi]`, in minimize-canonical form:

```
|w| <= tol            no condition                    (negligible multiplier)
w > 0                 requires v <= lo + tol · s      (at its lower bound)
w < 0                 requires v >= hi - tol · s      (at its upper bound)
```

which is dual feasibility and complementary slackness in one test: a
multiplier that is not negligible must point at a bound its value is
actually resting on. A multiplier pointing at an infinite bound is itself a
violation, of exactly its own magnitude. Row multipliers are the duals as
given; column multipliers are the reduced costs `d_j = c_j − A_j · y`,
recomputed here from the original matrix.

`s` is the scale of the value being tested, and it differs by kind:

```
row i      s = max(1, sum over j of |A_ij · x_j|)     the row's own traffic
column j   s = max(1, |x_j|)
```

A row activity is a sum, and a sum whose terms cancel cannot be pinned to an
absolute tolerance. Row 3 of Netlib's `finnis` used to add terms totalling
4.0e10 in magnitude and come to rest 1.5e-6 from its bound, where **one ulp
at 4.0e10 is 7.6e-6** — the residue is a fifth of a single rounding step at
the scale the row worked at. Judged absolutely at 1e-6, that row is "not at
its bound" and its multiplier of 28 is reported as a violation of 28, on a
solution whose duality gap is 2.2e-10. No double-precision answer can pass
that test and no amount of solver work can produce one; the demand is for
seventeen correct decimal digits of a sum that cancels ten orders of
magnitude.

**That row is the whole load the relative window was carrying, and D261
took it off.** `finnis`'s 4.0e10 came from four columns published on bounds
the solve had lent them; with those retired row 3 carries 7734 and rests
exactly on its bound. Measured over the standard set at `tol = 1e-6`
(`bench/measurements/02-170/run-window-need.sh`): before D261, **one
instance** had rows an absolute window would refuse — `finnis` row 0 at 3.39
times the window and row 3 at 1.52, both admitted by the relative one. After
it, **zero of 94**, worst 0. The window stays, and the reason is D24's: it
exists because a row activity is a sum, which is a fact about arithmetic and
not about this population, and a population that is quiet today is not an
argument for removing the only thing that would catch it. What has changed
is that no gate instance now demonstrates it, so the case above is written
in the past tense and the script is what re-asks the question.

A column value is one published number rather than a sum of cancelling
terms, so it takes the ordinary mixed absolute/relative form and nothing
more. The row case is the one that needed the argument.

**Why the scale cannot excuse a wrong answer.** This test is a diagnostic;
the gap below is the proof, and the two are tied together exactly. Since
`P − D` is the sum of `w_v · (v − bound_v)`, a row waived here at distance
`d` with multiplier `w` still contributes exactly `w · d` to the gap, at full
size and with no cancellation available to it — every term of that sum is
non-negative on a primal-feasible point. So the waiver can decline to report
a discrepancy twice; it cannot hide one. `tests/test_check.c` builds the case
where the sign condition is waived and the answer is refused anyway, with
`0 − (−500)` checked against `1000 × 0.5`, and the case where a row genuinely
off its bound is still reported at the full magnitude of its multiplier.

The exemption is for the condition and for nothing else. **Every
multiplier contributes to the dual objective below, including the ones
held to no condition** — the only thing a negligible multiplier is spared
is being required to rest on a bound.

That distinction is not a detail. `D(y)` is defined as the sum over
variables of the least `w · t` attainable in `[lo, hi]`, which makes it a
function of `y` alone; dropping terms from it by their magnitude is not
part of that definition. What gets dropped is `w · bound`, and that is
small only if the bound is: a multiplier of `1e-7` on a variable resting on
a bound of `1e6` carries `0.1` of dual objective. Discarding it while the
primal still counts `c_j v_j` invents a gap proportional to the tolerance —
which is what used to reject `pilot-ja`, whose duals are exactly correct.

Two other rules close that case and are both wrong, recorded here because
each looks reasonable. Contributing `w · v` makes the term cancel, so on a
model whose multipliers all fall under `tol` the gap is identically zero
for every feasible point and the checker certifies the whole polytope.
Choosing the bound nearest `v`, which is what HiGHS does for its own
diagnostic, produces negative terms that offset real residuals elsewhere in
the model, and computes `(−inf + inf) / 2` on a free variable.

**Gap.** As each multiplier is checked it contributes `w · bound` to the
dual objective, where `bound` is the one its sign points at. The gap is
then relative:

```
gap = |primal_objective − dual_objective| / (1 + |primal_objective| + |dual_objective|)
```

Both objectives appear in the scale, not just the primal. A relative measure
that normalises by one side alone reports a larger error the further the two
are apart, which is backwards — the scale should say how big the numbers being
compared are, not how badly they disagree. This is the form PDLP uses, which
HiGHS adopted for its own gap, and the same shape as the DIMACS error measures
used to validate benchmark results. Changing to it moved no verdict on the
Netlib set: 0 regressed, 0 improved, measured against the recorded baseline.

`dual_feasible` is the largest dual violation and the gap both being
within tolerance.

Because every multiplier contributes, `P − D` is exactly
`sum_v w_v (v_v − bound_v)` — each term the complementary-slackness residue
of a single variable. On a point that is *exactly* primal feasible every one
of them is non-negative, and that is what gives an accepted solution a
guarantee rather than a reassurance: `P − P* <= gap`, by weak duality.

**The halves, and why the gap alone does not carry that guarantee.**
Non-negativity is a property of feasibility, and the checker accepts points
that are feasible only within `tol`. An entity sitting `d` outside its bound
turns its own term negative, so the sum is a difference of two quantities and
not an accumulation of one:

```
Q = sum of the terms that are >= 0        N = sum of |the terms that are < 0|
P − D = Q − N,   gap = |Q − N| / (1 + |P| + |D|)
```

Both are reported, in the objective's own units, as `gap_positive` and
`gap_negative`. Neither decides anything.

They are there because `Q` and `N` cancel, and a gap has no way to say
whether it is small because both halves are small or because two large ones
met. The bound that survives the distinction is `P − P* <= Q`: it is the
positive half alone, so a negative half cannot buy it down. `tests/test_check.c`
builds the case where the gap reads zero on a point carrying 900 of each, and
`grow22` shows the same shape at the size a real instance produces — a gap of
`1.99e-13` over halves of `6.41e-05` and `1.24e-07`, which is to say the gap
understates its own bound by eight orders. `finnis` used to be the example
here, at `2.21e-10` over `1.05e-04` and `2.89e-05`, and D261 took its halves
down to `6.44e-11` and `1.63e-11`. The census of both halves over the
standard set is `bench/measurements/02-170/row-census-candidate.txt`.

What this is *not* is a false acceptance, and D24 says so in the same breath
as raising it: hiding a negative half costs an equal positive one, and the
positive half is exactly what bounds the suboptimality. The two halves are an
instrument for a question the gap could not be asked, not a repair to a hole
in it.

**The hole is somewhere else, and it is in the identity rather than in the
halves.** `P − D = sum_v w_v (v − bound_v)` needs every term, and a multiplier
whose sign points at an *infinite* bound has none to give: the term is minus
infinity, because the dual objective of a variable free in the improving
direction is unbounded below. Dropping it leaves a sum belonging to a
different problem — one where that variable had a finite bound — so `Q` stops
bounding anything. Two variables and one constraint build a point that is
arbitrarily suboptimal and on which `Q`, the gap and every violation all read
zero.

`gap_certified` says whether the sum was complete, and `max_dropped_multiplier`
how big the largest missing term's multiplier was. **Neither decides
anything**, and that is not caution: D47 measured the obvious threshold —
judging the multiplier against the traffic of the dot product that formed it,
the same move D23 made for rows — and it separates nothing, because what makes
a dropped term cost anything is the distance the variable would travel, which
is a property of the polytope and not of the column.

**So the distance is computed instead of thresholded.** `certified_suboptimality`
is `|w|` times how far that column can move on its own, with every other
variable pinned where it is. Nothing else moving means no other bound can be
broken, so the direction is feasible for its whole length and the number is a
lower bound on `P − P*` rather than an estimate — arrived at with no basis, no
factorization and no reference value. Where that distance is finite the
product is self-limiting, which is why this needs no threshold: a multiplier
that is really roundoff certifies a roundoff-sized suboptimality. Where it is
infinite the product is infinite for any nonzero multiplier at all, so those
are counted in `unquantified_rays` instead of being reported as a certificate
— split on this checker's own `|w| <= tol`, the definition of nonzero it
already uses everywhere else.

**The primal residue, relative.** `max_row_violation_relative` reports the
worst row residue as a fraction of what that row carries — `sum_j |a_ij x_j|`,
the same quantity the bound-proximity window is built from. It decides
nothing, and D24 is the argument for why it is not allowed to: primal
feasibility is the hypothesis the identity above stands on, so relaxing it
would remove D23's licence rather than extend it. The measurement is kept
because it is real — `greenbea` clears the absolute 1e-6 bar with 95% of the
margin to spare, at 4.66e-08 on a row carrying 6.5e+05, and the absolute
number cannot tell that from the same residue on a row carrying 0.7.
`finnis` was the example here, clearing the same bar with 16% of the margin;
D261 took its residue to 1.58e-13 and `greenbea` is the worst on the
standard set now (`bench/measurements/02-170/`).

The four tests are deliberately not independent. Activities come from a
scatter over the matrix while the dual objective accumulates from bounds,
so a corrupted dot product shows up as a nonzero gap even when it also
corrupts the reduced costs it would have to fool. The system is
overdetermined; one broken kernel cannot satisfy all of it.

## Presolve's tolerances

Defined in `src/presolve.c`. Presolve runs on the model as loaded, before
`sx_init` computes any scaling, so nothing here is comparable with the
solver's table above — those are magnitudes in scaled space and the scaling
depends on a matrix presolve has just changed. Nor is the checker's `tol`
usable here: it is a number the caller supplies to judge a finished answer,
and it was never measured for deciding whether to fold a bound. Every
constant below is new and each arrives with its own sweep.

| Name | Value | What it decides |
|---|---|---|
| `PRESOLVE_ROUND_ULPS` | 8 | Whether a residue left by a running difference is a number. **Five sites ask it**, and the list has been wrong in this table before, so it is spelled out: the solve's entry, asking whether a caller's box is inverted beyond rounding; the singleton row's fold, asking whether a column's interval has genuinely emptied; the emptied row, asking whether its bounds still admit zero after every column removed from it shifted them; the frozen row, asking whether its activity range has left its own bounds; and clause 1 of the activity pass, asking whether a live row can be satisfied at all. The window is this many `DBL_EPSILON` times the scale that produced the residue, never this value alone. The scale is the row's traffic at three of the four; the fold divides it by its own coefficient, because it judges `cur_rl[i] / a` and the error came down with it. **Eight ulps is a bound here and not a scale claim, and that rests on `cur_rl`/`cur_ru` being compensated** — a running difference of k terms would need k ulps, which D162 and D163 briefly added and D166 removed once there was no drift left to cover. See "the scale, and the term count" below, which is kept because the question will be asked again. Set from a measurement of the residues themselves rather than from a sweep: instrumenting all three sites over all three sets emits 32240 probe lines, of which 12 carry a residue above zero (the fold site prints only on a collapse, so its share of that total is collapses rather than reaches), **all twelve on `netlib-infeas`** and none below 3.69e8 ulps. So no feasible model on the 139 puts a residue anywhere in (0, 3.69e8 ulps], the constant may be set anywhere in that interval, and 8 is taken because it is where `ps_row_tol` already is. Swept 1, 2, 4, 8, 16, 64, 256 with `make clean` between settings: solved 94, objective ok 94, checker ok 94 and 7598 rows / 24695 columns removed at every one. The canary flips four times inside the grid and the seven binaries have seven distinct md5s — the second check is the one that matters, because the canary's four conflicts do not separate 2 from 4 or 8 from 16 |
| `PRESOLVE_IMPLIED_FREE_ULPS` | 8 | Whether the box a row implies on a singleton column lies inside the column's OWN box, so the column's bounds can never bind and it can be substituted out. Subtracted from the column's bounds rather than added to the implied ones, so the family declines at exact equality: being wrong here is silent, and drops a bound that was real. The window is this many `DBL_EPSILON` times `max(1, |b|, traffic) / |a_ij|`, which is the row sum's residue carried through the division that produces the implied end. **It is a switch, not a dial.** Swept 0, 1, 8, 64, 4096 with `make clean` between settings: rows removed set-wide read 9992, 8639, 8639, 8639, 8639, `maros-r7` reads 984, 980, 980, 980, 980, and solved / objective ok / checker ok are 94 at every one. One step, between 0 and 1, and four decades of nothing above it — the family's firing is bimodal, because an implied box is either comfortably inside the column's box or exactly at its bound and almost nothing lands in a 1e-12 relative band. The canary is in the instance rather than in a model built for it: 4 of `maros-r7`'s 984 candidate rows sit at exact equality, so 0 must read 984 and anything above it 980, and it does. That canary separates 0 from the rest and nothing else, so the check that carries the plateau is the second one — five settings, five distinct md5s of `presolve.o`. **Zero is not obviously wrong and is not free**: it removes 1353 more rows and reads a geometric mean of 0.9627x against 8, but `d2q06c` costs 2.2163x there, which crosses `bench/run.c`'s own 2.0x work bar. So 8 ships and the question of whether the window's absolute floor should exist at all is open in `TODO.md` |
| `JM_PRESOLVE_ROUNDS` | 16 | Cap on presolve's fixed-point rounds — a safety stop and not a quality knob, since the loop exits as soon as a round changes nothing, and it lands on top of the structural backstop `num_row + num_col + 1` rather than above it. Set where the propagation reaches its fixed point: swept over the standard set, rows removed go 6060, 7178, 7549, 7596, 7598, 7598, 7598, 7598 at 1, 2, 4, 8, 16, 32, 64, 128 rounds, and columns removed 22671, 24300, 24629, 24693, 24695, 24695, 24695, 24695. The canary is a chain of 200 singleton rows built to resolve one link per round, and it reads 1, 2, 4, 8, 16, 32, 64, 128. The cost is flat across the whole sweep — 97.2 s to 103.6 s at `J=12` against a set that takes about 99 s — so there is nothing to trade against |

## The scale, and the term count

**All three sites scale by the row's traffic, and that took three decisions to
arrive at.** The pass that tests a frozen row for feasibility after the round
loop used to scale by `ps_bound_scale(cur_rl[i], cur_ru[i])`, and the paragraph
here used to call that forced. It was not:

- `row_traffic[i]` no longer saturates to `+inf` when a column with a
  half-infinite box is relaxed out of a row. It accumulates only what a finite
  end absorbed, so the reason the bound scale stood in expired.
- The traffic is what the window has to cover, and the bound's magnitude is
  not.
- The bound's magnitude is worse than merely irrelevant. It is the magnitude
  of ONE END, and the test compares a computed activity against the OTHER
 : `-1e12 <= x0 + x1 <= 0` with both columns cost-0 in `[1e-4, 1]`
  is infeasible by 2e-4, and the lower bound alone bought a window of 1.78e-3
  on the upper side. It published `optimal`. The same model with
  `rl = -INFINITY` was refused correctly all along, which is the control.

`ps_row_tol` still cannot be used at that site: it scales by the LIVE traffic,
and a frozen row's live traffic is routinely zero, which collapses the window
to 1.776e-15 absolute whatever the row's scale.

**And the constant alone is not the whole window, because `cur_rl`/`cur_ru` are
a running difference.** Every removed column subtracts its own
`a * v` from both, with no compensation, so each subtraction rounds by up to
half an ulp of the partial it produces and the error after k of them goes with
k. Eight ulps covers a k of about three; **the largest k on the three sets is
325**, at the frozen-row test on Kennington, and this table owns that number —
the source comments point here rather than repeating it. So every window that
judges one of those numbers carries a second term:

> `k * DBL_EPSILON * (|the end being tested| + row_traffic[i])`

Three things about it, each of which was got wrong first and measured or tested
into shape (`bench/measurements/02-72/`):

- **The scale is not the traffic alone.** A partial is bounded by
  `|row_lower[i]| + traffic`, and near the firing boundary `cur_rl[i]` is near
  the ACTIVITY rather than near zero, so the traffic does not dominate. A row
  of activity 1e9 with 300 removals totalling 0.9 of traffic carries about
  1.8e-5 of error against a traffic-only window of 6.8e-14.
- **It is ONE end and not `ps_bound_scale`.** Scaling by the larger of the two
  brings D161's defect back through the count — two cost-0 singleton
  relaxations are two shifts, and `2 * eps * 1e12` is 4.4e-4 against an
  infeasibility of 2e-4. D161's own test caught it.
- **It is zero at k = 0**, which is what keeps D161 for a row nothing was ever
  removed from.

It is ADDED to the eight ulps, never substituted for them, so no window can
come out narrower than it was — narrowing one of these is what produces a false
INFEASIBLE. Measured over the three sets before landing: **0 verdicts flip** on
any of the 139, the twelve genuine infeasibility firings in `netlib-infeas`
survive it, and **the widest ABSOLUTE window is 6.587e-08**, at Kennington's
frozen-row test, against 6.494e-08 without the count. This table owns that
figure too.

**The base moved as well as the k-term, and the entry did not say so at
first.** The two halves are added where the wider scale used to be taken, so a
row with both traffics large pays eight ulps twice instead of once, whatever k
is. At k = 0 that is 8 ulps of 1, which is 1.78e-15, because a traffic above
zero implies a shift count above zero at every producer.

**The fourth site landed one entry later**, and the three that came
first were described here as "every site that judges a running difference"
while the fold did not carry the count. That sentence was false for one commit.
The fold now takes it, scaled by `row_traffic[i] / |a|` and by the end
`tightens_lo`/`tightens_hi` says the running difference supplied.

**The reopen condition is the absolute window and not a ratio.** A set carrying
a larger `rg.traffic` than Kennington's 3.66e7, or a row with a shift count far
above 325, is where these windows stop being comfortably under `PRIMAL_TOL`.

**And the count does not cover an error that arrives inside a VALUE.** When the
fold fixes a column at `cur_rl[i] / a`, that number carries row i's accumulated
error, and the row receiving the fixed column is charged one shift at its own
traffic. `bench/measurements/02-73/` has the model where that publishes
INFEASIBLE on a feasible model. **Carrying the error into the window was built
and refused**: it stops the refusal and then publishes a point violating
two rows by 7.5 times `CHECK_TOL`, because a window decides whether to refuse
and cannot correct a value that is already wrong.

**The error itself was removed instead, and the counts were then taken
back out.** `cur_rl`/`cur_ru` keep their residue through a Neumaier
accumulator, so the running difference no longer drifts and the fold fixes
columns at the value the model actually has. With no drift there were no terms
left to count, and `row_shifts`, `ps_shift_excess` and `ps_end_scale` are gone.

**So the term described above does not ship.** It is written down because the
question it answers is real and will be asked again: eight ulps is not a bound
on a running sum of k terms. The answer this project reached is that the sum
should not lose the terms, not that the window should be widened until it
covers the loss. Removing the counts also narrows four windows, and at the
emptied-row test that is the direction worth having — that test is the last
word on an emptied row and too wide there accepts an infeasible model silently.

The evidence that compensation covers what the counts covered is that all five
tests the counts were built for pass without them
(`bench/measurements/02-76/`), and those tests are the models: each was
validated against a tree that fails it.

**The same argument reaches the simplex.** `compute_primal` builds
`-N x_N` by walking the nonbasic columns in column order, so a row that meets a
large term before many small ones was dropping the small ones — the identical
shape one layer out, and it made the `-DJAOS_NO_PRESOLVE` build refuse a model
whose feasible point is exactly representable. That sum is compensated now,
with the same Neumaier accumulator and for the same reason `long double` is not
used. No constant moved: there is no window here to widen, only a sum to
take correctly. `bench/measurements/02-78/` carries the reading.

**And the published objective, which is the same sum one step further on
.** It is `obj_offset` plus `c_j x_j` over the published values, taken
on the model that publishes them. 81 of netlib's 94 agree bit for bit with
`jaos_check_solution` now, against 34. **What a compensated sum cannot reach
is the rounding of each product**: on `finnis` the accumulation is exact to
6.3e-09 while 2.65e-05 remains, all of it from rounding `c_j * x_j` to a
double, where one term of 6.5e11 rounds by up to 7.2e-05 on its own.
`bench/measurements/02-79/split-the-error.txt` separates the two, and any
claim that compensating a sum made it accurate should separate them the same
way first. **The `finnis` figures in this paragraph are D169's and the term
of 6.5e11 was a column published on a bound the solve had lent it; D261
retired it and `sum |c_j x_j|` there is 3.14e+05**. The argument is unchanged: a compensated sum
cannot reach a rounded product, and that is why the two-product exists.

## The proxy the constant used to rest on

Kept because it is what "measured from one side only" looks like, and because
the number it produced is still true.

> accumulated shift / `ps_bound_scale(cur_rl, cur_ru)` ≤
> `PRESOLVE_ROUND_ULPS` = 8

Measured over the 8293 frozen rows the standard set produces: worst ratio
**2.718e-4** (`finnis` row 99), or 4.834e-4 counting one rounding per stored
entry rather than the eight the proxy assumes.

The scale mostly recovers itself, which is why the margin is so wide: in an
emptied frozen row one bound falls to zero and the other retains the magnitude
that was subtracted, so `ps_bound_scale` returns the shift. `greenbea` row 57
carries 660 of shift and reads `cur_rl = -660, cur_ru = 0`. The 60 rows of
8293 where that does not happen are the ones with an infinite bound, where the
scale falls to its floor of 1; their largest shift is 153.

**It bounds the window from one side only.** It says the window is not too
tight. Nothing in it says the window is not too wide, and at 1e-9 it was: on a
row of magnitude 1e9 the window is 1.0, and

```
min x0  s.t.  x0 + x1 == 1e9 + 1,  x0 in [0.5, 0.5],  x1 in [0, 1e9]
```

is infeasible by 0.5 and walked straight through it, reaching
`jm_postsolve_solved` and publishing OPTIMAL with `x1` half a unit above its
own declared upper bound. The model's ratio is about 1.0, far inside the
5.6295e5 the proxy allowed, so no amount of tightening the proxy would have
found it.

The residue measurement replaced the proxy on both sides. Of the 19082 frozen
rows across all three sets, exactly 4 carry a residue above zero and all four
are on `netlib-infeas` at 1.5e15 ulps or more, so a window of 8 refuses
everything that should be refused and nothing else. `galenet` and `pilot4i`
are still caught, now with margins larger than D102 recorded rather
than smaller.

**One number in `src/presolve.c` is deliberately not in this table.** The
three readings of a row's activity range — the model is infeasible, the row
is forced to an extreme, the row can never bind — each ask whether a computed
sum equals a bound. That is not a judgement and has nothing to tune: the only
thing that can separate two numbers that should be equal is the rounding in
the sum, so the window is a small multiple of `DBL_EPSILON` times the traffic
through it. Making it a tunable instead cost 02-04 a campaign, and the raw
readings are in `bench/measurements/02-04/`.

`ps_row_tol` therefore keeps its own literal 8 and does NOT read
`PRESOLVE_ROUND_ULPS`, though the two agree today. 02-09 routed it through the
shared function for a few hours and put those three readings on the
`EXTRA_CFLAGS` hook, which review caught: `make netlib
EXTRA_CFLAGS=-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=64` reproduced 02-04's failure,
`pilot` INFEASIBLE with column 3554 pinned. Two constants that happen to be
equal are not one constant. If they ever have to move together, that is a
decision with a measurement behind it and not a shared symbol.

**Bound tightening is not here because it does not ship.** 02-04 built the
family, measured six variants of it against the standard set and refused all
six: every one returned INFEASIBLE on models that have an optimum. The
evidence is in the same directory and the reasoning is in `src/presolve.c`
beside the reading that would have been the fourth.

## The writers' numbers

None of these is a tolerance and none decides an answer. They are here
because every constant in the source has a home in this file, and because
the first one looks like a style choice and is not.

| Name | Value | What it decides |
|---|---|---|
| the digit count | 15, then 16, then 17 | How many significant digits `wr_num` prints. **It is the round trip, not the layout.** `%.17g` of a finite double always reads back as that double, which is the IEEE-754 guarantee, so seventeen is exact by construction; the writer tries fifteen and sixteen first and keeps one only when `strtod` returns the same value, so the short forms are exact by check. The fallback is the one path nothing checks at run time, and `src/write.c` asserts it. Measured over 5,110,541 values — 4,000,000 random bit patterns, one-ulp walks from 1.0, small rationals and decimal fractions — of which 2,222,696 reached the fallback: **0 did not read back**. The same reading says what the loop buys: fifteen digits survives 6% of random bit patterns and 86% of decimal fractions, which is what an MPS file usually carries |
| `NAME_LEN` | 24 | Buffer for a generated name. One prefix character, at most 19 digits of an `int64_t`, a terminator, rounded up. Not a limit the writer can reach |
| `NUM_LEN` | 32 | Buffer for a written number. Seventeen significant digits, a sign, a decimal point and a four-character exponent, rounded up. Not a limit the writer can reach |
| `LP_WRAP` | 72 | Column at which `jaos_write_lp` breaks an expression across lines. The reader does not care and a person reading the file does. Nothing measures it |

## Acceptance, for the Netlib gate

Separate from all of the above, and not a solver tolerance: an instance is
accepted when `|obj − ref| <= 1e-6 · max(1, |ref|)` against Koch's
reference values [22], with the checker green in original space. That
criterion is relative, and a test that pins a large objective absolutely is
stricter than the project's own gate — which makes it a test about
floating-point luck rather than about the solver.

## What is not settled

Instances have now argued with them. The Netlib gate has been run over the
whole standard set (`bench/results/netlib.txt`), and **all 94 instances come
back with the checker green** at the tolerance above. All eight of the
original failures closed, and **not one of them closed by moving a number**:

| instance | what it actually was | |
|---|---|---|
| `pilot-ja` | a contribution the checker was dropping | D21 |
| `finnis` | a bound-proximity test judged absolutely on a row that cancels ten orders of magnitude | D23 |
| `nesm` | a settled basis the dual simplex had never been handed back | D25 |
| `grow15` | a cycle that had been read as a stall | D26 |
| `etamacro` | a repair test reading the wrong quantity, in the wrong space | D27 |
| `greenbea` | a column with nowhere to rest, needing a basis change rather than a move | D28 |
| `pilot` | an answer read off an inaccurate solve of a fresh factorization | D29 |
| `pilot87` | a clean-up loop dispatching one column of twelve | D30 |

That is the case for leaving these numbers where they are, made by instances
rather than by argument: every failure anyone was tempted to blame on a
tolerance turned out to be something else. `etamacro` is the sharpest,
because it genuinely was a question about a tolerance's *space* — its breach
is `4.89e-8` scaled and `1.56e-6` published. The answer was not to change
the tolerance or to pick a space, but to test a quantity that has neither:
the term the breach contributes to the duality gap, which comes out the same
number either way.

**Nothing is left of that list**, and the last two are worth a paragraph each
because both were, at the time, the strongest case anyone had for moving a
number.

**`pilot` was the one case where a tolerance was genuinely the question, and
it was the primal one.** Everything else about that answer was right: the
objective inside `2.3e-5` of Koch against a bar of `5.6e-4`, the dual
violation exactly zero, the gap `6.6e-14`. What refused it was
`interval_violation`, an absolute test, on a row `1.73e-6` — 1.73 tolerances
— outside its bound. D24 refused to make that test relative for four reasons;
D28 recorded that one of them had expired, and what replaced it was stronger:
the relative figure said `pilot`'s row was `6.93e-9` of what the row carries
against `8.21e-17` for `finnis`, so a relative window would have waved through
a violation that was real. **The row was real, and it was the answer that was
wrong.** D29 refined both solves of the refresh that verifies an optimum, and
the residue went from `1.73e-6` to `6.73e-13` — four hundred thousand times
narrower than the window anyone was proposing to widen, on a solve that came
out *cheaper*. It reads `9.09e-13` today.

That is the whole argument of this section arriving at its own last case:
the instance that most looked like a tolerance was not one either.

`pilot87` was not a tolerance question at all, and it turned out not to be a
precision one either: an objective 7.6x outside the bar was a wrong answer,
and loosening a number to admit it would have converted a defect into a pass
and proved nothing. The defect was a clean-up loop dispatching one
column of twelve, and the objective now lands 1.33e-7 relative from
Koch's exact value.

That is the argument for the freeze rather than a footnote to it. Eight
instances were refused across the campaign, every one was tempting to blame
on a number, and every one was something else. A tolerance that survived
eight opportunities to be the culprit and never was is a number with evidence
behind it — which is what these now are, and why moving one from here
on takes a measurement on both sides, written beside it here.

## The exact arithmetic's one number

`src/exact.c` has a single constant and it is not a tolerance. It is a
capacity, and the difference matters: no setting of it can change an answer,
because it decides only how large a magnitude fits before an operation
returns false. Every other number in this file decides what counts as zero.

| constant | value | |
|---|---|---|
| `JM_EXACT_LIMBS` | 128 | Limbs of 32 bits in one magnitude, so 4096 bits, and a `jm_rational` is 1048 bytes. The floor is what one double costs: a finite double is `m * 2^e` with `e` no smaller than -1074, so its denominator needs up to 1075 bits and its numerator up to 1024, and 34 limbs holds either. The rest is headroom for the sums and products that read them. **Swept at D275 over the standard 94, `make clean` between settings and the reported capacity as the canary** (`bench/measurements/02-180/`):<br><br>`128` → 28 proved, 60 refused, 6 broken, 79 s<br>`256` → 40 proved, 48 refused, 6 broken, 68 s<br>`512` → 51 proved, 34 refused, 9 broken, 142 s<br>`1024` → 57 proved, 24 refused, 13 broken, **2500 s**<br><br>**The value stays at 128 for the seconds and not for the answers.** The reach it buys is real -- four doublings take the proved count from 28 to 57 and leave 70 of 94 with a verdict rather than a shrug -- and the last doubling costs 18x the one before it, because a multiply is quadratic in limbs. A caller who wants the reach can have it with `-DJM_EXACT_LIMBS=N`, which is what the constant is for.<br><br>**Widening it is bought with block size.** A `jm_bigint` is `4 * limbs + 8` bytes and a block of `k` rows holds `k*(k+1)` of them, so `VERIFY_BLOCK_BYTES` admits a block of 990 rows at 128 limbs and about 350 at 1024. `degen3` shows the trade: BROKEN at 128 on a 268 MiB table, refused at 256 where the same table is 532 MiB. Two constants, one of them shrinking what the other buys. Not reachable by widening at all: `pilot87` wants 2556 limbs and a 21 GiB block |

## The verifier's one number, and the one that was removed

`src/verify.c` has a single constant, and like `JM_EXACT_LIMBS` it is a
capacity rather than a tolerance: no setting of it can change an answer, only
whether the call gives one.

| constant | value | |
|---|---|---|
| `VERIFY_BLOCK_BYTES` | 536870912 | 512 MiB. What one call may hold for a single block's dense elimination. **Arithmetic, not a fitted value**: a block of `k` rows holds `k*(k+1)` numbers and a `jm_bigint` is `JM_EXACT_LIMBS` limbs plus a sign, so at 128 limbs a number is 520 bytes and this ceiling is a block of 990 rows. It exists so an oversized block is refused before the allocation rather than by it. Bounded below by what the gate needs: D272 measured the largest block of every gate basis, and the biggest that also passes the width test is `pds-20`'s 1542 rows, which needs 263 MiB at its own 27 limbs — under this ceiling, so it is not what refuses `pds-20`. Bounded above by nothing except the machine |

**A second constant was removed rather than left unmeasured.** An earlier
draft of `jaos_verify` kept `VERIFY_BOUND_MARGIN`, 64 bits of slack below the
capacity, to cover the rounding in a bound computed with `log2`. The bound is
integer arithmetic now and has no rounding to cover, so the margin had no
quantity to guard and no measurement on either side. The test is
`bound_bits > capacity_bits` and nothing else. **That test is not a
guarantee and the header says so**: it bounds the matrix minors, while the
right-hand side column an elimination carries also holds model bound values
and the accumulated denominator, and neither is in it. A basis that passes it
can still run out of limbs during the work, which is a refusal too, with
`terms` saying how far it got. No margin closes that gap; only bounding the
right-hand side would, and nobody has.

Two things in `tests/test_exact.c` pin the capacity rather than describe it.
The largest magnitude the array holds is built, and shifting it one bit,
adding it to itself and doubling it all return false. Beside that runs the
control, one bit lower, where all three succeed. That pair is what makes the
limit a measurement instead of a comment, and it caught the first version of
`jm_nat_shl`, which charged a spare limb for any shift that was not a whole
number of limbs and so refused a value that fits.
## Branch and bound's twenty-nine numbers, and three switches

All in `src/mip.c`: the first two are D288's, the four under them the root
cuts', then the cut depth's, the node cut cap's, the
cover rounds', the two stalls', the MIR cuts' three
, the aggregation's two, the dive heuristic's two
RINS's and the feasibility pump's two, the
dive's backtrack budget and its resume gap,
the branching rule's
, the two after it strong branching's, then the probe cap's
 and the probe depth's. The MIP set is where a sweep of any of
them runs. The rounds, the cut depth, the node cut cap, the cover rounds,
the two stalls, the MIR rounds, the aggregation steps, the dive
heuristic's solves and its depth, RINS's budget, the pump's rounds, the
backtrack budget,
the resume gap, the dive's degradation bound, the
reliability, the cap and the probe depth have theirs; the other eleven
are
listed with what each one waits for stated
beside it. Six switches sit beside the numbers as `constexpr bool`,
read on the same set:
`MIP_ROOT_CUT_DROP`, on since D306 (0.799x the work over the 24 with it,
none past 2x, `bench/measurements/02-202/`); `MIP_COVER_LIFT`, off; `MIP_NODE_MIR`, off; and `MIP_PUMP_GENERAL`, off
; `MIP_PUMP_ALWAYS`, whether the pump runs at the root where
something already holds an incumbent, D318's guard re-asked for the
objective pump; and
`MIP_RCFIX`, whether the root pulls in an integer column's far bound to
the furthest integer its reduced cost still allows once an incumbent
exists.

| constant | value | what it decides |
|---|---|---|
| `MIP_INT_TOL` | 1e-6 | how far a relaxation's value may sit from the nearest integer and count as integral, in the model's own units. A value inside it is published rounded. Below the primal tolerance it would call integral what the relaxation cannot place; far above it a fractional point would be published as an answer. Not swept: every line of `bench/results/miplib.txt` reads `int=0`, so no instance of the set sits near it and a sweep would move nothing |
| `MIP_CLIQUE_ROUNDS` | 4 | Rounds of clique cuts at the root. Measured on the MIP set against none: 0.9697x the work over the 24, `gen` 0.6224x and `p0282` 0.6881x, 22 instances within 0.1% and `mod010` 1.0963x at the root, none past 2x. Not swept beyond on and off |
| `MIP_ZERO_HALF_ROUNDS` | 0 | rounds of zero-half cuts at the root beside the other families: each row with integer coefficients on integer columns and an integer side, alone, in pairs and in triples, is halved with the bound rows that make every coefficient even, and rounded down where the right-hand side is odd and the slacks at the point sum below 1 (Caprara and Fischetti's {0, 1/2}-cuts, in the pairs-and-bounds heuristic). `jaos_set_mip_zero_half_rounds` and `--zero-half-rounds` override it. **Swept at 1, 2 and 4** over the MIP set (02-31): 1.210x, 1.182x, 1.219x, 1 to 4 better, 10 to 12 worse, 2 or 3 past 2x. The cuts are real and the trees they leave are longer: gt2 445 to 1175 nodes at 2.91x, enigma 2814 to 6749 at 2.50x, misc07 35287 to 66603 at 2.02x, against stein45 0.890x; and where none is found the scan is the cost (mod010 1.98x, 0 cuts). Off |
| `MIP_ZERO_HALF_ROW_CAP` | 100 | the tightest candidate rows a zero-half round pairs; pairs are cut off by their slack sum so the cap rarely binds. Not swept |
| `MIP_ZERO_HALF_TRIPLE_CAP` | 40 | the tightest candidate rows a round takes in triples. Not swept |
| `MIP_ZERO_HALF_CUT_CAP` | 50 | the cuts one zero-half round keeps, in enumeration order. Not swept |
| `MIP_FLOW_COVER_ROUNDS` | 0 | rounds of flow cover cuts at the root beside the other families: the two-entry rows `x - u y <= 0` with `y` binary give the variable upper bounds, a row whose columns all have lower bound 0 and whose inflow columns have finite capacities is a single-node flow set, the cover is the greedy prefix by `u (1 - y*) - x*` that exceeds the right-hand side, and the cut is Padberg, Van Roy and Wolsey's with `lambda y` on the outflow columns of capacity above `lambda`. `jaos_set_mip_flow_cover_rounds` and `--flow-cover-rounds` override it. **Swept at 1, 2 and 4** over the MIP set (02-31): 1.012x, 1.017x, 1.017x, none past 2x. Only blend2 and dcmulti carry the structure: blend2 0.986x on 4 cuts, dcmulti 1.493x on 12 with its tree 415 to 687 nodes; the other 22 pay the scan. Off |
| `MIP_FLOW_COVER_CUT_CAP` | 50 | the cuts one flow cover round keeps, in row order. Not swept |
| `MIP_CONFLICTS` | on | whether an infeasible node's Farkas proof is turned into a conflict row: the proof's column combination is priced over the node's bounds, the branching fixings on the path are relaxed to the global bounds one at a time, deepest first, while the proof's gap survives, and the fixings kept, when every one is a binary at a value, give the row `sum(x_j for j fixed at 0) - sum(x_j for j fixed at 1) >= 1 - |fixed at 1|`, added as a permanent row ahead of the node cut copies. Nodes' saved bases are padded with basic slacks for the rows added since they were saved. Skipped where the model has indicator rows or SOS sets, whose node bounds the proof does not see. `jaos_set_mip_conflicts` and `--conflicts` switch it. **Measured on the MIP set of 24** (02-31): 0.924x in work, 7 better, 5 worse, none past 2x: egout 0.206x (6841 to 887 nodes, 253 conflicts), enigma 0.807x, p0033 0.917x, dcmulti 0.923x, misc03 0.947x, against misc07 1.106x (35287 to 27605 nodes but 1563 rows in the relaxation) and l152lav 1.053x; 12 instances see no conflict and read 1.000x. On |
| `MIP_CONFLICT_MAX` | 32 | the most binaries a conflict row may hold; a longer conflict is dropped, since a row that forbids one long path prunes almost nothing. Not swept |
| `MIP_CONFLICT_GAP` | 1e-9 | the gap the proof must keep, relative to (1 + \|the rows' side\|), for a fixing to be dropped or the row to be written; below it the proof is rounding. Not swept |
| `MIP_SYMMETRY` | off | whether the root looks for the model's symmetries on its own; `MIP_ORBITAL` on runs the same search, so the search runs by default and this switch matters only with orbital branching off. The search: the coloured graph of columns, rows and nonzeros, colour refinement to an equitable partition (each vertex's new colour is its old colour and the sorted multiset of its edges' colours and neighbours' colours, re-ranked until the count stops growing), and a partition-backtracking search: the first leaf individualises the smallest vertex of the first non-singleton cell at each level; every other vertex of each of those cells, deepest level first, is individualised in its turn, refined and descended leftmost to a leaf, and the map between the two leaves is checked edge by edge; a map that holds is a generator, and the vertices its orbit already covers are not tried again at that level. `jaos_set_mip_symmetry` and `--symmetry` switch it on its own; the report carries the counts |
| `MIP_SYMMETRY_WORK` | 250 | the work the search may spend, as a multiple of (nonzeros + columns + rows); refinement rounds and automorphism checks count against it, and a search that runs out returns the generators it has. **Read off the MIP set** (02-31, a driver over jm_symmetry_find at caps from 1e4 to 1e8): 12 of the 24 carry a symmetry; the work each needs to finish, in multiples of its size, is stein27 190 (8 generators, one orbit of all 27 columns), rgn 229 (4 generators, 28 orbits), p0201 118, blend2 86, misc07 54, gen 30, enigma and misc03 under 20, misc06 490 (12 generators, 15 orbits) and air03 430 (13 generators, 13 orbits of two, 43M work against a 27M solve). 250 finishes every instance but the last two, which stop with the generators found so far; the instances with no symmetry finish under 20. **Swept at 100 against 250 with orbital branching on** (02-31): 0.943x against 0.835x, because a partial group gives p0201 a poorer orbit (1.807x against 0.412x) and rgn and stein27 half their gain; air03 reads 1.357x at 100 and 1.892x at 250, the search alone on a one-node tree. 250 held |
| `MIP_ORBITAL` | on | orbital branching and fixing (Ostrowski, Linderoth, Rossi and Smriglio): at a node, the generators that fix pointwise every binary the path set to 1 and every non-binary column the path branched on generate a subgroup, and its column orbits come by union-find; a branching on a fractional binary zeroes its whole orbit on the zero side (the one side is the plain `x = 1`), and at node entry every orbit holding a binary the path zeroed is zeroed. Valid because every zero on a path is a union of orbits of a group containing the node's, so the image of a solution under a stabilising symmetry keeps the path. `jaos_set_mip_orbital` and `--orbital` switch it, and on it runs the symmetry search. **Measured on the MIP set of 24** (02-31): 0.835x in work, 5 better, 7 worse, none past 2x: rgn 0.156x (4233 to 235 nodes), misc07 0.356x (27605 to 9063), p0201 0.412x, misc03 0.416x, stein27 0.543x, against enigma 1.224x (1584 to 2139 nodes on 123 widened branchings) and air03 1.892x, which is the search alone. Orbital fixing at node entry fired nowhere on the set, the widened branchings having zeroed the orbits already. On |
| `SYM_MAX_DEPTH` | 64 | the levels of the first leaf's path the search keeps and revisits; a deeper path is descended but not searched below that level. Not swept |
| `SYM_LEAF_CAP` | 64 | the leaves the backtracking under one alternative vertex may visit before that vertex is given up; a leftmost descent alone finds an automorphism only where the choice of vertex commutes with it, which a six-cycle already breaks. Not swept |
| `MIP_CLIQUE_FIX` | off | whether each node fixes by the root's clique table: the table holds the conflicts the all-binary rows put between literals (the pairs the clique cuts enumerate, built once after the root solve rather than every round) plus the implications probing found, and at a node every binary fixed to one setting fixes the literals in conflict with it, cascading, a node holding both sides of a conflict cut without a solve. `jaos_set_mip_clique_fix` and `--clique-fix` switch it on. **Measured on the MIP set of 24** (02-31): 1.026x in work, 6 better (p0201 0.881x, 797 to 675 nodes; rgn 0.892x; l152lav 0.909x; bell3a 0.932x on the same tree) and 9 worse (enigma 1.853x, 2814 to 5837 nodes; misc07 1.202x; p0282 1.097x), none past 2x, and not one node cut by a conflict anywhere, because a branching never fixes both sides of a conflict the relaxation already holds. The fixings move the vertex and the branching with it, the mechanism of D324. With probing feeding the table 1.094x. Off; what could reopen it is a table with cliques the rows do not state, from probing on a set where probing finds conflicts (it found none here), or a use of the table that fixes nothing at the node and only reads the conflicts for the cuts below the root |
| `MIP_CLIQUE_ROW_CAP` | 64 | The largest literals of a row and side that feed the conflict graph; the pairs are enumerated in weight order and stop at the first that does not conflict, so the cap bounds the work per row and reaches nothing on the MIP set, whose rows are shorter. Not swept |
| `MIP_GAP` | 1e-6 | the relative gap that closes the search: stop, and call the answer OPTIMAL, when no open node's bound beats the incumbent by more than `MIP_GAP * (1 + |incumbent|)`, in minimize form. `jaos_set_mip_gap` overrides it; 0 restores it. Not swept: every instance of the set closes with the bound equal to the incumbent |
| `MIP_CUT_ROUNDS` | 1 | rounds of Gomory mixed-integer cuts at the root, one cut per fractional integer column of the relaxation's basis per round; a round that adds nothing ends them, and `jaos_set_mip_cut_rounds` overrides it. **Swept at 0, 1, 2, 3 and 5** over the 17 instances the plain tree of D288 solves, dive off. Work against the plain tree, geometric mean of per-instance ratios: **0.660x at 1** (7 better, 8 worse, none past 2x, worst `p0201` at 1.66x); 0.609x at 2, with three past the gate's own regression factor of 2 (`p0201` 4.08x, `misc03` 3.50x, `rgn` 2.31x); 0.706x at 3, with `misc03` at 13.1x, `p0201` at 10.1x and `pk1` failing numerically at the root; 0.839x at 5 with the dive on, `stein45` no longer finishing. One is the setting with the best mean that keeps every instance under 2x. `bell3a`, which the plain tree does not finish in 40 s, finishes at every setting from 1 |
| `MIP_CUT_AWAY` | 0.01 | a basic integer column is cut only when its fraction sits inside `[AWAY, 1 - AWAY]`: the cut divides by the fraction and by its complement, and a fraction near 0 or 1 gives a cut the relaxation cannot hold to tolerance. The bound Balas, Ceria, Cornuejols and Natraj use (Gomory cuts revisited, 1996). Not swept: held |
| `MIP_CUT_DROP` | 1e-9 | a coefficient below `DROP` times the cut's largest is folded into the right-hand side through its column's bound, which keeps the cut valid and drops the entry; kept when that bound is infinite. Not swept: held |
| `MIP_CUT_SLACK` | 1e-15 | how far a cut's right-hand side is pulled back before the cut is judged and kept, in units of the cut's own span, `1 + \|rhs\| + Σ_j \|a_j\| max(1, \|l_j\|, \|u_j\|)`. A cut is built by summing terms and the sum rounds. A valid cut whose right-hand side rounds up by even one bit throws away the integer points that sat exactly on it, and when those are all the points the model has, the relaxation goes infeasible and the tree reports a feasible model infeasible. Pulling the right-hand side back only ever keeps points, so any slack is valid; what a larger one costs is a weaker cut and a looser bound. Beside it a cut is refused outright when its right-hand side is past the largest its own left side can reach over the current bounds, `Σ_j a_j (a_j > 0 ? u_j : l_j)`, since such a cut admits no point at all; that test is exact and needs no tolerance, but on its own it catches only the cuts that shut out everything. **Measured 2026-09-09 over 20000 generated models** of 4 to 9 integer columns and 1 to 5 rows, each compared against full enumeration and solved three ways: with the reach test alone the default settings lost 5 models to a false infeasible and the bare tree 1; with the slack beside it none of the 20000 fails under the default settings or the bare tree. The one read in full has a Gomory cut whose right-hand side is 5.55e-17 where exact arithmetic gives 0, with every coefficient negative and every column at zero or above, so nothing satisfies it; it takes a Gomory cut plus a cover or a MIR cut to reach that state, and Gomory alone never does. 1e-15 is four times the machine epsilon, the smallest slack that covers one rounding step, and it was chosen over 1e-11 because the larger value costs about 1e-10 of the tree's bound and lost `misc03` of the MIP set to the dual simplex's own settling check |
| `MIP_CUT_DYNAMISM` | 1e6 | the largest ratio of a kept cut's largest to smallest coefficient; a cut past it is not added. Not swept: held. The `pk1` failure at three rounds happened under it, so it is not what protects the root from a bad cut; the rounds count is |
| `MIP_CUT_DEPTH` | 3 | a node whose depth is at most this gets one round of Gomory cuts on its own relaxation, the root being depth 0 and getting `MIP_CUT_ROUNDS`; a node's cuts are read over its bounds, so they hold in its subtree only and are in the copy for exactly the nodes under it. `jaos_set_mip_cut_depth` overrides it. **Swept at 0, 1, 2, 4, 8 and 1000** over the MIP set: 1.056x at 1 (6 better, 10 worse, 3 past 2x), 1.260x at 2, 1.508x at 4, 1.948x at 8 over the 16 that finish, 2.440x at every node. The node counts fall on most instances at every depth (`egout` 39127 to 1803) and the rows carried cost more than the nodes saved (`p0201` 46x at every node), so 0 is the default and a deeper setting is refused as a default. With a slack cut leaving the relaxation: 0.896x at 1 (3 past 2x), 0.802x at 2 with `misc03` alone past 2x at 2.053x, 0.833x at 4 over the 16 that finish; 0 stayed, one instance short. **With four cuts per node: 0.955x at 1, 0.920x at 2, 0.835x at 3, 0.854x at 4 with one past 2x, 0.852x at 6, 0.827x at 8 with one past 2x, 0.877x at every node with three past 2x. 3 is the setting with the best mean that keeps every instance under 2x, and the default** |
| `MIP_NODE_CUT_CAP` | 4 | how many cuts a node below the root may add in its round, the most efficacious kept, violation over the cut's Euclidean norm, the earlier on a tie; 0 is no cap and the root's rounds are never capped. `jaos_set_mip_node_cut_cap` overrides it. **Swept at 0, 2, 4, 8 and 16 at depth 2, and at 3, 4 and 6 at depth 3** over the MIP set on the D300 baseline: at depth 2, 0.935x uncapped, 1.088x at 2, 0.920x at 4, 1.004x at 8, 0.948x at 16; at depth 3, 0.836x at 3, **0.835x at 4**, 0.994x at 6 with two past 2x. Not a smooth lever, and 4 is the setting with the best mean that keeps every instance under 2x at the depth that is the default |
| `MIP_COVER_ROUNDS` | 4 | rounds of knapsack cover cuts at the root beside the Gomory rounds; a round that adds nothing ends both families. `jaos_set_mip_cover_rounds` overrides it. **Swept at 0, 1, 2, 3, 4, 5 and 8** beside the default Gomory round, at 1 and 2 with the Gomory round off, and at 3 with two Gomory rounds, over the MIP set: 1.001x, 0.961x, 0.749x, **0.745x**, 0.767x, 0.804x against one Gomory round alone; covers alone 1.052x and 0.912x against the plain tree; two Gomory rounds with three covers 0.778x with two instances past 2x. Four is the setting with the best mean that keeps every instance under 2x, and the only one of these that moves a default |
| `MIP_CUT_STALL` | 0.0 | a root round that moves the bound by less than this times (1 + \|bound\|) is the last; 0 ends the rounds only when one adds nothing. `jaos_set_mip_cut_stall` overrides it. **Swept at 1e-4, 1e-3 and 1e-2** over the MIP set of 24: 1.007x, 1.172x and 1.121x against no stall, the last two with an instance past 2x; every round the stall removed was worth its solve, so 0 stays and the stall is refused as a default |
| `MIP_NODE_CUT_STALL` | 0.0 | a node whose round moves its bound by less than this times (1 + \|bound\|) gets no round under it, the root's whole phase judged the same way; 0 never switches a subtree off. `jaos_set_mip_node_cut_stall` overrides it. **Swept at 1e-3, 1e-2, 2e-2, 5e-2 and 1e-1** alone, and at the last four beside the root-cut drop: alone 0.993x over 23, 0.833x, **0.816x**, 0.832x and 0.897x with one past 2x; with the drop 0.832x, 0.782x, 0.773x and 0.768x over 23, each with `bell5` at the cap and `enigma` past 2x. It meets the bar alone and never beside the drop, which reads better and finishes every instance, so 0 stays |
| `MIP_MIR_ROUNDS` | 6 | rounds of mixed-integer rounding cuts on the model's rows at the root, beside the Gomory and cover rounds; a round that adds nothing ends every family. `jaos_set_mip_mir_rounds` overrides it. **Swept at 1, 2, 3, 4, 5, 6, 8 and 12** over the MIP set of 24 on the D306 defaults: 1.017x, 0.933x, 0.790x, 0.752x, 0.773x, **0.719x**, 0.725x and 0.737x against none, every setting from two down with no instance past 2x; six is the setting with the best mean and the curve is flat past it |
| `MIP_MIR_DELTAS` | 8 | how many scalings a row's MIR cut tries beyond 1: the \|a_j\| of the integer columns whose shifted value is fractional, in column order, deduplicated by exact equality. Decides which candidates are tried, never a number in an answer; the most violated wins. Not swept: held, since Marchand and Wolsey's own list is this set plus the divisors of the best, and a longer list only adds candidates the efficacy rule can reject |
| `MIP_MIR_ROUND` | 1e-9 | the rounding a MIR side's shifted right-hand side may carry and still be cut: `DBL_EPSILON` times the sum of the terms' magnitudes times the term count, in the row's own units, against this. The cut divides by the right-hand side's fraction and by its complement, so a computed fraction below the true one is a cut that is not valid, and a side whose sum cannot be placed to this gets no cut (the review's case: a column whose one finite bound is 1e15 puts the fraction on a multiple of 1/8 whatever the data). 1e-9 bounds the coefficient error at 1e-7 after the 1 / (1 - f0) factor that `MIP_CUT_AWAY` caps at 100, the primal tolerance's own scale. Not swept: held. The same exposure exists in the Gomory cut's basic value and is carried, since that value comes out of a solve and not a sum here |
| `MIP_MIR_AGGREGATE` | 0 | how many continuous columns a MIR aggregate may substitute out with another row before it is rounded; 0 is D309's single-row form. Each step takes the column with the largest coefficient in the aggregate that sits away from both its bounds and has not been picked already, the lowest index on a tie, and the lowest-indexed other row that holds it with a coefficient worth pivoting on and a finite bound on the side the multiplier's sign needs; the aggregate is rounded after every step. `jaos_set_mip_mir_aggregate` overrides it. **Swept at 1, 2, 3 and 6** over the MIP set of 24: 1.185x, 1.165x, **1.140x** and 1.192x against the single-row round, every arm leaving `bell5` at the 240 s cap and every arm with `gen` past 2x (8.60x at two steps), 1.030x over the 17 against 1.523x over the seven. The cuts are real, `egout` reading 0.564x with its tree halved, and cost more per round than they save, so 0 stays |
| `MIP_MIR_LAMBDA` | 1e6 | the largest multiplier an aggregation step may use, and 1 / this the smallest: the step forms `agg - lambda row_r`, so a multiplier far from 1 makes the aggregate a difference of numbers of very different size and the coefficients carry a rounding the cut's own map then multiplies by up to 1 / `MIP_CUT_AWAY`. Not swept: held, at `MIP_CUT_DYNAMISM`'s own bound on a kept cut's coefficient spread, and the per-coefficient magnitude test against `MIP_MIR_ROUND` is what actually refuses a side |
| `MIP_DIVE_HEURISTIC_DEPTH` | 0 | the deepest node the dive heuristic runs at, the root being 0; every node at this depth or above gets its own dive on its own relaxation, and 0 is the root alone, D313's form. `jaos_set_mip_dive_heuristic_depth` overrides it; nothing happens with the dive heuristic off. **Swept at 1, 2 and 4** over the MIP set of 24: 1.049x, 1.144x with two instances past 2x, 1.356x with four past 2x and `khb05250` at 2.846x on a tree that did not move. No node count moves at any depth, so it is judged on the first incumbent like D290 and D313: earlier on 4, 5 and 8 instances and later on none (`bell3a` node 230 to 3). Refused as a default because the rate is worse than what is already on: D290 moved 8 of 17 for 1.8%, D313 6 of 24 for 3.2%, depth 1 four more of 24 for 4.9% |
| `MIP_RINS` | 0 | how many relaxations a RINS dive may solve at a node: the integer columns the incumbent and the node's relaxation already place at the same integer are fixed there and D313's dive runs on what is left, once per distinct incumbent. 0, the default, is off. `jaos_set_mip_rins` overrides it. **Swept at 10, 50 and 200** over the MIP set of 24: 1.007x, 1.008x and 1.008x, none past 2x, every instance finished, and 50 and 200 byte-identical because a neighbourhood with most columns fixed ends in few solves. Refused as a default on the other column: it found a point on one instance of the 24 and moved no first incumbent, since D313's root dive reaches them first |
| `MIP_FEASPUMP` | 20 | how many rounds the feasibility pump may run at the root: each round rounds the point it holds and re-solves the copy for the point of the relaxation nearest that rounding in L1. It runs only while nothing has an answer yet, since this is the plain pump and looks for a feasible point rather than a good one. 0 is off. `jaos_set_mip_feaspump` overrides it. **Swept at 1, 3, 5, 20, 50 and 100** over the MIP set of 24: 1.006x, 1.011x, 1.014x, **1.026x**, 1.037x and 1.053x, with the first incumbent moving to node 1 on 0, 2, 4, 6, 6 and 6 instances and later on none, every instance finished and none past 2x. No node count moves at any setting, so it is judged on the first incumbent like D290 and D313; 20 is the setting with the best mean among those that reach every instance a larger one reaches, and 50 and 100 say the curve is flat past it |
| `MIP_PUMP_FLIPS` | 10 | how many integer columns a stalled pump moves to the other side: a rounding that comes back unchanged would repeat for ever, so the columns whose relaxation value sits furthest from the rounding are flipped, the lowest index breaking a tie. Fischetti, Glover and Lodi draw this count at random, which would break D8's bit-identical results, so it is fixed and the choice inside it is a total order. Not swept: held. It moves which points the pump visits and no number in an answer, and `tests/test_mip.c` carries a model whose rounding repeats so the path is executed |
| `MIP_PUMP_OBJ` | 0.5 | the objective pump's decay: each of the pump's rounds minimizes `(1 - a)` times the distance plus `a` times the model's own objective, the two scaled to comparable Euclidean norms, and `a` multiplies by this each round from 1, so the blend fades to the plain distance. 0 is the plain pump. `jaos_set_mip_pump_obj` overrides it. **Swept at 0.3, 0.5, 0.7 and 0.9** over the MIP set of 24: 0.987x, **0.984x**, 0.985x and 0.985x the work of the plain pump, 2 better and 0 worse at every decay, none past 2x, no node count moved; at 0.3 and 0.5 the first incumbent moves to node 1 on `gt2` (from 382) and later on none, at 0.7 and 0.9 `lseu`'s moves from node 1 to 47. 0.5 is the best mean and the largest decay that loses nothing; `dcmulti` 0.908x and `misc03` 0.786x are the two better, on trees that did not change, because the pump's own re-solves cost less when the objective moves less between rounds |
| `MIP_RCFIX_SLACK` | 1e-6 | the slack a reduced-cost fixing adds before it rounds down. Both terms of the quotient are known to the simplex's own tolerance, and adding slack before the floor only ever loosens the new bound, so the deduction stays valid whatever the slack is; what a larger one costs is a bound that is looser than it could be. Not swept: held, at the primal tolerance's own scale |
| `MIP_PROPAGATE` | 0 | how many passes of bound propagation a node makes before its relaxation is solved; 0 is off, and a pass that moves nothing ends the rounds. Each pass reads the model's own rows over the node's bounds, proves the node infeasible where a row's smallest activity is already above its upper bound, and pulls in the integer bounds the rows imply. `jaos_set_mip_propagate` overrides it. **Swept at 1, 2 and 4 passes at every node, and at 2 and 4 at the root alone** over the MIP set of 24: at every node **1.093x, 1.104x and 1.074x** with `bell5` unfinished at the cap in all three; at the root alone **1.051x** at both pass counts, the two arms byte-identical because the root's first pass finds everything the later ones would. `l152lav` 0.549x and `p0282` 0.726x against `gt2` 5.065x and `bell3a` 2.018x. Off at every setting. **Re-measured at the root alone with the clique cuts in** (02-31): 1.072x, 18 of 24 byte-identical, `bell3a` 2.531x and `p0201` 1.876x, none better; still off |
| `MIP_QUAD_PROPAGATE` | 4 | what `MIP_PROPAGATE` reads instead of 0 when the objective has a quadratic term. The barrier needs a strictly feasible point, and a node with an equality row plus branching bounds often has none: the primal residual falls to 1e-11, the dual iterate runs to 1e+7, `BARRIER_DIVERGE` fires and the node comes back `NUMERICAL_ERROR`. Propagation fixes the columns the rows have already forced, and the barrier takes a fixed column in its stride. `jaos_set_mip_propagate` overrides it, 0 included. **Measured 2026-09-09 over 400 generated cardinality models** (5 to 9 binary columns, equal or near-equal costs, one or two equality rows through a random integer point, `q` drawn from 2 to 12), each compared against full enumeration: **28 of 400 ended `NUMERICAL_ERROR` with propagation off, 7 at 1 pass, 3 at 4 and 3 at 16**, and not one arm ever answered wrongly. Four is where it stops paying. On 1200 generated models with one-sided rows and 600 with equalities the default answers all of them, so the passes cost nothing there. A node solve for a quadratic objective is a cold barrier run, far dearer than the warm simplex re-solve of a linear node, which is why the linear default stays 0 |
| `MIP_PROPAGATE_DEPTH` | -1 | the deepest node bound propagation runs at, the root being 0; negative is every node. The root's deductions are made over the model's own bounds, so they hold for every integer point of the model and the tree keeps them in its own `ilo` and `ihi` for nothing; a deeper node's are read over that node's bounds and are rebuilt at every node, which is what a positive depth pays for. `jaos_set_mip_propagate_depth` overrides it; nothing happens with `MIP_PROPAGATE` at 0. **Swept at 0 and 1** beside four passes over the MIP set of 24: the root alone reads 1.051x and one level down 1.080x with `bell5` lost at the cap, against 1.074x for every node. The root alone moves a bound on 6 of the 24 and the other 18 read **exactly 1.000x**, so a pass that finds nothing is free; what costs is `bell3a` at 2.531x with its tree 64077 to 117317 nodes off 16 moved bounds. Off with the feature |
| `MIP_PROP_SLACK` | 1e-9 | the slack a propagated bound keeps before it is rounded, the same argument as `MIP_RCFIX_SLACK`: a row's implied bound is a quotient of a difference of sums, so it is known to the size of those sums, and loosening before the floor keeps every integer point the row admits. Not swept: held |
| `MIP_PROP_INFEAS` | 1e-7 | how far a row's implied activity must sit outside its own bound before propagation calls the node infeasible with no relaxation solved, relative to (1 + \|the bound\| + \|the activity\|). Two orders above `MIP_PROP_SLACK` on purpose: a bound rounded too loosely costs strength, while a node pruned wrongly here is an optimum thrown away. Not swept: held |
| `MIP_PROP_MOVE` | 0.5 | how far a propagated bound must move a column before it is taken, in the column's own units. Only integer columns are pulled in, so a real move is a whole integer and half of one is the natural floor; it is what stops a bound that moved by rounding alone from churning the relaxation. Not swept: held |
| `MIP_TIGHTEN` | on | whether the root tightens coefficients before the tree: in a one-sided row, a binary column whose coefficient cannot make the row tight on its own, because the row's slack with that column at 0 is positive, has the coefficient shrunk by that slack and, for a positive coefficient in a `<=` row or a negative one in a `>=` row, the bound with it; the other sign keeps the bound. No integer point moves and the relaxation gains a face (Savelsbergh's coefficient improvement, on the tree's own copy, so the published rows are the model's). `jaos_set_mip_tighten` and `--no-tighten` override it. **Measured on the MIP set of 24** (02-31): it fires on 2, gen 1.001x and p0282 0.996x with its tree 5159 to 4997 nodes, 22 byte-identical, 0.9999x over the set, for one pass over the rows. Kept on as a reduction that costs nothing measurable and takes nothing away; the knapsack of the unit suite goes from a cover cut at the root to an integral root |
| `MIP_TIGHTEN_MIN` | 1e-9 | the slack a row must show, relative to (1 + \|its bound\|), before its coefficient is tightened at all, so a slack that is rounding alone changes nothing. Not swept: held |
| `MIP_PROBING` | off | whether the root probes the binary columns fractional at its relaxation, after the root solve and before the cuts: each is tried at 0 and at 1 with the rows propagated over the bounds for `MIP_PROBING_ROUNDS` rounds, most fractional first, under `MIP_PROBING_CAP`; a setting that makes some row impossible fixes the column the other way, the bounds both settings imply are kept as the tree's bounds, a column that fits neither way makes the model infeasible, and a root that moved is solved again. `jaos_set_mip_probing` and `--probing` switch it on. **Measured on the MIP set of 24** (02-31): the first form, every binary before the root solve with no cap, 1.566x with 5 past 2x (air03 136x, 10757 binaries against 91k nonzeros; misc07 35287 to 95949 nodes). This form, 1.109x with 0 better and bell3a 2.535x, at every cap from 0.5x to none. No probe fixes a column on any of the 24. What the probes find are bounds, and a bound written back moves the branching (bell3a 16 bounds, 64077 to 117317 nodes; p0201 6 bounds, 797 to 1369, 1.875x; gen 42 bounds, 1.514x on 5 nodes), the same mechanism D324 saw; where they find nothing the probe itself is the cost (air03 1.461x, 35 columns at 0.5x the root's work; mod010 1.124x). Off; what could reopen it is an instance set where a probe fixes a column, or the implications fed to the clique cuts instead of written as bounds |
| `MIP_PROBING_ROUNDS` | 2 | the propagation rounds each probe makes; one reaches a row's own neighbours, two a chain through them. Not swept: held |
| `MIP_PROBING_CAP` | 1.0 | the work the root's probing may spend, as a multiple of the work the root solve itself took; 0 is no cap. `jaos_set_mip_probing_cap` and `--probing-cap` override it. **Swept at 0.5, 1, 2 and 0** over the MIP set with probing on (02-31): 1.108x, 1.109x, 1.109x, 1.109x, one tree at every cap on 21 of the 24, because a probe costs two propagations over the rows and most instances probe every fractional binary inside 0.5x the root's work (air03 35 columns at 0.52x, l152lav 55 at 0.34x); the cap binds on p0201, egout, gen and p0282 only. 1 held as the default since no cap reads differently |
| `MIP_DIVE_BACKTRACK` | 0 | how many times a dive may resume from the deepest sibling it left on its stack once a node ends; 0 sends every sibling to the open set at once, D289's dive. `jaos_set_mip_dive_backtrack` overrides it; nothing happens with the dive off. **Swept at 0, 1, 2, 4, 16 and unbounded** with the nearer child and at 4 and unbounded with the pseudocost side, dive on, over the MIP set: 0.835x over 23 with `bell5` at the cap and two past 2x at 0, then 1.026x, 1.134x, 1.007x, 0.992x over all 24 with two past 2x, 1.170x with five past 2x; the pseudocost side 1.011x and 1.142x. No setting is under the bar, so 0 stays and the dive stays off |
| `MIP_DIVE_GAP` | 0.0 | how far a waiting sibling's bound may sit above the best open node's, as a fraction of (1 + \|best\|), for the dive to resume from it; 0 puts no bound on the resume. `jaos_set_mip_dive_gap` overrides it; nothing happens with the dive off. **Swept at 1e-4, 1e-3, 1e-2, 1e-1 and 1**, with no resume count, and at 1e-2 with a count of four, over the MIP set: 1.067x, 1.085x, 1.134x, 1.334x, 1.355x and 1.080x against the control, every one above 1.0x, so 0 stays and the dive stays off. The rule's first form compared the sibling against the heap alone, which a dive empties, and read one number at every fraction; the comparison spans the heap and the dive's stack now, and `tests/test_mip.c` fails if it stops deciding |
| `MIP_DIVE_HEURISTIC` | 50 | how many relaxations the root's dive heuristic may solve: on a copy of the root's relaxation as the cuts left it, fix the integer column nearest an integer there and solve again, up to this many times, and judge an integral point by `rounded_point` like every other heuristic point. 0 is off. `jaos_set_mip_dive_heuristic` overrides it. **Swept at 10, 50 and 200** over the MIP set of 24: 1.015x, **1.032x** and 1.036x the work, none past 2x and every instance finished at every setting, with the first incumbent moving earlier on 1, 6 and 7 instances and later on none. Judged on the first incumbent by D290's rule, since a heuristic cannot shrink a best-bound tree and no node count moves at any setting; 50 reaches six of the seven the largest setting reaches at nearly the smallest setting's work |
| `MIP_DIVE_DEGRADE` | 0.0 | how far a node's own bound may fall away from its parent's, as a fraction of (1 + \|parent\|), for the dive to go on into one of its children; 0 puts no bound on it, D289's form. `jaos_set_mip_dive_degrade` overrides it; nothing happens with the dive off. It reads the bound the branch itself reached, snapshotted before the node's own cut round, so the fraction means the same thing at every `MIP_CUT_DEPTH`. **Swept at 1e-3, 1e-2 and 1e-1** against the plain dive over the MIP set: 1.097x, 1.081x and 1.048x -- every fraction costs more than no bound, with `bell5` unfinished in every arm and in the plain dive too, so 0 stays and the dive stays off |
| `MIP_PC_EPS` | 1e-6 | the floor of a direction's pseudocost score: the score is the product of the two directions' expected gains, and a direction whose gain was 0 would otherwise zero the column out of the choice. Achterberg, Koch and Martin (Branching rules revisited, 2005) use the same floor. Decides an order between columns, never a number in an answer. Not swept: held |
| `MIP_RELIABILITY` | 0 | branches per direction before a column's pseudocost is trusted; below it the column's children are solved on the spot and the gains initialise the pseudocosts. `jaos_set_mip_reliability` overrides it. **Swept at 0, 1, 2, 4 and 8** over the MIP set, pseudocost branching, everything else at its default (`bench/measurements/02-192/`): work against 0 reads 0.971x at 1 (7 better, 9 worse, `mod010` 2.84x and `enigma` 2.07x past the gate's factor), 1.064x at 2, 1.173x at 4 and 1.437x at 8, while the node counts fall at every setting (`dcmulti` 585 to 135 at 4, `mod010` 7 to 3). The probes are worth their information and not their price: each is a full child solve. 0 is the default and the setting is refused as a default; `bench/refusals.txt` carries what reopens it |
| `MIP_STRONG_CANDIDATES` | 8 | how many unreliable columns a node probes, the best by pseudocost score. Not swept: held, since no setting of the count above was worth its work, and a cap on the candidates only lowers the price of a thing that did not pay at any price measured |
| `MIP_PROBE_CAP` | 0.0 | the work cap on each strong-branching probe as a multiple of the work the node's own relaxation took; a probe that reaches it stops and teaches nothing; 0 is no cap. `jaos_set_mip_probe_cap` overrides it. **Swept at 0, 0.5, 1 and 2** at reliability 1 and at 2 over the MIP set: at reliability 1, 0.971x uncapped, 1.228x at 0.5, 0.998x at 1, 1.018x at 2; at reliability 2, 1.064x, 1.307x, 1.081x, 1.091x. Every capped arm has an instance past 2x (`mod010` 4.51x at 1 with its tree unchanged at 7 nodes). A probe that reaches the cap pays its work and teaches nothing, so no cap is the default and the cap is refused as a default |
| `MIP_PROBE_DEPTH` | -1 | the deepest node at which strong branching probes, the root being 0; negative is every depth. `jaos_set_mip_probe_depth` overrides it. **Swept at 0 with reliability 1, 2, 4 and 8, and at 1 and 2 with reliability 4** over the MIP set: 0.987x at the root only, one reading at every reliability because every root candidate is unreliable (9 better, 5 worse, `mod010` 2.84x and `enigma` 2.58x past 2x), 1.010x one level down, 1.024x two. Refused as a default with D293; every depth stays the default |

