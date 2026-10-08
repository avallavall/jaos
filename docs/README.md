# The documents in docs/

One line per file. `SPECS.md`, `TODO.md` and `bench/refusals.txt` at the
top of the repository are the project's record. These pages explain the
code and the formats.

| file | what it holds |
|---|---|
| [`api.md`](api.md) | every function of `include/jaos.h`, in the header's order, with the rules every call follows |
| [`build.md`](build.md) | how to build, test and install on Linux, Windows and macOS, and the CMake package |
| [`cli.md`](cli.md) | the `jaos` tool: every command, every flag, the exit codes |
| [`feature-matrix.md`](feature-matrix.md) | JAOS beside HiGHS, SoPlex, Clp, SCIP, Gurobi and Hexaly, row by row |
| [`format-support.md`](format-support.md) | what each reader takes and refuses, and the contract every writer holds |
| [`scaling.md`](scaling.md) | Curtis-Reid scaling as JAOS computes it |
| [`tolerances.md`](tolerances.md) | every constant in the code and the measurement that set it |
| [`work-units.md`](work-units.md) | the deterministic budget: what each kernel charges |

## Research notes

`research/` holds designs and literature readings worked out on paper
before a change. Each note was written for one decision and is not
updated after it. In a note, `DECISIONS.md` means
`git show 2d3c56b:DECISIONS.md`, and `TODO.md §N` means the TODO of the
note's date.

| note | status |
|---|---|
| [`approximate-edge-pricing.md`](research/approximate-edge-pricing.md) | built for the primal simplex, measured and refused (D244 in `bench/refusals.txt`, `bench/measurements/02-156/`) |
| [`crash-basis.md`](research/crash-basis.md) | built, measured and refused (`SPECS-crash-basis` in `bench/refusals.txt`) |
| [`dual-postsolve-imposed-bound.md`](research/dual-postsolve-imposed-bound.md) | not built; D97, which needs it, stays refused |
| [`exact-verification.md`](research/exact-verification.md) | built: the exact proof of a basis and its values |
| [`harris-primal.md`](research/harris-primal.md) | built: the primal simplex's two-pass ratio test |
| [`hyper-sparsity.md`](research/hyper-sparsity.md) | built: the hyper-sparse FTRAN and BTRAN |
| [`netlib-campaign.md`](research/netlib-campaign.md) | the first Netlib gate campaign, closed |
| [`postsolve-basis-recovery.md`](research/postsolve-basis-recovery.md) | built: postsolve publishes a basis of `num_row` basics |
| [`primal-simplex.md`](research/primal-simplex.md) | built: the primal simplex; its §5 and §8 predate the barrier and Devex |
