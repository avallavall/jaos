# 02-342 — the network root's pool and 100 rounds

Taken on 2026-10-04 on the tree of a6ac153 (the cut-row MIR of 02-341), for
TODO row J7. MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work
units a model, against `bench/results/miplib.txt` and `miplib2017.txt` of
a6ac153.

## Why

On a6ac153 `p200x1188c`'s root takes 4.75e9 work units for 11757, 67% of it
in the relaxation's re-solves, and with `--mir-rounds 100` the root alone
spends 4e10. The pool of 02-339 keeps the relaxation small: from the second
round on, a round takes out every cut whose row is basic and slack by more
than 1e-6 (1 + |bound|), keeps it in a pool, and puts back the pool's
violated cuts before it separates.

## Root readings (`--node-limit 1`, `roots.txt`)

| model | a6ac153 | pool | pool, 100 rounds |
|---|---|---|---|
| `beasleyC3` | 737.1, 3.48e9 | 737.0, 0.68e9 | 737.0, 0.68e9 |
| `p200x1188c` | 11757, 4.75e9 | 11716, 0.37e9 | 13137, 1.00e9 |
| `exp-1-500-5-5` | 65516, 0.47e9 | 65363, 0.29e9 | 65363, 0.29e9 |
| `tr12-30` | 130156, 0.26e9 | 129929, 0.19e9 | 129929, 0.19e9 |
| `sp150x300d` | 68.53, solved | 68.53, solved | 68.53, solved |

At 50, 100, 200 and 400 rounds `p200x1188c` ends at 13137 the same way:
`MIP_NET_STALL` (1e-4) ends the rounds before round 50. With a stall of
3e-5 it reads 13270 for 1.16e9, with 1e-5 13973 for 2.68e9, with none
13973 for 5.40e9. Without the pool, 100 rounds on a6ac153 spend 4e10 work
units at the root.

## Arms

| arm | MIPLIB 3, geometric | worst | gap sum | primal | dual | `p200x1188c` bound | `tr12-30` incumbent / bound | `beasleyC3` incumbent / bound |
|---|---|---|---|---|---|---|---|---|
| a6ac153 | 1 | | 15.036 | 7.943 | 7.093 | 12019 | 131361 / 130286 | 835 / 739 |
| `lp20`: pool | 1.0045x | `gen` 1.256x (3 to 9 nodes) | 15.044 | 7.951 | 7.093 | 12071 | 132496 / 130131 | 831 / 740 |
| `lp100`: pool, 100 rounds | 1.0045x | `gen` 1.256x | 14.931 | 7.951 | 6.980 | 13767 | 132496 / 130131 | 831 / 740 |

`lp100` landed as `MIP_NET_ROUNDS` 100 and `MIP_NET_POOL_*`; the landed code
(`m3-land2.txt`, `m17-land2.txt`) reproduces its files to the byte.
`neos-3627168-kasai` holds 1012268 over 956147 against 1007463 over 959377.
`egout` reads 0.886x. Outside network mode nothing changes.
