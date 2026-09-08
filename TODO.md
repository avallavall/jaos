# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Bound propagation on the model's rows at the root, on by default only if
   it measures better** on the MIP set with the clique cuts in; the switch
   exists and is off.
2. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, and wine gives the Linux
   answers. Missing: a native Windows run, clang-cl, and the Python binding
   finding `jaos.dll`.
3. **Defect: the warm bench fails on two instances at HEAD.** `bench/warm`
   reports `grow15 ERROR cold solve failed` and `pilotnov REJECTED` with the
   warm and cold solves identical at 1528 iterations, both before the
   2026-09-08 changes; `bench/results/warm.txt` was last written before them.
   Find what the branched models do and fix or document it.
