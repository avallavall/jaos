# 02-508 — the root's cut re-solves price by dual Devex, a round's Gomory cuts stop at the relaxation's size, and a paying probe's copy is the relaxation

Taken on 2026-11-25 on 0814fa80, for J36's next step after 02-507: in
02-501's split the cut rounds' LP re-solves take 44.8% of the root's work
on the 94 that never leave the root, and lead on 34 of them
(`cut34.txt`). On `nexp-150-20-8-5`, `bnatt500`, `comp21-2idx` and
`neos-4532248-waihi` the root LP ends in under 3,000 iterations and more
than 99% of the 300 s go to the re-solves.

## The reading

`rounds.py` patches a tree with one line a re-solve (round, kind, cuts
added, rows, nonzeros and those the cuts added, iterations, work units,
seconds, status, bound), one a round for the separators (cuts and
nonzeros by family), and the LP's own summary line (refactorizations,
weight restarts). It does not move a path: at 2e9 work units every line
of the nine's records but `time` matches HEAD's (`trees.sh`, `read.sh`).
`summarise.py` sets the lines side by side. At 300 s on the nine of
`nine.txt` (`r300-read.txt`):

| instance | root LP iterations | first re-solve: cuts, nonzeros added (x the LP's) | its iterations, weight restarts |
|---|---|---|---|
| `academictimetablesmall` | 13,062 | 2,636, 129x | 2,666, 2,666, not done |
| `bnatt500` | 2,436 | 1,829, 35x | 87,101, 85,111, not done |
| `comp21-2idx` | 2,929 | 979, 20x | 20,086, 20,085, not done |
| `mzzv42z` | 62,834 | 1,035, 8.7x | 85,970, 81,001 |
| `neos-4532248-waihi` | 731 | 322, 0.8x | 41,510, 39,364, not done |
| `neos-960392` | 226,143 | 411, 44x | 21,301, 19,153, not done |
| `neos8` | 24,597 | 1,249, 1.2x | 84,737, 70,257 |
| `nexp-150-20-8-5` | 835 | 69, 0.33x | 14,657, 14,575 |
| `peg-solitaire-a3` | 21,598 | 1,015, 68x | 24,654, 9,447 |

Three things cost the re-solves:

1. **Pricing.** A re-solve starts from the root's basis with its
   steepest-edge weights at 1, and in a node solve from a basis a drifted
   weight restarts the whole set at 1 (02-503). The exact weights of a
   cut's row are far above 1, so the set restarts at nearly every pivot
   and the dual prices by the largest infeasibility: 02-479's finding
   below the root, here at the root. `neos8`'s rounds 2 to 5 add 0.2 to
   0.3 times its nonzeros and each takes 22,000 to 27,000 iterations, as
   many as its whole root LP; `mzzv42z`'s second round adds 74 cuts and
   runs 40,278.
2. **Density.** Six of the nine add 9x to 129x the relaxation's nonzeros
   in round 1, nearly all of them in Gomory cuts: on
   `academictimetablesmall` 1,794 Gomory cuts hold 34.6M nonzeros, 67% of
   its 28,926 columns each, against 15,016 for its 842 other cuts. An
   iteration of that re-solve costs 228 times one of the root LP's.
3. **A solve done twice.** When the aggregated MIR cuts lift the bound on
   a copy of the relaxation, the copy was dropped and the same cuts were
   added to the relaxation and solved again: on `nexp-150-20-8-5` every
   probe and the solve after it take the same iterations, half its 1.44e11.

## The arms

`variants.py` builds each on top of `rounds.py` (`arms.sh`, 6e10 work
units, `arms-6e10.txt`, `arms-6e10-density.txt`):

- `dx`: a cut re-solve hands pricing to dual Devex when its weights
  drift, as a lone LP does;
- `devex`: a cut re-solve prices by dual Devex from its first iteration,
  as a node below the root does;
- `exact`: exact weights at a re-solve's first iteration
  (`DSE_RESOLVE_EXACT` at 0), and `exactdx` with `dx`'s hand-over;
- `swap`: a probe whose cuts pay becomes the relaxation;
- `gd`F: a Gomory cut with more than F times the columns' count of
  nonzeros is dropped;
- `gb`B: a round's Gomory cuts add at most B times the relaxation's
  nonzeros, the sparsest first.

Bound at the root (and nodes) at 6e10:

| instance | base | `dx` | `devex` | `exact` | `swap`+`devex` | `dx`+`gb1` |
|---|---|---|---|---|---|---|
| `bnatt500` | 0, round 1 | 0, round 1 | 0, round 1 | 0, round 2 | 0, round 1 | 0, 16,445 nodes |
| `comp21-2idx` | 0, round 1 | 2 | 2 | 2 | 2 | 2, 8 nodes |
| `mzzv42z` | -21622, round 1 | -21400 | -21400 | -21622, round 1 | -21400 | -21386 |
| `neos8` | -3773, round 1 | -3729 | -3729 | -3735 | -3729 | -3729 |
| `nexp-150-20-8-5` | 39 | 49, 101 nodes | 50, 121 nodes | 51, 101 nodes | 50, 292 nodes | 49, 101 nodes |
| `peg-solitaire-a3` | 1, round 1 | 1, round 3 | 1, round 3 | 1, round 6 | 1, round 3 | 1, 5 nodes |

`academictimetablesmall`, `neos-4532248-waihi` and `neos-960392` stay in
round 1 under every arm. Under `dx` and `devex` a re-solve's weight
restarts fall from about one an iteration to a few hundred a solve, and
the two read alike; `devex` is the rule 02-479 gave the nodes below the
root. `exact` lifts `comp21-2idx` and `peg-solitaire-a3` further, but its
weights drift again, restart at 1 and restart 22,995 times on `mzzv42z`,
whose first round does not end. `swap` changes no path: its re-solves are
the ones the base takes, and only `nexp-150-20-8-5` has paying probes.
Under `gd0.1` `bnatt500`'s tree reaches 23,387 nodes and `comp21-2idx`'s
15, but on MIPLIB 3 it puts `enigma` at 17x, `mod010` at 15x and `gt2` at
4x: a tenth of a small model's columns is a few entries. `gb3` leaves
`comp21-2idx` at the root.

## MIPLIB 3

`m3.sh` builds each arm from the working tree and runs `make miplib`'s
set against its baseline (`miplib3-arms.txt`, by `m3cmp.py` against the
committed record):

| arm | gate | work, geometric | total | worst | best |
|---|---|---|---|---|---|
| `devex`+`swap` | PASS, 15 at other nodes | 0.952x | 0.867x | `misc03` 1.60x, `p0201` 1.59x, `blend2` 1.49x | `enigma` 0.48x, `misc07` 0.51x |
| and `gb0.5` | PASS, 16 | 0.902x | 0.830x | `misc03` 1.61x, `blend2` 1.49x | `enigma` 0.43x |
| and `gb1` | PASS, 15 | 0.917x | 0.791x | `misc03` 1.60x, `blend2` 1.49x, `stein27` 1.35x | `enigma` 0.44x, `l152lav` 0.51x |
| and `gb2` | PASS, 15 | 0.912x | 0.793x | `misc03` 1.60x, `p0201` 1.59x | `enigma` 0.34x |
| and `gd0.1` | 3 regressed | 1.237x | 0.971x | `enigma` 17x, `mod010` 15x, `gt2` 4.1x | |

02-503's arm that handed every warm node solve to dual Devex on a drift,
the probes included, read 0.997x and 0.911x (`node-devex-on-drift-everywhere`).

## The 34 at 300 s

`verify.sh` runs 02-501's `pair.sh` on the 34 of `cut34.txt`, the tree
of 0814fa80 against the change before the undo below (which moves none
of the 34), four legs at a time and the four largest by memory two at a
time (`pair-record.txt`, `pairsum.txt`). Every leg ends at the time limit.

| | before | after |
|---|---|---|
| leave the root | 2 | 11 |
| hold an incumbent | 6 | 13 |

The nine that leave: `bnatt400` (19,357 nodes), `bnatt500` (11,189),
`nexp-150-20-8-5` (864, incumbent 788, bound 43 to 52), `neos-787933`
(101, incumbent 219 to 127), `cvs16r128-89` (33, -2 to -12), `peg-solitaire-a3`
(17), `comp21-2idx` (6, incumbent 774, bound 0 to 2), `neos8` (2,
incumbent -1495) and `tbfp-network` (2). New incumbents also on
`mzzv42z`, `neos-1354092`, `k1mushroom` and `square41`. Bounds rise on 15:
`neos-5049753-cuanza` 464 to 511.3, `neos-873061` 95.9 to 108.3,
`thor50dday` 20839 to 22320, `bab6` -321641 to -293015,
`nursesched-medium-hint03` 34 to 39, `neos-1354092` 14 to 20,
`mzzv42z` -21400 to -21384 and eight more, `nexp-150-20-8-5`'s and
`comp21-2idx`'s among them; they fall on `tbfp-network` (23.503 to
23.461, at two nodes) and `k1mushroom` (-4141 to -4143).
`academictimetablesmall`, `neos-4532248-waihi`, `neos-960392`,
`neos-933966` and six more read as before.

## The root LP

The root LP is not a cut re-solve, so its walk is the same code. 02-506's
`root.sh` on 02-505's `root27.txt`, 0814fa80 against the change, two
pairs at a time (`root27-read.txt`): on the 26 that run out of 300 s
the root LP reaches 1.003x the iterations (geometric; 0.984x to 1.060x,
the load's spread), and `rail01`'s ends in the same 164,473.

## A re-solve that ends with no answer

MIPLIB 2017 at 1e10 work units (`miplib2017-read.txt`, by `m17cmp.py`
and 02-328's `m17sum.py`) under `devex`, `swap` and `gb1`: `p200x1188c`
went from optimal to `numerical_error` at its root with no bound. In
network mode its round 38 adds 16 aggregated MIR cuts; under dual Devex
that re-solve runs 28,577 iterations, its settled point is not dual
feasible from the basis nor cold, and the LP ends `numerical_error`
(under `dx`, `exact` and `exactdx` it ends optimal). The cut loop handed
that status to the whole solve, though the relaxation before the round
was optimal at 14228.41. Now a round whose re-solve ends in a numerical
error is undone: its rows leave, the basis before them comes back, the
relaxation is solved again from it and the rounds stop. `p200x1188c` is
optimal again at 15078 in 176 nodes, and no other line of MIPLIB 2017 or
of MIPLIB 3 moves with it.

| | solved | incumbents | primal | dual | sum |
|---|---|---|---|---|---|
| 0814fa80 (`bench/results/miplib2017.txt`) | 11 | 29 | 4.1539 | 5.1108 | 9.2647 |
| the change without the undo | 9 | 28 | 5.1966 | 6.1422 | 11.3388 |
| the change | 10 | 29 | 4.1966 | 5.1422 | 9.3388 |

02-474's six readings of one code span 1.03 in the sum. The instances
move both ways: `mas74` reaches its optimum (11939.48 to 11801.19),
`csched007` 405 to 398, `timtab1` 1036198 to 1007485, `neos-2657525-crna`
92.2 to 45.1 and `graphdraw-domain`'s bound 14618 to 15478; `mad` 0.115
to 0.181, `graphdraw-domain`'s incumbent 21741 to 22855, and `neos5`,
which closed at 165,457 nodes, holds 15 against a bound of 14.29 after
148,603.

## The change

`src/mip.c`, four parts:

1. The root's cut re-solves set `node_devex`, so they price by dual Devex
   from their first iteration.
2. After the root's Gomory round, `cutbuf_keep_sparse` keeps its cuts,
   sparsest first (ties by order), while their nonzeros stay within
   `MIP_GOMORY_ROUND_NZ` (1) times the relaxation's.
3. When a round's aggregated MIR cuts pay on the copy, the copy, solved,
   becomes the relaxation, with the relaxation's `node_keep`.
4. A cut round whose re-solve ends in a numerical error is undone and
   ends the rounds.

Before the Gomory cap, the working tree's records match the
`swap`+`devex` arm's on the nine at 8e9; with it, its MIPLIB 3 record
matches the `gb1` arm's line for line, the undo included.

Six tests in `tests/test_mip.c` pinned values of the old path and are
pinned again on the new one, each still reading what it reads: the
stalled-round test's cut counts (2, 1, 2), the propagation test's node
limit (1,500, where the tree with propagation finds its incumbent at
node 1,075 and the one without finds none), the ZI-rounding line's work
(45,670), the market-split seeds of the crossover (5) and DINS (14)
tests, and the linked-groups test, whose root now ends at 4 and closes at
node 244, so it reads that the LP-priced search finds the root's last
incumbent.
