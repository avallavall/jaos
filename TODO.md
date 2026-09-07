# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: the models people write, and the switches they expect

1. **The LP constructs still refused.** `Lazy Constraints` and `User Cuts`
   sections read as ordinary constraints; `-infinity` and `+infinity` as
   bound values; the remaining gap is quadratic terms, which are QP.
2. **Read other solvers' solution files.** HiGHS, Gurobi, CPLEX and the
   MIPLIB `.sol` shapes into a point file, so `jaos check --point` judges them.
3. **Options as strings.** `jaos_set_option(m, "name", "value")`,
   `jaos_get_option`, `jaos solve --opt name=value`, a `--params FILE`.
4. **Clique cuts** from a conflict graph on the binary columns, at the root.
5. **Python package.** `pyproject.toml`, `pip install .` building the shared
    library, `python -m jaos solve model.mps`.
6. **Defect: `klein2` cycles warm from its own infeasible basis.** A period-two
    cycle between the ratio test's pivot floor and `LU_AGREE_TOL`. Fix so the
    warm attempt answers without the cold retry.
