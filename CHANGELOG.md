# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The acceptance runner can judge a set whose instances are meant to be
  refused. `-e infeasible` keeps the shape and determinism checks, drops the
  reference objective and the checker — there is no solution to judge — and
  fails an instance that comes back with an optimum, flagging it as a false
  optimum. That is the only check anywhere in M1 that looks for a wrong
  answer rather than confirming a right one, and the gate has been asking
  for it since it was written (PLAN 2.9).
- `bench/netlib-kennington.manifest` and `bench/netlib-infeas.manifest`,
  with `make netlib-kennington` and `make netlib-infeas`. The manifests are
  headers only: both sets are distributed by netlib in packed emps form,
  Koch's plain-MPS mirror covers neither, and the acquisition route is open
  (PLAN Q6). The targets fail today and say why. `bench/fetch.sh` now takes
  `-m` and `-b` so it can serve any set once one is pinned.
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
