<!-- refreshed: 2026-08-12 -->
# Architecture

**Analysis Date:** 2026-08-12

## System Overview

```text
┌───────────────────────────────────────────────────────────────────────┐
│                       Public API (single header)                      │
│                          `include/jaos.h`                             │
└──────────────────────────────┬────────────────────────────────────────┘
                                │
        ┌───────────────────────┴────────────────────────┐
        │                                                  │
        ▼                                                  ▼
┌───────────────────────┐                    ┌──────────────────────────┐
│      File readers      │                    │   Direct / incremental   │
│  `src/mps.c`            │  jaos_load_lp()   │   load and edit          │
│  `src/lpfmt.c`           ├──────────────────►│   `src/model.c`          │
└───────────────────────┘                    └────────────┬──────────────┘
                                                            │ owns
                                                            ▼
                                              ┌──────────────────────────┐
                                              │   struct jaos_model       │
                                              │   (CSC matrix, bounds,    │
                                              │    costs, config, basis,  │
                                              │    solution, CSR mirror)  │
                                              └───────┬──────────┬────────┘
                                                       │          │
                                     jaos_solve() ─────┘          └───── jaos_check_solution()
                                                       │                            │
                                                       ▼                            ▼
                                    ┌──────────────────────────────┐   ┌───────────────────────────┐
                                    │   Solve pipeline              │   │  Independent checker       │
                                    │  `src/simplex.c` (jm_dual_    │   │  `src/check.c`             │
                                    │   simplex, struct sx)         │   │  reads only the model as   │
                                    │                                │   │  loaded; no scaling, no    │
                                    │  1. jm_model_scale()           │   │  factorization, no basis   │
                                    │     `src/scale.c`              │   └───────────────────────────┘
                                    │  2. jm_model_ensure_rowwise()  │
                                    │     `src/model.c`              │
                                    │  3. build_warm_basis() /       │
                                    │     build_initial_basis()      │
                                    │  4. run(): price -> ratio      │
                                    │     test -> pivot loop, using  │
                                    │     `src/lu.c` (jm_lu) for      │
                                    │     factorization/updates      │
                                    │  5. publish() writes result    │
                                    │     back into jaos_model        │
                                    └──────────────────────────────┘
```

Consumers sit outside this library boundary and link against the release
static archive exactly as any embedder would:

```text
┌─────────────────────────────┐   ┌──────────────────────────────┐
│  Acceptance-gate runner      │   │  Warm-start benchmark          │
│  `bench/run.c`                │   │  `bench/warm.c`                │
│  parse manifest -> solve ->   │   │  solve -> branch (perturb) ->  │
│  independent check -> diff    │   │  warm vs cold re-solve, both   │
│  vs `bench/*.baseline`        │   │  checked                        │
└─────────────────────────────┘   └──────────────────────────────┘
┌─────────────────────────────────────────────────────────────────┐
│  Competitor timing harness  `bench/compare/jaos_time.c`           │
└─────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Public API | The only stable contract; every symbol an embedder may call | `include/jaos.h` |
| Internal contract | Shared struct layouts, kernel prototypes, work-unit constants; included by every `src/*.c` | `src/jaos_internal.h` |
| Model lifecycle & store | Owns `struct jaos_model`: create/free, `jaos_load_lp`, dimension queries, per-entry read/write (`jaos_set_col_cost` etc.), incremental grow/shrink (`jaos_add_cols`/`jaos_add_rows`/`jaos_delete_cols`/`jaos_delete_rows`), config (tolerances, budgets, callbacks), basis storage, CSR mirror builder, error string | `src/model.c` |
| MPS reader | Fixed/free-layout MPS parsing into the CSC arrays `jaos_load_lp` expects | `src/mps.c` |
| LP reader | CPLEX-style LP-format token-stream parser, same `jaos_load_lp` target | `src/lpfmt.c` |
| Scaling | Curtis-Reid (default) and geometric-mean equilibration; computes `row_scale`/`col_scale` as exact powers of two via deterministic Jacobi-PCG | `src/scale.c` |
| Sparse LU | Markowitz-threshold factorization of the basis, Forrest-Tomlin updates, FTRAN/BTRAN (dense and pattern-reporting sparse variants) | `src/lu.c` |
| Dual simplex | The solve orchestrator: pricing (dual steepest edge), Harris two-pass ratio test, Bland fallback, pivoting, warm/cold start, re-entry after settling shifted bounds, result publication | `src/simplex.c` |
| Independent checker | Judges a claimed `(x, y)` against the model as loaded, in long-double arithmetic, with no access to solver state | `src/check.c` |
| Status strings | `jaos_status`/`jaos_solve_status` enum → human string | `src/status.c` |
| Shared internals | Growable-array helper (`jm_grow`/`JM_GROW`), name→value hash map (`jm_nmap`) used by both readers | `src/util.c` |
| Allocation | Overflow-checked array allocation (`jm_alloc_array`, `jm_calloc_array`) — every array allocation in JAOS goes through these two | `src/alloc.c` |
| Version | `jaos_version()` | `src/version.c` |

## Pattern Overview

**Overall:** A layered pipeline behind a single opaque-handle C API, with one
deliberately isolated component (the checker) that re-derives correctness
from first principles instead of trusting the pipeline that produced the
answer.

**Key Characteristics:**
- **One model owns its own state.** `jaos_model` (`src/jaos_internal.h:46`)
  is the sole mutable object; the solver (`sx` in `src/simplex.c:283`) builds
  a scaled *working copy* from it every solve and never mutates the model's
  authoritative CSC arrays — that copy is what makes the checker's
  independence real (`src/check.c:1`).
- **Scaling is transparent to everything downstream of it except the
  solver.** `row_scale`/`col_scale` live on `jaos_model` but only
  `simplex.c` reads them to build its working arrays; `check.c` never does.
- **Two derived views of the matrix are cached on the model and invalidated
  together:** the CSR mirror (`rowwise_valid`) that pricing needs to walk
  rows cheaply, and the scaling (`scale_valid`). Any load or matrix edit
  drops both (`model_matrix_is_stale`, `src/model.c:747`).
- **The solver is a closed local struct (`sx`), not a persistent object.**
  It is built in `sx_init` (`src/simplex.c:524`) at the top of every
  `jm_dual_simplex` call and freed at the bottom (`src/simplex.c:489`,
  `:3614`); nothing about pricing weights, pattern buffers or scratch
  vectors survives between solves except through the model's own
  `start_col_status`/`start_row_status` basis fields.
- **Everything reproducible is counted, nothing reproducible reads a
  clock.** `jm_work` (`src/jaos_internal.h:372`) accumulates deterministic
  work units inside the kernels; wall-clock time (`elapsed_seconds`,
  `src/simplex.c:3123`) is sampled only for the caller's time-limit budget
  and for the one non-reproducible number JAOS reports (`jaos_solve_time`).

## Layers

**Public API:**
- Purpose: the only stable, documented surface; everything else "carries no
  stability promise" (`include/jaos.h:4`)
- Location: `include/jaos.h`
- Contains: opaque `jaos_model`, status enums, load/edit/solve/read
  functions, the checker entry point
- Depends on: nothing internal (pure declarations)
- Used by: every `src/*.c` file (implements it), `bench/*.c`, `tests/test_*.c`

**Internal contract:**
- Purpose: shared struct definitions and kernel prototypes that must be the
  same translation-unit-wide truth, plus the deterministic work-unit
  constants
- Location: `src/jaos_internal.h`
- Contains: `struct jaos_model`, `jm_config`, `jm_var_status`, `jm_lu`,
  `jm_svec`, `jm_nmap`, `jm_work`, allocation/logging/error helpers
- Depends on: `include/jaos.h`
- Used by: every file under `src/`, and every test under `tests/` (tests are
  explicitly white-box — see Makefile `TEST_INC`)

**Model store:**
- Purpose: owns the problem data and everything the caller configures
- Location: `src/model.c`
- Contains: lifecycle, load, incremental edit, config setters, basis
  bookkeeping, CSR mirror construction
- Depends on: `src/jaos_internal.h`, `src/alloc.c`/`src/util.c` helpers
  (via prototypes only, linked at build time)
- Used by: `src/mps.c`, `src/lpfmt.c` (via `jaos_load_lp`), `src/simplex.c`,
  `src/check.c`, `bench/*.c`

**File readers:**
- Purpose: turn an MPS or LP-format file into the CSC arrays `jaos_load_lp`
  validates and copies
- Location: `src/mps.c`, `src/lpfmt.c`
- Contains: tokenizers, section parsers, `jm_nmap`-based name resolution,
  locale-independent number parsing
- Depends on: `src/model.c` (calls `jaos_load_lp`), `src/util.c` (`jm_nmap`)
- Used by: callers of `jaos_read_mps`/`jaos_read_lp`; `bench/run.c`,
  `bench/warm.c` load every instance this way

**Scaling:**
- Purpose: compute the row/column exponents the solver works in
- Location: `src/scale.c`
- Contains: Curtis-Reid least-squares scaling solved by Jacobi-preconditioned
  CG, geometric-mean equilibration
- Depends on: `src/jaos_internal.h` (reads `jaos_model`'s CSC arrays)
- Used by: `src/simplex.c` (`sx_init`, only if `!m->scale_valid`)

**Sparse LU:**
- Purpose: factor and incrementally update the basis matrix
- Location: `src/lu.c`
- Contains: `jm_lu_factor` (Markowitz threshold pivoting), `jm_lu_update`
  (Forrest-Tomlin), `jm_lu_ftran`/`jm_lu_btran` and their pattern-reporting
  sparse variants
- Depends on: `src/jaos_internal.h` only — no knowledge of `jaos_model`,
  scaling, or simplex state; it factors whatever CSC matrix it is given
- Used by: `src/simplex.c` exclusively

**Dual simplex (solve orchestrator):**
- Purpose: drive the model from an initial or warm basis to an optimal,
  infeasible, unbounded, or budget-limited outcome
- Location: `src/simplex.c`
- Contains: the local `sx` struct, setup (`sx_init`), basis construction
  (`build_initial_basis`, `build_warm_basis`), the main loop (`run`),
  pricing (`price_row`, `price_all`, `price_and_select`), ratio tests
  (`dual_ratio_test`, `primal_ratio_test`, plus the reachable
  `jm_harris_pick`/`jm_bland_pick`), pivoting (`pivot`), re-entry after
  settling shifted bounds (`reenter_after_settling`), result publication
  (`publish`)
- Depends on: `src/model.c` (reads/writes `jaos_model`), `src/scale.c`
  (triggers scaling), `src/lu.c` (factorization)
- Used by: `jaos_solve` (`src/model.c:384`) → `jm_dual_simplex`
  (`src/simplex.c:3549`)

**Independent checker:**
- Purpose: verify a claimed solution against the model *as loaded*, with no
  dependency on how — or whether — the solver produced it
- Location: `src/check.c`
- Contains: `jaos_check_solution`, long-double-accumulated row activities
  and dual objective, complementary-slackness and gap-certification logic
- Depends on: `src/jaos_internal.h` only for `struct jaos_model`'s original
  (unscaled) arrays — deliberately not `src/scale.c`, `src/lu.c`, or
  `src/simplex.c`
- Used by: `bench/run.c`, `bench/warm.c`, `tests/test_check.c`, and any
  caller wanting a second opinion on a claimed `(x, y)`

## Data Flow

### Primary Solve Path

1. Problem enters via a reader or a direct call:
   `jaos_read_mps`/`jaos_read_lp` (`src/mps.c:378`, `src/lpfmt.c:625`) parse
   the file and call `jaos_load_lp` (`src/model.c:604`) themselves; a caller
   using `jaos_load_lp` directly skips straight here.
2. `jaos_load_lp` validates (NaN, non-finite costs/matrix values, row-index
   range, duplicate indices) and copies into `jaos_model`'s owned CSC arrays;
   it invalidates `rowwise_valid` and `scale_valid` and clears the solution
   (`model_matrix_is_stale`, `src/model.c:747`).
3. `jaos_solve` (`src/model.c:384`) calls `jm_dual_simplex`
   (`src/simplex.c:3549`).
4. `sx_init` (`src/simplex.c:524`): if `!m->scale_valid`, calls
   `jm_model_scale` (`src/scale.c`); calls `jm_model_ensure_rowwise`
   (`src/model.c:1302`) unconditionally if stale; allocates the solver's
   scratch arrays; builds the scaled working copy (`av`, `arv`, `lo`, `up`,
   `cost`) from the model's own arrays and `row_scale`/`col_scale`.
5. `build_warm_basis` (`src/simplex.c:810`) tries the basis carried on the
   model (`start_col_status`/`start_row_status`); on failure or absence,
   `build_initial_basis` (`src/simplex.c:715`) builds the slack basis.
6. `run` (`src/simplex.c:3139`) loops: budget/callback checks →
   `price_row` selects a violated basic variable → `price_and_select`
   chooses an entering column via the ratio test → `pivot`
   (`src/simplex.c:1923`) updates the basis, calling `jm_lu_update`
   (`src/lu.c`) or, every `REFACTOR_EVERY` iterations or on a stability
   trigger, a full `refresh`/`refactorize` (`src/simplex.c:1208`, `:871`)
   that calls `jm_lu_factor`.
7. On reaching optimality: `settle_shifts` calls in any dual-feasibility
   loan, `reenter_after_settling` (`src/simplex.c:2882`) runs a bounded
   primal clean-up if settling moved anything, and `classify_optimum`
   (`src/simplex.c:3087`) decides between a genuine optimum and one that
   depended on JAOS's own invented bounds.
8. `publish` (`src/simplex.c:3428`) writes `solve_status`, `objective`,
   `sol_col`/`sol_row`/`sol_dual`/`sol_redcost`,
   `sol_col_status`/`sol_row_status`, work/iteration/time counters back onto
   `jaos_model`, converting out of scaled space with `row_scale`/`col_scale`.
9. The caller reads results through `jaos_objective`, `jaos_solution`,
   `jaos_basis`, `jaos_work_units`, `jaos_iterations`, `jaos_solve_time`
   (all in `src/model.c`).

### Independent Verification Path

1. Caller has `col_value` (and optionally `row_dual`) — typically straight
   from `jaos_solution`, but the checker accepts any array of the right
   shape.
2. `jaos_check_solution` (`src/check.c:483`) rebuilds row activities by a
   column-order CSC scatter-add in `long double`, entirely independent of
   whatever `simplex.c` computed.
3. Primal feasibility, dual sign conditions, complementary slackness and the
   primal-dual gap are checked from these independently-derived numbers;
   `jaos_check_report` is filled with magnitudes, not a single boolean, so
   the caller sees *how* wrong rather than only *whether*.
4. This path never touches `row_scale`/`col_scale`, `jm_lu`, or any `sx`
   state — the isolation is structural (no `#include` chain reaches those
   files from `check.c`), not just a convention.

**State Management:**
- `jaos_model` is the only long-lived mutable state; one model is used by
  one thread at a time (`include/jaos.h:102`).
- The solve's own state (`sx`) is stack-local and rebuilt every call;
  nothing in `src/simplex.c` is a static or global.
- Warm-start state crosses solves through exactly two model fields
  (`start_col_status`, `start_row_status`), separate from the *answer*
  fields (`sol_col_status`, `sol_row_status`) — see the comment at
  `src/jaos_internal.h:123`.

## Key Abstractions

**`jaos_model` (`struct jaos_model`, `src/jaos_internal.h:46`):**
- Purpose: the one object every public call operates on — problem data,
  derived caches (CSR mirror, scaling), caller configuration (`jm_config`),
  last solution, starting basis, error string
- Examples: allocated by `jaos_model_new`, freed by `jaos_model_free`
- Pattern: opaque handle; internals visible only through `jaos_internal.h`
  (white-box for tests and the library's own code, invisible to a
  consumer that only includes `jaos.h`)

**`sx` (local solver state, `src/simplex.c:283`):**
- Purpose: the scaled working copy of the problem plus all pricing/ratio-test
  scratch space for one solve
- Examples: built by `sx_init`, torn down by `sx_free`
- Pattern: stack-allocated, single-use, never exposed outside `simplex.c`

**`jm_lu` (sparse LU factorization, `src/jaos_internal.h:417`):**
- Purpose: `B = P' L E^-1 U Q'` factorization of the current basis, held in
  both row and column orientation, indexed by slot rather than by row/column
  so an update is O(dim) instead of touching every nonzero of U
- Examples: `jm_lu_factor`, `jm_lu_update`, `jm_lu_ftran`/`btran`
- Pattern: caller-owned workspace reused across factorizations
  (`jm_lu_init`/`jm_lu_free` bracket the solve, not each factorization)

**`jm_nmap` (name → value map, `src/jaos_internal.h:311`):**
- Purpose: resolve row/column names to indices while reading MPS or LP text
- Examples: used identically by `src/mps.c` and `src/lpfmt.c`
- Pattern: FNV-1a open addressing over one arena of NUL-separated names

**`jm_work` (deterministic work counter, `src/jaos_internal.h:372`):**
- Purpose: the reproducible currency every kernel bills against, so a
  regression is detectable independent of the machine that ran it
- Examples: `JM_WORK_NONZERO`, `JM_WORK_ELIMINATED`, `JM_WORK_FACTOR`,
  `JM_WORK_UPDATE`; threaded as a `jm_work *` through every kernel that
  touches a nonzero
- Pattern: accumulator passed by pointer, never read back inside the solver
  — only exposed to the caller after a solve via `jaos_work_units`

## Entry Points

**`jaos_solve` (`src/model.c:384`):**
- Location: `include/jaos.h` declares it, `src/model.c` implements it as a
  one-line dispatch to `jm_dual_simplex`
- Triggers: any caller ready to solve a loaded model
- Responsibilities: delegates entirely to the simplex module; the only
  thing it owns is clearing the model's error string first

**`bench/run.c` (acceptance-gate runner):**
- Location: `bench/run.c`
- Triggers: `make netlib`, `make netlib-kennington`, `make netlib-infeas`
  (and their `-baseline` variants)
- Responsibilities: reads a manifest, loads each `.mps` instance, solves it,
  judges the result against expected dimensions, a reference optimum, and
  `jaos_check_solution`; solves a second time and requires bit-identical
  digests; diffs against `bench/*.baseline` — never writes wall-clock time
  into that file

**`bench/warm.c` (warm-start benchmark):**
- Location: `bench/warm.c`
- Triggers: `make warm`, `make warm-kennington`
- Responsibilities: solves fresh, applies one deterministic branch-and-bound
  branching step, solves again warm (from the carried basis) and cold
  (after `jaos_clear_basis`), checks both, and reports the ratio — not a
  pass/fail gate (see `Makefile` comment at line 187)

**`bench/compare/jaos_time.c` (competitor timing harness):**
- Location: `bench/compare/jaos_time.c`
- Triggers: `make compare`
- Responsibilities: times JAOS alone, in the same shape `bench/compare/run-compare.sh`
  invokes the other solvers, on one rung of the ladder described in
  `bench/compare/README.md`

**`tests/test_*.c` (unit suite entry points):**
- Location: `tests/test_check.c`, `test_fuzz.c`, `test_lp.c`, `test_lu.c`,
  `test_model.c`, `test_mps.c`, `test_scale.c`, `test_simplex.c`,
  `test_version.c`
- Triggers: `make test` (dev flags), `make sanitize` (ASan+UBSan)
- Responsibilities: white-box unit tests, one file per `src/*.c` module
  (`test_lp.c` covers `src/lpfmt.c`), linked against `jaos_internal.h` so
  assertions can inspect solver-internal structures directly

## Architectural Constraints

- **Threading:** single-threaded per model (`include/jaos.h:102`, "one model
  is used by one thread at a time; distinct models are fully independent").
  No mutex, no atomics anywhere in `src/`. `bench/run.c`'s `-j N` concurrency
  is process-level (one instance per process), not thread-level.
- **Global state:** none. Every `src/*.c` file is free of file-scope mutable
  state; all solver state lives on the caller-owned `jaos_model` or on the
  stack-local `sx`.
- **Circular imports:** none possible by construction — every `src/*.c`
  includes only `jaos_internal.h` (which includes `jaos.h`) plus standard
  library headers; there is no `src/*.h` per module to create a cycle
  between modules.
- **Determinism (D8):** no clock, no address-dependent iteration order, no
  reassociated floating point (`-ffp-contract=off` is load-bearing, per
  `Makefile:52-58`), no unseeded randomness anywhere in the solve path. This
  is the constraint that shapes `scale.c`'s choice of Jacobi-PCG over a
  direct solve, `lu.c`'s slot-indexed permutations, and `simplex.c`'s
  clock-free progress-callback pacing.
- **Checker independence:** `src/check.c` must never gain an `#include`
  chain back to `scale.c`, `lu.c`, or `simplex.c`. This is stated as a
  design invariant in the file's own header comment and is what makes it an
  oracle rather than a second code path that happens to agree.

## Anti-Patterns

### Trusting carried numbers at a stopping point

**What happens:** `x_B` and the reduced costs are updated incrementally by
every pivot rather than recomputed, and the factorization is patched
(Forrest-Tomlin) rather than rebuilt, so both can drift from what a fresh
computation would show.
**Why it's wrong:** a solve could declare optimal or infeasible on numbers
that no basis actually supports — and it would do so exactly when the drift
happens to point toward looking feasible or looking unbounded, which is the
least convenient direction to be wrong in.
**Do this instead:** `run()` never accepts a terminal verdict (`r < 0` for
optimal, `q < 0` for infeasible) without first setting `s->verified` via one
free-of-charge `refresh(s, &ok, true)` from a fresh factorization
(`src/simplex.c:3242-3330`). This is documented in-line as the fix for two
real defects (D29, D39) where the naive version returned wrong verdicts on
specific reference instances.

### Folding a method choice into a caller-facing option

**What happens:** it is tempting to expose "which pricing rule" or "turn
scaling off" as a solver setting, the way `jaos_set_primal_tolerance` exposes
a tolerance.
**Why it's wrong:** D64 (referenced throughout `jaos.h` and `jaos_internal.h`)
draws the line at what depends on the *caller's data* (precision, budget,
where output goes) versus what depends on the *method* (pricing rule,
refactorization interval, scaling mode). A caller cannot be expected to know
which their model wants, and an option that asks hands back a problem that
belongs inside the library.
**Do this instead:** method constants live as `constexpr`/`#define` values
in `src/simplex.c` and `src/scale.c`, tunable only through
`EXTRA_CFLAGS`/`-D` at build time for sweeps (`Makefile:94-101`), never
through the public API.

## Error Handling

**Strategy:** every fallible public function returns a `jaos_status`; data
never leaves through a return value (`include/jaos.h:38-46`). A solve
outcome is a *separate* enum (`jaos_solve_status`) precisely so that "the
solve ran" and "the solve found an optimum" cannot be confused — hitting a
work or time limit is an honest report, not a failure.

**Patterns:**
- Internal failure detail is written to `jaos_model.err[256]` via
  `jm_set_err` (`printf`-style, `src/model.c:555`) and read back with
  `jaos_model_error`; a failed operation leaves the model's prior problem
  data untouched (checked by the fuzzer, `tests/test_fuzz.c`).
- Numerical abandonment (`JAOS_ERR_NUMERICAL`) is distinguished from a
  policy stop (`JAOS_SOLVE_*` outcomes with `JAOS_OK`): the former is a
  defect in JAOS and leaves the previous starting basis untouched rather
  than publishing anything derived from a run the solver does not vouch for
  (`src/simplex.c:3188-3211`, the "internal iteration guard" case).
- `JAOS_NODISCARD` (`[[nodiscard]]` under C23) is applied to every function
  whose return value carries a status, so a caller cannot silently ignore
  a failed call.

## Cross-Cutting Concerns

**Logging:** opt-in only — `jm_logging_at` (`src/jaos_internal.h:340`) gates
every call site so a solve nobody is watching pays one comparison and no
formatted string; `jm_log` (`src/model.c:372`) dispatches to the caller's
`jaos_log_fn`. Four levels (`JAOS_LOG_OFF`/`SUMMARY`/`PROGRESS`/`DETAIL`).
Logging is checked to never change the bits of an answer (D8, verified over
all 139 reference instances).

**Validation:** input validation is concentrated at the boundary —
`jaos_load_lp` and the two readers reject NaN, non-finite values, and
structural inconsistencies before anything downstream sees the data.
Trivially infeasible data (e.g. `lower > upper`) is deliberately *not*
rejected there; deciding feasibility is the solver's job.

**Authentication:** not applicable — JAOS is an embedded numerical library
with no network or process boundary of its own.
