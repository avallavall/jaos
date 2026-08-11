# External Integrations

**Analysis Date:** 2026-08-12

## Summary

JAOS has **no runtime external integrations**. It is an offline numerical
library: no network calls, no database, no auth provider, no webhooks, no
telemetry, and no config-service dependency exist anywhere in `src/` or
`include/jaos.h`. Everything a caller gets from JAOS comes back through the
C API, synchronously, in-process.

What *does* cross a process or file-system boundary is dev-time tooling
(fetching benchmark data and competitor solvers to measure against) and the
file formats the library itself reads. Both are documented below in place of
the usual API/DB/auth sections, per the mapping brief.

## APIs & External Services

**None at runtime.** No HTTP client, no SDK for any cloud service, is linked
into `libjaos.a`. The only network access anywhere in the repository is in
dev-time shell scripts (see "What crosses the process boundary" below), and
none of it runs as part of `make all`, `make test`, or `make sanitize`.

## Data Storage

**Databases:** None. JAOS has no persistence layer of its own.

**File Storage:** Local filesystem only, and only for two things:
1. Reading a problem from a file the caller names (`jaos_read_mps`,
   `jaos_read_lp`, `include/jaos.h:271,277`).
2. Dev-time benchmark instances and results (`bench/instances*/`,
   `bench/results/`), never touched by the library API itself.

There is no writer in the public API: "There is no way to write a file"
(`README.md`). A caller who wants to persist a model or a basis must do so
themselves, e.g. via `jaos_basis`/`jaos_set_basis` round-tripped through
their own storage.

**Caching:** None.

## Authentication & Identity

Not applicable. JAOS is a linkable C library with no network surface, so
there is nothing to authenticate against.

## Monitoring & Observability

**Error Tracking:** None built in. Errors surface synchronously as
`jaos_status` return values plus `jaos_model_error()` for a human-readable
message (`include/jaos.h:40-46,281`).

**Logs:** No default log destination — "a library that writes to stdout
because nobody told it not to cannot be embedded" (`include/jaos.h:330-335`).
A caller installs `jaos_set_log_callback` with a `jaos_log_fn` and picks a
`jaos_log_level` (`OFF` / `SUMMARY` / `PROGRESS` / `DETAIL`); until then the
library is silent. Logging is proven not to affect the computed answer — the
same model solved at `JAOS_LOG_DETAIL` returns identical bits to one solved
silently, checked over all 139 reference instances (D8).

## CI/CD & Deployment

**Hosting:** Not applicable — JAOS ships as a static library and header
(`build/release/libjaos.a` + `include/jaos.h`), not a deployed service.

**CI Pipeline:** None found in the repository (no `.github/workflows/`, no
CI config file). Builds and the acceptance gate are run manually via
`make` inside WSL, per `CLAUDE.md`.

## Environment Configuration

**Required env vars:** None. All runtime behaviour is set through the C API
(`jaos_set_work_limit`, `jaos_set_time_limit`, `jaos_set_primal_tolerance`,
`jaos_set_dual_tolerance`, `jaos_set_log_callback`, `jaos_set_log_level`,
`jaos_set_progress_callback` — `include/jaos.h:300-419`). Build-time
behaviour is set through Make variables (`CC`, `AR`, `J`, `LTO`, `NATIVE`,
`EXTRA_CFLAGS`, `PGO_LOAD`), not environment variables.

**Secrets location:** None exist in this repository — no credentials, no API
keys, no `.env` files.

## Webhooks & Callbacks

**Incoming:** None (no server, no HTTP listener).

**Outgoing:** None over the network. JAOS does define two **in-process C
callback types** that are the closest analogue:
- `jaos_log_fn` — invoked synchronously during a solve to emit a log line;
  must not call back into JAOS on the same model (`include/jaos.h:349-352`).
- `jaos_progress_fn` — invoked periodically during a solve with iteration
  count, work units and current primal infeasibility; may request the solve
  stop (`JAOS_CALLBACK_STOP`) but may not steer *how* it solves
  (`include/jaos.h:360-418`). Paced by iteration count, never by a clock, so
  determinism (D8) is preserved regardless of what the caller does with it.

## What crosses the process boundary (dev-time only)

None of the following ships in `libjaos.a`, is built by `make all`, or is
linked into the library. All of it is invoked manually or from a bench
`make` target, and everything downloaded is checksum-verified before use and
never committed to the repository (enforced by `.gitignore`).

**Benchmark instance corpora** — fetched by `bench/fetch.sh`:
- Standard set (94 instances): Koch's plain-MPS mirror at
  `https://www.zib.de/koch/perplex/data/netlib/mps` — gzipped MPS, no
  expansion needed.
- Kennington set (16 instances) and infeasible set (29 instances): netlib's
  packed format at `https://netlib.org/lp/data/kennington` and
  `https://netlib.org/lp/infeas`, expanded with netlib's own `emps.c`
  converter (fetched from `https://netlib.org/lp/data/emps.c`, pinned by
  sha256 `fee41f544f6873a5e12bc598947828dc9964ef0676162e4df55e915760e2be22`,
  compiled to a temp dir, never stored in the repo).
- Every fetched file's sha256 is checked against `bench/netlib.manifest`,
  `bench/netlib-kennington.manifest`, `bench/netlib-infeas.manifest` before
  use; a mismatch aborts the fetch.

**Competitor solvers for `bench/compare/`** — fetched, checksum-verified and
built by `bench/compare/fetch-solvers.sh`, pinned in
`bench/compare/solvers.manifest`:
| solver | version | licence | source |
|---|---|---|---|
| HiGHS | 1.15.1 | MIT | `github.com/ERGO-Code/HiGHS` |
| SoPlex | 8.0.3 | Apache-2.0 | `github.com/scipopt/soplex` |
| Clp | 1.17.11 | EPL-2.0 | `github.com/coin-or/Clp` (plus CoinUtils/Osi, pinned in `bench/compare/clp-deps.manifest`) |

Licences are re-verified at fetch time (grepped out of each archive's
licence file), not just trusted from the manifest, "because they change
between versions." Binaries land in `bench/compare/solvers/`, gitignored,
built into a scratch prefix nothing else on the machine sees. This is
explicitly not a D2/D12 violation: running another solver as a timing
competitor is not "code read from other solvers" — no competitor source is
read by JAOS's own code (`bench/compare/README.md`).

**Comparison harness** (`bench/compare/`): `jaos_time.c` (built as
`build/bench/jaos_time`) times JAOS itself against HiGHS/SoPlex/Clp on the
same instance set, at four escalating "tiers" (T0–T3) that isolate what each
missing JAOS feature (presolve, algorithm choice, tuned defaults) costs.
Records seconds — deliberately excluded from every other bench output — in
`bench/compare/results/`, always tagged with the machine that produced them,
since wall-clock numbers are explicitly not portable evidence (D17). This
harness is a separate `run-compare.sh`/`jaos_time.c` pipeline, not part of
the acceptance gate.

## File formats read (the public I/O surface)

The only data JAOS reads from outside the process is a problem file, via two
readers exposed in `include/jaos.h`:
- **`jaos_read_mps`** — fixed and free-layout MPS. Dialect choices (RHS on
  the objective row, RANGES conventions, BOUNDS types, Fortran `D`-exponent
  numbers, locale handling) are pinned in `docs/format-support.md`. Integer
  markers/bounds are recognized and rejected until MILP support lands (M3).
- **`jaos_read_lp`** — a subset of CPLEX-style LP format. Same error
  contract as the MPS reader; ranged constraints, in-constraint constants
  and integer sections are recognized and rejected for the same reason.

There is no writer for either format, and no other format (LP relaxation
files, solution files, etc.) is read or written by the library.

## Public C API surface (`include/jaos.h`)

The entire integration surface a consumer of JAOS sees. One header, one
static archive, `-lm` as the only additional link requirement. Grouped by
what each cluster does:

| Area | Functions |
|---|---|
| Versioning | `jaos_version` |
| Status/error strings | `jaos_status_str`, `jaos_solve_status_str` |
| Model lifecycle | `jaos_model_new`, `jaos_model_free`, `jaos_load_lp` |
| Dimension queries | `jaos_num_col`, `jaos_num_row`, `jaos_num_nz` |
| Read back one entry | `jaos_col_cost`, `jaos_col_bounds`, `jaos_row_bounds` |
| Incremental edits | `jaos_set_col_cost`, `jaos_set_col_bounds`, `jaos_set_row_bounds`, `jaos_set_coefficient`, `jaos_add_cols`, `jaos_add_rows`, `jaos_delete_cols`, `jaos_delete_rows` |
| File readers | `jaos_read_mps`, `jaos_read_lp`, `jaos_model_error` |
| Budgets & tolerances | `jaos_set_work_limit`, `jaos_set_time_limit`, `jaos_set_primal_tolerance`, `jaos_set_dual_tolerance` |
| Logging | `jaos_set_log_callback`, `jaos_set_log_level` |
| Progress/interrupt | `jaos_set_progress_callback` |
| Solving | `jaos_solve`, `jaos_status_of` |
| Reading the answer | `jaos_objective`, `jaos_solution` |
| Basis / warm start | `jaos_basis`, `jaos_set_basis`, `jaos_clear_basis` |
| Cost accounting | `jaos_work_units`, `jaos_iterations`, `jaos_solve_time` |
| Independent checker | `jaos_check_solution` |
| Misc | `jaos_infinity` |

`jaos_infinity()` and IEEE infinity conventions are the only "sentinel value"
protocol a caller needs to know; there is no other implicit contract (units,
encodings, etc.) beyond what each function's doc comment in `include/jaos.h`
states directly.

---

*Integration audit: 2026-08-12*
