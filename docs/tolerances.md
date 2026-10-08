# Tolerances

Every constant in `src/` that a solve compares against, caps or sizes: its
value, what it controls, why it has that value, and the folder under
`bench/measurements/` that holds the measurement. A change to any of them
goes here with its measurement. The weights of the work units are in
`docs/work-units.md`.

The constants live in three spaces. The solver runs on a scaled copy of the
model (`docs/scaling.md`), so its tolerances are magnitudes in scaled space.
Presolve runs on the model as loaded, before the scaling exists, so its
constants are in the caller's units. The checker also runs on the model as
loaded, with a tolerance the caller passes. None of the three is converted
into another.

The sets named below are the Netlib standard 94, the infeasible 29, the
Kennington 16, MIPLIB 3 (24 instances), the 2017 set (30 MIPLIB 2017
instances), Maros-Meszaros, QPLIB and CBLIB. Work is in work units. A ratio
is the geometric mean of per-instance work against the setting in use,
unless the row says otherwise. The gate fails an instance past 2x its
baseline work, so a setting that does that is not taken as a default.

## The solver's tolerances

Defined in `src/simplex.c`, `src/lu.c`, `src/chol.c`, `src/check.c`,
`src/scale.c`, `src/barrier.c`, `src/pdlp.c` and `src/concurrent.c`.
`LU_PIVOT_TOL` is in `src/jaos_internal.h`, because ranging, the barrier's
crash basis and the crossover's push factor their bases with the same
threshold.

| Name | Value | What it decides |
|---|---|---|
| `PRIMAL_TOL` | 1e-7 | How far a basic variable may sit outside its bound before it counts as violated, so it decides which rows the dual simplex repairs and when it stops. The bound-flipping ratio test reads it as `primal_tol * (1 + the reach the flips absorbed)`, because its remainder is a difference of sums as large as the flips (`tests/test_simplex.c`). On a model whose data spans far more decades than a double holds, a breach can sit under this in one column's units and be large in a row's (`bench/measurements/02-244/`) |
| `DUAL_TOL` | 1e-9 | The width of the dual's Harris window, and the size below which a reduced cost counts as zero, so it decides when the solve stops. At 1e-7 four Netlib instances publish a point that is not the optimum and 1e-8 still leaves `pilot` wrong; 1e-10 fails `dfl001`. 1e-9 costs 1.03x the work of 1e-7 on Netlib and 1.10x on Kennington (`bench/measurements/02-84/`, `bench/measurements/02-96/`) |
| `PIVOT_MIN` | 1e-9 | Smallest \|alpha\| the ratio test accepts as a pivot. A stability floor: `pivot` and `theta_dual` divide by it. Whether a pivot is rounding noise is `PIVOT_MARGIN`'s question |
| `PIVOT_MARGIN` | 1.0 | The noise floor, in ulps of a quantity's own terms: on an entry of `B^-1 M_q` against its column's largest entry in the two primal ratio tests, and on the pricing row's `alpha[q]` against `sum_i \|rho_i * a_iq\|`. Below it a candidate leaves the list. Relative, because an absolute floor is too strict or too lax by the column's scale. In the primal ratio tests it decides nothing below 3.3e-6 and reaches `wood1p` above 5.49; on `alpha[q]` it sits in a window from 0.35 to 20740 (`bench/measurements/02-122/`, `bench/measurements/02-124/`) |
| `PRIMAL_HARRIS_DELTA` | 0.5 | The Harris window of the two primal ratio tests, as a multiple of `s->primal_tol`, so it scales with `jaos_set_primal_tolerance`. It must stay at or below `primal_tol` for phase 1 to stay correct (`docs/research/harris-primal.md`); an `assert` in `primal_pick` checks it. 0.01 to 0.5 agree with the dual on the same 61 instances, one more than 1.0, and 0.5 is a power of two, so the product is exact (`bench/measurements/02-127/`) |
| `PHASE1_RISE_MAX` | 1.0 | How far the primal phase 1's total infeasibility may rise above its running minimum, as a fraction of it, before the solve ends `NUMERICAL_ERROR`. The sum cannot rise under an exact pivot; a rise means a near-singular basis. The largest rise on a good forced-primal solve is 4.3e-8 and `pilot87`'s is 8.1e11, and every value from 1e-5 to 1e2 stops `pilot87` at the same iteration and nothing else. Read with Curtis-Reid scaling; `JM_SCALE_NONE` is reached only by unit tests (`bench/measurements/02-133/`) |
| `LU_PIVOT_TOL` | 0.1 | Markowitz threshold: a pivot must be at least this fraction of the largest magnitude in its column |
| `LU_UPDATE_TOL` | 1e-9 | Floor on the new diagonal in a Forrest-Tomlin update, relative to the spike's largest magnitude. After elimination a legitimate pivot can be orders below the spike, so it is far looser than the Markowitz threshold |
| `FTRAN_HYPER_DEN` | 10 | FTRAN solves only the slots its right-hand side reaches when the predicted density is below 1/this, with bit-identical answers. Work units bill the reach walk, so it was set on instructions: of 5, 10, 20 and 40, 10 reads the fewest, 0.951x the full pass (`bench/measurements/02-31/`) |
| `FTRAN_DENSITY_KEEP` | 0.9 | The weight the previous density prediction keeps against the last solve of its kind. Not swept |
| `LU_AGREE_TOL` | 1e-5 | How far the BTRAN and FTRAN values of the pivot element may disagree before the factorization is rebuilt; they are one number in exact arithmetic. On the gates the worst is 7.8e-8, and one pivot is the first past every value from 1e-7 to 1e-3, so any value in that range acts the same |
| `IMPLIED_ROUNDS` | 64 | Cap on the checker's bound-propagation rounds, a safety stop. On the standard set the certified answers stop rising at 64 and the cost is flat |
| `DROP_REL` | 1e-14 | A value below this fraction of the basis matrix's largest magnitude is structurally absent. Relative, so a uniformly small basis is not called singular |
| `EXP_LIMIT` | 20 | The largest scale-factor exponent, `2^±20`, for Curtis-Reid and geometric scaling. Without it `dtoc3`'s factors passed `2^84` and the barrier published a wrong `OPTIMAL`; no Netlib factor reaches `2^20` (`bench/measurements/02-250/`) |
| `TINY` | 1e-300 | The same floor where no scale is available |
| `CHOL_PIVOT_REL` | 1e-14 | A Cholesky pivot at or below this fraction of its row's input diagonal is called zero and replaced by `CHOL_PIVOT_HUGE` (`TINY` when the diagonal is zero). Relative to the row's own diagonal, since `A D A^T` spans many decades late in a barrier run. A draft, set by `tests/test_chol.c` |
| `CHOL_PIVOT_HUGE` | 1e128 | What a replaced pivot becomes, so the solve gives that row a component near zero, the usual barrier treatment of a dependent row. Its square stays finite |
| `CHOL_DENSE`, `CHOL_DENSE_MIN` | 10, 16 | When asked (`jm_chol_symbolic_dense`, used by the conic solver), a node with more than `max(16, 10 sqrt(n))` neighbours is ordered last, AMD's rule and values. It keeps a large cone's ordering from going quadratic (`bench/measurements/02-254/`). Not swept |
| `CHOL_ND_TRY` | 100 | When the minimum-degree factor costs more than this many operations per input nonzero, the symbolic Cholesky also tries nested dissection and keeps the cheaper. Dissection wins only on the Maros-Meszaros grids, from 137 operations per nonzero up, so 100 sits under the smallest winner (`bench/measurements/02-344/`). Not swept |
| `CHOL_ND_MIN` | 1000 | The fewest rows for nested dissection to be tried; `aug3d`'s 1000 is the smallest that gains. Not swept |
| `CHOL_ND_LEAF` | 200 | The part size at which dissection hands over to minimum degree. Not swept |
| `CHOL_BLOCK` | 32 | Rows per block of the threaded numeric Cholesky; the factor is bit-identical at any thread count and block size. Of 16, 32, 64 and 128 on dfl001, 32 and 64 tie within noise (`bench/measurements/02-288/`) |
| `CHOL_BLOCK_WORK` | 1e6 | The eliminations a block needs before its rows go to threads. 1e5 and 1e6 tie within noise; 0 starts threads for blocks too light to pay (`bench/measurements/02-288/`) |
| `CHOL_THREADS_MAX` | 64 | The most threads one factor runs on; it sizes the thread handle arrays. Not swept |
| `BARRIER_TOL` | 1e-8 | Where the barrier stops: relative primal and dual residuals and relative gap, in scaled space, all at or below this. 1e-6 to 1e-10 all agree with the dual on 19 hard Netlib instances, and the crossover gives the checked point, so it stays at the customary 1e-8 |
| `BARRIER_STEP` | 0.99995 | The fraction of the step to the boundary taken after Mehrotra's corrector. From 0.9 up to Mehrotra's 0.99995, agreement and checker acceptance rise and work falls |
| `BARRIER_REG` | 1e-9 | Primal regularisation on each bounded variable's `Θ^{-1}`, scaled by the worst relative measure capped at 1, so it fades as the run converges. Of the values from off to 1e-6, only 1e-9 agrees with the dual on all 19 hard instances |
| `BARRIER_FREE_REG` | 1e-8 | The same term for a free variable, not scaled down. The instances with free columns do not separate 1e-6 to 1e-10, so it is held at the middle |
| `BARRIER_DELTA` | 1e-10 | Dual regularisation on the diagonal of `A Θ A^T`, so a dependent row gives a pivot of `delta`. On the pilot family and its neighbours only 1e-10 solves all eleven: `greenbea` stalls below it, `pilot-we` and `pilotnov` fail at 1e-8 |
| `BARRIER_START_MIN` | 1e-6 | The floor under Mehrotra's starting shifts; below it they become 1. When the costs lie in the range of `A^T` the shifts collapse and the walk never recovers; the smallest nonzero shift on the LP sets is 1.8e-4 |
| `BARRIER_MAX_ITER` | 200 | Iterations after which the barrier hands the model to the dual simplex. The longest converging run on the standard 94 takes 47. Not swept |
| `CROSS_PUSH` | on | The crossover pushes its basis guess to a vertex before the simplex (the primal half of Bixby and Saltzman's push). On the standard 94 it takes the agreed count from 77 to 80 for less work (`bench/measurements/02-287/`) |
| `CROSS_PUSH_PRIMAL` | on | The primal simplex finishes from a pushed basis, which is primal feasible. Finishing with the dual reads worse than no push at all (`bench/measurements/02-287/`) |
| `CROSS_PUSH_SNAP` | 1e-9 | How near a bound, relative to (1 + \|bound\|), a nonbasic column must sit to be put on it before the push; the barrier's own accuracy. Not swept |
| `CROSS_PUSH_PIVOT` | 1e-7 | The smallest direction entry, relative to the largest, the push's ratio test reads. Not swept |
| `CROSS_PUSH_FEAS` | 1e-9 | The slack, relative to (1 + \|bound\|), the first pass of the push's Harris ratio test allows. Not swept |
| `CROSS_PUSH_UPDATE_TOL` | 1e-9 | The smallest pivot ratio the push accepts in an LU update before it refactors. Not swept |
| `BARRIER_DIVERGE` | 1e6 | The multiple of the data past which an iterate that made no progress is divergent, and the model goes to the dual simplex for a verdict. A converging run reaches at most 3.8e4 on the standard 94 and infeasible 29, and 18 of 19 infeasible runs pass 1e6; 1e8 and 1e10 delay or lose those verdicts (`bench/measurements/02-220/`) |
| `BARRIER_DENSE_FACTOR` | 10 | A column with more nonzeros than this times the average leaves the normal matrix and returns through a Sherman-Morrison-Woodbury correction. 20 reads the same; 5 makes the barrier worse where it newly fires (`bench/measurements/02-223/`) |
| `BARRIER_AUG_FLOOR` | 1e-30 | The smallest pivot magnitude the augmented system's quasi-definite LDL keeps; a smaller or wrong-signed pivot is replaced with the block's sign and counted. A floor against division by zero, far under `BARRIER_DELTA`. Not swept |
| `BARRIER_DENSE_MIN` | 30 | The count a column must exceed to be dense at all. Not swept |
| `BARRIER_DENSE_MAX` | 100 | Above this many dense columns none is left out, since each costs a solve per factorization. Not swept |
| `BARRIER_AUG_TRY` | 1e8 | For a diagonal quadratic objective, the normal factor's operation count above which the barrier also builds the augmented system and keeps the cheaper. With no floor the second analysis costs boyd1 85x its solve; 1e7 and 1e9 read the same (`bench/measurements/02-272/`) |
| `BARRIER_AUG_EDGE` | 1.0 | How much cheaper the augmented factor must be to be taken. 0.5 and 2.0 read the same |
| `BARRIER_DIVERGE_QP` | 1e10 | `BARRIER_DIVERGE` for a QP, which has no simplex to fall back on. Five of seven Maros-Meszaros models the LP limit stopped solve from 1e8, and huestis at 1e10; on LPs that limit made two infeasible runs overrun before the handoff (`bench/measurements/02-250/`) |
| `BARRIER_TOL_QP` | 1e-10 | Where a QP walk's second leg stops when the push does not settle from the `BARRIER_TOL` point. It lets liswet10 and liswet11 pass; stopping every QP there would lose qsierra and qgrow22 and cost 13% more work (`bench/measurements/02-250/`) |
| `BARRIER_LEG2_ITERS` | 50 | The most iterations of the second leg; the models it wins need 7. Not swept |
| `BARRIER_STALL_ITERS` | 5 | Iterations without progress after which a QP walk is stalled and takes equal step lengths with sigma floored. Of 3, 5 and 10 on 3000 generated QPs, 3 fires on healthy walks and 10 reads like 5 (`bench/measurements/02-249/`) |
| `BARRIER_STALL_DROP` | 0.9 | The factor the worst residual must fall by to count as progress. Not swept |
| `BARRIER_STALL_SIGMA` | 0.5 | The floor under sigma once stalled. 0.3 and 0.5 solve all five stalled models; 0.5 keeps a margin over 0.2, which fails one (`bench/measurements/02-249/`) |
| `BARRIER_STALL_DELTA` | 1e-3 | What `BARRIER_DELTA` is multiplied by once a QP walk stalls on its primal residual, undone at the first replaced pivot. From 1e-3 down five of ten Maros-Meszaros models reach `OPTIMAL`; the LP path never reads it (`bench/measurements/02-250/`) |
| `BARRIER_NEAR_TOL` | 1e-6 | How close a QP walk at `BARRIER_MAX_ITER` must be for the push to be tried before the handoff. 1e-6 to 1e-4 win the same model, so the tightest stays (`bench/measurements/02-250/`) |
| `BARRIER_MU_DEAD` | 1e-30 | The complementarity under which a QP walk within `BARRIER_NEAR_TOL` is pushed early, since the walk then gains only a fixed fraction per step. On Maros-Meszaros it fires on six instances, five for less work (`bench/measurements/02-295/`). Not swept |
| `BARRIER_REG_RETRY` | 1e-8 | The primal regularisation the augmented system is refactored with after a pivot was replaced at `BARRIER_AUG_FLOOR`, which grows by `BARRIER_REG_GROWTH` up to `BARRIER_REG_MAX` (`bench/measurements/02-249/`). Not swept on its own |
| `BARRIER_REG_GROWTH` | 100 | The growth factor of that floor. Not swept |
| `BARRIER_REG_MAX` | 1e-2 | Where the floor stops growing. qgrow22 needs 1e-2; the generated QPs read the same from 1e-6 (`bench/measurements/02-250/`) |
| `QP_PUSH_REG` | 1e-6 | The proximal term on a free variable in the push that finishes a QP (`qp_push`), so a flat direction of `Q` moves the point by at most `rt / QP_PUSH_REG`. 1e-4 sends 44% of 3000 generated QPs to a second round; at 1e-8, 8 pushes never settle (`bench/measurements/02-248/`) |
| `QP_PUSH_TOL` | 1e-9 | Where the push is settled, in scaled space, on boxes and reduced costs. 1e-7 buys nothing; 1e-11 costs 44% of the models a second round (`bench/measurements/02-248/`) |
| `QP_PUSH_ROUNDS` | 40 | Rounds of the push before the barrier's point stands. The longest push on 6000 generated QPs took 3. Not swept |
| `QP_PUSH_DELTA` | 1e-8 | The dual regularisation on the push's rows; 1e-10 lost pivots on the LP-like Maros-Meszaros QPs (`bench/measurements/02-250/`). Not swept |
| `QP_PUSH_REFINE` | 8 | Passes of iterative refinement on the push's solve. When the passes cannot move a row, the push releases that row's pinned variable with the smallest dual slack. `bx_polish` uses `QP_PUSH_REG`, `QP_PUSH_DELTA` and this count to polish a point the checker refuses (`bench/measurements/02-365/`). Not swept |
| `QP_PUSH_CG` | 100 | Conjugate-gradient steps of the one polish run after the push settles with its rows still off, for systems refinement cannot close (liswet2) (`bench/measurements/02-293/`). Running it every round was refused (`qp-push-cg-rounds`). Not swept |
| `QP_PUSH_USER_TOL` | 1e-7 | The push's tests in the model's units, beside the scaled ones, because the checker judges in those units: reduced costs, rows by the checker's own relative test, and boxes (`bench/measurements/02-250/`). Not swept |
| `QP_PUSH_NEAR` | 1e-7 | At the start, a variable within this times `1 + max(\|b\|, \|bounds\|)` of a bound is pinned unless its dual slack is under this times its distance; after a partial step, within this times `1 + \|bound\|`. With the model-wide scale alone, `qgrow22`'s bounds of 3.2e7 pinned columns far from their bounds (`bench/measurements/02-318/`). Not swept |
| `QP_PUSH_GAP` | 1e-8 | The push takes another round while the free variables' reduced costs times distances exceed this times `1 + \|objective\|` and keep halving, since the checker sums the same products (`bench/measurements/02-318/`). Not swept |
| `QP_PUSH_PIN_GAP` | 1e-7 | A pinned variable with a small wrong-signed reduced cost is still freed when that cost times its box width exceeds this times `1 + \|objective\|` (`tests/data/qp_pin_gap.mps`). At 1e-8 QPLIB_10069 took 2.75x the work (`bench/measurements/02-366/`). Not swept |
| `PROBE_CERT_TOL` | 1e-6 | When the dual simplex judges a QP's rows and bounds, an `INFEASIBLE` verdict stands only if its ray certifies at this tolerance, and an unbounded direction only if `jaos_check_ray` certifies it with small curvature; otherwise `NUMERICAL_ERROR`. The CLI checker's default. Not swept |
| `QP_PUSH_FREEINGS` | 3 | How many times a full step may free wrongly pinned variables; past 3 it goes on while each freeing leaves fewer wrong signs (`bench/measurements/02-248/`). Not swept |
| `QP_PUSH_EXTRAPOLATE` | 1e6 | The most the push stretches a step along the rows' null space on a flat face, only when the stretch adds at least one more step and keeps the rows within a tenth of tolerance. `qsierra` settles in 8 rounds; the generated QPs do not change. Not swept |
| `QP_PUSH_DENSE_THETA` | 1e-30 | The `theta` a pinned dense column gets, which makes it absent from the Schur complement. Not swept |
| `PDLP_TOL` | 1e-4 | Where the first-order method stops: relative primal, dual and gap 2-norms in scaled space. The reference's default. The crossover publishes a vertex either way, and 1e-4 gives every verdict 1e-6 gives for fewer iterations (`bench/measurements/02-221/`) |
| `PDLP_MAX_ITER` | 200000 | Iterations before the handoff to the dual simplex, above the slowest small instance that converges (israel, 123773) (`bench/measurements/02-221/`). Not swept |
| `PDLP_CHECK_EVERY` | 64 | Iterations between KKT evaluations, the reference's interval. Not swept |
| `PDLP_INFEAS_TOL` | 1e-8 | How close the difference of iterates must be to a ray for the method to call the model infeasible or unbounded (Applegate et al., 2021); scale free. Feasible models reach no lower than 1.1e-7, infeasible ones 5.7e-11 to 7.2e-9 |
| `PDLP_INFEAS_FROM` | 4096 | The iteration before which the ray test does not run. It cuts the test's cost from 0.83% to 0.10% of the work and sits far under the earliest firing measured |
| `PDLP_RESTART_SUFFICIENT` | 0.2 | The reference's β_sufficient for a restart. Not swept |
| `PDLP_RESTART_NECESSARY` | 0.8 | The reference's β_necessary. Not swept |
| `PDLP_RESTART_ARTIFICIAL` | 0.36 | The reference's β_artificial. Not swept |
| `PDLP_RUIZ_ROUNDS` | 10 | Rounds of Ruiz equilibration before the method, then one Pock-Chambolle pass; the reference's count. Of 0, 5, 10 and 20, 10 finishes the most of the standard 94 inside 10x the dual's work (`bench/measurements/02-222/`) |
| `CONCURRENT_SLICE` | 134217728 | The first-round work budget of each concurrent run (dual, primal, barrier), so a model the dual settles inside it costs exactly the dual. 1.34e8 is the smallest budget at which every instance agreed inside 10x the dual's work. The sweep predates the resume of parked runs; `bench/results/concurrent.txt` holds the current reading |
| `CONCURRENT_GROWTH` | 8 | What each budget multiplies by after a round with no answer. Of 4, 8 and 16, 4 runs too many rounds and 16 overshoots. Not retaken since the resume |
| `PDLP_STEP_TRIES` | 64 | Times the adaptive step may shrink in one iteration; never reached on the standard 94. Not swept |
| `DSE_MIN` | 1e-12 | Floor on a steepest-edge weight, which cancellation can drive to zero |
| `DEVEX_RESET` | 3.0 | Primal Devex (`cfg.primal_devex`) resets its framework when a true weight and its estimate differ by more than this factor. Of 2, 3 and 10, 3 reaches the dual's answer most often |
| `DSE_DRIFT` | 10.0 | How far a carried steepest-edge weight may drift from the exact one before the set restarts. In the dual, a drift with exact weights hands pricing to dual Devex (`DUAL_DEVEX_RESET`); with guessed weights or in a MIP node, the set restarts (`bench/measurements/02-31/`) |
| `DUAL_DEVEX_RESET` | 10.0 | Dual Devex's reset factor. From 2 to 1000, 10 has the lowest work and `pilot` bounds it on both sides (`bench/measurements/02-278/`) |
| `PSE_CHEAP_RESTARTS` | 64 | How many drift restarts of the primal's steepest-edge weights use the cheap slack-basis reset before phase 2 rebuilds them exactly. Exact rebuilds in phase 1 made `pilot87` and `maros-r7` fail |
| `DSE_RESOLVE_EXACT` | 1 | A root re-solve after cuts takes exact weights once it has run this many times `nrow + ncol + 1` iterations. `csched008`'s cut rounds fall from 1.29e10 to 3.71e9 work units, MIPLIB 3 unchanged (`bench/measurements/02-334/`). Not swept |
| `DSE_GUESS_RESTARTS` | 64 | How many times a warm start's guessed weights may drift past `DSE_DRIFT` before exact ones replace them; never in a MIP node (`warm-weights-eager`). Of 1 to 128 on the warm bench, 64 has the lowest work (`bench/measurements/02-31/`) |
| `DUAL_PERTURB` | 1e-6 | The cost perturbation the dual applies at its first stall, scaled by `1 + \|cost\|` and a fixed hash of the column index, repaid at the end through `shift`. It breaks the ties a degenerate walk cycles on. Not swept: one decade above `DUAL_TOL` times the largest reduced costs (`bench/measurements/02-31/`) |
| `ARTIFICIAL_BOUND` | 1e10 | The bound dual phase 1 lends a column whose cost points at a missing bound. No verdict depends on it; it decides how often the method gives up |

These numbers in `src/simplex.c` are counts:

| Name | Value | What it decides |
|---|---|---|
| `REFACTOR_EVERY` | 64 | Basis updates before a refactorization. 32 costs 5% to 9% less work but loses three orders of `pilot87`'s accuracy and raises `wood1p`'s suboptimality bound to 2e-9 (`bench/measurements/02-92/`, `bench/measurements/02-299/`) |
| `VERIFY_UPDATES` | 8 | The most updates the factors may carry when a solve verifies its answer without refactorizing. Of 0 to 32 on MIPLIB 3, 8 has the lowest work, 0.844x (`bench/measurements/02-353/`) |
| `ITER_SANITY_FACTOR` | 200 | Times `rows + columns + 1`, a ceiling against a loop that does not end; hitting it is a library error |
| `SETTLE_ROUNDS` | 32 | Times a settled point may go back to the dual simplex. At 128 `wood1p` costs 1.49x for the same answer |
| `SETTLE_ROUNDS_PRIMAL` | 256 | The same for `cfg.force_primal`. Agreement with the dual rises up to 256, and 512 is byte-identical (`bench/measurements/02-157/`, `bench/measurements/02-161/`) |
| `POLISH_ROUNDS` | 4 | Refinements of an optimum in the model's units before publication, for a row whose scaling hides a residue past `PRIMAL_TOL`. No cold Netlib or Kennington optimum needs one (`bench/measurements/02-31/`). Not swept |
| `WARM_REPAIR_MAX_SHORT` | 8 | How many basic members a mapped starting basis may miss and still be repaired; past it the solve starts cold. 8 has the lowest warm/cold work on Netlib; 4 leaves `pilot` at 31x (`bench/results/warm.txt`) |

## The checker's tolerance

`jaos_check_solution` takes one tolerance, `tol`, from the caller and
applies it in the model's own units. It has no default. The report's fields
are in `docs/api.md`. Activities are Neumaier sums over Dekker's exact
products in `double`; `long double` is not used, because its width differs
between x86-64 and aarch64. The model is put in minimize form first.

**Primal.** A violation is `max(lo − v, v − hi, 0)` over the finite bounds.
`primal_feasible` reads the column violation, the row violation divided by
the row's traffic (`Σ |a_ij x_j|` floored at 1), and the integrality and
cone violations. A row with coefficients
near 1e12 cannot be met closer than its own rounding, so the row test is
relative.

**Dual.** A multiplier `w` on a value `v` with bounds `[lo, hi]`, in
minimize form:

```
|w| <= tol            no condition                    (negligible multiplier)
w > 0                 requires v <= lo + tol · s      (at its lower bound)
w < 0                 requires v >= hi - tol · s      (at its upper bound)
```

```
row i      s = max(1, sum over j of |A_ij · x_j|)     the row's own traffic
column j   s = max(1, |x_j|)
```

A multiplier that points at an infinite bound is a violation of its own
size. Column multipliers are the reduced costs `c_j − A_j · y`, recomputed
from the model. A row activity is a sum whose terms can cancel, so its
distance from a bound is judged against the size of its terms. This scale
cannot hide a wrong answer: a row excused at distance `d` still adds `w · d`
to the gap (`bench/measurements/02-170/`).

**Gap.** Every multiplier, including those under `tol`, adds `w · bound` to
the dual objective, where `bound` is the one its sign points at. The gap is

```
gap = |primal_objective − dual_objective| / (1 + |primal_objective| + |dual_objective|)
```

with both objectives in the scale, as in PDLP and HiGHS. `dual_feasible` is
the dual violation and the gap both within `tol`. `P − D` equals
`sum_v w_v (v − bound_v)`, and on a point feasible only within `tol` some
terms are negative. `gap_positive` and `gap_negative` report the two
halves. `gap_positive` bounds `P − P*` whatever the negative half is, and
neither half decides anything. A multiplier pointing at an infinite bound
leaves the sum incomplete (`gap_certified`), and `certified_suboptimality`
then charges `|w|` times how far that column can move alone, a lower bound
on `P − P*`. Where that distance is infinite the column is counted in
`unquantified_rays`. In a model with no quadratic rows and no cones, a
column with a diagonal `q > 0` in `Q` and nothing off the diagonal is
charged `w²/(2q)` when that minimiser lies in its box
(`bench/measurements/02-293/`).

## Presolve's tolerances

Defined in `src/presolve.c` and `src/aggregate.c`. Presolve runs on the
model as loaded, before `sx_init` scales it, so these are magnitudes in the
caller's units.

| Name | Value | What it decides |
|---|---|---|
| `PRESOLVE_ROUND_ULPS` | 8 | Whether a residue left by a running difference is a number, as this many `DBL_EPSILON` times the scale that produced it. Four sites read it: the solve's entry (`jm_box_inverted`), the singleton row's fold, the emptied row and the frozen row, plus a postsolve debug assertion. No feasible gate model leaves a residue between 0 and 3.69e8 ulps, so 1 to 256 give the same results; 8 matches `ps_row_tol` (`bench/measurements/02-09/`) |
| `PRESOLVE_IMPLIED_FREE_ULPS` | 8 | The window, in `DBL_EPSILON` times `max(1, \|b\|, traffic) / \|a_ij\|`, by which a singleton column's implied box must sit inside its own box to be substituted out. It declines at exact equality, since a wrong yes drops a real bound. Only 0 reads differently, and it costs `d2q06c` 2.2x (`bench/measurements/02-12/`) |
| `AGG_ROW_MAX` | 3 | The longest equality the aggregator substitutes a column out of; longer rows spread fill. 3 is the longest that passes all four gates; 4, 5 and 8 each put an instance past 2x. Models with integer columns are not aggregated (`bench/measurements/02-285/`, `bench/measurements/02-301/`) |
| `AGG_PIVOT_REL` | 0.5 | How large the substituted coefficient must be against its row's largest, so the multipliers stay at most 2. With rows of 3 only 0.5 passes all four gates; the pilot family moves erratically with it (`bench/measurements/02-285/`) |
| `AGG_FILL_MAX` | 8 | The largest Markowitz count a substitution may have; the smallest wins. 16 and 32 cost more and put instances past 2x (`bench/measurements/02-285/`, `bench/measurements/02-301/`) |
| `AGG_PASSES` | 8 | Cap on the aggregator's passes, a safety stop. Not swept |
| `AGG_IMPLIED_FREE_ULPS` | 0 | The aggregator's implied-free window. At 8 it substitutes nothing on stocfor3 or cycle, whose flow rows imply the bound exactly; an exactly implied bound is redundant (`bench/measurements/02-285/`) |
| `AGG_CANCEL_ULPS` | 8 | A substituted sum within this many `DBL_EPSILON` of its larger term is dropped as zero. Not swept |
| `JM_PRESOLVE_ROUNDS` | 16 | Cap on presolve's rounds, a safety stop. On the standard set the removals stop changing at 16 and the cost is flat |

Every presolve window scales by the row's traffic, never by one bound's size
(`bench/measurements/02-72/`). `cur_rl` and `cur_ru`, a row's bounds as
columns leave it, use a Neumaier accumulator, so they carry no drift and
eight ulps needs no term for the count of columns removed
(`bench/measurements/02-76/`). Widening a window to cover a value already
wrong was refused: it published a point 7.5 times `CHECK_TOL` outside two
rows (`bench/measurements/02-73/`). The simplex's `compute_primal` and the
published objective are compensated sums for the same reason
(`bench/measurements/02-78/`, `bench/measurements/02-79/`).

`ps_row_tol` judges whether a row's activity range makes it infeasible,
forced or redundant. It keeps its own literal 8: routed through
`PRESOLVE_ROUND_ULPS` at 64 it made `pilot` INFEASIBLE
(`bench/measurements/02-04/`, `bench/measurements/02-09/`). Presolve has no
bound tightening; all six variants built returned INFEASIBLE on feasible
models (`bench/measurements/02-04/`).

## The writers' numbers

None of these decides an answer.

| Name | Value | What it decides |
|---|---|---|
| the digit count | 15, then 16, then 17 | Significant digits `wr_num` prints: 15 or 16 when `strtod` reads them back exactly, else 17, which always does. `src/write.c` asserts it |
| `NAME_LEN` | 256 | `JAOS_NAME_MAX + 1`, the writer's name buffer; `lp_substitute` refuses a name that does not fit. Not swept |
| `NUM_LEN` | 32 | Buffer for a written number of seventeen digits, sign, point and exponent |
| `LP_WRAP` | 72 | Column at which `jaos_write_lp` breaks an expression, for a person reading the file |

## Acceptance, for the Netlib gate

An instance is accepted when `|obj − ref| <= 1e-6 · max(1, |ref|)` against
Koch's reference values [22], with the checker green in the model's units.
A test that pins a large objective absolutely is stricter than the gate.

## The exact arithmetic and the verifier

These are capacities. They decide whether an operation or a block fits,
and no setting of them changes an answer.

| Name | Value | What it decides |
|---|---|---|
| `JM_EXACT_LIMBS` | 128 | 32-bit limbs per magnitude in `src/exact.c` (4096 bits); one double needs 34. More limbs prove more bases, but 1024 take about 18x the time of 512. `-DJM_EXACT_LIMBS=N` widens it (`bench/measurements/02-180/`, `bench/measurements/02-358/`) |
| `VERIFY_BLOCK_BYTES` | 536870912 | 512 MiB, the most one verify call may hold for a block's dense elimination: a block of 1007 rows at 528 bytes a number. It refuses an oversized block before allocating. Bounded only by the machine |
| `EXACT_PIVOT_CAP` | 1000 | Exact pivots, flips and shifts `jaos_set_exact` may take to repair one basis. The largest repair on the standard 94 took 82 pivots (`bench/measurements/02-358/`). Not swept |

There is no `VERIFY_BOUND_MARGIN`: the width test it padded is gone, and the
verifier checks each operation instead (`bench/measurements/02-358/`).
`VERIFY_PROD_BITS` is under "The other constants".

## Branch and bound

The numbers and switches of `src/mip.c`, and two numbers of
`src/symmetry.c` (`SYM_MAX_DEPTH`, `SYM_LEAF_CAP`). `MIP_LOG_EVERY` and
`MIP_STEER_ROUNDS` are under "The other constants". The setters and options
that override them are in `docs/api.md`. Settings are read on MIPLIB 3, by
the geometric mean of work with every instance under 2x, and on the 2017
set at 1e10 work units by the sum of the primal and dual gaps; a cut family
pays there when it brings that sum to 0.95x. A heuristic that moves no node
count is judged by how early the first incumbent arrives. Network mode is
the root's cut mode for a model with many continuous columns under binaries
(`MIP_NET_MIN_COLS`).

| Name | Value | What it decides |
|---|---|---|
| `MIP_ROOT_CUT_DROP` | on | Lets a root cut leave the relaxation below a node where it does not bind. 0.799x the work on MIPLIB 3, none past 2x (`bench/measurements/02-202/`) |
| `MIP_COVER_LIFT` | off | Gives each cover cut Balas's lifted coefficients instead of extending it by every heavier item. The 2017 set reads 1.001x, short of 0.95x (`bench/measurements/02-298/`) |
| `MIP_NODE_MIR` | off | Adds MIR cuts over a node's own bounds to the node's Gomory round |
| `MIP_PUMP_GENERAL` | off | Gives the pump an extra column and two rows per general integer column, so the column's distance to its rounding counts wherever the rounding lies |
| `MIP_PUMP_ALWAYS` | off | Runs the pump at the root even when an incumbent exists |
| `MIP_RCFIX` | on | Once an incumbent exists, the root pulls an integer column's far bound in as far as its reduced cost allows. MIPLIB 3 reads 0.944x (`bench/measurements/02-335/`) |
| `MIP_RESTART` | off | Restarts the tree from the root once the root's reduced costs fix `MIP_RESTART_FRAC` of the integer columns (`mip-restart` in `bench/refusals.txt`). Network mode restarts whatever this says, keeping the first root's cuts (`bench/measurements/02-345/`) |
| `MIP_INT_TOL` | 1e-6 | How far a relaxation value may sit from an integer and count as integral, in the model's units; such a value is published rounded. A smaller value asks for more than the relaxation's own accuracy, and a larger one publishes fractional points. Not swept: no MIPLIB 3 instance comes near it |
| `MIP_CLIQUE_ROUNDS` | 4 | Rounds of clique cuts at the root. 0.97x the work on MIPLIB 3, none past 2x. Not swept beyond on and off |
| `MIP_ZERO_HALF_ROUNDS` | 0 | Rounds of zero-half cuts at the root (Caprara and Fischetti, pairs-and-bounds): integer rows alone, in pairs and in triples, halved with the bound rows that make every coefficient even. Off: at 1 to 4 rounds the cuts lengthen the trees and MIPLIB 3 reads 1.18x to 1.22x; the 2017 set reads 1.000x (`bench/measurements/02-31/`, `bench/measurements/02-298/`) |
| `MIP_ZERO_HALF_ROW_CAP` | 100 | The tightest candidate rows a zero-half round pairs. Not swept |
| `MIP_ZERO_HALF_TRIPLE_CAP` | 40 | The tightest candidate rows a round takes in triples. Not swept |
| `MIP_ZERO_HALF_CUT_CAP` | 50 | The cuts one zero-half round keeps, in enumeration order. Not swept |
| `MIP_FLOW_COVER_ROUNDS` | 5 | Rounds of flow cover cuts at the root (Padberg, Van Roy and Wolsey) on single-node flow sets, with variable upper bounds read from rows `x - u y <= 0`. 5 rounds read 1.000x on MIPLIB 3 and 0.989x on the 2017 set, and raise the root bounds of sp150x300d and p200x1188c (`bench/measurements/02-317/`). Other counts not swept |
| `MIP_FLOW_COVER_CUT_CAP` | 50 | The cuts one flow cover round keeps, in row order. Not swept |
| `MIP_HULL_ROUNDS` | 20 | Rounds of hull cuts at the root: for a short row with few integer points in its box, an LP over the listed points finds the most violated valid inequality, and its right-hand side is recomputed exactly. A row that is its own hull is skipped. It solves `neos-3381206-awhea` at the root, and MIPLIB 3 reads 0.984x (`bench/measurements/02-356/`). Not swept |
| `MIP_HULL_COLS` | 8 | The most columns a row may have for hull cuts. Not swept |
| `MIP_HULL_POINTS` | 1024 | The most integer points a row's box may hold for hull cuts; `neos-3381206-awhea`'s rows hold 576. Not swept |
| `MIP_HULL_KEEP` | 10 | A root round in which hull cuts reach at least one row in `MIP_HULL_KEEP` does not count as stalled (`MIP_MIR_MORE_STALL`). At 0 `neos-3381206-awhea` stops before its bound rises; with every such round `p0033` costs 1.9x (`bench/measurements/02-356/`) |
| `MIP_CONFLICTS` | on | Turns an infeasible node's Farkas proof into a permanent conflict row over the binaries the path fixed, after relaxing every fixing the proof does not need. Skipped with indicator rows or SOS sets. 0.924x the work on MIPLIB 3, none past 2x (`bench/measurements/02-31/`) |
| `MIP_CONFLICT_MAX` | 32 | The most binaries a conflict row may hold; a longer one prunes almost nothing. Not swept |
| `MIP_CONFLICT_GAP` | 1e-9 | The gap the proof must keep, relative to (1 + \|the rows' side\|), for a fixing to be dropped or the row written; below it the proof is rounding. Not swept |
| `MIP_SYMMETRY` | off | Runs the root's symmetry search (colour refinement, then partition backtracking) on its own. `MIP_ORBITAL` runs the same search, so this matters only with orbital branching off |
| `MIP_SYMMETRY_WORK` | 250 | The work the search may spend, as a multiple of (nonzeros + columns + rows); a search that runs out keeps the generators found. At 250 every symmetric MIPLIB 3 instance finishes but misc06 and air03; 100 gives p0201 a poorer orbit, 0.943x against 0.835x (`bench/measurements/02-31/`) |
| `MIP_ORBITAL` | on | Orbital branching and fixing (Ostrowski, Linderoth, Rossi and Smriglio): the zero side of a branch zeroes the binary's whole orbit under the node's stabiliser, and node entry zeroes every orbit the path zeroed. 0.835x the work on MIPLIB 3, none past 2x (`bench/measurements/02-31/`) |
| `SYM_MAX_DEPTH` | 64 | The levels of the first leaf's path the search revisits. Not swept |
| `SYM_LEAF_CAP` | 64 | The leaves the backtracking under one alternative vertex may visit before giving it up. Not swept |
| `MIP_CLIQUE_FIX` | on | At each node, a binary fixed to one value fixes the literals in conflict with it in the root's clique table (the all-binary rows' conflicts and probing's implications), cascading; a node holding both sides of a conflict is cut without a solve. With RINS on, MIPLIB 3 reads 0.735x in summed work and 1.002x in the geometric mean, and the 2017 set 0.994x (`bench/measurements/02-325/`) |
| `MIP_CLIQUE_ROW_CAP` | 64 | The largest literals of a row and side that feed the conflict graph; it bounds the work per row. Not swept |
| `MIP_GAP` | 1e-6 | The relative gap that closes the search: no open node beats the incumbent by more than `MIP_GAP * (1 + \|incumbent\|)`. Under `jaos_set_mip_gap_rule`'s relative rule the 1 drops out. Not swept: every MIPLIB 3 instance closes with bound equal to incumbent |
| `MIP_CUT_ROUNDS` | 1 | Rounds of Gomory mixed-integer cuts at the root. Swept at 0 to 5: 1 has the best mean with every instance under 2x, and 2 and 3 put several past 2x. A later reading of 2 gave 1.111x (`bench/measurements/02-351/`) |
| `MIP_CUT_AWAY` | 0.01 | A basic integer column is cut only when its fraction lies in `[AWAY, 1 - AWAY]`, since the cut divides by the fraction and its complement. Balas, Ceria, Cornuejols and Natraj's bound. Not swept |
| `MIP_CUT_DROP` | 1e-9 | A coefficient below `DROP` times the cut's largest is folded into the right-hand side through its column's bound, which keeps the cut valid; kept when that bound is infinite. Not swept |
| `MIP_CUT_SLACK` | 1e-15 | How far a cut's right-hand side is pulled back, in units of its span `1 + \|rhs\| + Σ_j \|a_j\| max(1, \|l_j\|, \|u_j\|)`. A rounded-up side can cut off the integer points on it and make a feasible model read infeasible; pulling back only keeps points. Over 20000 generated models checked by enumeration it removes every false infeasible. 1e-15 covers one rounding step; 1e-11 loosened the bound enough to lose `misc03` |
| `MIP_CUT_DYNAMISM` | 1e6 | The largest ratio of a kept cut's largest to smallest coefficient. Not swept: held |
| `MIP_CUT_DEPTH` | 3 | Nodes down to this depth get one round of Gomory cuts on their own relaxation, valid in their subtree. Swept from 0 to every node: 3 has the best mean (0.835x) with every instance under 2x |
| `MIP_NODE_CUT_CAP` | 4 | How many cuts a node below the root keeps, the most efficacious first; 0 is no cap. Swept at 0 to 16: 4 has the best mean with every instance under 2x at the default depth |
| `MIP_COVER_ROUNDS` | 4 | Rounds of knapsack cover cuts at the root. Swept from 0 to 8: 4 has the best mean (0.745x against Gomory alone) with every instance under 2x |
| `MIP_CUT_STALL` | 0.0 | A root round that moves the bound by less than this times (1 + \|bound\|) is the last; 0 stops only when a round adds nothing. At 1e-4 to 1e-2 every round it removed was worth its solve (`bench/measurements/02-202/`) |
| `MIP_NODE_CUT_STALL` | 0.0 | A node whose round moves its bound by less than this share gets no round under it; 0 never stops. Alone 2e-2 reads 0.816x, but beside the root-cut drop every value puts `enigma` past 2x and the drop alone reads better (`bench/measurements/02-202/`) |
| `MIP_MIR_ROUNDS` | 6 | Rounds of MIR cuts on the model's rows at the root. Swept from 1 to 12: 6 has the best mean (0.719x against none) and the curve is flat past it |
| `MIP_MIR_MORE` | 20 | The most MIR rounds outside network mode: past `MIP_MIR_ROUNDS` a round runs only while the one before lifted the bound by `MIP_MIR_MORE_STALL`, because some roots sit flat and then climb. MIPLIB 3 reads 1.005x and the 2017 gap sum falls from 16.80 to 16.54; 20 rounds without the rule put `gt2` at 2.5x (`bench/measurements/02-332/`) |
| `MIP_MIR_MORE_STALL` | 1e-4 | The share of (1 + \|bound\|) such a round must lift the bound by; `MIP_NET_STALL`'s value. Not swept |
| `MIP_MIR_FLIP_GAIN` | 1e-9 | How much flipping an integer column to its other bound must raise a MIR cut's efficacy to be kept (Marchand and Wolsey's complementation), outside network mode on rows with a continuous column. The 2017 gap sum falls from 14.19 to 13.48 and MIPLIB 3 reads 1.000x (`bench/measurements/02-357/`). Not swept |
| `MIP_MIR_DELTAS` | 8 | How many scalings a row's MIR cut tries beyond 1, taken from the fractional integer columns' coefficients. Not swept: a longer list only adds candidates the efficacy rule can reject |
| `MIP_MIR_ROUND` | 1e-9 | The rounding a MIR side's shifted right-hand side may carry and still be cut, `DBL_EPSILON` times its terms' magnitudes times their count. An understated fraction gives an invalid cut; 1e-9 keeps the coefficient error under 1e-7 after the 1 / (1 - f0) that `MIP_CUT_AWAY` caps at 100. Not swept |
| `MIP_MIR_AGGREGATE` | 6 | How many continuous columns a MIR aggregate may substitute out with other rows before it is rounded; 0 is the single-row form. Each step picks the column farthest from its bounds (in network mode the largest coefficient) and the row that leaves the least bound distance. Unguarded, every count put `gen` past 2x; behind `MIP_MIR_AGG_GAIN`, 6 reads 1.012x on MIPLIB 3 and 0.986x on the 2017 set (`bench/measurements/02-321/`, `bench/measurements/02-347/`) |
| `MIP_MIR_AGG_GAIN` | 1e-2 | How much a root round's aggregated MIR cuts must lift the bound, as a share of `1 + \|bound\|`, measured on a copy of the root LP; below it aggregation stops for the solve. A quadratic objective never aggregates, since its relaxations are cold barrier solves. 1e-4 keeps weak cuts that grow bell3a's and dcmulti's trees 1.6x to 1.9x, and 1e-3 puts khb05250 at 2.1x (`bench/measurements/02-321/`, `bench/measurements/02-349/`) |
| `MIP_MIR_LAMBDA` | 1e6 | The largest multiplier an aggregation step may use, and 1 / this the smallest, since a multiplier far from 1 makes the aggregate a difference of very different sizes. Not swept: held at `MIP_CUT_DYNAMISM` |
| `MIP_DIVE_HEURISTIC_DEPTH` | 0 | The deepest node the dive heuristic runs at; 0 is the root alone. At 1, 2 and 4 no node count moves and the work rises to 1.05x to 1.36x |
| `MIP_RINS` | 50 | How many relaxations a RINS dive may solve at a node, with the integer columns fixed where incumbent and relaxation agree; 0 is off. A quadratic objective takes 0, since its dives are barrier solves. 10, 50 and 200 read the same on MIPLIB 3; with clique fixing see `MIP_CLIQUE_FIX` (`bench/measurements/02-325/`) |
| `MIP_LOCAL_BRANCHING` | 0 | How many binaries the local branching tree may flip from the incumbent; 0 is off. 10 and 20 flips read 0.975x and 0.992x on the 2017 set and finish nothing (`bench/measurements/02-286/`) |
| `MIP_LOCAL_BRANCHING_NODES` | 1000 | The node limit of one local branching tree, a small share of an instance's 1e10 units. Not swept |
| `MIP_START_NODES` | 1000 | The node limit of the tree that completes a partial MIP start. On p0201 with half its columns given it finds the optimum inside it. Not swept |
| `MIP_NODE_SELECT` | 1 | The open node the tree takes when it does not dive: 0 the lowest bound, 1 the lowest pseudocost estimate. With the bound every fifth pick the 2017 set reads 0.896x and MIPLIB 3 0.922x, none past 2x (`bench/measurements/02-286/`) |
| `MIP_ESTIMATE_BOUND_EVERY` | 5 | Under the estimate order, every this-many-th pick takes the lowest bound so the tree's bound keeps rising. 10 reads better on the 2017 set but leaves bell5 without an incumbent at twice its work; 5 finishes it |
| `MIP_RESTART_FRAC` | 0.2 | The share of integer columns the root's reduced costs must fix before `--restart` restarts the tree. 0.2 and 0.05 fired on none of the 2017 set. Held |
| `MIP_BATCH_MAX` | 64 | The largest round `mip_tree_batch` can ask the linear tree for, and the size of the round's arrays; the linear tree's `CT_BATCH_MAX`. Rounds of 4 cost 1.37x on MIPLIB 3, so the default round is 1 (`bench/measurements/02-290/`) |
| `MIP_FEASPUMP` | 20 | Rounds of the feasibility pump at the root, while nothing has an answer yet; 0 is off. From 1 to 100 no node count moves, and 20 reaches every early incumbent a larger value does at 1.026x the work |
| `MIP_PUMP_FLIPS` | 10 | How many integer columns a stalled pump moves to the other side, furthest from the rounding first. Fischetti, Glover and Lodi draw it at random, which would break bit-identical results. Not swept |
| `MIP_PUMP_OBJ` | 0.5 | The objective pump's decay: each round weighs the model's objective by `a` against the distance, and `a` multiplies by this from 1. At 0.3 to 0.9 all read about 0.985x the plain pump; 0.5 is the largest decay that delays no first incumbent |
| `MIP_RCFIX_SLACK` | 1e-6 | The slack a reduced-cost fixing adds before it rounds down; slack only loosens the bound, so the deduction stays valid. Not swept: held at the primal tolerance's scale |
| `MIP_PROPAGATE` | 0 | Passes of bound propagation over the rows before a node's relaxation is solved; 0 is off. At 1, 2 and 4 passes MIPLIB 3 reads 1.05x to 1.10x, because the moved bounds change the branching (`bench/measurements/02-31/`) |
| `MIP_QUAD_PROPAGATE` | 4 | What `MIP_PROPAGATE` reads instead of 0 when the objective is quadratic, where a node solve is a cold barrier run. On 400 generated cardinality QPs checked by enumeration, four passes take 13% less work than none |
| `MIP_PROPAGATE_DEPTH` | -1 | The deepest node propagation runs at; negative is every node. The root alone reads 1.051x and one level 1.080x, against 1.074x at every node. It has no effect while `MIP_PROPAGATE` is 0 |
| `MIP_PROP_SLACK` | 1e-9 | The slack a propagated bound keeps before it is rounded, as with `MIP_RCFIX_SLACK`. Not swept |
| `MIP_PROP_INFEAS` | 1e-7 | How far a row's implied activity must sit outside its bound, relative to (1 + \|bound\| + \|activity\|), to prune a node unsolved. Two orders above `MIP_PROP_SLACK`, because a wrong prune throws an optimum away. Not swept |
| `MIP_PROP_MOVE` | 0.5 | How far a propagated bound must move an integer column before it is taken, so rounding alone never churns the relaxation. Not swept |
| `MIP_TIGHTEN` | on | Tightens coefficients of binaries in one-sided rows on the tree's copy (Savelsbergh's coefficient improvement); no integer point moves. MIPLIB 3 reads 0.928x (`bench/measurements/02-307/`) |
| `MIP_TIGHTEN_MIN` | 1e-9 | The slack a row must show, relative to (1 + \|its bound\|), before a coefficient is tightened. Not swept |
| `MIP_PROBING` | off | Probes the root's fractional binaries at 0 and at 1 with propagation, fixing a column whose one side is impossible and keeping the bounds both sides imply. On MIPLIB 3 it fixes nothing and reads 1.109x, because the bounds it finds change the branching (`bench/measurements/02-31/`) |
| `MIP_PROBING_ROUNDS` | 2 | Propagation rounds per probe. Not swept |
| `MIP_PROBING_CAP` | 1.0 | Probing's work as a multiple of the root solve's; 0 is no cap. 0.5 to no cap all read 1.108x to 1.109x (`bench/measurements/02-31/`) |
| `MIP_DIVE_BACKTRACK` | 0 | How many times a dive resumes from the deepest sibling on its stack; 0 sends every sibling to the open set. No value from 1 to unbounded meets the bar, so the dive stays off |
| `MIP_DIVE_GAP` | 0.0 | How far above the best open bound a sibling may be for the dive to resume from it; 0 is no limit. `tests/test_mip.c` fails if it stops deciding. Every value from 1e-4 to 1 costs more than none |
| `MIP_DIVE_HEURISTIC` | 50 | Relaxations the root's dive heuristic may solve, fixing the column nearest an integer each time; 0 is off. At 50 the first incumbent comes earlier on six of the seven instances 200 reaches, at 1.032x the work |
| `MIP_DIVE_DEGRADE` | 0.0 | How far a node's bound may fall from its parent's for the dive to go on into its children; 0 is no limit. Every value from 1e-3 to 1e-1 costs more than none |
| `MIP_PC_EPS` | 1e-6 | The floor under each direction's gain in the pseudocost product score, so a zero gain does not erase a column (Achterberg, Koch and Martin). Not swept |
| `MIP_RELIABILITY` | 0 | Branches per direction before a pseudocost is trusted; below it the children are solved on the spot. Node counts fall at every value, but each probe is a full solve and 1 puts two instances past 2x (`bench/measurements/02-192/`) |
| `MIP_STRONG_CANDIDATES` | 8 | How many unreliable columns a node probes. Not swept: no reliability setting paid |
| `MIP_NODE_PRESOLVE_TRIAL` | 50 | How many node relaxations the tree counts iterations for before `MIP_NODE_PRESOLVE_ITERS` decides (`bench/measurements/02-352/`). Not swept |
| `MIP_NODE_PRESOLVE_ITERS` | 20.0 | When the trial nodes average more simplex iterations than this, later warm node relaxations skip the LP presolve, which can leave the parent's basis far from the reduced model's optimum. At 10 to 40, 20 has the best mean (0.903x) and 10 puts `enigma` at 3.6x (`bench/measurements/02-352/`) |
| `MIP_NOINC_DIVE_AFTER` | 1000 | The node count from which a tree with no incumbent dives until it finds a point; not applied under `--dive`. From node 0 `enigma` costs 8x; 1000 is the earliest that leaves MIPLIB 3 unchanged, and the 2017 gap sum falls from 14.45 to 13.94 (`bench/measurements/02-355/`) |
| `MIP_NOINC_DIVE_BACKTRACKS` | 100 | How many times that dive resumes from its stack before a new dive starts. Of 16, 100 and 1000, 100 gives the lowest 2017 gap sum (`bench/measurements/02-355/`) |
| `MIP_PROBE_CAP` | 0.0 | The work cap on each strong-branching probe as a multiple of the node's; 0 is no cap. Every capped value puts an instance past 2x |
| `MIP_PROBE_DEPTH` | -1 | The deepest node at which strong branching probes; negative is every depth. Probing at the root only puts two instances past 2x |
| `MIP_IMPLIED_PASSES` | 20 | Passes over the rows deriving implied bounds before the root's coefficient tightening; an integer column fixed this way is fixed for the tree (`bench/measurements/02-328/`). Not swept |
| `MIP_IMPLIED_MOVE` | 1e-3 | How far an implied bound must move, relative to (1 + \|old bound\|), to be taken, so a cycle of shrinking steps stops. The fixings lift `sp150x300d`'s root from 4.89 to 34.14; taking every tightened integer bound sent `bell5` past 4 GB (`bench/measurements/02-328/`). Not swept |
| `MIP_PARITY_WORK` | 100 | The largest elimination mod 2 the root's parity step runs, in units of (nonzeros + columns + rows). It fixes every binary of `enlight_hard`, which then solves at the root (`bench/measurements/02-331/`). Not swept |
| `MIP_PRESOLVE` | on | Merges two continuous columns joined by an equality row with opposite coefficients and side 0. MIPLIB 3 reads 0.996x and the 2017 gap sum falls from 15.45 to 15.28; other sides are left out because they break the variable-bound rows network c-MIR reads (`bench/measurements/02-337/`) |
| `MIP_PRESOLVE_PASSES` | 20 | The most passes of the MIP presolve; the models read end in 2 to 5. Not swept |
| `MIP_NET_MIN_COLS` | 50 | The fewest continuous columns under a binary, through two-entry rows, for network mode; `-DJAOS_MIP_NET_MIN_COLS_VALUE` overrides it at build time. With `MIP_NET_SHARE` it keeps `bell5`, `bell3a` and `flugpl` out, which the network rounds cost up to 64x (`bench/measurements/02-328/`). Not swept |
| `MIP_NET_SHARE` | 1/3 | The share of the continuous columns that must sit under a binary for network mode. Not swept |
| `MIP_NET_ROUNDS` | 100 | The root's MIR and flow cover rounds in network mode, where MIR cuts substitute variable bounds first (c-MIR) and may build on earlier cuts. `MIP_NET_STALL` ends them before round 50 on every network model (`bench/measurements/02-341/`, `bench/measurements/02-342/`). Not swept |
| `MIP_NET_STALL` | 1e-4 | The rounds end after one that lifts the bound by less than this share of (1 + \|bound\|). Not swept |
| `MIP_NET_CUT_CAP` | 200 | The cuts one network round keeps. Without it `exp-1-500-5-5`'s root takes 7.6x the work for a weaker bound |
| `MIP_NET_PARALLEL` | 0.5 | The largest cosine between two kept cuts of one round. At 0.9 the round's cuts repeat each other. Not swept further |
| `MIP_NET_HEUR_CAP` | 0.25 | The root's dive and pump work in network mode, once an incumbent exists, as a share of the root's work (`bench/measurements/02-329/`). Not swept |
| `MIP_NET_F0_HI` | 1e-6 | A network c-MIR cut is formed when its fraction `f0` lies in `[MIP_CUT_AWAY, 1 - MIP_NET_F0_HI]`. With `MIP_CUT_AWAY` at both ends `sp150x300d`'s root reached 56.4 against 63.9 (`bench/measurements/02-329/`) |
| `MIP_NET_POOL_CAP` | 200 | In network mode, slack basic cuts leave the relaxation for a pool and at most this many violated ones come back each round. It cuts `beasleyC3`'s root work to a fifth (`bench/measurements/02-342/`). Not swept |
| `MIP_NET_POOL_EFF` | 1e-4 | The efficacy a pool cut needs to come back. Not swept |
| `MIP_NET_POOL_SLACK` | 1e-6 | How far past its bound a cut's activity must be to leave the relaxation or come back. The pool is off outside network mode because it holds `neos-911970`'s root bound at 23.26 against 45.42 without it (`bench/measurements/02-339/`). Not swept |
| `MIP_SUBMIP_NODES` | 500 | The node limit of the sub-MIP heuristic, RENS at a root with no incumbent and RINS elsewhere. It gives `neos-911970` and `binkar10_1` their first point (`bench/measurements/02-328/`). Not swept |
| `MIP_SUBMIP_FIXED` | 0.3 | The share of integer columns a sub-MIP must fix to run. Not swept |
| `MIP_SUBMIP_WORK` | 20000 | The work cap of one sub-MIP, in units of (nonzeros + columns + rows). Not swept |
| `MIP_SUBMIP_EVERY` | 100 | The fewest nodes between two sub-MIPs below the root. Not swept |
| `MIP_SUBMIP_SHARE` | 0.1 | The share of the tree's work the sub-MIPs below the root may take. Not swept |
| `MIP_SUBMIP_ROOT` | 0.5 | A root sub-MIP's work as a share of the root's work. 0.5, 2 and no share read 1.040x, 1.090x and 1.101x on MIPLIB 3 with the same 2017 points (`bench/measurements/02-328/`) |
| `MIP_FJ_WORK` | 20000 | The most work one feasibility jump may spend, in units of (nonzeros + columns + rows). The jump runs at the root of a linear model with no incumbent. Not swept |
| `MIP_FJ_SAMPLE` | 25 | How many violated rows one jump step draws, with a seeded generator. Not swept |
| `MIP_FJ_ROOT` | 0.5 | One jump's work as a share of the root's. A successful jump needs at most 0.29x; uncapped, failures put `p0033` at 4x, and at 0.5 MIPLIB 3 reads 1.014x (`bench/measurements/02-329/fj-root.txt`) |
| `MIP_OBJ_DENOM` | 100 | The largest cost denominator the objective step looks for: when every costed column is integer, the objective lies on a grid and a bound is rounded up to it. It closes `sp150x300d` and reads 0.941x on MIPLIB 3 (`bench/measurements/02-328/`). Not swept |
| `MIP_OBJ_LCM` | 1e6 | The largest common denominator of the costs the step accepts. Not swept |
| `MIP_OBJ_FIT` | 1e-9 | How close a cost times a denominator must sit to an integer, relative to max(1, \|product\|). Not swept |
| `MIP_OBJ_ROUND` | 1e-6 | How far below a grid value, in steps, a bound may sit and still round to it. Not swept |
| `MIP_OBJ_ROUND_REL` | 1e-9 | The same slack per step of the bound's own size. Not swept |

## The feasibility relaxation

All in `src/relax.c`, and none is a tolerance. A freed integer column is held
in its own bounds widened by `M` on each freed side, and `M` grows until the
total comes out at or below it; that total is then the answer for the free
box too. The first three constants cost rounds, never an answer.
`RELAX_BOX_ROUNDS` and `RELAX_ROUND_WORK` stop the search with
`JAOS_ERR_NUMERICAL`, naming the widest box tried, and
`RELAX_LATTICE_CELLS` sizes the proof that runs then.

| Name | Value | What it decides |
|---|---|---|
| `RELAX_BOX_FLOOR` | 1 | The least `M` starts from, so a zero LP bound does not make the first box a point |
| `RELAX_BOX_START` | 2 | `M` starts at this multiple of the LP bound, rounded up to an integer. At 2 one generated model in 2000 needs a second round (`bench/measurements/02-230/`) |
| `RELAX_BOX_GROWTH` | 2 | The factor `M` grows by. On 12000 generated models no total moves past 1e-9 against the free box (`bench/measurements/02-230/`). Not swept: it only sets how many extra solves run |
| `RELAX_BOX_ROUNDS` | 16 | How many boxes the search over the columns may try, so a model with no integer point in any box stops (`tests/data/relax_runaway.mps`). Not swept |
| `RELAX_ROUND_WORK` | 64 | A later round's work as a multiple of the first's; a box twice as wide costs about four times the work on an infeasible model. The caps change nothing where an answer exists (`bench/measurements/02-297/`). Not swept |
| `RELAX_LATTICE_CELLS` | 4096 | The most numbers, rows times (columns + 1), a block of equality rows may hold for the Hermite-form proof that no box holds an integer point; a larger block is skipped. The proof never claims infeasible on 4000 models with a planted point, and settles 2198 of 4000 with the point moved by a half (`bench/measurements/02-362/`). Not swept |

## The conic interior point

All in `src/conic.c`. The walk runs on a Ruiz-scaled copy, so its
tolerances are magnitudes there; the Newton finish and the checks run on the
model as loaded. The readings are `bench/measurements/02-253/` (3000
generated models, one variant at a time) and `bench/measurements/02-254/`
(the 29 continuous CBLIB 2014 instances under 70 MB). "Fail" counts
generated models that fail a check, always as a numerical error; the
variants were read with `CONIC_REG` at 1e-8, where 12 fail. At the values
below, 8 fail, and CBLIB gives 26 optima, all taken by the checker.

| Name | Value | What it decides |
|---|---|---|
| `CONIC_TOL` | 1e-10 | The walk stops `OPTIMAL` when relative primal, dual and gap measures are at or below it; the gap takes the larger of the objectives' difference and `s'z`, since without `s'z` the checker refused 9 of 28 CBLIB optima. 1e-9 fails 13; 1e-11 fails 12 for 13% more work (`bench/measurements/02-254/`) |
| `CONIC_STALL_ITERS` | 3 | Iterations without a new best after which a walk within `CONIC_TOL_ROUGH` stops. 3 is the smallest that cuts no walk short; 1 and 2 change answers (`bench/measurements/02-254/`) |
| `CONIC_TOL_STALL` | 1e-8 | A stopped walk's point within this answers `OPTIMAL` after the Newton finish. Not swept on its own |
| `CONIC_TOL_ROUGH` | 1e-6 | A stopped walk's point within this stands only if the checker takes both sides. 1e-8, which turns the rule off, fails 14; 1e-5 reads like 1e-6 |
| `CONIC_TOL_INFEAS` | 1e-8 | The infeasibility and ray certificate tests, with `tau < kappa`. 1e-7 fails 23 on rays too rough for the checker; 1e-9 fails 16 and loses three planted infeasibilities |
| `CONIC_TOL_INFEAS_STALL` | 1e-5 | The same tests on a stalled walk, still checked. 1e-6 fails 13; 1e-4 reads like 1e-5 |
| `CONIC_MAX_ITER` | 200 | Iterations before the walk stops; the longest of the 3000 takes 26. Not swept |
| `CONIC_STEP` | 0.99 | The fraction of the step to the cones' boundary taken. 0.95 fails 14 for 19% more iterations; 0.999 fails 10 for 45% more work |
| `CONIC_REG` | 1e-7 | Static regularisation of the quasi-definite Newton systems. 1e-9 to 1e-6 fail 61, 12, 10 and 11; 1e-6 leaves a dual violation of 1.7e-6, while 1e-7 keeps every optimum clean for 2.8% more work than 1e-8 |
| `CONIC_PIVOT` | 1e-13 | The floor under an LDL pivot, replaced with its sign kept. Not swept |
| `CONIC_REFINE` | 10 | Passes of iterative refinement against the unregularised system. Not swept |
| `CONIC_RUIZ` | 10 | Rounds of Ruiz equilibration; a cone's rows share one factor. Not swept |
| `CONIC_SCALE_MIN`, `CONIC_SCALE_MAX` | 1e-4, 1e4 | The clamp on every Ruiz factor and the objective's scale. Not swept |
| `CONIC_STALL_STEP` | 1e-10 | A step below this is a stall. Not swept |
| `CONIC_NEWTON_STEPS` | 2 | Newton steps of the finish; a step that does not lower the KKT residual is undone. One step fails 15; four read like two for 1.5% more work |
| `CONIC_NEWTON_WIDE` | 64 | A boundary cone wider than this enters the finish as a diagonal plus a rank-one term, since CBLIB's widest cones made the dense form too large. 16 and 256 read the same (`bench/measurements/02-254/`) |
| `CONIC_PSD_TOL` | 1e-10 | A quadratic row's pivoted Cholesky stops at a pivot below this times the largest; a nonzero remainder refuses the row as not convex. Not swept |
| `CONIC_QC_DENSE` | 3000 | The most columns a quadratic row's dense `Q` may touch. Not swept |
| `CONIC_RAY_ZERO` | 1e-7 | Ray parts and certificate multipliers below this times the largest are zeroed before the checker sees them; it also bounds a narrower cleanup tried first (`bench/measurements/02-263/`). 1e-9 fails 91; 1e-5 reads like 1e-7 |
| `CONIC_RAY_ACTIVE` | 1e-6 | A refused ray is projected, by conjugate gradients, onto the rows it moves by less than this times the row's traffic; this took seed 2 from 24 failures to 3. 1e-8 and 1e-4 read the same |
| `CONIC_RAY_ITERS`, `CONIC_RAY_TOL` | 100, 1e-24 | Steps of that projection and its stop. Not swept |
| `CONIC_NEWTON_ROUNDS` | 4 | Runs of the Newton finish, each dropping the columns whose reduced cost pushes them off their bound. 3 or more solve CBLIB's three `sched_*_orig` at the same work; 1 solves none (`bench/measurements/02-273/`) |
| `CONIC_LOOSE_MARGIN` | 1e-9 | Outside a tree node, a linear row whose box activity stays inside its sides by this times `1 + Σ\|a_ij bound_j\|` leaves the walk (`tests/data/g_cone_badbox.mps`, `bench/measurements/02-322/`). Not swept |
| `CONIC_CERT_TILT` | 32 | The ladder width, `2^-32` to `2^32` of the step, when a refused certificate is re-weighted. 32 or wider fixes 11 of 12 ball models; 12 and 24 fix 10 (`bench/measurements/02-270/`) |
| `CONIC_CERT_SWEEPS` | 1 | Passes over the rows of that search. 2, 3 and 6 fix the same 11 |
| `CONIC_CERT_CALLS` | 1024 | Checker calls the coordinate climb may make, each charged the model's size in work units. 256 fixes 7, 512 fixes 8, 1024 and above 11 |

## The conic tree

`src/conictree.c` is the branch and bound for a model with cones or
quadratic rows and integer columns. Its gap is `MIP_GAP`. The reading is
`bench/measurements/02-255/`, 3000 generated models checked against brute
force.

| Name | Value | What it decides |
|---|---|---|
| `CT_INT_TOL` | 1e-6 | A value within this of an integer is integral; also the zero of an SOS member and the margin a semi-continuous column must reach. `MIP_INT_TOL`'s value. Not swept: every answer agrees with brute force to 2.5e-16 |
| `CT_PC_EPS` | 1e-6 | The floor under each gain in the pseudocost product, as in the MIP tree. Not swept |
| `CT_BATCH_MAX` | 64 | The largest round `mip_tree_batch` can ask the conic tree for. A cap; the default round is 1 (`bench/measurements/02-264/`) |

The conic tree's root rounding and halving dive take the MIP tree's switches
and `MIP_DIVE_HEURISTIC`. The MIP tree runs the same two at the root of a
quadratic model with no incumbent (`bench/measurements/02-259/`). Rounding
at deeper nodes was refused (`conic-rounding-schedule`).

## The other constants

The first group decides a solve. The rest are sizes a format or a buffer
sets, and cadences of checks and output.

| Name | Value | What it decides |
|---|---|---|
| `STALL_FACTOR` | 10 | The dual turns to Bland's rule after `STALL_FACTOR * (nrow + ncol + 1)` iterations without a better total infeasibility. The longest plateau on a terminating instance is 1.67 times its size (measured in commit 0661af8) |
| `PERTURB_STALL_FACTOR` | 1 | The dual perturbs its costs (`DUAL_PERTURB`) after this times `nrow + ncol + 1` iterations without progress, in every solve including MIP nodes. At 1 every Netlib instance that moves gets better; 0.5 makes `pilot` and `truss` worse (`bench/measurements/02-280/`, `bench/measurements/02-333/`). Perturbing at the start was refused (`dual-perturb-at-start`) |
| `NOISE_MARGIN` | 1e5 | A wrong-signed reduced cost below `NOISE_MARGIN * DBL_EPSILON` times its column's traffic is rounding when the simplex re-enters a column. Noise and real costs sit seven orders apart and 1e5 is the middle (measured in commit af171bc) |
| `SPARSE_ALPHA_DEN` | 4 | The pricing row goes through its pattern while it holds at most `nvar / SPARSE_ALPHA_DEN` entries (measured in commit 0af0412) |
| `SPARSE_RHO_DEN` | 4 | The row `rho` goes through its pattern while it holds at most `nrow / SPARSE_RHO_DEN` entries (measured in commit 9e400be) |
| `SPARSE_COL_DEN` | 8 | The forward solve keeps its answer's pattern while it holds at most `nrow / SPARSE_COL_DEN` entries. Not measured |
| `PIVOT_SEARCH_LIMIT` | 4 | Candidates the Markowitz search examines once it holds an acceptable pivot. Not swept |
| `REPAIR_ATTEMPTS` | 4 | Repairs of a singular basis in one refactorization, a backstop. Not swept |
| `MIP_STEER_ROUNDS` | 1000 | Rounds of a node callback at one node, a backstop. Not swept |
| `CONIC_RAY_PROBE_TOL` | 1e-6 | The ray checker's tolerance for a direction the conic solve finds. Not swept |
| `VERIFY_PROD_BITS` | 256 | The exact verifier's bound keeps a longer product as its top 256 bits and an exponent, rounded up. Not swept |
| `GEO_MAX_PASS` | 20 | Passes of geometric-mean scaling, reached only by tests (`docs/scaling.md`). Not swept |
| `GEO_TOL` | 1e-3 | Geometric scaling stops when the row spread improves by less than this in log2. Not swept |
| `CR_MAX_ITER` | 30 | Conjugate-gradient iterations of Curtis-Reid scaling. Not swept |
| `CR_TOL` | 1e-8 | Curtis-Reid stops when `r'z` falls to `CR_TOL^2` of its start. Not swept |
| `BIG` | 2^996 | Above it `jm_two_product_residue` returns 0, since `SPLIT` times the factor could overflow. Arithmetic |
| `SPLIT` | 134217729 | `2^27 + 1`, Dekker's constant for the exact product. Arithmetic |
| `CONCURRENT_ARMS` | 3 | The concurrent solve's arms: dual, primal, barrier |
| `TIME_CHECK_EVERY` | 64 | Simplex iterations between clock reads for a time limit |
| `PROGRESS_EVERY` | 64 | Simplex iterations between progress callbacks |
| `LOG_EVERY` | 1000 | Simplex iterations between log lines |
| `MIP_LOG_EVERY` | 100 | Branch-and-bound nodes between log lines |
| `OSIL_INF` | 1e30 | OSiL's infinity, set by the format |
| `QPLIB_INF` | 1e20 | QPLIB's infinity until the file's own line sets it |
| `NAME_MAX_LEN` | 255 | The longest name the LP reader takes |
| `MAXTOK` | 16 | The most fields on one MPS line |
| `OSIL_MAX_ATTR` | 16 | The most attributes the OSiL reader keeps on one tag |
| `JM_NAME_BUF` | 24 | The buffer for a positional name such as `C<j+1>` |
| `JM_NL_OPTIONS` | 9 | The option slots of a `.nl` header |
| `PROOF_LINE` | 4096 | The longest line the proof-file reader takes |
| `SLURP_CHUNK` | 65536 | Bytes per read of a file into its growing heap buffer |
| `JM_READ_DECLARED_FLOOR` | 2^20 | The largest declared count the `.nl`, QPLIB, CBF and OSiL readers take from a file shorter than that count in bytes (`jm_declared_fits`), so a tiny file cannot make the reader allocate terabytes. Real files reach 0.069 counts per byte (`bench/measurements/02-277/`) |
| `WINDOW` | 32768 | DEFLATE's window (RFC 1951) |
| `MIN_MATCH`, `MAX_MATCH` | 3, 258 | DEFLATE's shortest and longest match (RFC 1951) |
| `HUFF_MAXSYM` | 288 | The literal and length alphabet of RFC 1951 |
| `GZ_FHCRC`, `GZ_FEXTRA`, `GZ_FNAME`, `GZ_FCOMMENT`, `GZ_RESERVED` | 0x02, 0x04, 0x08, 0x10, 0xe0 | The gzip header's flag bits (RFC 1952) |
| `HASH_BITS`, `HASH_SIZE` | 15, 32768 | The deflate encoder's hash table over three-byte prefixes. Not swept |
| `CHAIN_MAX` | 128 | Earlier positions the encoder tries per match; the output is 1.3387x `gzip -9`'s size (`bench/measurements/02-217/`). Not swept |

`JM_WORK_NONZERO`, `JM_WORK_ELIMINATED`, `JM_WORK_UPDATE` and
`JM_WORK_FACTOR` are the work units' weights, and `docs/work-units.md`
carries them.
