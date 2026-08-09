# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries say what changed and what it cost. The reasoning lives where it
belongs: `DECISIONS.md` for closed decisions, `PLAN.md` for what is still
open, `bench/README.md` for the gate, and the commit each entry came from.

## [Unreleased]

### Added

- `jaos_basis` reports where every column and row activity rests in the basis
  behind the answer — basic, at a bound, or free. It is the part of a
  solution the values cannot carry: a basic variable sitting exactly on a
  bound reads no differently from a nonbasic one resting there, and only one
  of the two is a constraint the optimum is held by. PLAN 2.4 promised it and
  M1 had not delivered it (D33).
- A primal ratio test, used only to clean up after the dual solve. `greenbea`
  goes from REJECTED to checker ok in eight pivots, at fifteen significant
  digits of Koch's value, and `pilot`'s objective comes inside tolerance from
  390x outside it. Grows M1's declared scope by that ratio test and nothing
  else; the primal simplex stays out (PLAN 2.1). 0 regressed, 2 improved on
  the standard 94; 0/0/0 on the other two sets (D28).
- Bland's rule as a fallback a detected cycle switches on. `grow15` had been
  running to the iteration guard at 189201 iterations and now solves in 21653.
  Costs nothing where nothing cycles: `grow22`, `grow7` and `truss` are
  bit-identical (D26).
- The dual simplex re-enters from the settled point, moving a nonbasic to its
  other bound and running again. `nesm` goes from REJECTED to checker ok for
  seven iterations; `pilot` and `pilot87` improve two orders of magnitude on
  the dual violation. 0 regressed, 1 improved; 0/0/0 elsewhere (D25).
- Three fields in `jaos_check_report`: `gap_positive` and `gap_negative` split
  the gap into the two sums it is the difference of, and
  `max_row_violation_relative` reports the row residue against the row's
  traffic. No verdict reads any of them. The record carries all three per
  instance (D24).
- A fuzzer for both readers, `tests/test_fuzz.c`: truncation at every offset
  of every corpus file, seeded edits, random bytes, keyword salad, and named
  edge cases, each fed to both readers. 11543 cases in the suite, 1.6M under
  `JAOS_FUZZ_SCALE=200`, clean under ASan+UBSan. Closes the M1 gate's
  condition 4, which had been asserted and never tested (PLAN 2.9).
- Baselines for the Kennington and infeasible sets, with
  `make netlib-kennington-baseline` and `make netlib-infeas-baseline`. Both
  gates report PASS, which is the state in which a summary line cannot show a
  change (D21).
- `bench/run.c` takes `-e infeasible`: shape and determinism still hold, the
  reference objective and the checker drop out, and an instance returning an
  optimum fails as a false optimum. The only check in M1 that looks for a
  wrong answer rather than confirming a right one (PLAN 2.9).
- The two remaining M1 instance sets, pinned and running:
  `bench/netlib-kennington.manifest` (16) and `bench/netlib-infeas.manifest`
  (29), expanded with netlib's `emps` — fetched and checksummed by
  `bench/fetch.sh`, never stored here (PLAN Q6, D11). Kennington passes 16 of
  16 on the first run; the infeasible set refused 28 of 29 with no false
  optima. Each set fetches into its own directory, because `greenbea` names
  two different models across sets.
- `bench/netlib.baseline` records what every instance did, and `make netlib`
  diffs against it. The gate's own verdict read `NOT MET` for almost the whole
  of M1 and reads `PASS` now, and at neither end can it answer the question
  every change raises — did this make anything worse? Work may grow 2x before
  it counts as a regression.
  `make netlib-baseline` rewrites it and is never a side effect of running
  the gate (D21).

### Fixed

- A valid model could come back as `JAOS_ERR_INVALID_INPUT` — the code the
  library reserves for a caller's mistake — from inside `jaos_solve`.
  `pivot()` reports a failed basis update by asking for a rebuild and
  returning success, and `primal_cleanup` was the one loop that pivoted
  without reading that flag: both triangular solves return without writing
  once the factorization is wrecked, so the ratio test and pricing row that
  follow were computed from whatever the buffers last held, and the update's
  own guard was what stopped it. The loop now leaves and lets the caller
  refresh. `pilot` at a refactorization interval of 48 goes from that error
  to `optimal`; the other eleven cells of the same sweep and all 139
  reference instances are unchanged, digest for digest (D48).

- A model with a finite optimum could be reported `INFEASIBLE`. The verdict
  is reached when no pivot clears `PIVOT_MIN`, and those are exactly the
  magnitudes that drift in a factorization patched by many updates — so
  optimality was being re-checked against a fresh factorization (D20) while
  infeasibility was accepted the first time it was reached. It now gets the
  same second opinion. Costs 0.04% and moves no iteration count; the 29
  genuinely infeasible models are still refused, 29 of 29 (D39).

### Changed

- The elimination stops rebuilding a column when there is nothing to
  eliminate. It scattered every column of every pivot row into a dense
  buffer and pushed the survivors back regardless of what the pivot column
  held — and on a basis that is already triangular it holds nothing, so the
  pass was copying each column onto itself. On `fit2p` that was 97% of
  pivots and 344 million appends per eleven factorizations. **`fit2p` goes
  from 8.25 s to 2.46 s**, and with the entry below, from 12.87 s to 2.46 s.
  Surgical by construction — `maros-r7`, the highest-fill instance, moves
  1.02x. All 139 reference instances identical, digest for digest (D56).
- Appending to a sparse vector tests its own capacity instead of calling
  across a translation unit to be told there was room. `jm_svec_push` called
  `jm_grow` twice per element and `jm_grow` lives in `util.c`, so in the
  build JAOS ships — `-O2`, no LTO — that was 63% of every instruction
  executed on `fit2p` and charged no work unit at all. **`fit2p` goes from
  12.87 s to 8.42 s and `maros-r7` from 50.5 s to 36.7 s**; under `-flto` it
  changes nothing, because the compiler was already doing it. All 139
  reference instances identical, digest for digest (D55).
- The FTRAN reports where its answer is nonzero too, and the steepest-edge
  recurrence and both updates of `x_B` walk that instead of every row. This
  one needed no ordering, because all three readers are elementwise — which
  is why its threshold sits at half the other two. Work falls **1.557x on
  Kennington**, 1.010x on the standard set and 1.037x on the infeasible one:
  the largest single entry of M2 so far, with no digest and no iteration
  count moved (D44).
- The BTRAN reports where its answer is nonzero, taken from the pass that
  was already visiting every slot to permute it back, and pricing walks that
  instead of scanning the whole pricing row. On Kennington that scan was 27%
  of everything the solver billed. It needed a second loop of the same
  length to go with it — the reset of basic slots — because one charge had
  been standing for both. Work falls **1.255x on Kennington**, 1.007x on the
  standard set and 1.008x on the infeasible one; **all 139 instances get
  cheaper and none gets dearer**, with no digest and no iteration count
  moved (D43).
- The exact steepest-edge weight is summed over the pattern of the row it is
  the norm of, instead of over the dimension. That pattern costs nothing to
  have: pricing already walks the whole of `rho` in ascending order looking
  for rows to skip. Work falls **1.208x on Kennington**, 1.009x on the
  standard set and 1.025x on the infeasible one — all 139 instances cheaper,
  no digest and no iteration count moved. The other half of the same charge,
  the steepest-edge update itself, needs FTRAN to hand over a pattern and is
  still open (D42).
- The dual step walks the pricing row's pattern as well, which is the other
  and larger half of the same idea. It needed one thing the ratio test did
  not: the loop also repairs reduced costs that have drifted past their
  bound, so skipping a variable is only safe where that repair would have
  done nothing. `duals_dirty` names that condition and the two places that
  break it pay one full sweep to restore it. Work falls **1.451x on
  Kennington**, 1.015x on the standard set and 1.015x on the infeasible one,
  again with **no digest and no iteration count moving**. Kennington is now
  1.895x cheaper than when M2's pricing work began (D41).
- The pricing row is read through its pattern rather than in full wherever
  that pattern is small. `alpha = rho' M` is dense storage holding a sparse
  vector — 0.1% of the variables on `ken-18`, which is two thirds of the
  Kennington set's dense sweeping on its own, against 83.6% on `osa-60` —
  and both the ratio test's scan and the clear that starts the next
  iteration walked every slot to find the few that were there. Work falls
  **1.306x on Kennington**, 1.014x on the standard set and 1.012x on the
  infeasible one; 138 of 139 instances get cheaper, none gets dearer, and
  **no digest and no iteration count moves anywhere** (D40).
- BTRAN solves only the slots that can produce a nonzero, found by searching
  the factor's dependency graph instead of computing zeros to discover they
  are zero. 96.7% of them are. Work falls 1.040x on the standard set, 1.095x
  on the infeasible one and 1.057x on Kennington — `ken-18` included — with
  **no digest and no iteration count moving anywhere**, because the skipped
  slots are exactly zero rather than nearly zero (D38).
- A published zero is now always `+0.0`. `-0.0` is the same number and every
  check agreed it was, but it made two identical answers differ in bytes —
  which is what a solution digest compares. Normalising moves 90 of 94
  digests and not one work unit, and without it D38 could not have been told
  apart from a change that alters answers (D37).
- Pricing walks the matrix by row instead of by column, so a zero in the
  pricing row skips a whole row of the matrix rather than being spread
  across every column that touches it. Total work over the 110 solved
  reference instances falls 1.19x, 96 of them get cheaper, the best is 2.21x
  and the worst 14 pay up to 5% — and **not one solution digest or iteration
  count moves**, on any of the three sets (D35).
- The claim that a different C library cannot change a JAOS answer is
  measured rather than argued. `log2` in the scaling was the only libm result
  IEEE leaves unpinned anywhere in the solver; perturbing it moves no scale
  factor of the 139 instances until the offset is about 4x10^8 ulps, and the
  probe was shown to fire before its silence was believed (D34).
- The work-unit table loses its per-iteration row. The weight is zero and it
  is measured, not assumed: the basis update turns out to be 1.8% of an
  iteration rather than the whole of it, and the other 98% is already charged
  by dimension. No figure anywhere moves — this is an attribution of the
  units, not a reweighting of them (D32).
- The re-entry weighs a column on what its wrong sign costs the objective,
  not on the size of the breach. `etamacro` goes from REJECTED to checker ok
  for 9 extra iterations; every other instance of the standard 94 is
  bit-identical and `pds-20` costs one iteration more. Two earlier forms of
  this were measured and left out, one of them costing `pds-20` 3.2x — both
  in D27.
- The checker's bound-proximity test scales with what the value being tested
  is made of: the window is `tol * s`, with `s = max(1, sum_j |A_ij x_j|)`
  for a row and `max(1, |x_j|)` for a column. Row 3 of Netlib's `finnis`
  carries 4.0e10 of traffic and was being asked to rest within 1e-6 of a
  bound where one ulp is 7.6e-6. `finnis` goes from REJECTED to checker ok
  and nothing else moves: 0 regressed, 1 improved, 0 new on the standard 94,
  0/0/0 on the other two sets. D23 carries the identity that makes the
  loosening safe and the case it must still reject; `docs/tolerances.md`
  carries the formulas.
- The checker's primal feasibility test stays absolute. Extending D23 to it
  was measured and refused: primal feasibility is the hypothesis of the
  identity D23 rests on rather than a test beside it, and exactly one
  instance of the 94 exceeds 1e-6 on a row. D24 has the reasoning, and the
  `Q`/`N` instrumentation the argument turned up as pending work.
- The checker's relative gap is scaled by `1 + |primal| + |dual|` rather than
  `max(1, |primal|)`, which reported a larger error the further the two
  objectives were apart. This is the PDLP form HiGHS adopted. No verdict
  moved on the Netlib set, which is the only reason it is in.
- The checker no longer drops a small multiplier from the dual objective.
  Below tolerance it is still held to no sign condition, but it contributes
  `w * bound` like any other, because what gets dropped is small only if the
  bound is. That is what had been rejecting `pilot-ja`, whose dual violation
  is exactly zero; checker-green went from 86 to 87 with nothing else moving.
  D22 has the reasoning, including two repairs that were measured and
  rejected first — one of which passed the entire gate while being vacuous.
- The solver core is back to where it was on 2026-08-06, reverting ten
  commits that had not been run against the Netlib set. Measured per instance
  rather than by the summary line, they fixed `grow15` and `nesm` and cost
  `pilot-we` (a feasible problem reported infeasible), `pilotnov` and
  `grow22` (2179 iterations to 167865). The summary line was identical before
  and after, because the gains and the losses cancelled.
- A model whose optimum lies beyond the bound dual phase 1 lends is refused
  with a numerical error naming the column, rather than answered. Reaching
  such an optimum needs a phase 1 that lends nothing. No instance of the 139
  ever reached the lent bound, so the loan size is a performance parameter and
  not a correctness risk, and Q9 closed on that (D31).
- `jaos_solution` refuses to report anything for a solve that found no
  optimum, under the same rule as `jaos_objective`: a buffer of zeros cannot
  be told apart from an answer that is genuinely zero.
- Builds pin `-ffp-contract=off`. C23 lets the compiler fuse `a*b+c` where
  the hardware offers it, which would make the same model produce different
  bits on different machines — the opposite of what D8 promises.
- `docs/plan-m2.md` is now `docs/research/plan-m2.md`, alongside the notes it
  is built on. It is a proposal; `PLAN.md` is the plan. Its speedup figures
  come from the literature and none has been measured in JAOS (D17).

### Fixed

- The primal clean-up decides which columns want a pivot before it moves any
  of them, and calls in each candidate's own cost loan before judging it. It
  had been able to take exactly one pivot per call: `wants_a_pivot` reads the
  duals out of `rho`, which the first pivot overwrites with a pricing row, and
  underneath that `pivot()` lends away every other candidate's sign condition.
  `pilot87` goes from an objective `2.28e-3` out to `1.33e-7` relative, its
  dual violation from `1.87e-5` to `0`, and it gets cheaper. **The standard
  Netlib gate now reports PASS** — 94 of 94 on every condition. 0 regressed,
  3 improved; 92 of the 94 keep their exact iteration count (D30).
- The refresh that verifies a declaration of optimality refines its two
  solves — one step of iterative refinement on `x_B` and on `y`. `pilot` was
  rejected on a row `1.73e-6` outside its bound that no basic variable
  violates; it is the residual of the basis solve, and refining takes it to
  `6.73e-13`. Checker goes from 92 to 93 of 94. The solve loop is deliberately
  left unrefined: 93 of the 94 instances take exactly the iteration count they
  took before, and total work falls 0.029% (D29).
- A basis the factorization finds singular is repaired rather than ending the
  solve. `jaos_internal.h` had always said rank deficiency is a fact the
  caller acts on by replacing basis columns, and the caller did not: pairing
  the rows the factorization could not cover with the basis positions it
  could not use gives a completion that is triangular by construction.
  `gran` reaches INFEASIBLE where it used to give up after 1728 iterations,
  taking the infeasible gate to 29 of 29, and no instance of the standard set
  moves. `repair_singular_basis` carries the proof and why no unit test can produce one.
- Every reference optimum in `bench/netlib.manifest` is now Koch's.
  `maros-r7` and `pilot87` had fallen back to the netlib readme, so `pilot87`
  was judged against a value 1.26e-6 from the exact optimum where this gate's
  tolerance is 1e-6. The report's PostScript carries the table as literal
  text where its PDF did not; `bench/koch-refs.py` extracts it and
  `bench/koch-verify.py` reproduces 82 of the pinned references exactly with
  none in disagreement. No verdict moves.
- The acceptance runner reads the whole manifest before solving anything, and
  closes the file first. It used to parse a line, solve that instance, then
  parse the next, holding the file open across the run; a rewrite in that
  window shifted every later byte offset and `sctap2` was judged against a
  reference no line of the file ever carried. Instance paths are now
  bound-checked rather than silently truncated.
- PLAN 2.8 and Q10 recorded a cause for the dual violations that the
  measurement does not support, and both now carry the measurement instead.
  The six open instances had been grouped by the size of the number the
  checker prints — which is a multiplier's magnitude, not a distance —
  and measured one at a time they are four separate things. Two repairs for
  the largest group were implemented, measured and reverted; both reported
  `greenbea`, a feasible model, as INFEASIBLE.
- `jaos_check_solution`'s contract in the public header said "tol is an
  absolute tolerance on violations" after D23 had already scaled the
  bound-proximity test. It now says which half is which.
- A solve no longer stops on values it carried rather than computed. Basic
  values and the factorization both drift as pivots accumulate, so a solve
  stopped exactly when its numbers looked feasible without being it. The
  point is recomputed from a fresh factorization and priced again before the
  answer is accepted — on `afiro`, the difference between finishing 1.8e-5
  from a bound and finishing on it. Costs one refactorization per solve,
  which the work counter bills (D20).
- An unbounded model is identified by a direction along which the objective
  has nothing to stop it, checked against the bounds the model declares. It
  used to be identified by a variable coming to rest on a bound the solver
  had invented, which a model with a large but finite optimum does too. On
  3000 generated LPs, 8 called unbounded now solve to an optimum the checker
  accepts, with no other model changing its answer or its work (D19).
- The Netlib gate accounts for the objective constant an MPS file can declare
  through an `RHS` entry on its objective row. JAOS applies it, following the
  convention CPLEX documents; the published optima do not, so a correct
  answer differed from both reference sets by exactly that constant on the
  one instance where it is visible. The reader is unchanged, deliberately.
- Settling up can no longer park a basic variable on an artificial phase-1
  bound, which would manufacture the evidence of unboundedness after the
  verdict was already read.
- A failed solution-buffer allocation no longer leaves the model believing
  the buffers exist; a later solve would have written through the missing
  ones.
- The published work count includes the final kernel run of publishing
  itself. It used to be taken one BTRAN too early.
- The release build, which `main` could not complete: `build_crash_basis`
  declared `nsel` twice and left a variable unused, and `-Werror` refused the
  object. Nothing depended on either variable.
- A 648-byte leak across 13 allocations that AddressSanitizer reported in the
  LU tests, gone with the revert of the code that introduced it.

### Added — the initial implementation

- `bench/`: the Netlib acceptance gate, runnable. `make netlib` fetches the
  94 instances of the standard set — pinned by sha256 in a committed
  manifest, never stored in the repository — and judges each solve against
  the published optimum, the independent checker, and a second solve that
  must agree bit for bit. Reference values are Thorsten Koch's exact
  rationals where they exist; the netlib readme's differ from them beyond
  the gate's tolerance on eight instances. The last full run is recorded in
  `bench/results/netlib.txt` and the README summarises it. The gate is not
  met yet, and the record is what says so.
- `docs/tolerances.md`: every number a solve compares against, which space it
  acts in, and the formulas the independent checker judges with.
- `docs/work-units.md`: what a work unit is, where each weight is charged, and
  what sits outside the budget — so a work limit can be chosen rather than
  guessed.
- `README.md`: what JAOS is, what it solves today, how to build it, and the
  design commitments that constrain it.

- `DECISIONS.md`: the durable record of closed design decisions — language and
  toolchain, dependency policy, determinism, problem scope, and the rules under
  which correctness and speed may be claimed.
- `PLAN.md`: the staged build order for the whole declared scope, and a fully
  specified first milestone — LP correctness on the Netlib set — with acceptance
  criteria, draft tolerances and work-unit weights, and a verified bibliography.
- Build scaffold: `make all | test | sanitize | clean` on GCC 14 / C23; the
  public header `jaos.h` with status codes and version query; Unity v2.7.0
  vendored under `tests/` as the dev-time test harness.
- Model core: create/load/query/free for LPs in bounded form, with validating
  copy-on-load (sorted CSC, explicit zeros dropped, structural errors rejected)
  and an internal row-wise mirror.
- Independent solution checker: judges a claimed primal (and optionally dual)
  solution against the model in original space — bound and activity violations,
  dual sign conditions including complementary slackness, and the objective gap.
- MPS reader (`jaos_read_mps`): fixed and free layouts, RANGES, all continuous
  BOUNDS types, OBJSENSE, objective constants, Fortran D exponents,
  locale-independent number parsing; every rejection carries a line number via
  `jaos_model_error`. Integer constructs are recognized and rejected until MILP
  lands. Dialect decisions documented in `docs/format-support.md`.
- LP-format reader (`jaos_read_lp`): CPLEX-style core dialect with line-wrapped
  expressions, labels, glued coefficients, repeated-variable summing, all bound
  forms including `free` and infinities. Ranged constraints, constants inside
  constraints and integer sections are recognized and rejected with line
  numbers.
- Matrix scaling: Curtis-Reid by default, geometric-mean equilibration as an
  option. Factors are exact powers of two, so applying them adds no rounding
  error, and the stored matrix is left untouched. See `docs/scaling.md`.
- Sparse LU factorization with Markowitz threshold pivoting, and the forward
  and transposed triangular solves built on it. A singular matrix is reported
  through the factorization's rank rather than treated as a failure.
- Forrest-Tomlin basis updates: replacing one basis column costs work
  proportional to the change instead of a full refactorization. A replacement
  that would leave an untrustworthy pivot is refused rather than accepted
  quietly.
- Deterministic work counter wired into the factorization and solve kernels —
  the currency of the reproducible budget.
- Dual simplex: `jaos_solve` finds optimal solutions for linear programs with
  bounds, ranged rows and equalities, in either objective sense, and reports
  infeasible models as infeasible. Results come out through `jaos_solution`,
  `jaos_objective` and `jaos_status_of`.
- Budgets: `jaos_set_work_limit` (reproducible) and `jaos_set_time_limit`
  (wall-clock), reported back through `jaos_work_units` and
  `jaos_iterations`. Pricing and the ratio test are charged to the work
  counter, so a work limit bounds the whole solve rather than only its
  linear algebra.
- A model this build cannot solve yet reports `JAOS_SOLVE_UNSUPPORTED` with
  an explanation, rather than being rejected as invalid input.
- `jaos_objective` reports through a status and an out-parameter, so a
  genuine objective of zero is distinguishable from having no answer.
- Dual steepest-edge pricing: the solver picks which violated constraint to
  repair by how far the repair actually travels, not by the size of the
  violation as written. A model in mixed units no longer sends it after the
  row with the smallest units first.
- Harris' two-pass ratio test: the pivot is chosen by spending a bounded
  amount of dual feasibility — one tolerance's worth — on a better
  conditioned one. Where many candidates block at the same point, which is
  most of a degenerate solve, this is what decides between them.
- Bound flipping in the ratio test: a variable with two finite bounds no
  longer stops an iteration short. It is swapped to its other bound and the
  step carries on, so a model whose answer fills one bounded column after
  another is solved in one long step instead of one step per column.
- The dual feasibility the ratio test spends on better-conditioned pivots is
  now lent rather than given away: the cost of the affected column is moved
  just enough to keep it feasible, the loan is recorded, and it is called
  back before any answer is reported, so what comes out belongs to the
  problem that was asked about. Where calling it back leaves a column on the
  wrong side, it is swapped to its other bound whenever that keeps the
  solution feasible — which can only improve the objective.
- Steepest-edge pricing weights are checked against an exactly known value
  every iteration, repaired where the exact one is available, and restarted
  when they have drifted past being informative. Pricing that has quietly
  stopped meaning anything costs iterations rather than answers, so nothing
  else would have reported it.
- Solving now runs on a scaled copy of the model, computing a Curtis-Reid
  scaling first if none was chosen, so a model whose coefficients span many
  orders of magnitude is solved on numbers that do not. The model itself is
  untouched and every answer — values, activities, duals, reduced costs —
  comes back in the units it was written in.
