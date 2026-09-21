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

## The checks outside `make test`

```
make coverage     # the share of each src/ file's lines the unit suite runs
make valgrind     # the unit suite under valgrind's memcheck
make fuzz         # one libFuzzer target per model reader, under clang-20
```

`make coverage` builds the library and the unit suite at `-O0` with
`--coverage` in `build/cov/`, runs the suite, and prints one line per
`src/` file (gcov-14; `GCOV=` names another). A file with a low figure is
where a new test reaches the most untested code. The first reading, 87.25%
of 30107 lines with the file-by-file table, is in
`bench/measurements/02-276/`.

`make valgrind` runs every unit-test program under valgrind with
`--leak-check=full`, and an invalid access, an uninitialised value or a
definite leak fails it. It takes about a minute and a half, so it is not
part of `make test`.

`make fuzz` builds `build/fuzz/fuzz_mps`, `fuzz_lp`, `fuzz_nl`,
`fuzz_osil`, `fuzz_qplib` and `fuzz_cbf` from `tests/fuzz_readers.c`, with
libFuzzer, ASan and UBSan (`FUZZ_CC=` names another clang). Each writes its
input to a file and calls one reader. Seed a corpus from `tests/data/` and
run one:

```
mkdir -p corpus/nl && cp tests/data/*.nl corpus/nl/
build/fuzz/fuzz_nl corpus/nl -max_total_time=600 -rss_limit_mb=2048
```

A finding is fixed with a test that reads the smallest file showing it.
The first run of all six is in `bench/measurements/02-277/`.

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
4. the documents it changes: `docs/api.md`, `docs/cli.md`, `docs/format-support.md` and
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
