# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.
2. **Defect: a branched `grow15` never finishes cold, and a branched
   `pilotnov` publishes a point 5.4e-5 outside a row.** Both come from
   `bench/warm`, which branches on the first fractional column of the LP
   optimum. `grow15` with `x0 <= 575295`: cold from the slack basis the dual
   simplex makes no progress for 9261 iterations, switches to Bland at
   10558 and trips the guard at 185201; with the switch disabled it is
   still walking at 81733 iterations and 2e9 work, so the walk cycles and
   not the fallback. The warm start needs 1 iteration. `pilotnov` with
   `x2 <= 4`: warm and cold agree on -4497.2761882 after 1528 iterations,
   but the point violates a row by 5.4e-5 absolute (3.7e-10 relative), and
   the checker's rule is 1e-6 absolute, the same rule the netlib gate
   applies and the unbranched model passes. Two fixes: something for dual
   degeneracy in the first, the published point's unscaled row residue in
   the second. The branched models are one `set_col_bounds` away from the
   netlib files. What the cycle looks like: from iteration 3999 on, rows
   199 and 228 alternate and columns 383/403 and 445/449 swap in and out,
   a period of four with pivots of 1e2 and 1e-2 and dual steps of 1e-10 to
   1e-6 whose sign follows the leaving side, so every step is legal and the
   state still repeats; the cost shifts `shift_to_feasible` writes on each
   pivot are what absorb the gain. Two things tried and dropped (02-31):
   shifting the entering column's cost to zero before the step (branched
   grow15 finishes in 11707 iterations, plain grow15 goes 1871 to 20364),
   and sweeping every shift after each recomputation of the duals
   (branched 1674, plain 11923, three unit tests fail). The next thing to
   try is a deterministic cost perturbation for a stalled dual, settled by
   the existing `settle_shifts`, measured on the three LP gates.
3. **Primal simplex: the 17 of 94 standard instances that overrun or
   disagree with the dual** (`bench/results/primal.txt`). One at a time,
   each with its own diagnosis; the row in SPECS stays partial until the
   count is zero.
