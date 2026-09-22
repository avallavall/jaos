# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Milestone A, publishing 0.4.0, ended with the tag `v0.4.0`. Milestone B,
performance, ended on 2026-09-22 with P0 re-taken. Milestone C, reach and
polish, ended on 2026-09-22 (`bench/measurements/02-293/` to `02-296/`).

## Milestone D: the two gaps a caller meets first

Both rows come from a `SPECS.md` row that reads **partial**, and both end
at a caller's model rather than at a benchmark set.

D1 **The feasibility relaxation over the columns never ends on a model
   with no integer point.** SPECS §5, "Feasibility relaxation". Over the
   columns the elastic copy frees every column, and a free integer column
   gives the tree an unbounded space. The bound `M` doubles until the
   total comes out at or below it, so a model whose rows plus integrality
   admit no point at all doubles for ever: `tests/data/relax_runaway.mps`
   only stops on `relax --work-limit N`, and the copy then ends
   `work_limit`. The fix needs a cap on the doubling and a verdict that
   says what the cap means, since a wider box is not proved empty.
   Verify: `jaos relax --cols tests/data/relax_runaway.mps` ends with that
   verdict and a message naming the widest box tried, in bounded work and
   with no work limit given, and `tests/test_relax.c` checks it.

D2 **The `.nl` reader takes only a constant body, so a quadratic
   objective is refused.** SPECS §7, "Modelling-system links". Pyomo
   writes `min x'Qx + c'x` as a nonlinear objective body, and
   `src/nl.c`'s `nl_expr` refuses every body but `n<number>`. The fix
   needs a reader for the body's prefix expression that takes a form of
   degree two or less: `n` constants, `v` variables, `o0` plus, `o1`
   minus, `o2` times, `o3` divide by a constant, `o5` power of two,
   `o16` unary minus and `o54` sumlist, refusing anything else by name
   and refusing a product whose degree passes two. The objective's
   quadratic part goes to `jaos_set_quadratic`, a row's to
   `jaos_set_row_quadratic`. Verify: a hand-written `.nl` whose objective
   is `x0² + 2 x0 x1 + x1² - x0` reads back with the same `Q` as the
   model built through the API, `tests/test_nl.c` checks the refusals by
   name, and `docs/format-support.md` says what the reader takes.
