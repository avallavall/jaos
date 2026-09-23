# 02-302 — doubleton bound moves and the any-row implied-free test

Taken on 2026-09-23 for TODO row H1, on the tree of 1d6c9ee. HiGHS's
presolve takes `stocfor3` to 8259 rows and JAOS's to 13305
(`bench/measurements/02-301/`). `doubleton-moves.patch` adds two things
to the aggregator in `src/aggregate.c`:

- `anyrow`: a column counts as implied free when any of its rows implies
  each of its bounds, not only the row being aggregated (D97's test with
  general row sides).
- `moves`: on a doubleton equation whose columns are not implied free,
  the removed column's bounds move onto the other column (widened by two
  ulps), and postsolve rebuilds the dual of the row from the other column
  when the kept column ends on a moved bound. An aggregated model found
  infeasible this way is solved again without the aggregator, for the
  certificate. Off under the barrier and PDLP.

`split.sh both moves anyrow` copies the tree to `~/jaos-rf-a<variant>`,
applies the patch, turns one half off for `moves` and `anyrow`, and runs
netlib, the infeasible set and Kennington. `python3
../02-301/summarise.py REPO moves anyrow` sets each copy against the
committed results. The `both` reading was taken in the main tree.

| variant | work vs HEAD (139) | past 2x | gate lines | `stocfor3` rows, work |
|---|---|---|---|---|
| both | 0.9913 | gosh 3.58, etamacro 2.57, dfl001 2.50, bgindy 2.13 | netlib 10 (woodw rsub 33.2x) | 12089, 0.865x |
| moves | 0.9762 | gosh 3.59, etamacro 2.57, bgindy 2.05 | netlib 13, infeasible 2 | 13305, 1.000x |
| anyrow | 1.0029 | dfl001 2.15 | netlib 8 (tuff rsub 126.8x) | 12089, 0.865x |

Every verdict agrees in every variant. Neither half reads at or under
0.95x without an instance past 2x, so neither is kept (bench/refusals.txt:
`agg-doubleton-moves`, `agg-any-row-implied-free`).

- `moves`: the checker refuses `standata` and `standmps` (suboptimality
  bound 0.0288 and 0.0307), so the duals postsolve rebuilds are wrong on
  those two models; the cause was not traced. `gosh` (5721 to 12333
  iterations) and `bgindy` are infeasible only after the move, so they
  pay for the aggregated solve and the plain one.
- `anyrow`: `dfl001`'s aggregated solve ends with a numerical error and is
  solved again without the aggregator (6071 rows after presolve in the
  result line), which is the 2.15x. `pilotnov` reads 0.55x and `pds-20`
  0.68x.
