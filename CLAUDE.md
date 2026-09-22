# JAOS — Just Another Optimization Solver

An LP, MIP, QP and conic solver in C23. No dependencies, Apache 2.0, Linux/GCC 14.
Built and tested under WSL; the Windows side has no compiler. The POSIX
calls sit behind `src/jaos_sys.h`, and `tests/windows.sh` cross-compiles
with mingw-w64 and runs the tool under wine, both installed in the WSL.

## The record is three files

- `SPECS.md` — the complete list of features JAOS must have, one row each,
  with a status: done, partial (says what is missing), missing, out of scope.
  It is closed. A row changes status when its feature lands. Nothing else
  goes in it.
- `TODO.md` — the backlog for the current milestone. Rows taken from SPECS.
  Delete a line in the same commit that lands it. When the file is empty,
  pick the next rows from SPECS and fill it again.
- `bench/refusals.txt` — ideas that were built, measured and found worse.
  One line each with what would make it worth trying again. Read it before
  trying a performance idea.

There is no changelog, no decisions file and no session log. Git history is
the log. Old decisions (`D<n>` in docs) are in `git show 2d3c56b:DECISIONS.md`.
Code carries no comments. `docs/` holds the constants (`tolerances.md`), the
formats, the CLI reference and the comparison with other solvers.

## The loop

1. Take items from `TODO.md`. Build them as a batch. Every feature reaches:
   the C API in `include/jaos.h`, a test in `tests/`, the CLI in `cli/jaos.c`
   with a check in `tests/cli.sh`, and `python/jaos/` at both layers
   (`Model` in `model.py` over the ctypes calls of `_native.py`, and
   `Problem` in `problem.py`) with a test in `python/test_jaos.py`. Update
   `docs/api.md`, `docs/cli.md` or `docs/format-support.md` when a
   function, a command or a format changes.
2. `make test && make sanitize`. `make python-test` when `python/` changed.
3. Only when solver internals changed (`simplex.c`, `lu.c`, `presolve.c`,
   `aggregate.c`, `scale.c`, `mip.c`): `make netlib netlib-infeas J=12`,
   then `make netlib-kennington J=2` in its own job, and `make miplib J=2`
   in its own job when `mip.c` changed. Kennington and MIPLIB run at `J=2`
   because a MIPLIB tree once grew past 8 GB at a higher J.
   `make maros-meszaros` when `barrier.c` or `chol.c` changed.
   `make cblib` when `conic.c`, `conictree.c` or `chol.c` changed.
   Read `bench/results/*.txt` against the baselines; no instance may
   regress. When the dual or the primal simplex, presolve or the crossover
   changed, also re-take `make primal barrier pdlp concurrent warm J=12`
   and read them. Those readings went stale for two weeks and hid three
   bugs. Rewrite a baseline only with its `*-baseline` target and only
   after reading the diff.
4. Update the SPECS row, delete the TODO line, commit, push.

Commits and pushes are at Claude's discretion. No reviewer agents, no
instruction counts, no record checker.

## Rules

- Bit-identical results on every machine and every run. No clock decides
  anything, no iteration order depends on an address, no reassociated
  floating point, no unseeded randomness. `-ffp-contract=off` is load-bearing.
- No dependencies and no code read from other solvers. Papers and textbooks
  only. Unity in `tests/vendor/` is the one exception.
- A new constant goes in `docs/tolerances.md` with the measurement that set it.
- Work units are the cost. Seconds never go in `bench/results/` or a baseline.

## WSL

```
wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && make test"
```

Put a long command sequence in a script file and run the file. `$?` does not
survive the Git Bash to WSL boundary. Push from Windows: the remote's SSH
alias exists only there. Never `git add -A`. Stage explicit paths.
