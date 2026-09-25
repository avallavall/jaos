# 02-325 — the eight MIP switches of TODO J11, read on the tree J7 leaves

Taken on 2026-09-25 on the tree of 3ba4192, for TODO row J11. Each switch
that is off by measurement was turned on alone, with `-O name=value`, on
MIPLIB 3 (`make miplib`'s set of 24) and on the 2017 set at 1e10 work
units, at J=2 and 4 GB a solve, against a default run taken the same day.
`arms.sh` runs the eight arms and `both.sh` the pair below. Work is summed
and read as a geometric mean of each instance's ratio over the instances
both runs solve; the 2017 set is read by the gap sum of
`bench/measurements/02-298/gapsum.py` (primal and dual gap, each capped at
1 an instance).

## One switch at a time

| arm | option | MIPLIB 3 solved | work, geometric | work, sum | 2017 gap sum |
|---|---|---|---|---|---|
| default | | 24 | 1.000x | 1.000x | 1.000x |
| zerohalf | `mip_zero_half_rounds=2` | 24 | 1.200x | 1.029x | 0.999x |
| coverlift | `mip_cover_lift=true` | 24 | 1.042x | 0.972x | 1.001x |
| rins | `mip_rins=50` | 24 | 1.025x | 1.001x | 0.992x |
| localbranch | `mip_local_branching=10` | 24 | 2.023x | 1.961x | 0.948x |
| propagate | `mip_propagate=4` | 24 | 1.081x | 0.972x | 1.033x |
| rcfix | `mip_rcfix=true` | 24 | 1.010x | 1.001x | 1.000x |
| probing | `mip_probing=true` | 23 | 1.075x | 1.004x | 1.000x |
| cliquefix | `mip_clique_fix=true` | 24 | 0.980x | 0.734x | 1.002x |

What moves:

- **zerohalf**: p0033 4.64x, misc03 2.47x, enigma 1.96x, mod010 1.64x.
  On the 2017 set only two bounds rise, `enlight_hard` 22.5 to 23.0 and
  `neos5` 14 to 14.125.
- **coverlift**: misc03 2.75x, `l152lav` 0.957x. On the 2017 set two
  models get a first incumbent, `neos-911970` at 179.51 (reference 54.76)
  and `mad` at 2.405 (reference 0.0268), both past the gap cap of 1, and
  `neos-911970`'s bound falls from 29.98 to 28.47.
- **rins**: dcmulti 1.115x, the rest within 1%. On the 2017 set `mas74`'s
  incumbent goes from 17243.1 to 14769.7 (reference 11801.2); nine other
  models move only their bounds, by less than 2e-4 of themselves.
- **localbranch**: rgn 9.57x, p0033 5.36x, gen 4.66x, misc06 3.97x,
  dcmulti 3.79x and ten more past 1.5x. On the 2017 set five incumbents
  improve (`p200x1188c` 38071 to 16858, `tr12-30` 195766 to 167155,
  `exp-1-500-5-5` 85205 to 68734, `neos-3754480-nidda`,
  `neos-3046615-murg`) and twelve bounds fall, since the searches take
  part of the budget the tree had.
- **propagate**: bell5 8.32x, bell3a 1.85x, flugpl 0.27x. On the 2017 set
  18 bounds fall and 4 rise (`enlight_hard` 22.5 to 28), and three
  incumbents get worse (`markshare_4_0` 1 to 3, `gen-ip054`,
  `neos-3046615-murg`) where one gets better (`neos-3754480-nidda`).
- **rcfix**: p0201 1.66x; on the 2017 set one bound moves in its 9th digit.
- **probing**: `bell5` runs out of memory at 4 GB (its tree grows past
  the cap), gen 3.49x. On the 2017 set two bounds move by less than 1e-6
  of themselves.
- **cliquefix**: `l152lav` 0.614x (1742 to 1207 nodes), bell5 1.291x
  (14767 to 20089 nodes), dcmulti 0.898x, lseu 0.920x. On the 2017 set
  `graphdraw-domain`'s bound falls from 14115 to 13180 and three others
  fall by less than 3e-3 of themselves, `exp-1-500-5-5`'s rises from 61253
  to 61361 and `csched007`'s from 295.03 to 295.20, and
  `neos-3046615-murg`'s incumbent goes from 1697 to 1715.

Only clique fixing takes work off MIPLIB 3, and only RINS and local
branching take gap off the 2017 set. Local branching doubles MIPLIB 3.

## Clique fixing and RINS together

`both.sh` runs the pair (`m3-both.txt`, `m17-both.txt`):

| arm | MIPLIB 3 solved | work, geometric | work, sum | 2017 gap sum |
|---|---|---|---|---|
| cliquefix | 24 | 0.980x | 0.734x | 1.002x |
| rins | 24 | 1.025x | 1.001x | 0.992x |
| both | 24 | 1.002x | 0.735x | 0.994x |

On MIPLIB 3 the pair costs bell5 1.292x (14767 to 20089 nodes), rgn
1.145x, misc03 1.141x, misc07 1.138x (8793 to 11377 nodes), misc06 1.104x
and p0033 1.096x, and saves `l152lav` 0.614x, enigma 0.756x (3401 to 2836
nodes), bell3a 0.806x on the same tree, lseu 0.921x and dcmulti 0.957x. On
the 2017 set it keeps RINS's `mas74` incumbent and clique fixing's bound
moves.

## QPLIB's 17 convex MIQPs

`miqp.sh` runs them at 1e10 work units (`miqp-default.txt`,
`miqp-both.txt`, `miqp-cliquefix.txt`). With the pair on, RINS's dives are
barrier solves and take the budget from the tree: QPLIB_3694's bound falls
from 96.94 to 91.49 (7 nodes to 1), QPLIB_3698's from 136.0 to 126.5,
QPLIB_3792's from 270.2 to 258.1, QPLIB_3861's from 171.7 to 158.9, and
QPLIB_3547's incumbent goes from -0.5382 to -0.5296. Clique fixing alone
writes the same lines as the default, work aside on two models.

## The default

`MIP_CLIQUE_FIX` goes on and `MIP_RINS` goes from 0 to 50, with a model
whose objective is quadratic taking 0 unless it sets its own, as MIR
aggregation does since 02-321. MIPLIB 3 and the 2017 set then read as
`m3-both.txt` and `m17-both.txt`, and the MIQPs as `miqp-cliquefix.txt`.
