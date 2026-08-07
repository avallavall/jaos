# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A fuzzer for the two readers, `tests/test_fuzz.c`, which closes the M1
  gate's condition 4 — truncated and corrupted input must produce errors and
  never crash. That condition had been asserted and never tested:
  `tests/data/` covers malformed *content*, one file per rejection class, and
  every one of those files is well-formed enough to reach the check that
  rejects it. Nothing was cut mid-record, no byte was flipped inside a
  number, nothing was empty and nothing was random.

  Five classes, all deterministic (D8) — the corpus is the sorted contents of
  `tests/data`, the mutations come from a splitmix64 written out in the test
  rather than `rand()`, so the same commit fuzzes the same bytes on every
  machine and a failure reproduces from its case label: every prefix of every
  corpus file, small random edits, uniform noise, random sequences of real
  keywords, and named shapes at the sizes where a buffer decision changes.
  Every case is offered to both readers, since an LP file handed to the MPS
  reader is corrupted input by any definition. What is asserted is that the
  reader returns one of the statuses its API declares, that a failed read
  leaves the previous problem untouched, and that the same bytes read twice
  give the same answer.

  **11543 cases in the suite, 1623443 under `JAOS_FUZZ_SCALE=200`, all clean
  under ASan+UBSan.** A fuzzer that finds nothing on its first run is not
  evidence until it is shown to be able to find something, so the instrument
  was checked the way the gate's rules are: `split()` in the MPS reader had
  its bounds test changed from `n == MAXTOK` to `n > MAXTOK`, a one-token
  stack overflow, and the fuzzer caught it at `src/mps.c:46` — in the
  random-edit class, not in a hand-written case.
- Baselines for the other two gate sets, and `make netlib-infeas-baseline`
  and `make netlib-kennington-baseline` to rewrite them. `bench/netlib-infeas.baseline`
  existed but nothing passed it to the runner, and Kennington had none at
  all. Both of those gates report PASS, which is exactly the state in which a
  summary line cannot show a change: an instance that still ends INFEASIBLE
  after eighty times the work has regressed and only a per-instance diff says
  so (D21).
- The acceptance runner can judge a set whose instances are meant to be
  refused. `-e infeasible` keeps the shape and determinism checks, drops the
  reference objective and the checker — there is no solution to judge — and
  fails an instance that comes back with an optimum, flagging it as a false
  optimum. That is the only check anywhere in M1 that looks for a wrong
  answer rather than confirming a right one, and the gate has been asking
  for it since it was written (PLAN 2.9).
- The two remaining instance sets of the M1 gate, pinned and running for the
  first time: `bench/netlib-kennington.manifest` (16) and
  `bench/netlib-infeas.manifest` (29), with `make netlib-kennington` and
  `make netlib-infeas`. Both are served by netlib in packed form, so
  `bench/fetch.sh` expands them with netlib's `emps` — fetched, verified
  against a pinned sha256, built to a temporary directory and never stored
  here, the same rule the instances follow (PLAN Q6, D11). `fetch.sh` takes
  `-m`, `-b` and `-p` for that, and each set fetches into its own directory
  because `greenbea` names two different models across sets.

  **Kennington passes outright: 16 of 16 on every condition**, including
  ken-18 at 105127x154699 and osa-60 at 232966 columns, with the independent
  checker green throughout. An order of magnitude past the standard set, so
  whatever ails the seven open instances there, it is not that the readers
  or the factorization stop working at scale.

  The infeasible set reports **28 of 29 correctly refused with no false
  optima anywhere**, which is the outcome it exists to measure. `gran`
  returns a numerical error instead of a verdict — it does not claim an
  optimum, it fails to reach one — and is a new open item in PLAN 2.8. Its
  shape check also caught a defect in the manifest on the first run:
  `greenbea` carries 111 free rows beyond its objective row and JAOS loads
  all of them, where the count first written had excluded every N row.
- The Netlib gate records what every instance did, in
  `bench/netlib.baseline`, and `make netlib` diffs against it. The gate's own
  verdict is all-or-nothing and reads `NOT MET` for the whole of M1, so it
  cannot answer the question every change raises — did this make anything
  worse? Any of an instance's four predicates going from holding to not
  holding now fails the run on its own, and so does work growing past 2×.
  Improvements are reported too. `make netlib-baseline` rewrites the
  baseline and is deliberately never a side effect of running the gate.

### Changed

- The checker's relative gap is now scaled by
  `1 + |primal| + |dual|` rather than by `max(1, |primal|)`. Normalising by
  one side alone reports a larger error the further the two objectives are
  apart, which is backwards: the scale should say how big the numbers being
  compared are, not how badly they disagree. This is the PDLP form HiGHS
  adopted and the shape of the DIMACS error measures. It moved no verdict on
  the Netlib set — 0 regressed, 0 improved against the recorded baseline —
  which is the only reason it is in: a change to an acceptance criterion that
  improved the score would have to be argued for on much more than
  convention.
- The independent checker no longer drops a multiplier from the dual
  objective for being small. Below the tolerance it is still held to no
  sign condition — requiring a variable onto a bound on the strength of a
  number indistinguishable from zero would reject solutions for their
  rounding — but it contributes `w * bound` like any other, because what
  gets dropped is small only if the bound is. A multiplier of 1e-7 on a
  variable resting on a bound of 1e6 carries 0.1 of dual objective, and
  losing it while the primal still counts its cost invented a relative gap
  of 9% on a pair that was optimal on every count. That is what had been
  rejecting `pilot-ja`, whose dual violation is exactly zero. Netlib
  checker-green goes from 86 to 87 with no other instance moving. D22 has
  the reasoning, including the two repairs that were implemented, measured
  and rejected first — one of which passed the entire gate while being
  vacuous. `docs/tolerances.md` carries the rule.
- The solver core is back to where it was on 2026-08-06, reverting ten
  commits that had not been run against the Netlib set. Measured per
  instance rather than by the summary line, they fixed `grow15` and `nesm`
  and cost `pilot-we` (a feasible problem reported infeasible), `pilotnov`
  (checker rejection) and `grow22` (2179 iterations to 167865). The summary
  line was identical before and after, because the gains and losses
  cancelled. `grow15` therefore remains open, and PLAN.md Q10 now carries
  what the two attempts at it established.
- `docs/plan-m2.md` is now `docs/research/plan-m2.md`, alongside the two
  research notes it is built on. It is a proposal; `PLAN.md` is the plan.
  Its speedup figures come from the literature and none has been measured
  in JAOS, which under D17 is the difference between a reason to try
  something and a claim about this solver.
- A model whose optimum lies beyond the bound dual phase 1 lends is now
  refused with a numerical error naming the column, rather than answered.
  Reaching such an optimum needs a phase 1 that lends nothing, which is
  still open; until then the solve says it cannot get there instead of
  reporting the model unbounded.
- `jaos_solution` now refuses to report anything for a solve that found no
  optimum, under the same rule as `jaos_objective`: a buffer of zeros could
  not be told apart from an answer that is genuinely zero.
- Builds pin `-ffp-contract=off`. C23 lets the compiler fuse `a*b+c` where
  the hardware offers it, which would make the same model produce different
  bits on different machines — the determinism promise says exactly the
  opposite.

### Fixed

- A basis the factorization finds singular is now repaired instead of ending
  the solve. `jaos_internal.h` has always said that rank deficiency is a fact
  the caller acts on by replacing basis columns, and the caller did not act
  on it — `refresh()` reported `NUMERICAL_ERROR`, and with no message.

  What the LU hands back is exactly the two lists the repair needs: the rows
  it pivoted and the basis positions it used. Whatever is missing from those
  lists is a row nothing covers and a column that turned out to depend on the
  rest, equally many of each. Pairing them off and putting the logical of an
  uncovered row into the dependent position gives a basis that is nonsingular
  by construction rather than by hope — order the rows as (pivoted,
  uncovered) and the result reads `[[P, 0], [Q, -I]]`, whose determinant is
  `det P * det(-I)`, and P is nonsingular because triangularizing it is what
  the factorization just did.

  **`gran` of the infeasible set now returns INFEASIBLE**, the verdict it was
  owed, after 2058 iterations; it used to give up at 1728. That takes the
  infeasible gate from 28 of 29 to **29 of 29, PASS** — condition 1c of
  PLAN 2.9 — and the standard set is untouched: 0 regressed, 0 improved, 0
  new against `bench/netlib.baseline`. That it changes nothing elsewhere is
  the point, not a disappointment: the repair only runs where the solve
  previously ended, so no instance that already worked can take a different
  path.

  A singular basis is not something a model can cause. The dual simplex only
  pivots on an alpha above `PIVOT_MIN`, so every basis it assembles is
  nonsingular in exact arithmetic and a singular one is always the residue of
  carried error — which is why no small test can produce one, and why the
  new tests in `tests/test_simplex.c` cover the family `gran` belongs to
  (rank-deficient constraint matrices, where the danger is a wrong verdict
  rather than a crash) instead of claiming to exercise the repair itself.

  The three sites that report a numerical error out of `refresh` now say what
  happened and at which iteration. They used to publish `NUMERICAL_ERROR`
  with an empty message, which is how `gran` cost a diagnosis: there was
  nothing to read.
- PLAN 2.8 and Q10 recorded a cause for the dual violations that the
  measurement does not support, and both now carry the measurement instead.
  The six instances the checker rejects had been grouped by the size of the
  number reported, and that number is the magnitude of the offending
  multiplier — it says nothing about how far anything is from where it
  should be. Measured one at a time, against the distance from the bound and
  the traffic through the row or column, they are four separate things:

  `finnis`, reported at 28 and called structural, is the most accurate answer
  of the six. Its row lands 1.52e-6 from a bound while carrying 4.0e10 of
  traffic — a fifth of one ulp at that scale — its duality gap is 3.96e-11,
  and it publishes no violated sign condition in scaled space at all. The
  checker's "at a bound" test is absolute where only a relative test has
  meaning, and no double-precision answer can pass it on that row.

  Q10 attributed the rest to cost shifting, and it is right — but confirming
  that took a second measurement, because the first asked the wrong question.
  Reading `shift[v]` on the column that violates its sign condition says the
  shifts explain one case of three. That test is wrong: `d_j = c_j − y' M_j`
  with `y = B^-T c_B`, so a shift resting on a *basic* variable moves every
  nonbasic reduced cost at once and the violating column need carry none of
  its own. Measuring `d` on both sides of the settlement instead, every
  offending column on `etamacro`, `nesm` and `greenbea` is dual feasible
  before the shifts come off and infeasible after.

  What that route costs is the finding. On `greenbea`, repaying shifts of at
  most 7.09e-6 across 907 basic variables takes one reduced cost from +5.67
  to −1.33 — a perturbation four orders below every tolerance in PLAN 2.6
  arriving as a violation of five, with `B^-1` on that basis standing between
  them. So the size of a residue is not evidence about the size of its cause,
  which is what had kept `finnis` and `greenbea` in one group. A shift
  reaches `c_B` because a variable shifted while nonbasic keeps the perturbed
  cost when it enters the basis and the repayment waits for the end of the
  solve; repaying at the moment of entry is the candidate repair, untested,
  and it touches every instance.
- `bench/README.md` and the manifest said Koch's results do not cover
  `maros-r7` and `pilot87`, so both take their reference from the netlib
  readme. His tables cover both. His value for `pilot87` is about
  301.71034733 against the 301.71072827 the manifest carries — a difference
  of 3.8e-4 where the gate's tolerance on that instance is 3.0e-4, so it is
  being judged against a reference outside tolerance of the exact optimum.
  The verdict is unchanged either way and the reference is left alone: the
  exact rationals were published at a URL that no longer resolves, and
  parsing them out of the report PDF reproduced only 23 of the 92 values
  already known to be his. Recorded rather than guessed.
- The release build, which `main` could not complete: `build_crash_basis`
  declared `nsel` twice and left a variable unused, and `-Werror` refused
  the object. The only reason anything built was an uncommitted change in a
  working copy. Nothing depended on either variable — `nsel` was
  incremented in two places and read in none.
- A 648-byte leak across 13 allocations that AddressSanitizer reported in
  the LU tests, gone with the revert of the code that introduced it.
- A failed solution-buffer allocation no longer leaves the model believing
  the buffers exist: a later solve on the same model would have written
  through the missing ones.
- The published work count now includes the final kernel run of publishing
  itself; it used to be taken one BTRAN too early.
- Settling up can no longer park a basic variable on an artificial phase-1
  bound: that would manufacture the evidence of unboundedness after the
  verdict was already read.
- The Netlib acceptance gate now accounts for the objective constant an MPS
  file can declare through an `RHS` entry on its objective row. JAOS applies
  it, following the convention CPLEX documents; the published Netlib optima
  do not include it, so a correct answer differed from both reference sets
  by exactly that constant on the one instance where it is visible. The
  manifest carries the constant and the comparison allows for it. The reader
  is unchanged, deliberately: dropping the constant would break every model
  whose author meant it.
- A solve no longer stops on values it carried rather than computed. Basic
  values and the factorization both drift as pivots accumulate, and
  optimality was being judged on the drifted ones — so a solve stopped
  exactly when its numbers looked feasible without being it. The point is
  now recomputed from a fresh factorization and priced again before the
  answer is accepted. On the first Netlib instances read, this is the
  difference between a solution the independent checker rejects and one it
  accepts: `afiro` finished 1.8e-5 away from a bound, 177 times outside the
  solver's own tolerance. Costs one refactorization per solve, which the
  work counter bills.
- An unbounded model is now identified by a direction along which the
  objective has nothing to stop it, checked against the bounds the model
  itself declares. It used to be identified by a variable coming to rest on
  a bound the solver had invented, which a model with a large but perfectly
  finite optimum does too — and such a model was reported unbounded. On a
  sweep of 3000 generated LPs, 8 that were called unbounded now solve to an
  optimum the independent checker accepts, and no model that already solved
  changed its answer, its iteration count or its work units.

### Added

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
