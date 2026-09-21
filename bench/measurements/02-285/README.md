# 02-285 — the aggregator: an implied free column substituted out of a short equation

Taken on 2026-09-21 on 3086162 with the aggregator (TODO B3).

## Why

stocfor3 was the worst instance against HiGHS (33.0x) and Clp (23.6x), and
02-20 found that HiGHS's aggregator owns that gap. 02-10 counted the
doubleton equalities a sixth family could remove and found 99.7% of them
behind D97's refused bound transfer. That count took an endpoint as free
only when its box was `(-inf, +inf)`: 19 of 6153 on netlib, 0 of 60382 on
Kennington.

D105's test is wider. A column is implied free by a row when the box the
row implies on it, from the other columns' bounds, lies inside its own box.
Such a column's bounds can never bind, so it can leave with its row and no
bound moves anywhere.

## The count

`census.c` reads each model after stage one's presolve and counts the
equality rows that have a column the row implies free, of any column
length. `census.txt` holds the result. The columns are: rows and columns
after stage one; equality rows; doubleton equality rows; doubletons with an
implied free column; equality rows of any length with one; the same limited
to rows and columns of at most 4 entries; and the fill those would add.

| instance | equality rows | doubletons | implied free doubletons | rows with an implied free column |
|---|---|---|---|---|
| stocfor3 | 8797 | 2032 | 2032 | 7308 |
| ken-18 | 78732 | 38880 | 35093 | 35241 |
| pds-20 | 30088 | 2261 | 2261 | 5664 |
| bnl2 | 1178 | 744 | 738 | 974 |
| cycle | 911 | 193 | 193 | 730 |

So most doubletons need no bound transfer.

## What was built

`src/aggregate.c`, run after stage one, or on the model as loaded when
stage one removes nothing. It passes over the equality rows. In a row of 2
to `AGG_ROW_MAX` entries it takes, among the columns whose coefficient is
at least `AGG_PIVOT_REL` times the row's largest and which the row implies
free, the one with the smallest Markowitz count, up to `AGG_FILL_MAX`. The
column is substituted into every other row it has and into the objective,
and the row and the column are recorded as they are at that moment.

Postsolve goes back through the records. The column's value comes from
its row. Its row's dual is `(c_j - Σ a_rj y_r) / a_ij`, so its reduced cost
is 0. The column is basic and the row sits at its bound. Every other
column keeps the reduced cost the reduced model gave it, because the
substitution changed its cost and its entries by the same multiple of the
row. A Farkas ray and an unbounded ray lift the same way, with the cost at
0. The aggregator does not run on a model with integer columns, cones or
quadratic rows, on a warm start, or in the MIP tree.

`tests/test_aggregate.c` solves five small models through it: an
optimum, the same as a maximization, two substitutions where the second
column was the first one's partner, an infeasible model whose certificate
the checker takes, and an unbounded one whose ray the checker takes.

## The first reading, and the window

The first build kept stage one's window, 8 ulps inside the column's box
(`PRESOLVE_IMPLIED_FREE_ULPS`). It substituted nothing on stocfor3 or
cycle: their flow rows imply a column's bound exactly, as a sum of columns
at 0. A bound implied exactly is redundant, so `AGG_IMPLIED_FREE_ULPS` is 0.

## The sweep

Netlib and the infeasible set, work against the baselines. This sweep ran
before the fix that also aggregates models stage one leaves whole.

| row entries | pivot | fill | netlib | past 2x | failures | infeasible |
|---|---|---|---|---|---|---|
| 4 | 0.01 | 8 | 0.9019x | 0 | dfl001 `NUMERICAL_ERROR` | 0.9159x |
| 2 | 0.01 | 8 | 0.9215x | 0 | | 0.9715x |
| 4 | 0.1 | 8 | 0.9212x | pilot4 4.8x | dfl001 `NUMERICAL_ERROR` | 0.9159x |
| 8 | 0.1 | 16 | 0.8723x | pilot 4.1x | | 0.9160x |
| 2 | 0.1 | 8 | 0.9174x | 0 | | 0.9715x |
| 2 | 0.1 | 32 | 0.9126x | 0 | | 0.9715x |
| 2 | 0.5 | 8 | 0.9291x | pilot 3.3x | | 0.9694x |
| **3** | **0.5** | **8** | **0.8873x** | **0** | | **0.9148x** |

The pilot family moves chaotically with the setting. The last row is the
one setting read that passes, not the top of a curve.

## The setting that ships, on all four gates

Rows of up to 3 entries, pivot 0.5, fill 8, after the fix
(`*-aggH2.txt`):

| gate | work | iterations | past 2x | answers |
|---|---|---|---|---|
| netlib, 94 | 0.8879x | 0.9203x | 0 | 94 solved, checker ok, deterministic |
| infeasible, 29 | 0.7712x | 0.7312x | 0 | 29 refused |
| Kennington, 16 | 0.7445x | 0.8393x | 0 | 16 solved |
| MIP set, 24 | 1.0000x | 1.0000x | 0 | byte-identical |

The largest gains: bore3d 0.283x, bnl2 0.521x, stocfor3 0.617x, pds-02
0.399x, ken-18 0.409x, ex72a 0.097x. The largest losses: pilotnov 1.694x,
boeing1 1.328x, cre-d 1.266x.

The setting at rows of 2, pivot 0.1, fill 32 was read again after the fix
too (`*-aggF2.txt`): netlib 0.9120x, infeasible 0.8315x.

## The suboptimality bounds

The runner flags an answer whose suboptimality bound more than doubles.
Five netlib answers do: bore3d 3.3e-13 to 8.5e-13, cycle 2.9e-12 to
1.2e-11, scagr7 1.4e-15 to 3.2e-15, scfxm2 5.3e-16 to 1.1e-15 and scrs8
8.8e-15 to 3.9e-14. Eight fall by more than half, and the geometric mean of
the ratio over the 94 is 0.946. On Kennington none moves past 2x either
way.

On cycle the checker's largest terms are row duals of about 1e-16 with the
wrong sign, on rows whose other side is an implied bound 1.5e6 away. The
substitution changes which vertex and which basis the dual walks to, and
the rounding in a degenerate dual moves with it.
