# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries say what changed and what it cost. The reasoning lives where it
belongs: `DECISIONS.md` for closed decisions, `PLAN.md` for what is still
open, `bench/README.md` for the gate, and the commit each entry came from.

## [Unreleased]

### Added

- A primal ratio test, used only to clean up after the dual solve has
  finished. `greenbea` goes from REJECTED to checker ok for **eight pivots**,
  its objective moving from -72555233.859378919 to -72555248.129846007 against
  Koch's exact -72555248.129845992 — fifteen significant digits — and its dual
  violation from 2.66 to 0. `pilot`'s objective comes inside tolerance from
  390x outside it, with its dual violation at 0 and its gap at 6.6e-14.
  Standard set 0 regressed, 2 improved, 0 new; Kennington and the infeasible
  set 0/0/0, both still PASS, with fifteen of Kennington's sixteen
  bit-identical.

  This grows M1's declared scope, deliberately and by one thing. What is in is
  a ratio test and the basis change `pivot()` already performs, on a column
  the residue names. What stays out is everything that makes a primal
  *method*: pricing to choose an entering column, a phase 1 of its own, its
  own weights. A primal simplex chooses what enters; this is told. PLAN 2.1
  now says where that line runs, because a line that moves once moves again by
  drift if nobody writes down where it stopped.

  It is what `greenbea`'s ten columns needed and what nothing else could
  reach: they rest at a lower bound with no upper bound, so the term any
  repair test could weigh — `w * bound` — does not exist for them. Two
  guarantees come free that the re-entry of D25 does not have: the point stays
  primal feasible, because that is what the ratio test is for, and the
  objective cannot rise, because `d_q` points the way `q` travels. D28.

- Bland's rule, as a fallback a detected cycle switches on. `grow15` had been
  running to the internal iteration guard at 189201 iterations; instrumented,
  it is a cycle of period four over two rows and four variables, repeating
  bit for bit from iteration ~3000 — not the stall Q10 diagnosed, since half
  its iterations take a real dual step of ~1.7e-6 and the four cancel
  exactly. It now solves in 21653 iterations at an objective matching Koch's
  to sixteen digits.

  PLAN recorded that Bland's rule had been tried and did not fix it. What had
  been tried was a smallest-index tie-break inside the Harris window, which
  carries none of the guarantee: that needs the exact minimum quotient, no
  widening, and the index rule on the leaving choice too. Built properly it
  solves `grow15` outright — and cannot be the default, because it costs 25x
  on `25fv47` and takes `grow22` from 2179 iterations to no answer at all.

  So the trigger is failing to improve on the best total primal infeasibility
  for `STALL_FACTOR` times `nrow + ncol + 1`, computed in a loop `price_row`
  was already running, and it switches off again the moment the total
  improves. The factor is 10 against a measured worst healthy plateau of 1.67
  (`truss`) and a cycling one of 198 (`grow15`) — and it is a constant that
  cannot change an answer, only how many iterations it takes to reach one.
  Every instance that does not cycle is bit-identical. D26.

- The dual simplex re-enters from the settled point. Once the borrowed costs
  are called in, any nonbasic whose sign condition is breached and that has a
  real bound on the other side is sent to it, and the method runs again from
  there — bound flipping, which the ratio test already does, applied at the
  end instead of mid-iteration. `nesm` goes from REJECTED to checker ok with
  its dual violation at exactly 0; `pilot` and `pilot87` improve by two
  orders of magnitude on the dual and by seven and three on the gap without
  changing verdict. 0 regressed, 1 improved, 0 new on the standard 94; 0/0/0
  on the other two sets, both still PASS. The settled point is saved first
  and any re-entry not ending in a second optimum is discarded — a model just
  proved to have an optimum has not become infeasible, and returning
  INFEASIBLE for one is exactly how the two earlier repairs of this residue
  failed. D25 carries the reasoning and what the mechanism cannot reach.
- Three fields in `jaos_check_report`, all of which decide nothing.
  `gap_positive` and `gap_negative` split the gap into the two sums it is the
  difference of, so `P - P* <= Q` becomes a bound the checker publishes
  instead of a consequence of a hypothesis nobody was testing; a gap can be
  small because both halves are or because two large ones cancelled, and it
  cannot say which. On the 94 that is not hypothetical: `finnis` reports a
  gap of 3.96e-11 over halves of 4.25e-5 and 2.89e-5. `max_row_violation_relative`
  is the row residue against what the row carries, which D24 said it would
  keep in the report and out of the predicate. The record carries all three
  per instance. No verdict moved (D24).
- A fuzzer for both readers, `tests/test_fuzz.c`: truncation at every offset
  of every corpus file, seeded edits, random bytes, keyword salad, and named
  edge cases, each fed to both readers. 11543 cases in the suite, 1.6M under
  `JAOS_FUZZ_SCALE=200`, clean under ASan+UBSan. Closes the M1 gate's
  condition 4, which had been asserted and never tested (PLAN 2.8.4).
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
  diffs against it. The gate's own verdict reads `NOT MET` for the whole of
  M1, so it cannot answer the question every change raises — did this make
  anything worse? Work may grow 2x before it counts as a regression.
  `make netlib-baseline` rewrites it and is never a side effect of running
  the gate (D21).

### Changed

- The re-entry moves a column when its wrong sign costs objective and the
  reduced cost carrying it is a number rather than rounding. `etamacro` goes
  from REJECTED to checker ok for 9 extra iterations; every other instance
  of the standard 94 is bit-identical and `pds-20` costs one iteration more
  than its baseline. It took three attempts and the first two are in
  PLAN 2.8.1 because each is what pointed at the next.

  The breach has no scale-free reading — `etamacro`'s is 4.89e-8 scaled and
  1.56e-6 published, inside the solver's zero on one side of a change of
  variable and past the checker's tolerance on the other — and judging it in
  the published space takes `pilot87` from a solve to a tripped iteration
  guard. What does have one is the term it contributes to `P - D`: `|d|`
  times the width of the box, which comes out the same in either space
  because `publish` divides `d` by the same `gamma` it multiplies the value
  by. It is also exactly what D24 made the checker publish as `Q`.

  That alone cost `pds-20` 3.2x its work, by flipping columns whose reduced
  costs are 1e-10 — rounding, carried by boxes a thousand wide. So `|d|`
  counts only above `eps` times the traffic through its column, which is
  D23's argument for a row read down a column, not a second test: a product
  is only as good as its factors. Measured over both feasible sets, that
  ratio is at least 5.055e8 on every column that should move and 2.1 to 36
  on `pds-20`'s — seven orders of daylight, and the margin saturates. D27.

  `tests/test_simplex.c` carries the case that matters more than the
  campaign: a three-column model whose optimum is readable by eye at
  `x = (1, 0, 0)` and objective zero. The solver used to stop at 4.997e-8
  with a certificate that did not carry, and the test asserted that wrong
  answer. It now reaches the optimum, gap zero, both halves zero.
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
  such an optimum needs a phase 1 that lends nothing, which is still open
  (PLAN Q9).
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

- A basis the factorization finds singular is repaired rather than ending the
  solve. `jaos_internal.h` had always said rank deficiency is a fact the
  caller acts on by replacing basis columns, and the caller did not: pairing
  the rows the factorization could not cover with the basis positions it
  could not use gives a completion that is triangular by construction.
  `gran` reaches INFEASIBLE where it used to give up after 1728 iterations,
  taking the infeasible gate to 29 of 29, and no instance of the standard set
  moves. PLAN 2.8.2 has the proof and why no unit test can produce one.
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
