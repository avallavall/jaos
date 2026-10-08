# 02-360 — a strong-branching probe that learns from its work cap

Taken on 2026-10-08 on the tree of d45bad0, for TODO rows J7 and J11 and
D293's reopen clause ("a probe that learns from a child stopped by a work
cap"). A probe is a full `jaos_solve` of the node LP with one bound moved,
under `--probe-cap M` times the node's own work; a probe that reaches the
cap teaches nothing today.

`probe-lagrange.diff`, under `JAOS_PLAG`, takes a capped probe's published
basis, factors it, solves for its duals and computes the Lagrangian bound:
each column's reduced cost times its cheaper bound plus each row's dual
times its cheaper side, valid for any duals with no infinite term. The gain
over the node's bound then feeds the pseudocost as a finished probe's does.

At a cap the published basis is not dual feasible: on `p0201` the duals of
`<=` rows come out positive (287 on row 12), so every bound needed an
infinite side and none was taken. With each dual first projected to the
sign its row allows, the bounds are valid but weak, the pseudocosts fill
with gains near zero, and the trees grow (`ptest2.sh`, reliability 4,
20e9 work units; note that `JAOS_PLAG=` with an empty value counts as set,
so both of that script's columns ran the learning form):

| model | cap | nodes, no learning | nodes, learning | work, no learning | work, learning |
|---|---|---|---|---|---|
| `p0201` | 1 | 158 | 373 | 1.61e8 | 2.17e8 |
| `p0201` | 0.25 | 135 | 310 | 0.99e8 | 1.28e8 |
| `misc07` | 1 | 5105 | 8098 | 1.99e9 | 2.99e9 |
| `misc07` | 0.25 | 6583 | 14515 | 2.14e9 | 5.33e9 |

Refused as `probe-lagrange-bound`. The default tree (reliability 0) takes
351 nodes on `p0201` and 13020 on `misc07`; reliability 4 with a cap of 1
alone takes `misc07` to 0.52x and `bell5` to 0.48x of the default's work,
and `p0201` to 2.64x (`ptest.sh`).
