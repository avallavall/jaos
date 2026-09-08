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
   of the weight update itself on the dense ones; bound perturbation was
   refused (`bench/refusals.txt`). The row in SPECS stays partial until
   the count is zero.
3. **MIP presolve, the rest: a clique table.** Coefficient tightening
   landed on 02-31 (`--tighten`, 0.9999x on the MIP set). Probing exists
   behind `--probing` and `--probing-cap` and is off: both forms are
   measured and refused (`bench/refusals.txt`, mip-probing-root), the
   second at 1.109x with no column fixed on any of the 24. A clique
   table would feed the clique cuts from the model's rows, and is where
   the implications probing finds could go instead of into bounds.
