# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Milestone A, publishing 0.4.0, ended with the tag `v0.4.0`. Milestone B,
performance, ended on 2026-09-22 with P0 re-taken. Milestone C, reach and
polish, ended on 2026-09-22 (`bench/measurements/02-293/` to `02-296/`).

The file is empty. Milestone D, the gaps a caller meets first, ended on
2026-09-22: the feasibility relaxation over the columns ends by itself on
a model with no integer point (`bench/measurements/02-297/`), and the
`.nl` reader takes a body of degree two, so a Pyomo quadratic objective
reads.
