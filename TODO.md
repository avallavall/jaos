# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

Milestone A, publishing 0.4.0, ended with the tag `v0.4.0`. Milestone B,
performance, ended on 2026-09-22 with P0 re-taken. Milestone C, reach and
polish, ended on 2026-09-22 (`bench/measurements/02-293/` to `02-296/`).
Milestone D, the gaps a caller meets first, ended the same day
(`bench/measurements/02-297/`).

## Milestone E: the switches that are off

E1 **The three cut families are off and no reading has landed one.**
   SPECS §4, "Flow cover, zero-half, lifted cover cuts". All three exist
   and were measured worse on MIPLIB 3: flow covers at 1.012x
   (`--flow-cover-rounds`), zero-half at 1.182x (`--zero-half-rounds`)
   and lifted covers behind `--cover-lift`. Both readings predate the
   aggregator, the node order and the parking of a failed node, and
   neither was taken on the 2017 set, where the tree spends its whole
   budget and a cut that lifts the bound shows. The fix needs the 2017
   reading of each family, by the pay rule milestone B used: the gap sum
   at or under 0.95x of the default with no fewer solved and no fewer
   incumbents. Verify: `make miplib2017 J=4 MIPLIB2017_ARGS='-O ...'`
   per family, the four result files kept in a measurement folder, and
   either a default changed with `make miplib` re-read against its
   baseline, or a line in `bench/refusals.txt` with the numbers.
