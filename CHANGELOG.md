# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
