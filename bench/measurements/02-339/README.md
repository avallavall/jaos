# 02-339 — a pool that keeps the root's relaxation small

Taken on 2026-10-04 on the tree of 53fcab9, for TODO row J7 (the networks'
root bound). `mir-agg-distance` and `net-agg-steps` (02-330) reopen with
"a cut pool that keeps the root's LP small". MIPLIB 3 at J=2 and 4 GB a
solve, the 2017 set at 1e10 work units a model, against
`bench/results/miplib.txt` and `miplib2017.txt`.

## What was built

`root-pool.patch` holds three environment switches over 53fcab9.

- `JAOS_ROOTPOOL=1`: from the root's second cut round on, a round starts by
  taking out of the relaxation every cut whose row is basic and whose
  activity is over its bound by more than 1e-6 (1 + |bound|)
  (`JAOS_ROOTPOOL_SLACK`). The cut stays in the pool. The round then scans
  the pool for cuts the current point violates with an efficacy over 1e-4,
  adds back the 200 most efficacious and solves again before it separates.
  It runs in network mode only, unless `JAOS_ROOTPOOL_ALL` is set.
- `JAOS_NETSTEPS=K`: K aggregation steps in network mode instead of 6.
- `JAOS_AGGDIST=1`: the aggregation substitutes out the continuous column
  farthest from its bounds (02-330's `mir-agg-distance`).

The arms `rp` and `ns12rp` ran an earlier form: the pool on every model,
and a cut taken out when its row is basic, slack or not.

## Root readings (`--node-limit 1`)

`roots.txt` holds the relaxation after the root's cuts and the root's work.

| model | 53fcab9 | pool | 12 steps | 12 steps and pool | HiGHS's root |
|---|---|---|---|---|---|
| `beasleyC3` | 719.6, 0.68e9 | 722.3, 0.40e9 | 739.7, 3.02e9 | 738.7, 0.94e9 | 733 |
| `p200x1188c` | 9980, 0.088e9 | 9990, 0.093e9 | 9992, 0.145e9 | 9979, 0.119e9 | 11640 |
| `exp-1-500-5-5` | 65183, 0.24e9 | 65193, 0.21e9 | 65736, 0.35e9 | 65703, 0.28e9 | 65684 |
| `tr12-30` | 116340 | 116340 | 116539 | 116655 | |

The pool keeps `beasleyC3`'s relaxation at 2141 to 2596 rows where it
grew past 4000, and makes the 12 steps cost a third of what they did.

## Arms

| arm | form | MIPLIB 3, geometric | worst | 2017 gap sum | dual part | `beasleyC3` incumbent / bound | `p200x1188c` bound |
|---|---|---|---|---|---|---|---|
| base | 53fcab9 | 1 | | 15.28 | 7.288 | 797 / 724 | 10789 |
| `rp` | pool on every model, basic rows out | 1.017x | `enigma` 2.585x, `mod008` 1.741x | 16.77 | 7.825 | 809 / 726 | 10541 |
| `ns12rp` | the same, 12 steps, distance rule | 1.082x | `egout` 3.288x | 16.68 | 7.774 | 793 / 745 | 10796 |
| `rp2` | pool in network mode, slack rows out | 1.001x | `gen` 1.024x | 15.43 | 7.305 | 844 / 727 | 10504 |
| `ns12rp2` | the same, 12 steps, distance rule | 1.068x | `egout` 3.290x | 15.31 | 7.263 | 830 / 741 | 10461 |

On every model the pool takes `neos-911970`'s bound from 51.8 to 23.26:
its root sits flat for five rounds while its cuts gather, and with the
pool the cuts that are slack in those rounds leave and the bound never
moves (root-only, 23.26 against 45.42, with the slack rule too). In
network mode the 12 steps lift `beasleyC3`'s bound at the limit from 724 to
741, and its incumbent gets worse (797 to 830); `p200x1188c`'s bound falls
in every arm. The distance rule, which these arms apply outside network
mode too, lifts `timtab1`'s bound from 414399 to 432840, as it did in
02-330. No arm lowers the gap sum, and the 12 steps cost MIPLIB 3's
`egout` 3.3x.

Refused as `net-root-pool` in `bench/refusals.txt`.
