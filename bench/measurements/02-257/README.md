# 02-257 — JAOS as an AMPL solver, read back by Pyomo

AMPL calls a solver as `solver STUB -AMPL`: the solver reads `STUB.nl`,
takes its options from `$solver_options` and the words after `-AMPL`,
and writes `STUB.sol`, the message and the answer that AMPL reads back
when the solver exits 0 (Gay, "Hooking Your Solver to AMPL"). Pyomo's
`asl:` interface and JuMP's AmplNLWriter call solvers the same way.

Since 2026-09-19 `jaos STUB -AMPL` does this, and `jaos_write_sol_ampl`
writes the file. The layout was not taken from any code: it is the
message, a blank line, `Options` with the option values of the `.nl`
header (`g3 1 1 0` gives `3`, `1`, `1`, `0`), then the counts of rows,
of row duals sent, of columns and of column values sent, the duals, the
values, and `objno 0 CODE`. The duals go out for a continuous optimum;
the values go out for an optimum or a limit that left a point. CODE is
AMPL's: 0 solved, 200 infeasible, 300 unbounded, 400 a limit with a
point, 401 a limit without one, 500 a failure, which is also what a file
JAOS cannot read or an option it does not know gives, with the reason in
the message.

## The reading

`pyomo.sh` makes a virtual environment with Pyomo, here 6.10.1, and runs
`pyomo_asl.py`: eight small models through
`SolverFactory("asl:jaos", executable=build/cli/jaos)`.

| model | Pyomo reads back |
|---|---|
| LP, 2 columns and 3 rows | `optimal`, x 4 and y 1, objective -7, duals 0, 0 and -2 |
| MIP, an integer, a binary and a continuous column | `optimal`, i 6, b 1, z 1/3, objective 33.33 |
| infeasible LP | `infeasible`, no point |
| unbounded LP | `unbounded`, no point |
| knapsack, 18 binaries | `optimal`, objective 475 |
| the knapsack with `work_limit` 1 | `maxIterations`, no point |
| the knapsack with an unknown option | `internalSolverError`, JAOS's message |
| a quadratic objective | `internalSolverError`: JAOS's `.nl` reader refuses a nonlinear body |

Every value and every dual is the one JAOS's own solution file gives,
and Pyomo maps each code to its own termination condition. Pyomo passes
its solver options through `$jaos_options`, so an option set on the
process's own environment does not reach JAOS; `opt.options[...]` does.

## JuMP

`jump.sh` runs `jump_asl.jl`, the same models through
`AmplNLWriter.Optimizer(build/cli/jaos)` under Julia 1.13.0, JuMP 1.31.2
and AmplNLWriter 1.4.1. The first run failed on every model, for two
reasons:

- JuMP's `.nl` declares its objective nonlinear on header line 3 when
  the body is a constant (`n7`), and JAOS refused a nonzero count there
  and on line 5. The reader now takes those counts and lets each body
  decide: a nonlinear body is still refused where it is read.
- AmplNLWriter checks that `STUB.sol` gives as many rows as the model it
  wrote, and a file JAOS refused came back with 0 rows and 0 columns.
  A refused file now keeps its header's counts and options, and the
  `.sol` reports the failure with them.

After the two fixes:

| model | JuMP reads back |
|---|---|
| LP | `LOCALLY_SOLVED`, x 4 and y 1, objective -7, duals 0, 0 and -2 |
| MIP | `LOCALLY_SOLVED`, i 6, b 1, z 1/3 |
| infeasible LP | `LOCALLY_INFEASIBLE` |
| unbounded LP | `DUAL_INFEASIBLE`, JuMP's word for an unbounded primal |
| knapsack | `LOCALLY_SOLVED`, objective 475 |
| the knapsack with `work_limit=1` after `-AMPL` | `OTHER_LIMIT`, no point |
| the knapsack with an unknown option | `OTHER_ERROR`, JAOS's message |

JuMP writes the options on the command line after `-AMPL`, and says
`LOCALLY_SOLVED` for code 0 because the protocol does not say whether
the solver proves a global optimum.

JuMP also writes an `x` segment, the starting values, and AMPL does the
same when the modeller gives some. The reader used to drop it; for a
model with integer columns it is now the MIP start, a column it does
not name starting at 0. The start is judged like any caller's start,
so one that is not a feasible integer point is not taken.

## Files

- `pyomo.sh`, `pyomo_asl.py`: the Pyomo reading.
- `jump.sh`, `jump_asl.jl`: the JuMP reading.
