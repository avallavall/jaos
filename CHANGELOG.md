# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
