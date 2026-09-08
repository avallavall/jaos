# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.
2. **Primal simplex: the 9 of 94 standard instances that overrun or
   disagree with the dual** (`bench/results/primal.txt`), down from 17 once
   phase 2 stopped shifting costs (02-31). Four stall in phase 1: d6cube,
   degen3, dfl001, maros-r7; four run phase 2 past 10x the dual's work:
   bnl2, fit1d, fit2d, scsd8; pilot87's phase 1 ends in a numerical error.
   One at a time, each with its own diagnosis; the row in SPECS stays
   partial until the count is zero. What the stalls are (02-31): not
   cycles but degenerate walks. d6cube's phase 1 takes 3204 iterations and
   12x the dual's whole work with the infeasibility falling steadily, and
   57% of its steps are zero-length, 50% in phase 2; degen3 78% in phase
   1; scsd8 81% and 67%; bnl2 39% and 16%. Bound perturbation, at the
   start and on demand at the ties, was built and measured worse
   (`bench/refusals.txt`, primal-bound-perturbation): it shortens no walk,
   it only displaces its vertices. What is left to try is on the pricing
   side, a rule that changes the entering choice at a degenerate vertex,
   or a steepest-edge primal against Devex; either is measured on
   `bench/results/primal.txt` with the dual path untouched.
