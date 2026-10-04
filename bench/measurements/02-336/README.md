# 02-336 — MIPLIB 3's large trees: root points and strong branching

Taken on 2026-10-04 on the tree of e24e854, for TODO row J7 (MIPLIB 3:
`bell3a`, `bell5`, `l152lav`). MIPLIB 3 at J=2 and 4 GB a solve.

## What HiGHS does on them

HiGHS 1.15.1 with `bench/compare/highs-mip.opt` finds the optimum at the
root on all three with its sub-MIP and closes them in 215, 322 and 19
nodes. Its root bounds are 873833 (`bell3a`), 8660781 (`bell5`) and
4669.5 (`l152lav`). JAOS's roots reach 870924, 8653018 and 4661.2, hold
160055600 (lock rounding), 12419324 (the feasibility jump) and 5244 (the
pump), and the trees take 82261, 20089 and 1362 nodes.

## Better root points do not shrink the trees

`submip-root.patch` holds two switches over the root sub-MIP:
`JAOS_RENSALWAYS` runs RENS at the root with an incumbent too, cut off at
it, and `JAOS_SUBROOT=K` sets the root sub-MIP's work to K times the
root's work, where `MIP_SUBMIP_ROOT` sets 0.5. RENS alone finds nothing
better on the three. At K = 5 or 20 `bell3a`'s root point becomes 882094
or 880869 and `bell5`'s 8974288 (at 20), and the trees keep their sizes:
`bell3a` 82601 nodes, `bell5` 20089, `l152lav` 1362. The trees are spent
proving the bound.

## Strong branching, read again

`-O mip_reliability=K` with a work limit of 2e10 a model, against
`bench/results/miplib.txt` of e24e854:

| reliability | solved | work, geometric | work, sum | below 0.5x | past 2x |
|---|---|---|---|---|---|
| 1 | 23 | 1.209x | 1.576x | `gt2` 0.149x, `p0282` 0.400x | `bell5` at the limit, `misc03` 3.550x, `rgn` 2.679x |
| 2 | 24 | 1.060x | 0.965x | `gt2`, `blend2` 0.339x, `bell5` 0.471x, `flugpl` 0.472x, `p0282` 0.495x | `misc03` 4.715x, `enigma` 3.018x, `rgn` 2.996x, `mod008` 2.059x |
| 4 | 24 | 1.138x | 0.856x | `gt2`, `blend2` 0.354x, `bell5` 0.414x | `misc03` 5.379x, `enigma` 3.513x, `rgn` 3.440x, `mod008` 2.796x |
| 8 | 24 | 1.253x | 0.886x | `gt2`, `bell5` 0.435x | `misc03` 6.336x, `enigma` 6.263x, `rgn` 4.125x, `mod008` 3.238x, `dcmulti` 2.430x, `khb05250` 2.332x, `p0201` 2.085x |

At 4, `l152lav` closes in 69 nodes for 0.81x its work and `bell3a` in
63661 for 0.66x. The large trees gain and the small ones pay for every
probe, as D293 read in 02-192. The probes learn only from a child solved to
optimality: a child stopped by a work cap, or proved infeasible, teaches
nothing (`strong_probe` in `src/mip.c`).

## A budget for the probes

`sb-budget.patch` lets a node probe only while the probes' work so far stays
under `JAOS_SBQUOT` times the node relaxations' work plus `JAOS_SBOFS`
times the model's nonzeros, columns and rows: a limit on the probes' share
of the tree's work. At reliability 4:

| quotient | allowance | work, geometric | work, sum | past 2x |
|---|---|---|---|---|
| 0.5 | 1000 | 1.089x | 0.982x | `rgn` 2.382x, `misc03` 2.149x |
| 0.5 | 100 | 1.071x | 0.922x | `misc03` 3.650x, `rgn` 2.433x |
| 0.2 | 100 | 1.022x | 0.959x | `rgn` 2.277x, `misc03` 2.270x, `misc07` 2.173x |

The budget takes the probes' price down, and `rgn` and `misc03` still end
with two to three times the nodes they reach without probes (127 to 377
and more, 240 to 489 and more). Refused with D293.

`sb-infeasible.patch` branches at once on a column one of whose probe
children is infeasible (`JAOS_SBINF`): 1.127x at reliability 4 and 1.125x
at 1, `rgn` still at 421 nodes. Infeasible children do not explain the
larger trees.
