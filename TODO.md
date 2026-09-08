# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, and wine gives the Linux
   answers. Missing: a native Windows run, clang-cl, and the Python binding
   finding `jaos.dll`.
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
   netlib files.
