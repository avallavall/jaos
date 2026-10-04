# 02-333 — the node LPs of the MIP tree perturb on the first stall

Taken on 2026-10-04 on the tree of 7ae097b, for TODO row J7. The settings
are 02-328's; the arms ran from 02-329's `arms.sh`. `arms.patch` is the
measured `simplex.c`: two switches over node solves (`cfg.node_solve`).
`JAOS_NODEPERTURB` lets a node solve perturb its costs after
`PERTURB_STALL_FACTOR`'s plateau, as every other dual solve does, where it
waited for `STALL_FACTOR`'s, ten times longer. `JAOS_NODELONG=K` makes a
node solve's steepest-edge weights exact once, when it has run `K` times
`nrow + ncol + 1` iterations; a node solve keeps guessed weights
otherwise (`DSE_GUESS_RESTARTS`, warm-weights-eager).

## The root of csched008

`csched008`'s relaxation takes 19866 iterations and 8.7e8 work units cold.
Its six cut rounds leave the bound at 171 and take 267296 iterations and
3.52e10 work units, so at the 2017 limit of 1e10 the root never ends and
the bound reads -inf. With heuristics off and `--node-limit 1`:

| switches | iterations | work units |
|---|---|---|
| none | 267296 | 3.52e10 |
| `NODEPERTURB` | 116568 | 1.29e10 |
| `NODELONG=1` | 157174 | 1.42e10 |
| both, `NODELONG=1` | 29156 | 3.52e9 |
| both, `NODELONG=5` | 82934 | 1.07e10 |
| both, `NODELONG=20` | 116568 | 1.29e10 |

## The arms

Against `bench/results/miplib.txt` and `bench/results/miplib2017.txt` of
7ae097b.

| arm | switches | MIPLIB 3 geo | sum | worst | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|---|---|
| 7ae097b | | 1 | 1 | | 3 | 25 | 8.077 | 8.460 | 16.536 |
| b7 | both, `NODELONG=1` | 1.051 | 1.152 | `enigma` 2.196 | 3 | 25 | 8.031 | 7.300 | 15.330 |
| b7l | `NODELONG=1` | 1.037 | 1.196 | `enigma` 1.524 | | | | | |
| b7p, b8 | `NODEPERTURB` | 0.981 | 0.939 | | 3 | 25 | 8.061 | 8.308 | 16.368 |
| b7pl5, b7pl20 | both, `NODELONG` 5 and 20 | 0.981 | 0.939 | | | | | | |

`b8` is the default: one condition left out of `price_row`. On MIPLIB 3
two trees move, `enigma` 0.704x (2836 to 1983 nodes) and `l152lav` 0.904x.
On the 2017 set `neos-911970`'s bound at the limit goes from 45.0 to 52.0
(HiGHS's root 52.1), `neos-2657525-crna`'s incumbent from 825 to 53.8,
`pk1`'s bound from 8.09 to 8.42 and `neos5`'s from 14 to 14.04;
`csched007`'s bound falls from 295.2 to 291.9 and `csched008`'s root still
passes the limit.

`b7` adds the exact weights after one size of iterations: `csched008` gets
a bound of 171 at the limit and the gap sum reads 15.33, but `enigma` runs
8016 nodes (2.196x) and `l152lav` 1.192x, `misc07` 1.254x. At five or
twenty sizes the weights never turn exact on MIPLIB 3 and `csched008`'s
root still costs over 1e10. Refused as `node-dse-exact-long`.

## The other readings

The LP gates and readings (`netlib`, `netlib-infeas`, `netlib-kennington`,
`primal`, `barrier`, `pdlp`, `concurrent`, `warm`) are byte-identical: no
solve outside the MIP tree takes this path. `miqp-b8.txt`, QPLIB's 17
convex MIQPs as in 02-332, has every status, incumbent, bound and node
count of 02-332's `miqp-b6.txt`.
