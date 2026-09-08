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

## Milestone: the tree, the rest

6. **Flow cover cuts** at the root: a row read as a single-node flow set,
   its continuous columns' variable upper bounds found in the two-entry
   rows `x <= u y` with `y` binary, a flow cover picked by a greedy
   heuristic, and the Padberg, Van Roy and Wolsey inequality with the
   `L-` strengthening on the outflow side. Measured on the MIP set;
   lands on or off by the reading, behind `--flow-cover-rounds`.
7. **Conflict analysis**: a node whose relaxation is infeasible gives a
   Farkas ray, and the branching bounds the ray needs give a conflict
   constraint over the binaries fixed on the path, added for the rest of
   the search. Measured on the MIP set; lands on or off by the reading,
   behind `--conflicts`.

