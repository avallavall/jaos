# 02-295 — the push tried when mu is gone

Taken on 2026-09-22 on the tree of 2ed4237, for TODO row C4.

## What QPLIB_8785 did

At 1e11 work units QPLIB_8785 (10399 columns, the augmented system)
reached the library's objective, 7867.491149, by iteration 36. From
iteration 30 on its primal residual was 1.4e-16 and `mu` fell from
1e-70 to 2e-195, while its dual residual shrank by a factor of 0.77 per
iteration: 9.6e-5 at iteration 30, 8.2e-7 at 48 and 4.8e-8 at 59, where
the budget ran out, six iterations short of `BARRIER_TOL`.

## The change

A quadratic walk within `BARRIER_NEAR_TOL` (1e-6) on all three measures
with `mu` under `BARRIER_MU_DEAD` (1e-30) stops and hands its point to the
push. If the push settles, that is the answer. If not, the walk goes on
from where it stopped, and everything after is as before.

## Readings

`make maros-meszaros J=4`: 137 solved, 137 objective ok, 136 checker ok,
nothing regressed. The rule fired on six instances, each ending at the
same objective:

| instance | work before | work after |
|---|---|---|
| q25fv47 | 4254843371 | 932693114 |
| qgrow22 | 67023693 | 44583604 |
| aug2d | 78986535 | 63300956 |
| aug2dc | 78986535 | 63300956 |
| genhs28 | 25108 | 20114 |
| huestis | 13359474 | 13365372 |

QPLIB's 19 convex QPs (types CCB, CCL and DCL) at 1e11 work units,
`qplib-cqp-1e11-main.txt` against `qplib-cqp-1e11-early.txt`:
QPLIB_8785 goes from the work limit to `OPTIMAL` at 7867.4911488 in
8.5e10 work units, and QPLIB_8845 takes 7.6e8 work units where it took
4.1e9. The other 17 read the same. The CLI's checker, at its default
tolerance of 1e-7, refuses QPLIB_8785's answer on the model's own gap,
1.3e-7: the rows imply bounds on some columns, and the terms charged to
them, about 2e-3 of objective, drop out of that gap. The certified
suboptimality is 6.4e-17, and at the runners' 1e-6 the checker takes it.

QPLIB's 17 convex MIQPs at 1e10: QPLIB_3980 takes 167 nodes where it took
86, its bound -2.6155 where it was -2.6858; the other 16 read the same
(`qplib-miqp-1e10-*.txt`).

`tests/test_barrier.c` builds GENHS28 from its definition, the sum of
`(x_i + x_(i+1))²` under `x_i + 2 x_(i+1) + 3 x_(i+2) = 1`, and checks that
the push settles from the stopped walk.
