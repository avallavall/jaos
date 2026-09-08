# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.
2. **Primal simplex: the 6 of 94 standard instances that run past 10x the
   dual's work** (`bench/results/primal.txt`): d6cube, degen3, dfl001,
   fit1d, fit2d, seba. Down from 17 once phase 2 stopped shifting costs
   and from 9 with steepest-edge pricing (02-31: iterations primal/dual
   1.43x to 1.23x, work 2.75x to 2.65x, pilot87 solves). What is left is
   the long degenerate phase 2 of d6cube, degen3 and dfl001 and the cost
   of the weight update itself on the dense ones; bound perturbation and
   a hashed choice at a tie in the ratio test were refused
   (`bench/refusals.txt`, primal-bound-perturbation and primal-tie-hash,
   the second in four gates, the grow family refusing every one and the
   six not moving in any). What is left unmeasured is a perturbation on
   the pricing side. The row in SPECS stays partial until the count is
   zero.

## Milestone: read more, say more

8. **Read `.nl`** (AMPL's nl format, the text form): the header, the
   linear objective, the linear rows with their bounds, the column bounds,
   the integer columns, the names from a `.col` and `.row` file beside it
   when present. A file with a nonlinear expression is refused by name.
   C API `jaos_read_nl`, the CLI's readers pick it by extension, Python at
   both layers, tests on files written by hand.
9. **Thread count**: `jaos_set_threads` and `--threads N` accept 1, refuse
   0 and negatives, and refuse more than 1 with a message saying JAOS runs
   one thread, so a caller porting from another solver gets an answer
   rather than a silent ignore. Options list, CLI, Python, docs.


