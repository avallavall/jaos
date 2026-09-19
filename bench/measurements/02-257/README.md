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

## Files

- `pyomo.sh`, `pyomo_asl.py`: the reading.
