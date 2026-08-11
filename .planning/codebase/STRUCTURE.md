# Codebase Structure

**Analysis Date:** 2026-08-12

## Directory Layout

```
jaos/
├── include/            # Public API — the only header a consumer includes
│   └── jaos.h
├── src/                # Library implementation (builds into libjaos.a)
│   ├── jaos_internal.h #   shared internal contract, included by every src/*.c and every test
│   ├── model.c          #   jaos_model lifecycle, load/edit, config, basis storage
│   ├── mps.c             #   MPS format reader
│   ├── lpfmt.c            #   LP format reader
│   ├── scale.c            #   Curtis-Reid / geometric-mean scaling
│   ├── lu.c                #   sparse LU factorization + Forrest-Tomlin updates
│   ├── simplex.c            #   dual simplex solve orchestrator (largest file, ~3600 lines)
│   ├── check.c              #   independent solution checker
│   ├── status.c              #   enum -> string helpers
│   ├── util.c                 #   growable arrays, name->value map
│   ├── alloc.c                 #   overflow-checked allocation
│   └── version.c                #   jaos_version()
├── tests/              # Unity-based unit suite, white-box (includes jaos_internal.h)
│   ├── test_check.c
│   ├── test_fuzz.c      # reader robustness under corrupted input
│   ├── test_lp.c         # tests src/lpfmt.c
│   ├── test_lu.c
│   ├── test_model.c
│   ├── test_mps.c
│   ├── test_scale.c
│   ├── test_simplex.c
│   ├── test_version.c
│   ├── data/             # small hand-built .mps/.lp fixtures, valid and deliberately broken
│   └── vendor/unity/     # vendored test framework (dev-time only, never linked into the library)
├── bench/               # Acceptance gate and benchmarking — first-class, not an afterthought
│   ├── run.c              # netlib/netlib-kennington/netlib-infeas acceptance runner
│   ├── warm.c              # warm-vs-cold re-solve benchmark (ratio, not a gate)
│   ├── fetch.sh             # fetches + checksum-verifies instances; nothing downloaded is committed
│   ├── koch-refs.py          # generates reference optima from an independent source
│   ├── koch-verify.py         # verifies koch-refs.py output
│   ├── *.manifest             # which instances belong to each set, and their expected outcome
│   ├── *.baseline               # deterministic recorded results (work units, digests) diffed against — never wall-clock
│   ├── results/                  # output of the latest runs (gitignored working area)
│   ├── instances/                # fetched .mps files, standard set (not committed — data, ignore for source review)
│   ├── instances-infeas/          # fetched .mps files, infeasible set (same)
│   ├── instances-kennington/       # fetched .mps files, Kennington set (same)
│   └── compare/                     # competitive timing vs HiGHS/SoPlex/Clp
│       ├── jaos_time.c                # times JAOS alone
│       ├── run-compare.sh              # orchestrates the ladder
│       ├── fetch-solvers.sh             # fetches/checksums/builds the competitors
│       ├── solvers.manifest, clp-deps.manifest
│       ├── *.args, *.opt                # per-solver, per-tier CLI flags
│       ├── solvers/                     # fetched competitor source/binaries (not committed)
│       └── results/                      # comparison output (gitignored working area)
├── docs/                # Design documentation, not API reference
│   ├── format-support.md   # MPS/LP dialect decisions
│   ├── scaling.md            # what scaling does and why Curtis-Reid
│   ├── tolerances.md          # every tolerance formula, in full
│   ├── work-units.md            # what the deterministic work counter charges for
│   └── research/                  # longer background notes cited by DECISIONS.md
│       ├── crash-basis.md
│       ├── hyper-sparsity.md
│       └── netlib-campaign.md
├── build/               # All compiled output (gitignored)
│   ├── release/            # -O3 [+flto], what `make all` produces; libjaos.a lives here
│   ├── dev/                 # -Og, what `make test` builds and runs
│   ├── asan/                 # -Og -fsanitize=address,undefined, what `make sanitize` runs
│   └── bench/                  # bench/run, bench/warm, bench/jaos_time binaries
├── .claude/             # Project-local Claude Code configuration
│   ├── skills/             # this repo's domain skills (jaos-measure, fp-numerics, c-perf, ...)
│   └── agents/               # subagent definitions (jaos-measurer, numerics-reviewer, literature-scout)
├── .planning/           # GSD planning artifacts
│   └── codebase/           # this document and its siblings
├── Makefile             # build/test/bench/compare/pgo targets — WSL/Linux/GCC only, see CLAUDE.md
├── CLAUDE.md             # entry point for how to work in this repo
├── SPECS.md               # what JAOS is built to be, feature-by-feature status
├── PLAN.md                 # what is open, in execution order
├── DECISIONS.md             # closed decisions and the measurement that closed them (D1, D2, ...)
├── CHANGELOG.md               # what changed and what it cost, a few lines per entry
├── README.md                    # project overview
└── LICENSE                       # Apache-2.0
```

## Directory Purposes

**`include/`:**
- Purpose: the entire public surface, one file
- Contains: `jaos.h` only — every declaration a consumer may rely on
- Key files: `include/jaos.h`

**`src/`:**
- Purpose: the library implementation, compiled into `build/release/libjaos.a`
- Contains: eleven `.c` files plus the shared internal header; no
  subdirectories — the module count is small enough that a flat layout is
  the deliberate choice (see `Makefile:119`, `SRC := $(wildcard src/*.c)`)
- Key files: `src/jaos_internal.h` (read this before any other file in the
  directory — it is the shared vocabulary every module assumes)

**`tests/`:**
- Purpose: the unit suite, one file per `src/*.c` module plus a fuzz test
- Contains: Unity-based test files, a `data/` directory of hand-built
  fixture files (both valid and deliberately malformed), and the vendored
  Unity framework
- Key files: `tests/test_simplex.c` (largest, ~106K — the solver has the
  most surface to cover), `tests/test_fuzz.c` (gate condition 4, PLAN.md
  §2.9: malformed input must error, never crash)

**`bench/`:**
- Purpose: the acceptance gate and the benchmarking suite — treated as a
  first-class component, not an afterthought. `bench/run.c` is what decides
  whether a change is accepted at all (PLAN.md §2.9); `bench/compare/` is
  what measures competitiveness against other solvers.
- Contains: runner programs (`run.c`, `warm.c`), fetch/verify scripts,
  manifests (which instances, what outcome is expected), baselines
  (deterministic recorded results), and the `compare/` subtree for
  competitor timing
- Key files: `bench/run.c` (the gate itself), `bench/README.md` (what the
  gate checks and why), `bench/compare/README.md` (comparison methodology)
- Note: `bench/instances*/**.mps` are fetched benchmark data files, not
  source — never edited, never committed, checksum-verified by
  `bench/fetch.sh` on every use

**`docs/`:**
- Purpose: design documentation for things too detailed to live as comments
  — formulas, dialect tables, full tolerance derivations
- Contains: four top-level `.md` files plus `research/` for longer
  background notes that `DECISIONS.md` entries cite
- Key files: `docs/tolerances.md` (every tolerance formula), `docs/work-units.md`
  (what the deterministic work counter bills)

**`build/`:**
- Purpose: all compiled output, entirely derived and gitignored
- Contains: `release/`, `dev/`, `asan/`, `bench/` — one subtree per build
  configuration, mirroring the Makefile's `$(B)/<config>/%.o` pattern
- Generated: yes
- Committed: no

**`.claude/`:**
- Purpose: project-local Claude Code configuration — domain skills and
  subagent definitions specific to working on JAOS (see the skill table in
  `CLAUDE.md`)
- Contains: `skills/` (loaded at specific moments named in `CLAUDE.md`),
  `agents/` (subagent `.md` definitions)

## Key File Locations

**Entry Points:**
- `include/jaos.h`: the public API surface
- `src/model.c:384` (`jaos_solve`): dispatches into the solver
- `bench/run.c`: the acceptance-gate program
- `bench/warm.c`: the warm-start benchmark program

**Configuration:**
- `Makefile`: build/test/bench/compare/pgo targets and every compiler flag,
  with the measurement behind each one in a comment
- `src/jaos_internal.h` (`jm_config`, inside `struct jaos_model`): runtime
  configuration a caller sets (tolerances, budgets, callbacks)

**Core Logic:**
- `src/simplex.c`: the solve orchestrator — start here for anything about
  *how* a model is solved
- `src/lu.c`: the numerical kernel the solver leans on hardest
- `src/model.c`: the data model everything else operates on

**Testing:**
- `tests/test_*.c`: one file per module, white-box, run via `make test` /
  `make sanitize`
- `bench/`: the acceptance gate (correctness + determinism at scale), a
  different kind of test from `tests/` and run on a different cadence

## Naming Conventions

**Files:**
- `src/*.c`: one module per file, lower snake_case, named after what it
  owns (`model.c` owns the model, `simplex.c` the solve, `check.c` the
  checker) — no `_impl`, `_internal`, or similar suffixes
- `tests/test_<module>.c`: mirrors the `src/` module it covers, with one
  named exception — `test_lp.c` covers `src/lpfmt.c` (the LP-format reader),
  not a file literally named `lp.c`
- `docs/*.md`: topic-named, lowercase-hyphenated

**Functions:**
- `jaos_*`: every public API function, declared in `include/jaos.h`,
  implemented across `src/*.c`
- `jm_*`: internal functions shared across modules or used by tests as
  white-box entry points, declared in `src/jaos_internal.h`
- No prefix, `static`: functions private to one translation unit (the
  majority of `src/simplex.c`'s ~60 functions are static; only
  `jm_dual_simplex` and the handful of `jm_*` functions the header exposes
  are visible outside it)

**Types:**
- `jaos_*` (public) / `jm_*` (internal) struct and enum names, matching the
  function-prefix convention exactly
- One deliberate exception: `sx`, the local solver-state struct in
  `src/simplex.c` — short because it is never named outside that file and
  appears in almost every function signature in it

**Directories:**
- Lowercase, single word where possible (`src`, `tests`, `bench`, `docs`,
  `include`, `build`)
- `bench/instances*`: pluralized, suffix names the subset
  (`instances-infeas`, `instances-kennington`)

## Where to Add New Code

**New public API function:**
- Declaration: `include/jaos.h`, grouped under the relevant `/* ---- */`
  section banner (problem data, reading/changing, file readers, solving,
  watching/stopping, checker)
- Implementation: whichever `src/*.c` already owns that concern (a new
  setter on `jaos_model` goes in `model.c`; a new solve control goes beside
  the existing ones there too — solving *itself* stays in `simplex.c`)
- Tests: the matching `tests/test_<module>.c`

**New file format reader:**
- Implementation: new `src/<format>fmt.c` or `src/<format>.c` following the
  pattern in `src/mps.c` / `src/lpfmt.c` — parse into the same CSC shape,
  call `jaos_load_lp` to hand off, use `src/util.c`'s `jm_nmap` for name
  resolution
- Declaration: one new `jaos_read_<format>` entry in `include/jaos.h`,
  documented dialect decisions go in `docs/format-support.md`
- Tests: new `tests/test_<format>.c`, plus fixture files under `tests/data/`
  (both valid and deliberately malformed, one file per rejection class) and
  coverage added to `tests/test_fuzz.c`

**New solver capability (e.g. primal simplex, MILP branch and bound):**
- Check `PLAN.md` and `SPECS.md` first — this is the kind of change that
  needs a phase, not a drive-by addition
- Implementation: `src/simplex.c` for anything inside the dual-simplex loop;
  a genuinely separate method (primal simplex, branch and bound) would
  likely be its own new `src/*.c` file given the pattern the existing
  modules set
- Anything that changes what a run costs needs a new `JM_WORK_*` constant
  or reuse of an existing one (`src/jaos_internal.h:374-377`) — see
  `docs/work-units.md`
- Any new numeric constant needs a measurement on both sides (see
  `CLAUDE.md`, "Every number needs a measurement on both sides") before it
  can be a threshold

**Utilities:**
- Shared helpers usable by more than one module: `src/util.c` (growable
  arrays, name maps) or `src/alloc.c` (allocation) — both are intentionally
  small and dependency-free within `src/`

## Special Directories

**`build/`:**
- Purpose: compiled objects, libraries, and bench binaries
- Generated: yes (`make all`, `make test`, `make sanitize`, `make bench`, ...)
- Committed: no

**`bench/instances/`, `bench/instances-infeas/`, `bench/instances-kennington/`:**
- Purpose: fetched Netlib LP benchmark data (`.mps` files)
- Generated: yes, by `bench/fetch.sh`, checksum-verified against the
  relevant `.manifest`
- Committed: no — fetched fresh (or from a local cache) on demand,
  deliberately never redistributed

**`bench/results/`, `bench/compare/results/`:**
- Purpose: output of the latest `netlib*`/`warm*`/`compare` runs
- Generated: yes
- Committed: no (working area; `bench/*.baseline` is the committed,
  deterministic record these runs are diffed against)

**`bench/compare/solvers/`:**
- Purpose: fetched, checksum-verified, built copies of the competitor
  solvers (HiGHS, SoPlex, Clp) used only for `make compare`
- Generated: yes, by `bench/compare/fetch-solvers.sh`
- Committed: no

**`tests/vendor/unity/`:**
- Purpose: vendored copy of the Unity test framework — one of exactly two
  permitted exceptions to "no external code" (the other is netlib's `emps`
  converter), and neither is extended
- Generated: no (vendored, checked in)
- Committed: yes — but never linked into the library itself, dev-time only

---

*Structure analysis: 2026-08-12*
