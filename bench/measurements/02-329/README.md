# 02-329 — MIP: lock rounding first, a feasibility jump, two network cut changes

Taken on 2026-10-04 on the tree of 9ab84a4, for TODO row J7, the second
batch after 02-328. The settings are 02-328's: the 2017 set at 1e10 work
units, J=2 and 5 GB a solve; MIPLIB 3 at J=2 and 4 GB. `arms.patch` is the
measured tree: 9ab84a4 with the batch behind environment switches.
`arms.sh` runs one arm. The shipped code has no switches and writes the
records of the arm `b2f` line for line. `m3cmp.py`, `m17cmp.py` and
`m17sum.py` are 02-328's.

## The batch

1. **Lock rounding first** (`JAOS_LOCKSFIRST`). 02-328 ran lock rounding
   after the root's dive and pump. On `beasleyC3` the dive and the pump
   ran uncapped before the first incumbent and spent 7.9e9 work units.
   Run first, lock rounding gives the incumbent that starts the cap of
   `MIP_NET_HEUR_CAP`, and the root heuristics spend 6.2e8.
2. **A wider fraction window for network c-MIR** (`JAOS_F0HI`, now
   `MIP_NET_F0_HI` = 1e-6). The cut is formed while the fraction of its
   scaled right-hand side is up to 1 - 1e-6, where 02-328 stopped at 0.99.
   `sp150x300d`'s root bound goes from 56.4 to 63.9.
3. **Both aggregation rules in network mode** (`JAOS_AGGBOTH`). The
   aggregation of MIR base rows substitutes out a continuous column only
   while the column sits strictly inside its bounds. 02-328 measured that
   against the variable bounds in network mode. Now each round runs the
   aggregation twice, once against the variable bounds and once against the
   simple bounds. `tr12-30`'s bound at the limit goes from 89334 to 118133.
4. **A feasibility jump** (`JAOS_FJ`, `MIP_FJ_*`). At the root of a linear
   model with no incumbent after lock rounding, a search moves one column at
   a time to the value that most lowers the weighted violation of its rows.
   Each step draws 25 violated rows with a seeded xorshift generator and
   scores the columns in them. When no move lowers the violation, every
   violated row's weight rises by 1. The search starts from the relaxation's
   point, then from zero. A point with no violated row has its integer
   columns fixed and the rest solved, as lock rounding does.

`JAOS_AGGSIMPLE` (the simple bounds alone) is the arm `b2c` and is not
kept: `exp-1-500-5-5` no longer solves.

## The arms

MIPLIB 3 against `bench/results/miplib.txt` of 9ab84a4, the geometric mean
and the sum of each instance's work ratio, all 24 at the reference in every
arm. The 2017 set against `bench/results/miplib2017.txt` of 9ab84a4 by
`m17sum.py`.

| arm | switches | MIPLIB 3 geo | sum | worst | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|---|---|
| 9ab84a4 | | 1 | 1 | | 2 | 20 | 12.133 | 9.334 | 21.467 |
| b2a | lock rounding first | 0.992 | 1.000 | | 2 | 20 | 12.012 | 9.331 | 21.343 |
| b2b | b2a, `F0HI=1e-6` | 0.988 | 1.000 | | 2 | 20 | 12.017 | 9.306 | 21.323 |
| b2c | b2b, simple bounds alone | 1.008 | 1.000 | `egout` 1.259 | 1 | 20 | 11.622 | 9.176 | 20.798 |
| b2d | b2b, both rules | 1.020 | 1.000 | `egout` 1.493 | 2 | 20 | 11.732 | 9.114 | 20.846 |
| b2e | b2d, the jump uncapped | 1.183 | 1.057 | `p0033` 4.009 | 2 | 24 | 8.939 | 9.117 | 18.056 |
| b2f | b2d, the jump at `MIP_FJ_ROOT` | 1.014 | 1.002 | `egout` 1.493 | 2 | 24 | 9.064 | 9.117 | 18.181 |

`b2f` is the default. On MIPLIB 3 every objective is at the reference and
the gate passes. Past 1.1x: `egout` 1.493x and `gen` 1.126x (trees of 3
nodes, where the second aggregation pass costs a share of a small root),
and `p0033` 1.179x (the jump fails within its cap). `p0201` reads 0.818x,
`gt2` 0.925x and `mod008` 0.939x.

On the 2017 set `sp150x300d` solves at 3.07e8 work units (5.06e8 before)
and `exp-1-500-5-5` at 2.30e9 (3.68e9). Changes against 9ab84a4:

| instance | 9ab84a4 incumbent | bound | b2f incumbent | bound | reference |
|---|---|---|---|---|---|
| neos-2657525-crna | none | 0 | 825.06 | 0 | 1.81 |
| graphdraw-domain | none | 13180.3 | 24231 | 13131.3 | 19686 |
| neos-3381206-awhea | none | 416 | 458 | 416 | 453 |
| neos-3627168-kasai | none | 958700 | 1005997 | 958711 | 988586 |
| tr12-30 | 191315 | 89334 | 145619 | 118133 | 130596 |
| beasleyC3 | 981 | 695 | 889 | 702 | 754 |
| markshare2 | 1413 | 0 | 122 | 0 | 1 |
| mas74 | 12180.1 | 11213.8 | 12536.2 | 11213.8 | 11801.2 |
| neos-3754480-nidda | 13362.3 | -505572 | 13819.9 | -504524 | 12941.7 |
| neos-911970 | 55.60 | 29.78 | 55.33 | 29.76 | 54.76 |
| mad | 0.2088 | 0 | 0.2916 | 0 | 0.0268 |
| p200x1188c | 15078 | 10930 | 15078 | 10789 | 15078 |

`p200x1188c`'s bound rises to 11336 under the wider window (`b2b`) and
falls to 10789 under both aggregation rules (`b2d`), whose extra cuts slow
its nodes. `mad` loses its point of 0.2088 with the jump (`b2e`), and
`mas74` and `neos-3754480-nidda` end worse from lock rounding first on
(`b2a`). The other models move by less
than 1e-3 of themselves.

## The feasibility jump's cost

`fj-root.txt` holds a root-only run (`--node-limit 1`) of the uncapped jump
(`b2e`'s switches) on the models where it found a point or failed. Each
`FJLOG` line came from a print after the two searches, not in
`arms.patch`: the work before the jump, then for each search whether it
found a point (1) and the work it took. A second line is the root of a
sub-MIP, which runs a tree of its own. Where the jump finds a point it
needs at most 0.29x the root's work (`graphdraw-domain`, 4.8e5 against
1.7e6). Where it fails it runs to `MIP_FJ_WORK`, 20000 passes over the
model: on `misc03` 4.6e7 a search against a root of 9.7e5, which took
`misc03` to 3.13x and `p0033` to 4.01x in `b2e`. `MIP_FJ_ROOT` caps each
search at half the root's work, which keeps the four 2017 points.

## The cut check

02-328's `fcnet.sh` at 2000 models on seeds 1 to 5 against brute force:
0 failed on every seed, and the digests are 02-328's
(19f996539dfa9c86, 8d03f3cacc1ad431, 93700d35b45d08a0, 6c31b36882b54a3f,
af128ab6cafdfc0d).

## Left for J7

`p200x1188c` holds its optimum and a bound of 10789 (HiGHS's root 11640).
`beasleyC3` holds 889 against 754 with a bound of 702. `enlight_hard`,
`glass4`, `timtab1`, `ic97_potential` and the two `csched` models have no
point; the jump fails on all of them within its cap. `neos-911970` and
`neos-3381206-awhea` still need deeper root rounds.
