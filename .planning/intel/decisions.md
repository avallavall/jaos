# Decisions

Extracted from `DECISIONS.md` (type ADR, precedence 0, locked). All 92 entries
are closed decisions carrying the measurement that closed them; the document's
own header states "Closed decisions only" and "Every heading below is the
decision itself, not a topic", so each `decision:` below is the source heading
verbatim. Every entry is locked: it is a constraint on the roadmap, not a
proposal for it.

Source paths are relative to the repository root.

---

## D1 — Language: C23, GCC, Linux
- source: DECISIONS.md
- status: locked
- decision: Language: C23, GCC, Linux. MSVC is explicitly not a target; Linux is the execution platform; GCC 14 is the minimum supported compiler.
- scope: language, toolchain, execution platform

## D2 — No external dependencies
- source: DECISIONS.md
- status: locked
- decision: No external dependencies. Nothing outside the C standard library and the system threading primitives.
- scope: dependencies, shipped artifact

## D3 — CPU only
- source: DECISIONS.md
- status: locked
- decision: CPU only. GPU execution is out of scope — not deferred, out.
- scope: execution target

## D4 — Generic solver
- source: DECISIONS.md
- status: locked
- decision: Generic solver. Not specialised to scheduling, routing or any other domain.
- scope: domain specialisation

## D5 — A library that does not know its consumers
- source: DECISIONS.md
- status: locked
- decision: A library that does not know its consumers. No API or algorithmic decision may be justified by how a particular consumer happens to be built.
- scope: API justification, consumer coupling

## D6 — Scope is the full mathematical-programming taxonomy
- source: DECISIONS.md
- status: locked
- decision: Scope is the full mathematical-programming taxonomy — LP; MILP; QP and MIQP; QCQP and MIQCQP; SOCP and MISOCP; NLP; MINLP; specialised network algorithms. SDP is a distant candidate. Starting with LP is a build order, not a scope reduction.
- scope: solver scope, build order

## D7 — First milestone
- source: DECISIONS.md
- status: locked
- decision: First milestone: MPS and LP readers, plus LP solved by revised dual simplex, producing correct optima across the Netlib LP test set. Scaling sits inside this milestone.
- scope: M1 definition

## D8 — Deterministic by default
- source: DECISIONS.md
- status: locked
- decision: Deterministic by default. Same input, same parameters, same result and same search path, on every machine. No algorithmic decision may read the clock; two separate budgets (wall-clock time limit, deterministic work limit); no iteration order may depend on memory addresses.
- scope: determinism, budgets

## D9 — Primal heuristics inside, metaheuristics outside
- source: DECISIONS.md
- status: locked
- decision: Primal heuristics inside, metaheuristics outside. Feasibility pump, diving, RINS, RENS and local branching belong inside branch & bound; genetic algorithms, tabu search, ALNS and problem decomposition stay in the consumer.
- scope: MILP heuristics boundary

## D10 — Constraint programming is out of scope
- source: DECISIONS.md
- status: locked
- decision: Constraint programming is out of scope. It is a different paradigm sharing almost no machinery with the simplex and B&B core; if ever wanted it is a separate project.
- scope: paradigm scope

## D11 — Third-party licences, and who authorises an exception
- source: DECISIONS.md
- status: locked
- decision: Third-party licences, and who authorises an exception. Acceptable licences if an exception is ever warranted are Apache 2.0, MIT, BSD-2/3, ISC, zlib; GPL, LGPL and EPL are excluded outright. No exception is taken unilaterally — the maintainer decides.
- scope: licensing, exception authority

## D12 — Implemented from the literature, not from other people's code
- source: DECISIONS.md
- status: locked
- decision: Implemented from the literature, not from other people's code. Papers, theses, textbooks and algorithm documentation only; no other solver's source is consulted, permissive or copyleft alike.
- scope: implementation sources

## D13 — Threading: a thin layer of our own over pthreads
- source: DECISIONS.md
- status: locked
- decision: Threading: a thin layer of our own over pthreads. Neither OpenMP nor C11 `<threads.h>`; spawn/join, mutex, condition variable, barrier and C11 atomics behind `jaos_thread.h`.
- scope: threading layer

## D14 — Build: one plain Makefile
- source: DECISIONS.md
- status: locked
- decision: Build: one plain Makefile. GNU make and GCC, no CMake, no meson.
- scope: build system

## D15 — Test-only dependencies are exempt from D2
- source: DECISIONS.md
- status: locked
- decision: Test-only dependencies are exempt from D2. Framework: Unity (MIT), three files vendored under `tests/`, never linked into the library. D11's licence rules still apply.
- scope: test framework

## D16 — The work unit is a public contract
- source: DECISIONS.md
- status: locked
- decision: The work unit is a public contract. Base currency is nonzeros processed with fixed documented integer weights per discrete event; the counter counts events, never time. The unit's definition changes only at a major version.
- scope: work units, public API

## D17 — No claim without a run
- source: DECISIONS.md
- status: locked
- decision: No claim without a run. No statement about correctness or speed without executing the relevant reference set. Benchmark numbers additionally require a controlled environment — WSL2 is adequate for development and regression catching, not for published figures.
- scope: measurement discipline, benchmark host

## D18 — What the independent checker guarantees, and what it does not
- source: DECISIONS.md
- status: locked
- decision: What the independent checker guarantees, and what it does not. Independence rests on independent inputs (model as loaded and claimed solution only — no basis, no factorization, no solver state), redundant identities, and better arithmetic (`long double`). It cannot detect a model the loader built wrongly.
- scope: checker independence, checker limits

## D19 — Unboundedness is read off a ray, never off an invented bound
- source: DECISIONS.md
- status: locked
- decision: Unboundedness is read off a ray, never off an invented bound. The size of the lent bound participates in no verdict; the remaining case is refused rather than answered.
- scope: unboundedness verdict, dual phase 1

## D20 — Optimality is not declared on carried numbers
- source: DECISIONS.md
- status: locked
- decision: Optimality is not declared on carried numbers. A declaration of optimality is a proposal: the point is recomputed from a fresh factorization and priced again before the solve ends.
- scope: optimality declaration, refresh

## D21 — A gate that fails means nothing until it can fail differently
- source: DECISIONS.md
- status: locked
- decision: A gate that fails means nothing until it can fail differently. A run is judged twice — the all-or-nothing gate, and a per-instance baseline diff. Updating the baseline is a separate command; improvements are reported as well as regressions.
- scope: acceptance gate, baseline

## D22 — A tolerance excuses a condition, never a contribution
- source: DECISIONS.md
- status: locked
- decision: A tolerance excuses a condition, never a contribution. Every multiplier contributes `w · bound` to the dual objective with the bound picked by its sign, including multipliers held to no sign condition.
- scope: checker dual objective

## D23 — A bound-proximity test is judged against what the value is made of
- source: DECISIONS.md
- status: locked
- decision: A bound-proximity test is judged against what the value is made of. The window is `tol · s`, with `s = max(1, sum_j |A_ij x_j|)` for a row and `s = max(1, |x_j|)` for a column.
- scope: checker bound proximity

## D24 — The primal feasibility test stays absolute
- source: DECISIONS.md
- status: locked
- decision: The primal feasibility test stays absolute. Primal feasibility is the hypothesis D23's identity stands on, not a test beside it; the relative measurement is published as `max_row_violation_relative` and decides nothing. `gap_positive` and `gap_negative` are accumulated separately so a small gap can be told from two large halves cancelling.
- scope: primal feasibility test, gap halves

## D25 — A settled point is handed back to the method, and the method's answer is not trusted over the one it started from
- source: DECISIONS.md
- status: locked
- decision: A settled point is handed back to the method, and the method's answer is not trusted over the one it started from. Anything other than a second optimum is discarded. `SETTLE_ROUNDS = 32` is a backstop, not a limit meant to bind.
- scope: re-entry loop, settle rounds

## D26 — Bland's rule is a fallback a detected cycle switches on, never the default
- source: DECISIONS.md
- status: locked
- decision: Bland's rule is a fallback a detected cycle switches on, never the default. `STALL_FACTOR = 10` times `nrow + ncol + 1`; the trigger is failing to improve on the best total reached, and Bland's switches off the moment the best improves.
- scope: anti-cycling, stall detection

## D27 — The re-entry moves a column when its wrong sign costs objective, and when the reduced cost carrying it is a number
- source: DECISIONS.md
- status: locked
- decision: The re-entry moves a column when its wrong sign costs objective, and when the reduced cost carrying it is a number. Judge on the contribution to the duality gap (a product invariant under scaling) rather than on the breach in either space; `NOISE_MARGIN = 1e5` against the column's own traffic.
- scope: re-entry criteria, scaled vs published space

## D28 — A primal ratio test enters M1; the primal simplex does not
- source: DECISIONS.md
- status: locked
- decision: A primal ratio test enters M1; the primal simplex does not. The scope grows by exactly one thing — a primal ratio test feeding the basis change `pivot()` already performs. No pricing, no phase 1 of its own, no second set of weights. Only bounds the model declared may block.
- scope: M1 scope, primal ratio test, primal clean-up

## D29 — The refresh that verifies an optimum refines its two solves; the solve loop does not
- source: DECISIONS.md
- status: locked
- decision: The refresh that verifies an optimum refines its two solves; the solve loop does not. Both solves are refined, unconditionally, at every refresh whose result can be published.
- scope: iterative refinement

## D30 — The primal clean-up judges every candidate before it moves any of them, and against the costs the model owns
- source: DECISIONS.md
- status: locked
- decision: The primal clean-up judges every candidate before it moves any of them, and against the costs the model owns. Each candidate's own loan is called in before it is judged; the candidate set is a snapshot of one point, never re-asked mid-loop.
- scope: primal clean-up, cost shifting

## D31 — What the campaign settled, and the tolerances freeze where they stand
- source: DECISIONS.md
- status: locked
- decision: What the campaign settled, and the tolerances freeze where they stand. The tolerances are frozen at their current values, not one of which was moved to close an instance; Q1 (dual phase 1 by artificial bounds) survives, Q3 (no presolve forced into M1), Q9 (the refusal never fired) and Q10's perturbation half all close.
- scope: tolerance freeze, Q1, Q3, Q9, Q10

## D32 — The fixed cost of a simplex iteration is zero, and the row leaves the table
- source: DECISIONS.md
- status: locked
- decision: The fixed cost of a simplex iteration is zero, and the row leaves the table. `JM_WORK_UPDATE` at 64 and `JM_WORK_FACTOR` at 4096 stay exactly as they are.
- scope: work-unit weights, per-iteration constant

## D33 — The public API's shape, as decided rather than as drafted
- source: DECISIONS.md
- status: locked
- decision: The public API's shape, as decided rather than as drafted. One public header `include/jaos.h`; opaque `jaos_model`; `int64_t` indices and counts, `double` values; `jaos_status` split from `jaos_solve_status`; no solution data leaves by pointer; `jaos_basis` with its own status enum; SemVer, and no ABI freeze at 0.x.
- scope: public API shape, statuses, basis query

## D34 — The one place D8 rested on an argument now rests on a measurement
- source: DECISIONS.md
- status: locked
- decision: The one place D8 rested on an argument now rests on a measurement. No realistic libm can change a JAOS result: a library would have to be wrong by roughly 4x10^8 ulps of `log2` before one scale factor moved.
- scope: cross-machine determinism, scaling, log2

## D35 — Pricing walks the matrix by row, and the answer does not move
- source: DECISIONS.md
- status: locked
- decision: Pricing walks the matrix by row, and the answer does not move. `price_all` accumulates `alpha = rho' M` over the CSR mirror where `rho` is nonzero. Filtering basic columns out of the sweep is not made.
- scope: pricing, row-wise traversal

## D36 — A scatter-form BTRAN is rejected: the saving is real and the arithmetic is not free
- source: DECISIONS.md
- status: locked
- decision: A scatter-form BTRAN is rejected: the saving is real and the arithmetic is not free. Skipping was bought by reordering a cancelling sum; two feasible models came back INFEASIBLE. The route to the same saving runs through predicting the pattern instead of reordering the sum.
- scope: BTRAN, reordering

## D37 — A published zero is a zero, and the digest was lying about it
- source: DECISIONS.md
- status: locked
- decision: A published zero is a zero, and the digest was lying about it. The sign of published zeros is normalised on `publish`.
- scope: published values, solution digests

## D38 — BTRAN skips the slots it can prove are zero, and proves it by not touching them
- source: DECISIONS.md
- status: locked
- decision: BTRAN skips the slots it can prove are zero, and proves it by not touching them. Gilbert-Peierls reachability over U's rows; bit-identical because the skipped slots are exactly zero, not nearly zero.
- scope: BTRAN, hyper-sparsity

## D39 — Infeasibility gets the second opinion optimality already got
- source: DECISIONS.md
- status: locked
- decision: Infeasibility gets the second opinion optimality already got. `INFEASIBLE` is no longer accepted the first time it is reached. `REFACTOR_EVERY` stays at 64 — one of only two values that come out completely clean over a 16..256 sweep. Varying a parameter that must not change any verdict is a method worth keeping.
- scope: infeasibility verdict, REFACTOR_EVERY, trajectory sweeps

## D40 — The pricing row is read through its pattern, and the pattern costs a comparison
- source: DECISIONS.md
- status: locked
- decision: The pricing row is read through its pattern, and the pattern costs a comparison. `SPARSE_ALPHA_DEN = 4`, one notch to the dense side of where the work counter puts it, because the counter cannot see a cache miss. Ascending order is the correctness condition, not tidiness.
- scope: sparse pricing, SPARSE_ALPHA_DEN

## D41 — The dual step walks the pattern too, and an invariant is what makes that safe
- source: DECISIONS.md
- status: locked
- decision: The dual step walks the pattern too, and an invariant is what makes that safe. Walking the pattern is correct exactly when every nonbasic cost the step does not touch is already dual feasible; `compute_duals` and `primal_cleanup` set `duals_dirty` and pay for one full sweep. `SPARSE_ALPHA_DEN` stays at 4.
- scope: dual update sparsity, duals_dirty invariant

## D42 — The exact weight is summed over rho's pattern, which nobody had to go and find
- source: DECISIONS.md
- status: locked
- decision: The exact weight is summed over rho's pattern, which nobody had to go and find. The BTRAN half is taken; the FTRAN half (`jm_dse_update`) needs the solve to hand over a pattern and is left open.
- scope: steepest-edge exact weight

## D43 — The solve says where its answer is, and one charge had been standing for two loops
- source: DECISIONS.md
- status: locked
- decision: The solve says where its answer is, and one charge had been standing for two loops. `SPARSE_RHO_DEN = 4`.
- scope: sparse triangular solve, SPARSE_RHO_DEN

## D44 — The forward solve says where its answer is, and this one needed no ordering
- source: DECISIONS.md
- status: locked
- decision: The forward solve says where its answer is, and this one needed no ordering.
- scope: sparse FTRAN, SPARSE_COL_DEN

## D45 — The work counter is calibrated against a clock, and it is optimistic by a factor that is not constant
- source: DECISIONS.md
- status: locked
- decision: The work counter is calibrated against a clock, and it is optimistic by a factor that is not constant. `SPARSE_COL_DEN` goes from 2 to 8; `SPARSE_RHO_DEN` and `SPARSE_ALPHA_DEN` stay at 4. A change is judged on three things from here: solution digests for correctness, work units for determinism and cross-machine comparability, and a same-instance time ratio.
- scope: work counter calibration, how a change is judged

## D46 — The factorization's fill is measured, the pivot search is confirmed at 4, and a set total is two instances
- source: DECISIONS.md
- status: locked
- decision: The factorization's fill is measured, the pivot search is confirmed at 4, and a set total is two instances. `PIVOT_SEARCH_LIMIT` stays at 4; caching `col_max_abs` is refused on its premise; a change that does not move every instance in the same direction is reported as a geometric mean of per-instance ratios, never as a sum.
- scope: LU fill, PIVOT_SEARCH_LIMIT, geometric mean rule

## D47 — A reduced cost is a rate, and the checker certifies a bound it cannot prove
- source: DECISIONS.md
- status: locked
- decision: A reduced cost is a rate, and the checker certifies a bound it cannot prove. Where a wrong-signed multiplier sits on an unbounded improving direction the term is minus infinity and is dropped, so `gap_positive` can read zero on an arbitrarily suboptimal point. The cheap repair — judging a reduced cost against its own dot-product traffic — is measured and refuted; no local test on a reduced cost can separate a harmful dropped term from a harmless one.
- scope: checker bound, carried defect 1

## D48 — One loop pivoted without asking whether the factorization still existed
- source: DECISIONS.md
- status: locked
- decision: One loop pivoted without asking whether the factorization still existed. `primal_cleanup` leaves the loop when `needs_refactor` is raised; the caller rebuilds and the outer round re-derives the candidate set.
- scope: primal clean-up, LU update failure

## D49 — The re-entry loop stops making progress and the round cap is what ends it
- source: DECISIONS.md
- status: locked
- decision: The re-entry loop stops making progress and the round cap is what ends it. The loop is not revisiting a basis; a cycle detector keyed on the basis would run past it. Nothing is changed by this entry.
- scope: re-entry convergence, carried defect 2

## D50 — Two repairs undo each other, and the loop publishes whichever one it stopped on
- source: DECISIONS.md
- status: locked
- decision: Two repairs undo each other, and the loop publishes whichever one it stopped on. Pivoting reduces the breach by two orders of magnitude and moving puts it back; the answer published is decided by where the round cap happens to fall.
- scope: re-entry oscillation, carried defect 2

## D51 — The residue is the loan ledger
- source: DECISIONS.md
- status: locked
- decision: The residue is the loan ledger. The worst breach a round publishes is the largest cost that round borrowed; every clean-up pivot borrows in order to repair, and repaying is what creates the next round's work.
- scope: cost shifting, re-entry mechanism

## D52 — The first competitive measurement: 4.07x, and it is not the algorithm
- source: DECISIONS.md
- status: locked
- decision: The first competitive measurement: 4.07x, and it is not the algorithm. HiGHS 1.15.1 at tier T0; the target is not fewer iterations, it is a cheaper iteration.
- scope: T0 vs HiGHS, comparison harness

## D53 — Two rivals agree on what a JAOS iteration costs, and that makes it a number worth attacking
- source: DECISIONS.md
- status: locked
- decision: Two rivals agree on what a JAOS iteration costs, and that makes it a number worth attacking. SoPlex 8.0.3 joins the rung; the per-iteration ratio is the same number whichever rival measures it.
- scope: T0 vs SoPlex, per-iteration cost

## D54 — The seventeen is two different things, and only one of them is visible to the counter
- source: DECISIONS.md
- status: locked
- decision: The seventeen is two different things, and only one of them is visible to the counter. The cost of a billed unit spans 0.795 to 11.686 µs — 14.7x — across the timed set.
- scope: work-unit cost calibration, profiling targets

## D55 — The shipping build paid 1.5x for a capacity check it could not inline
- source: DECISIONS.md
- status: locked
- decision: The shipping build paid 1.5x for a capacity check it could not inline. A diagnostic build's output is not a result until a clean build of the same flags confirms it.
- scope: LU array append, profiling method

## D56 — The elimination rebuilt every column of every pivot row, including when there was nothing to eliminate
- source: DECISIONS.md
- status: locked
- decision: The elimination rebuilt every column of every pivot row, including when there was nothing to eliminate. Compact the column where it stands when `piv_n == 0`, bit-identically.
- scope: LU elimination

## D57 — The gate runs its instances at once, because nothing it records is a second
- source: DECISIONS.md
- status: locked
- decision: The gate runs its instances at once, because nothing it records is a second. `-j N` on the acceptance runner, `J=N` on every netlib make target; the record is byte-identical to a sequential run, checked by diffing. What `J` invalidates is the seconds — a time ratio still needs `J=1`. A worker that dies is not an instance that passed.
- scope: parallel gate runner, J, seconds validity

## D58 — The elimination asked for capacity once per entry it wrote, and the entries are billions
- source: DECISIONS.md
- status: locked
- decision: The elimination asked for capacity once per entry it wrote, and the entries are billions. Ask once per column instead. `compact_pivot_row` was refuted as the suspect by the profile — measure before repairing.
- scope: LU capacity reservation

## D59 — The multipliers belong to the pivot, so the column stops being copied twice to meet them
- source: DECISIONS.md
- status: locked
- decision: The multipliers belong to the pivot, so the column stops being copied twice to meet them. The column is walked once, in place, and its fill appended after it.
- scope: LU elimination, scatter

## D60 — The comparison rebuilt its solver only when its own driver changed, so it measured last night's
- source: DECISIONS.md
- status: locked
- decision: The comparison rebuilt its solver only when its own driver changed, so it measured last night's. The staleness test now covers every source the binary is made of, and every record carries the commit it came from plus a `WITH UNCOMMITTED CHANGES` marker. A measurement tool has to make being wrong noisy.
- scope: comparison harness staleness, record provenance

## D61 — The pricing row has no sparsity to exploit, and the calls around it are not the cost either
- source: DECISIONS.md
- status: locked
- decision: The pricing row has no sparsity to exploit, and the calls around it are not the cost either. `SPARSE_ALPHA_DEN = 4` is confirmed where it stands; forcing the two dense sweeps' helpers inline is refused, at 0.9969x.
- scope: pricing density, inlining

## D62 — One set of shipping flags, chosen by measurement, and the counter costs nothing
- source: DECISIONS.md
- status: locked
- decision: One set of shipping flags, chosen by measurement, and the counter costs nothing. `make` builds `-O3 -flto -g -DNDEBUG` and `make pgo` rebuilds from a profile; `-march=native` is refused as a default and survives as `NATIVE=1`; the archive needs `gcc-ar`; the public budget API is free.
- scope: shipping build flags, PGO, Q11

## D63 — The gap's iterations are weight restarts, and the threshold that causes them is what keeps the answers right
- source: DECISIONS.md
- status: locked
- decision: The gap's iterations are weight restarts, and the threshold that causes them is what keeps the answers right. `DSE_DRIFT = 10.0` is bounded on both sides and the interior is one value wide; restarting the weights to the exact one rather than to 1.0 is refused. The cure is a pricing rule that does not depend on an exact recurrence surviving — Devex.
- scope: steepest-edge weight restarts, DSE_DRIFT, Devex candidacy

## D64 — The options API configures the contract and never the method, and it is setters
- source: DECISIONS.md
- status: locked
- decision: The options API configures the contract and never the method, and it is setters. The API owes a caller `PRIMAL_TOL` and `DUAL_TOL` plus logging and callbacks; everything else is method and stays measured and fixed. Values that are not finite and non-negative are refused rather than clamped; zero means the default.
- scope: options API, contract-versus-method line

## D65 — The solver speaks only when spoken to, and never on a clock
- source: DECISIONS.md
- status: locked
- decision: The solver speaks only when spoken to, and never on a clock. `jaos_set_log_callback` and `jaos_set_log_level`, four levels, no default destination, paced by iteration count. The closing summary reports refactorizations, weight restarts and stalls.
- scope: logging, verbosity

## D66 — Changing a model discards its answer, and the two that do not touch the matrix leave the derived data alone
- source: DECISIONS.md
- status: locked
- decision: Changing a model discards its answer, and the two that do not touch the matrix leave the derived data alone. Bound and cost changes leave the scaling and the row-wise mirror exactly correct; `lower > upper` is accepted as a model with no feasible point.
- scope: model modification, derived data

## D67 — Setting a coefficient is three operations, because the stored matrix has an invariant
- source: DECISIONS.md
- status: locked
- decision: Setting a coefficient is three operations, because the stored matrix has an invariant. Replace, delete when the new value is zero, insert in sorted position; both derived copies are dropped and rebuilt.
- scope: coefficient modification, matrix invariant

## D68 — A basis outlives the answer it produced, and that one line is warm re-solve
- source: DECISIONS.md
- status: locked
- decision: A basis outlives the answer it produced, and that one line is warm re-solve. The basis is stored apart from the answer; a modification discards one and keeps the other; `jaos_clear_basis` asks for a cold solve. Dual feasibility comes from cost shifting rather than artificial bounds, and the weights restart at one.
- scope: warm re-solve, basis lifetime, clear_basis

## D69 — What warm re-solve buys: 182x the iterations and 60x the work, on a branching step
- source: DECISIONS.md
- status: locked
- decision: What warm re-solve buys: 182x the iterations and 60x the work, on a branching step. The perturbation is one branch-and-bound branching step whose size comes from the model's own numbers; the cold number, the perturbation and the answers are all checked before the ratios are believed.
- scope: warm campaign, `make warm`

## D70 — A budget that cannot be resumed is only half a budget
- source: DECISIONS.md
- status: locked
- decision: A budget that cannot be resumed is only half a budget. The basis is kept for WORK_LIMIT, TIME_LIMIT, INFEASIBLE and UNBOUNDED and cleared from the published arrays; `JAOS_SOLVE_NUMERICAL_ERROR` alone keeps none.
- scope: budget resumption, stopping points

## D71 — The checker says when its bound is not a bound, and the count is 98 of 110
- source: DECISIONS.md
- status: locked
- decision: The checker says when its bound is not a bound, and the count is 98 of 110. `gap_certified` and `max_dropped_multiplier` are reported and neither decides anything. Route A — report the bound as void — would void the gate, and the distribution has no gap to threshold at.
- scope: dropped terms, carried defect 1

## D72 — `pilot87`'s iteration guard: not a cycle, and the anti-cycling rule is the reason
- source: DECISIONS.md
- status: locked
- decision: `pilot87`'s iteration guard: not a cycle, and the anti-cycling rule is the reason. Under Bland's rule the solve visits 1,136,521 distinct basis states; the anti-cycling rule and the progress measure are about different quantities. Diagnosis only — no repair here.
- scope: carried defect 3 diagnosis

## D73 — The certificate D47 wanted, without the factorization it thought it needed
- source: DECISIONS.md
- status: locked
- decision: The certificate D47 wanted, without the factorization it thought it needed. `certified_suboptimality` is `|w|` times how far a column can move alone; it is a sound lower bound that never overclaims, and it is refuted as a verdict — at a vertex the first tight row stops the column, so it reads the same on answers known to be 1.04e-3 wrong as on correct ones. Route B needs the simplex direction after all.
- scope: certified suboptimality, unquantified rays

## D74 — Does the re-entry's clean-up need to borrow at all? Measured: yes, and `pilot87` is the whole price
- source: DECISIONS.md
- status: locked
- decision: Does the re-entry's clean-up need to borrow at all? Measured: yes, and `pilot87` is the whole price. Removing the loan leaves correctness untouched and costs `pilot87` 2.372x its iterations for the 0.980x it buys `pilot`. The direction is closed.
- scope: re-entry loan, carried defect 2

## D75 — The non-aliasing claim holds, and `restrict` belongs inside the kernel rather than on the API
- source: DECISIONS.md
- status: locked
- decision: The non-aliasing claim holds, and `restrict` belongs inside the kernel rather than on the API. Local qualifiers inside the kernels, never on a signature. The measurement is deferred, not taken.
- scope: restrict, aliasing audit

## D76 — `restrict` measured and refused: what makes it safe here is what makes it worthless
- source: DECISIONS.md
- status: locked
- decision: `restrict` measured and refused: what makes it safe here is what makes it worthless. 0.995x in the shipping build and 1.0053x with `-flto` removed — the two builds disagree about the sign and both are inside the noise. Reverted. Q11 closes in full.
- scope: restrict, Q11 closure

## D77 — A dimension change keeps the basis exactly when what is left is still a basis
- source: DECISIONS.md
- status: locked
- decision: A dimension change keeps the basis exactly when what is left is still a basis. Additions append so no existing index moves; a new row is a transpose rather than an append; deletion takes a set. One exception: a new column with no finite bound drops the whole basis.
- scope: add and delete rows and columns, basis survival

## D78 — A load was discarding the logging callback, and the list that preserved settings was the defect
- source: DECISIONS.md
- status: locked
- decision: A load was discarding the logging callback, and the list that preserved settings was the defect. Configuration moved into a `jm_config` sub-struct that the load saves and restores whole; the list is gone.
- scope: configuration preservation across a load

## D79 — A callback may look, and may stop a solve, and may not steer one
- source: DECISIONS.md
- status: locked
- decision: A callback may look, and may stop a solve, and may not steer one. Invoked on a fixed iteration count and never on a clock; a stop is `JAOS_SOLVE_INTERRUPTED` and keeps its basis. A watcher is told iterations, work units and total primal infeasibility — and deliberately not an objective.
- scope: progress callbacks, determinism of the hook

## D80 — The comparison was timing a warm re-solve, and the feature that broke it shipped two weeks earlier
- source: DECISIONS.md
- status: locked
- decision: The comparison was timing a warm re-solve, and the feature that broke it shipped two weeks earlier. `jaos_clear_basis` before each timed solve, outside the timed region, and the driver aborts if two cold repeats disagree. A feature can redefine an instrument in a file the feature never touches.
- scope: comparison harness, warm re-solve contamination

## D81 — The ladder is climbed: presolve is worth 1.42x, and a primal simplex is worth nothing
- source: DECISIONS.md
- status: locked
- decision: The ladder is climbed: presolve is worth 1.42x, and a primal simplex is worth nothing. T0 to T1 moves iteration counts by exactly 1.000x, so having no primal simplex costs nothing on the standard set and phase 6 item 7 should never again be justified as a speed argument. Presolve buys HiGHS 1.417x and SoPlex 1.136x against a per-iteration gap of 2.53x that no rung moves — "the cheaper iteration is the larger lever", which the entry states reorders the plan.
- scope: tier ladder T0-T3, presolve value, primal simplex value, plan ordering

## D82 — Partial pricing on the leaving-row sweep, refused: it saves the cheap units and buys the expensive ones
- source: DECISIONS.md
- status: locked
- decision: Partial pricing on the leaving-row sweep, refused: it saves the cheap units and buys the expensive ones. Correctness closes it regardless — `pilot` publishes OPTIMAL out of tolerance with the checker green, `wood1p` is rejected, `woodw` takes 131x the iterations. Reverted. Bland's rule cannot be given a slice and the progress measure cannot be fed a partial total.
- scope: partial pricing, leaving-row sweep

## D83 — Clp is the third reading, and the two were not agreeing by coincidence
- source: DECISIONS.md
- status: locked
- decision: Clp is the third reading, and the two were not agreeing by coincidence. At T0: 3.72x vs HiGHS 1.15.1, 1.34x vs SoPlex 8.0.3, 3.77x vs Clp 1.17.11, on 1.47x, 0.70x and 1.67x their iterations and 2.54x, 1.92x and 2.26x per iteration. Phase 1 is complete.
- scope: T0 vs Clp, per-iteration cost as a property of JAOS

## D84 — Multiple pricing, refused too, and phase 6 item 3 closes with both halves measured
- source: DECISIONS.md
- status: locked
- decision: Multiple pricing, refused too, and phase 6 item 3 closes with both halves measured. The standard gate reads NOT MET at every K; the Kennington saving is untrusted for D45's reason. The leaving-row sweep is the wrong thing to make cheaper. The profile points at `admit_candidate` instead.
- scope: multiple pricing, phase 6 item 3 closure

## D85 — A free nonbasic improves in the direction its reduced cost points, and the status was never able to say which that was
- source: DECISIONS.md
- status: locked
- decision: A free nonbasic improves in the direction its reduced cost points, and the status was never able to say which that was. `wants_a_pivot` and `primal_ratio_test` read the sign of the reduced cost rather than the status; bit-identical for every bounded status. It did not need the primal simplex it was filed as waiting for. Carried defect 4 closes.
- scope: carried defect 4, free nonbasic

## D86 — `pilot87`'s iteration guard is a factorization that stopped agreeing with itself, and the two solves each iteration already pays for can say so
- source: DECISIONS.md
- status: locked
- decision: `pilot87`'s iteration guard is a factorization that stopped agreeing with itself, and the two solves each iteration already pays for can say so. `LU_AGREE_TOL = 1e-5` on the disagreement between BTRAN's `alpha_q` and FTRAN's `col[r]`; a pivot is declined only while `n_updates > 0`, and a declined iteration is not billed. D72's proposed progress measure is refuted — the dual objective is not monotone here. 256 is still broken, slow instead of wrong. Carried defect 3 closes.
- scope: carried defect 3, LU_AGREE_TOL, stability trigger

## D87 — The checker bounds what the rows imply, which closes D47's constructed case and not its real one
- source: DECISIONS.md
- status: locked
- decision: The checker bounds what the rows imply, which closes D47's constructed case and not its real one. Activity-based tightening from the model alone; sound because an implied bound moves neither the feasible region nor `P*`. Iterating the propagation is refuted — an implied bound is sound but slack. Route B does not need the solver's basis. Defect 1 remains open with its scope reduced.
- scope: implied bounds, checker margin

## D88 — The gate watches the dropped term, because the checker cannot judge it and the predicate cannot see it
- source: DECISIONS.md
- status: locked
- decision: The gate watches the dropped term, because the checker cannot judge it and the predicate cannot see it. `DROP_REGRESSION_FACTOR = 2.0` and `DROP_FLOOR = 1e-9`, both measured on both sides and calibrated by injecting a fault. It catches the change from right to wrong, not wrongness itself.
- scope: gate regression detector, baseline quantity

## D89 — The re-entry loop keeps its best round, and "best" is defensible before it is close
- source: DECISIONS.md
- status: locked
- decision: The re-entry loop keeps its best round, and "best" is defensible before it is close. Lexicographic: a round inside dual tolerance beats one that is not, then the lower objective, then the smaller violation — both quantities computed in the model's own space. D49's factor of 280 is attributed to the per-column scaling. The published answer stops depending on where `SETTLE_ROUNDS` falls; the oscillation itself is left open.
- scope: re-entry publication criterion, carried defect 2

## D90 — The warm start stops refusing a free nonbasic, because the defect it was avoiding is fixed
- source: DECISIONS.md
- status: locked
- decision: The warm start stops refusing a free nonbasic, because the defect it was avoiding is fixed. `cycle` goes from 1537 warm iterations to 16; the standard-set geometric means improve from 0.0055 to 0.0052 in iterations and 0.0166 to 0.0162 in work.
- scope: warm start free nonbasic, warm campaign means

## D91 — The bound and the verdict stop being one number, and D47 closes
- source: DECISIONS.md
- status: locked
- decision: The bound and the verdict stop being one number, and D47 closes. `pos`/`neg` carry every term and bound the suboptimality; `pos_model`/`neg_model` carry only terms from bounds the model declared and are what the verdict reads. Propagation iterated to a fixed point at `IMPLIED_ROUNDS = 64`. Certification goes to 64 of 110; `relative_suboptimality` replaces the dropped term as the watched quantity and no verdict reads it.
- scope: checker verdict/bound separation, IMPLIED_ROUNDS, carried defect 1 closure

## D92 — A residue only a pivot can remove was hidden by the scaling, and the repair is the union of the two readings rather than either one
- source: DECISIONS.md
- status: locked
- decision: A residue only a pivot can remove was hidden by the scaling, and the repair is the union of the two readings rather than either one. `breached` asks whether there is a sign-condition breach in either space; substituting the published reading for the scaled one is refused at both scopes. `bench/warm.c` now checks both answers and names which side was refused. `settled_dual_violation` applies its tolerance after conversion.
- scope: dual breach space, warm campaign checking, clean-up pivot selection
