# 02-289 — the crossover's dual push, refused

Taken on 2026-09-22 on the tree of e4a9f82 (the primal push, 02-287) for
TODO row B6, by `bench/barrier` on the standard 94 at the cap of 10x the
dual's work. The bar was the primal push's own reading: fewer than 14
overruns and a geometric mean under 3.1775x the dual.

`dual-push.diff` is the candidate in its row-wise form (the column-wise
form prices each nonbasic column through `push_dot` instead). After the
primal push has put every
nonbasic column on a bound, it computes the reduced costs at the barrier's
duals. Each basic column whose reduced cost is past `CROSS_DUAL_TOL` times
(1 + |its cost|), and whose value sits on the bound that reduced cost asks
for, is taken out of the basis by a dual ratio test over its row of the
basis inverse (Harris's two passes). The duals move with each step, a
nonbasic column that reaches a reduced cost of zero enters, and the LU is
updated. When nothing blocks, the column stays basic with a reduced cost
of zero.

| arm | agreed | overrun | barrier / dual work | iterations |
|---|---|---|---|---|
| the primal push alone (02-287) | 80 | 14 | 3.1775 | 0.2567 |
| dual push, each column priced, tolerance 1e-7 | 76 | 18 | 3.1692 | 0.2129 |
| the same at 1e-9 | 76 | 18 | 3.1750 | 0.2081 |
| the same, the dual simplex after it | 74 | 20 | 3.1586 | 0.2108 |
| dual push priced through the row-wise matrix | 78 | 16 | 3.2083 | 0.2101 |
| the same, the dual simplex after it | 77 | 17 | 3.2439 | 0.2124 |

Every arm fails the bar. The dual push lowers the simplex's iterations after
it by about a fifth, and its own pricing costs more than that saves. The
first form prices every column once per pushed basic column: sctap2,
sctap3, sierra and tuff pass the cap inside the push itself, before the
simplex starts. Pricing through the row-wise matrix over the row's nonzeros
brings the overruns from 18 to 16, still two more than the primal push
alone, and its mean is worse. The dual after the push is worse than the
primal after it in every form.
