# 02-488 — crossover, mutation and DINS: three sub-MIPs around the points already found

Taken on 2026-11-18 on 37720d32, for TODO row J32. `read.sh` takes the
readings from copies of 37720d32 with `prototype.patch` and of the landed
commit. `prototype.patch` holds every generic heuristic this batch built,
switched by bits of `JAOS_GH` (0, the default, is 37720d32); 02-489 and
02-490 read its dives and its ZI rounding from the same copies.

- `miplib2017/*.txt`: MIPLIB 2017's 30 at 1e10 work units through the
  CLI, two solves at once: `lns` is `JAOS_GH=448` (crossover, mutation,
  DINS), `land` the landed commit. `*-who.txt` lists, per model, each
  heuristic's runs, incumbents and work, and what found the last
  incumbent. `m17cmp.txt`, `sum.txt`: 02-474's and 02-328's tables
  against `bench/results/miplib2017.txt`.
- `miplib3/*.txt`, `m3cmp.txt`: MIPLIB 3 through `bench/run` against
  `bench/miplib.baseline`: `null` is `JAOS_GH=0`, `lns` the three,
  `lns-noroot` and `lns-root01` the same with crossover's root share
  (`JAOS_GH_CROOT`) at 0 and 0.1 against 0.5, `land` the landed commit.
- `g394/*.txt`: 02-394's 34 general-integer models that the tree left
  at 2e9 and HiGHS proved, written by `gen394.py`, at 5e8.

## What was built

Three neighbourhoods of a sub-MIP (`submip_run`: 500 nodes, `MIP_SUBMIP_WORK`),
each with a detector, a share of the work and seeded choices, in
`gh_for_point` (`src/mip.c`), called once per node after the other
heuristics:

- crossover fixes the integer columns where the pool's three best points
  agree, if they agree on half of them (`MIP_CROSS_FIXED`). It runs once
  per change of the pool: at the root under half the root's work, and in
  the tree at most every 100 nodes under 5% of its work. The tree now
  keeps its ten best distinct points (`MIP_CROSS_POOL`); the caller's
  pool is still the first `mip_pool_size`.
- mutation fixes a seeded random 80% of the integer columns at the
  incumbent.
- DINS (Ghosh, 2007) fixes the integer columns where the incumbent is
  within a half of the node's relaxation and of the root's, and boxes the
  rest to the incumbent plus or minus its distance to the node's
  relaxation, rounded up.

Mutation and DINS share one slot every 200 nodes and 5% of the tree's
work. The scheduler picks between them by (4 incumbents + 1 + the square
root of their runs + 1) over each one's runs + 1 (`MIP_GH_REWARD`). Every
sub-MIP's cutoff is the incumbent's `prune_at`, the bound a node must beat,
so an incumbent better by rounding noise does not count: in the first
prototype, 22.9999999999973 replaced 22.9999999999984 on a market split.

## MIPLIB 2017

| | primal | dual | sum |
|---|---|---|---|
| 37720d32 | 5.5284 | 6.1232 | 11.6517 |
| the three (`lns`) | 5.1385 | 6.1255 | 11.2640 |
| landed | 5.1957 | 6.1223 | 11.3180 |

The same 9 are solved. The primal gaps that move, landed against
37720d32:

| model | 37720d32 | landed | its last incumbent from |
|---|---|---|---|
| `timtab1` | 0.591 | 0.382 | mutation |
| `beasleyC3` | 0.093 | 0.011 | DINS |
| `neos-3046615-murg` | 0.066 | 0.039 | crossover |
| `neos-3627168-kasai` | 0.0175 | 0.0022 | mutation |
| `neos-3754480-nidda` | 0.0055 | 0 | RINS, after mutation's |
| `csched007` | 0.154 | 0.134 | RINS |
| `mas74` | 0.0076 | 0.032 | mutation |
| `tr12-30` | 0.0003 | 0.0007 | crossover |

`mas74` loses in both readings: at 37720d32, RINS found 11891 at node
271,116 of 321,147; the landed tree stops at 304,009 nodes with
mutation's 12180. `neos-2657525-crna` goes from 825 to 79.9 (DINS), still
capped at 1 against its optimum of 1.81.

Over the landed reading (`land-who.txt`), crossover ran on 26 models and
found incumbents on 12, mutation on 23 and 11, DINS on 23 and 5; each found
the last incumbent on two or three. Where both run, a run of DINS costs
more than one of mutation on 20 of the 23 models (1.8e5 to 1.7e8 work
units against 2.7e4 to 2.4e7), so it takes most of the slot's 5%.

## MIPLIB 3

| | geometric | total | worst |
|---|---|---|---|
| the three | 0.998x | 1.003x | `misc06` 1.43x |
| crossover at the root off | 0.970x | 1.007x | `misc07` 1.11x |
| crossover at the root at 0.1 | 0.985x | 1.002x | `misc07` 1.11x |
| landed | 0.993x | 1.004x | `misc06` 1.43x |

None regresses. `misc06`'s root crossover spends 16.4e6 of its 66e6 and
finds nothing; crossover at the root keeps `MIP_SUBMIP_ROOT`, the share
of the root's other sub-MIP, since the 2017 reading was taken with it.

## 02-394's general-integer models

The three need an incumbent, so the 28 of 34 with a point at 5e8 stay
28. Five points move: four better (`biggeneral-160` -74 to -142, `482`,
`587`, `398`) and one worse (`384`, -1021 to -1008).

## The verdict

Landed. MIPLIB 3 holds, and MIPLIB 2017's primal gap sum goes from 5.53
to 5.20 with the dual side held; `mas74` and `tr12-30` lose, five gain.
