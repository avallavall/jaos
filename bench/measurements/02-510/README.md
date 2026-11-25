# 02-510 — the MIP root's LPs perturb their costs on a run of degenerate steps

Taken on 2026-11-25 on ac0ab7aa, for J36's next step after 02-508: the
first cut round's re-solve does not end inside 300 s on
`neos-4532248-waihi`, `academictimetablesmall` and `neos-960392`.

## The reading

`rounds.py` is 02-508's per-round lines, moved to the code of 02-508's
change. `diag.py` adds to the dual's line every 1000 iterations the total
infeasibility and the rows outside their bounds, the objective under the
shifted and the original costs, and how many of those 1000 steps were
degenerate (a dual step of at most 1e-12), how many columns the ratio test
flipped and how many it admitted. At 120 s (`three-read.txt` has the 300 s
lines of every arm):

- `neos-4532248-waihi`: the root bound is 0, and every one of the first
  31,000 steps of the first re-solve is degenerate. The objective stays at
  0 while the total infeasibility runs from 439 to between 12,000 and
  55,000 over 10,000 to 17,000 rows. The best infeasibility, the number
  the log printed, holds at 336.4.
- `academictimetablesmall`: its optimum is 0, the root bound. Every step
  is degenerate; the infeasibility runs from 389 to 26,000. Its first
  re-solve ends after 90,810 iterations and 148.6 s, at bound 0.
- `neos-960392`: its optimum is -238, the root bound. A quarter of the
  steps are degenerate, 60,258 of its 64,226 columns and rows carry a
  cost shift, and the objective under the shifted costs falls from -238 to
  -238.00003 while the original one stays at -238. Iterations 60,000 to
  64,000 repeat one state: the same total infeasibility, flips and
  candidates every 1000 iterations, until the exact weights of
  `DSE_RESOLVE_EXACT` break it at 64,161.

On all three the dual walks a face of optima, and the reduced costs give
the ratio test no direction. The dual's cost perturbation (`DUAL_PERTURB`)
gives it one, but it comes only after `PERTURB_STALL_FACTOR` times the
model's size in iterations without a better infeasibility: 254,165 on
`waihi`.

## The arms

`pz.py` perturbs a cut re-solve's costs once K steps in a row are
degenerate (`JAOS_PZ=K`), `pzr.py` the root LP's (`JAOS_PZR=K`). The first
re-solve (`three-read.txt`; 200 s for `detail`, `pz0` and `pz50`, 300 s
for the rest, beside other runs):

| instance | before | K = 0 | K = 50 | K = 100 | K = 200 |
|---|---|---|---|---|---|
| `neos-4532248-waihi` | over 45,765 | 8,735 | 11,051 | 10,162 | 8,626 |
| `academictimetablesmall` | 90,810 | 11,096 | 13,457 | 12,495 | 14,322 |
| `neos-960392` | over 194,710 | 1,394 | 4,631 | 3,435 | over 144,975 |

MIPLIB 3 (`miplib3-arms.txt`, by `m3cmp.py` against HEAD):

| K | moved | work, geometric | total | worst |
|---|---|---|---|---|
| 0 | 24 | 1.168x | 1.103x | `enigma` 42.8x (1 node to 7,132), `misc07` 1.79x |
| 50 | 2 | 1.025x | 1.134x | `misc07` 1.76x |
| 100 | 1 | 1.006x | 1.000x | `enigma` 1.16x |
| 200 | 0 | 1.000x | 1.000x | |

At K = 0 the root LP itself (`rootlp-pzr0.txt`, 300 s): `neos-960392`'s
ends in 6,176 iterations and 2.9 s against 226,143 and 75 s, and
`neos-3656078-kumeu`'s in 17,538 and 10.1 s, which never ended in 300 s.
`rail02`, `s100`, `buildingenergy` and `chromaticindex1024-7` still do
not end.

## The change

`src/simplex.c`: a dual step is degenerate when the entering column's
reduced cost is within the dual tolerance. When `cfg.perturb_degenerate`
is set, the dual perturbs its costs as at a plateau once
`PERTURB_DEGENERATE_STEPS` (200) steps in a row are degenerate. Nothing
else sets the flag, so a lone LP walks as before. `src/mip.c` sets it for
the root's first solve and for the cut rounds' re-solves.

At 200 MIPLIB 3 does not move (`miplib3-change.txt`). The 300 s lines of
the change (`three-read.txt`, `wt`):

| instance | root LP | first re-solve | at 300 s |
|---|---|---|---|
| `neos-4532248-waihi` | 2,449 iterations, 5.7 s | 10,134, 31.0 s, bound 0.370 | bound 0.370 |
| `academictimetablesmall` | 4,182, 2.0 s | 6,253, 6.5 s | 2 nodes |
| `neos-960392` | 6,688, 2.7 s | 3,051, 2.5 s | incumbent -207 |
| `neos-3656078-kumeu` | 64,426, 58.3 s | 15,141, 14.5 s | bound -18413.2 |

## The 94 and the 20 at 300 s

`pair.sh` runs 02-501's `pair.sh` with HEAD against the change, both with
the round lines, on 02-501's `root96.txt` less `s250r10` and
`satellites2-60-fs` (past 9 GB in 02-491), split by 02-491's peak memory:
`heavy-b.txt` two legs at a time under 7 GB each, `heavy-a.txt` three at a
time, `rest.txt` six at a time and its failed legs again two at a time;
then 02-491's 20 solved (`solved20.txt`) two at a time
(`pair-record.txt`, `pairsum.txt`). The load ran from 2 to 12 on 12
logical CPUs. Six legs refused their cap and ran again with more memory
after their twin had run: `bobin`'s, `bohle`'s (alone under 13 GB),
`map10`'s, `map16715-04`'s, `cuanza`'s and `supportcase19`'s, so those
pairs did not run under the same load.

| | before | after |
|---|---|---|
| at the root at 300 s, of the 94 | 79 | 66 |
| solved, of the 114 | 20 | 22 |
| hold an incumbent, of the 114 | 49 | 55 |

`neos8` closes at -3719 in 3 nodes and `neos-1122047` at its root at 161.
Out of the root: `CMS750_4` (1,727 nodes, incumbent 630 to 538),
`lectsched-5-obj` (123), `neos-787933` (16, 183 to 168),
`neos-3216931-puriri` (10), `tbfp-network` (7), `nursesched-medium-hint03`
(5), `atlanta-ip`, `cod105`, `academictimetablesmall`, `net12` and
`map10` (2 or 3). New incumbents at the root on `neos-960392`,
`neos-933966`, `satellites2-40`, `chromaticindex512-7`, `comp07-2idx`,
`neos-3555904-turama`, `netdiversion` and `neos-3402294-bobin`, whose
root LP now ends in 831 iterations and 53 s, and `neos-3402454-bohle`'s
root LP ends too. Two lose the incumbent they held, both out of the root
in either leg: `neos-5195221-niemur` and `ns1208400`. Of the 20, every
status is the same in both legs; `swath1` and `neos-3083819-nubu` reach
the limit in both under load 8 to 12, and `app1-1` takes 167 s against
55 s on a new path.

## The gates

`make test` and `make sanitize` pass. `netlib` and `netlib-infeas` (J=4),
`netlib-kennington` and `miplib` (J=2) read 0 regressed, 0 improved, 0
new, 0 at other nodes, and every record of `bench/results/` that they and
`primal barrier pdlp concurrent warm` write is byte-identical: no lone LP
sets the flag. `make miplib2017` (`miplib2017-read.txt`, by 02-328's
`m17sum.py` and 02-508's `m17cmp.py`): solved 10 and 29 incumbents on
both sides, sum 9.3388 to 9.3394, inside 02-474's spread of 1.03.
`assign1-5-8` reaches its optimum 212, `mad` 0.181 to 0.118, `tr12-30`
131245 to 130615 and `neos-911970`'s bound 52.30 to 54.16;
`csched007` 398 to 405 (its root LP ends on another vertex, the cuts lift
the bound to 291.8 against 295.2 and the heuristics take 77% of the
work against 19%), `neos-2657525-crna` 45.1 to 80.4 and `supportcase26`
1764 to 1845.

## The root LPs that do not end

`relaxmps.py` writes a model's LP relaxation. On six of them, 300 s, one
thread, beside other runs (`rootlp-read.txt`): JAOS's barrier factors 72M
nonzeros on `rail02` and makes one iteration, and does not end `s100` or
`buildingenergy`; HiGHS 1.15.1's dual simplex does not end `rail02`,
`s100` or `buildingenergy` either, and ends `ex9` in 11,830 iterations and
28.6 s, `supportcase10` in 104,360 and 207 s and `chromaticindex1024-7` in
81,372 and 139 s, where JAOS's dual reaches 15,146, 34,836 and 68,065
iterations in 300 s. HiGHS's presolve removes 60,475 of `supportcase10`'s
165,684 rows, 41,382 of them by doubleton equations and the rest as
parallel rows; JAOS's removes 400. gprof on `ex9`'s root LP: the dense
back substitution with U 19%, the pricing row 11%, `pviol_set` 9%
(39,000 calls an iteration, one a row, on the dense column's path), BTRAN
13%.
