# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Milestone A, publishing 0.4.0, ended with the tag `v0.4.0`. Milestone B,
performance, ended on 2026-09-22 with P0 re-taken. Milestone C, reach and
polish, ended on 2026-09-22 (`bench/measurements/02-293/` to `02-296/`).

## Milestone D: the gaps a caller meets first

The row comes from a `SPECS.md` row that reads **partial**, and it ends at
a caller's model rather than at a benchmark set. D1, the feasibility
relaxation that never ended over the columns, landed on 2026-09-22
(`bench/measurements/02-297/`).

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
