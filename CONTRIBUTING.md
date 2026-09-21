# Contributing to JAOS

## Build and test

GCC 14 or later on Linux, or under WSL on Windows.

```
make              # build/release/libjaos.a
make test         # the unit suite, the CLI's test, the install, CMake and Windows checks
make sanitize     # the unit suite under ASan and UBSan
make python-test  # the Python binding, when python/ changed
```

A change that touches the solver itself (`src/simplex.c`, `src/lu.c`,
`src/presolve.c`, `src/scale.c`, `src/mip.c`) also runs the gates and reads
their results against the committed baselines:

```
make netlib netlib-infeas J=12
make netlib-kennington J=2
make miplib J=2              # when src/mip.c changed
```

No instance may regress. A baseline is rewritten only with its
`*-baseline` target, and only after the diff has been read.

## The rules a change must hold

- **Bit-identical results on every machine and every run.** No clock
  decides anything, no iteration order depends on an address, no floating
  point is reassociated, no randomness is unseeded. `-ffp-contract=off` is
  load-bearing.
- **No dependencies, and no code read from other solvers.** Papers and
  textbooks only. Unity in `tests/vendor/` is the one exception.
- **A new constant goes in `docs/tolerances.md`** with the measurement that
  set it.
- **Work units are the cost.** Seconds never go in `bench/results/` or a
  baseline; `docs/work-units.md` says what each kernel charges.
- **Code carries no comments.** What a change does and why goes in its
  commit message; a constant's reason goes in `docs/tolerances.md`. The
  exceptions are the SPDX line, a script's `#!` line, and documentation a
  tool reads: Python docstrings, .NET XML comments and Julia docstrings.

## Where a feature has to reach

A feature is done when it reaches all of these, in one batch:

1. the C API in `include/jaos.h`, and a test in `tests/`;
2. the tool in `cli/jaos.c`, with a check in `tests/cli.sh`;
3. `python/jaos` at both layers (the ctypes calls and the modelling
   layer), with a test in `python/test_jaos.py`;
4. the documents it changes: `docs/cli.md`, `docs/format-support.md` and
   the README.

## The record

- `SPECS.md` lists every feature JAOS must have, with its status. A row
  changes status when its feature lands.
- `TODO.md` is the current backlog. A row leaves it in the commit that
  lands it.
- `bench/refusals.txt` lists ideas that were built, measured and found
  worse, each with what would make it worth trying again. Read it before
  trying a performance idea.

There is no changelog. The git history is the log, so a commit message
says what changed, what was measured and what the numbers were.

## Reporting a problem

Open an issue on GitHub with the model file (or the smallest one that
shows the problem), the command, and what `jaos --version` prints. A
security problem goes through `SECURITY.md` instead.
