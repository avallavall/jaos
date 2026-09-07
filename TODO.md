# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: the models people write, and the switches they expect

1. **Semi-continuous variables.** MPS `SC` bound, LP `semi-continuous`
   section, `jaos_set_col_semicontinuous`, branching on the zero side.
2. **SOS1 and SOS2 constraints.** MPS `SOS` section, LP `SOS` section,
   `jaos_add_sos`, branching by the weight split.
3. **Indicator constraints.** LP `->` syntax, `jaos_add_indicator`, enforced
   by branching on the indicator.
4. **The LP constructs still refused.** Every construct
   `docs/format-support.md` lists as unsupported, until the list is empty.
5. **Read other solvers' solution files.** HiGHS, Gurobi, CPLEX and the
   MIPLIB `.sol` shapes into a point file, so `jaos check --point` judges them.
6. **Options as strings.** `jaos_set_option(m, "name", "value")`,
   `jaos_get_option`, `jaos solve --opt name=value`, a `--params FILE`.
7. **Clique cuts** from a conflict graph on the binary columns, at the root.
8. **Python package.** `pyproject.toml`, `pip install .` building the shared
    library, `python -m jaos solve model.mps`.
9. **Defect: `klein2` cycles warm from its own infeasible basis.** A period-two
    cycle between the ratio test's pivot floor and `LU_AGREE_TOL`. Fix so the
    warm attempt answers without the cold retry.
