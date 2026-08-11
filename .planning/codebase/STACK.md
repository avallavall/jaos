# Technology Stack

**Analysis Date:** 2026-08-12

## Languages

**Primary:**
- C23 (`-std=c23`) - all shipped library code, `src/*.c`, `include/jaos.h`

**Secondary:**
- POSIX shell (`bash`/`sh`) - dev-time tooling: `bench/fetch.sh`, `bench/compare/fetch-solvers.sh`, `bench/compare/run-compare.sh`
- Python 3 - two one-off reference-extraction scripts, `bench/koch-refs.py` and `bench/koch-verify.py`; not built, not linked, the library does not depend on Python (`bench/README.md`)
- GNU Make - `Makefile`, the entire build system

## Runtime

**Environment:**
- Linux (execution platform). Development happens on Windows through WSL2 (`DECISIONS.md` D1). There is no Windows-native build — the Windows side has no compiler.
- Ubuntu 24.04 is the toolchain verification target named in D1.

**Package Manager:**
- None. Zero external dependencies by design (`DECISIONS.md` D2). No lockfile, no manifest of library dependencies — the closest equivalents are `bench/netlib.manifest` (LP test instances) and `bench/compare/solvers.manifest` (competitor solver binaries for benchmarking), both dev-time only and checksum-pinned rather than package-manager-resolved.

## Frameworks

**Core:**
- None (no application framework — this is a linkable static library, `build/release/libjaos.a`)

**Testing:**
- Unity (MIT licence) - vendored under `tests/vendor/unity/` (`unity.c`, `unity.h`, `unity_internals.h`, `LICENSE.txt`, `PROVENANCE.md`). Never linked into the shipped library (`DECISIONS.md` D15). `UNITY_INCLUDE_DOUBLE` is defined project-wide so double-precision assertions are available, since a solver's test suite lives on them.
- This is one of exactly two exceptions to the "no code read from other solvers/no dependencies" rule (`CLAUDE.md`); the other is netlib's `emps` used as a dev-time converter, also never redistributed.

**Build/Dev:**
- GNU Make - the sole build tool (`DECISIONS.md` D14: "one plain Makefile, no CMake, no meson")
- GCC 14 (`gcc-14`), with `gcc-ar` (not plain `ar`) for archiving, so an LTO object archive keeps its symbols visible to the linker plugin regardless of how the host's binutils were configured (`Makefile:33-39`)

## Key Dependencies

**Critical:**
- None outside the C standard library. `LDLIBS := -lm` in `Makefile` is the only link dependency beyond libc — "libm is the only thing JAOS links against beyond libc" (`Makefile:60-62`).

**Infrastructure:**
- Unity test framework (`tests/vendor/unity/`) - dev-time only, see above.
- netlib's `emps.c` - fetched at dev/bench time by `bench/fetch.sh`, checksum-pinned (`fee41f544f6873a5e12bc598947828dc9964ef0676162e4df55e915760e2be22`), compiled to a temp directory and never stored in the repo. Used to expand netlib's packed-format Kennington and infeasible-set instances to plain MPS. It carries no licence, which is why it is a dev-time tool rather than a redistributed dependency (`bench/README.md`, `PLAN.md` Q6).

## Configuration

**Environment:**
- No runtime environment-variable configuration for the library itself — `jaos_model` is configured entirely through the C API (tolerances, work/time budgets, log level, callbacks; see `include/jaos.h`).
- Build-time knobs are all Make variables, not env vars or config files: `CC`, `AR`, `J`, `LTO`, `NATIVE`, `PGO_CFLAGS`, `EXTRA_CFLAGS`, `PGO_LOAD` (all documented at the top of `Makefile`).
- No `.env` files, no secrets, nothing credential-shaped anywhere in the repo.

**Build:**
- `Makefile` (repo root) is the single build config file. No `tsconfig`-equivalent, no separate build-system config — everything (flags, targets, test wiring, bench wiring, PGO) lives in this one file.
- `.gitattributes` forces `text=auto eol=lf` in the working tree regardless of the Windows-side `core.autocrlf`, since the build runs under WSL.

## Platform Requirements

**Development:**
- Windows host with WSL2 running Ubuntu 24.04 (or native Linux), GCC 14+, GNU Make.
- `curl`, `sha256sum`, `gunzip`, `tar`, `cmake` are required by the bench/compare tooling (`bench/fetch.sh`, `bench/compare/fetch-solvers.sh`) but not by the library build itself.
- `wsl -d Ubuntu-24.04 -- bash /mnt/c/path/to/script.sh` is the invocation pattern for running any build/test command from the Windows side (`CLAUDE.md`).

**Production:**
- Linux only, GCC-compiled. The library ships as a static archive `build/release/libjaos.a` plus the single public header `include/jaos.h`. No installer, no packaging step, no distribution channel defined in the repo — a consumer links the archive directly.
- Compiler flags used for the shipping build: `-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -Werror -O3 -flto -g -DNDEBUG` (from `RELEASE_CFLAGS` / `SHIP` in `Makefile:48-103`; see "Compiler flags" below for what each buys).
  - `NATIVE=1` (`-march=native -mtune=native`) is available but off by default: it measured as noise (1.0072x) and produces a binary that is not portable across CPU generations, which the README flags as undistributable (`README.md`, `DECISIONS.md` D62).
  - `LTO=0` drops `-flto` for toolchains whose binutils lack a linker plugin.
  - `make pgo` is a separate, opt-in rebuild (profile-guided optimisation) worth 1.1122x over the plain shipping build — three times what every shipping flag combined is worth — but is not part of `make` because it needs the network (to fetch benchmark instances) and takes minutes rather than seconds (`Makefile:300-334`, `README.md`).

## Compiler flags and what each is for

| Flag | Purpose |
|---|---|
| `-std=c23` | Language standard. Chosen for `constexpr`, `typeof`, fixed-underlying-type enums, `[[nodiscard]]`, `unreachable()`, and checked integer arithmetic (`<stdckdint.h>`) used in sparse-matrix index arithmetic — not for codegen speed (`DECISIONS.md` D1). |
| `-Wall -Wextra -Wpedantic -Werror` | Warnings as errors; part of every build variant (release, dev, ASan). |
| `-ffp-contract=off` | **Load-bearing for bit-reproducibility, not decoration** (`CLAUDE.md`, `Makefile:52-58`). Without it, the compiler may fuse `a*b+c` into an FMA wherever the target CPU offers one — meaning the same model could produce different bits on aarch64 (baseline FMA) vs. x86-64 (no FMA). JAOS promises bit-identical results across machines and runs (D8), so this flag enforces IEEE-exact arithmetic that determinism silently assumed. |
| `-O3` | Shipping optimisation level. Measured 1.0055x over `-O2` — noise, kept as the base rung anyway (`DECISIONS.md` D62). |
| `-flto` | The one flag in the shipping set that measurably does something: 1.0330x (`Makefile:64-89`, D62). Requires building the archive with `gcc-ar` rather than `ar`. |
| `-march=native -mtune=native` (opt-in, `NATIVE=1`) | 1.0072x — inside measurement noise — and makes the binary CPU-generation-specific (illegal instruction on older CPUs). Off by default for portability. |
| `-g` | Kept in the shipping build. Costs nothing at run time; it's what the profiler reads, and profiling the shipped build located four of one milestone's fixes (`README.md`). |
| `-DNDEBUG` | Strips assertions from the release build (present in `RELEASE_CFLAGS`, absent from `DEV_CFLAGS`/`ASAN_CFLAGS`). |
| `-Og -g` (dev/ASan builds) | Debug-friendly optimisation for `make test` / `make sanitize`. |
| `-fsanitize=address,undefined -fno-omit-frame-pointer` | ASan+UBSan build (`make sanitize`), dev-time only. |
| `-fprofile-generate` / `-fprofile-use -fprofile-correction -Wno-missing-profile` | The two passes of `make pgo` (instrument, then rebuild from the recorded profile). |

## Build and test targets (from `Makefile`)

| Target | What it does |
|---|---|
| `make` / `make all` | Release static library, default target → `build/release/libjaos.a` |
| `make test` | Builds and runs the unit suite (dev flags, `-Og -g`, Unity) |
| `make sanitize` | Unit suite under ASan+UBSan |
| `make bench` | Builds the Netlib acceptance runner (`build/bench/run`) |
| `make netlib` | Fetches instances if needed, runs the 94-instance standard-set acceptance gate |
| `make netlib-baseline` | Rewrites the standard-set baseline (never a side effect of `make netlib`) |
| `make netlib-kennington` / `make netlib-kennington-baseline` | The 16-instance Kennington subset and its baseline |
| `make netlib-infeas` / `make netlib-infeas-baseline` | The 29-instance infeasible subset and its baseline |
| `make warm` / `make warm-kennington` | What warm re-solve buys — reports a ratio, not a pass/fail gate |
| `make compare-build` | Builds `build/bench/jaos_time`, the timing harness entry point |
| `make compare-solvers` | Fetches, checksum-verifies and builds the competitor solvers (HiGHS, SoPlex, Clp), nothing else |
| `make compare` | Times JAOS against the competitors on one rung of the comparison ladder |
| `make pgo` | Rebuilds the library from a profile of it solving the standard set (instrument → load → rebuild) |
| `make clean` | Removes all build output (`build/`) |

`J=N` (default 1) runs N instances at once in any netlib target — one process each via fork, not threads — for a byte-identical record in a fraction of the wall time; `J=1` is required whenever the printed seconds (not the deterministic work-unit record) are being read (`Makefile:41-46`, `bench/README.md`).

## Concurrency model

The library itself is single-threaded and has no internal parallelism (no `pthread` usage found in `src/`). Parallelism exists only in the dev-time bench harnesses (`bench/run.c`, `bench/warm.c`, `bench/compare/jaos_time.c`), which fork one OS process per concurrent instance solve via the `-j`/`J` flag — each process links and runs the library exactly as any other consumer would.

---

*Stack analysis: 2026-08-12*
