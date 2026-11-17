# 02-470 — moves over the disjunctions of big-M rows

Taken on 2026-11-17 on ec95542 with this change, for TODO row J32.
`read.sh` takes every reading from a copy of HEAD:
- `miplib3/landed.txt`, one solve at a time;
- `full/landed.txt` and `full/sum.txt`, one solve at a time;
- `search.txt`, from a second copy with `trace.py`'s prints, the four
  models of the shape side by side at 1e10;
- `shape.txt`, from `shape.py`.

`full/before.txt` is 02-469's record. The two benchmark readings ran on a
copy built before the development switches were taken out. Taken again
from the landed commit, MIPLIB 3 and MIPLIB 2017 read the same records.
`variants.txt` holds the development readings at 1e9. `switches.patch`
puts back the switches (`-DBM_KICK`, `-DBM_SHARE`, `-DBM_FIXKICK` for a
fixed kick, `-DBM_TRACE`) for its second block; the first block came
before the backoff, which has no switch.

## The shape

02-463's census puts `glass4`, `graphdraw-domain`, `supportcase26` and
`neos-3754480-nidda` in one family: binaries that pick which side of a
disjunction holds, through big-M rows.
- `glass4` places 9 rectangles in a strip and pays 1e6 for its length.
  Each of the 36 pairs has a partitioning row over 6 to 10 binaries, and
  each binary turns on one relative position, a row over the pair's
  coordinates with M = 8.4e6.
- `graphdraw-domain` places 10 boxes on a grid of general integer
  coordinates. Each of 45 pairs picks one of 4 relative positions in a
  partitioning row; M is 48 to 56 against coefficients of 1, so the
  census's ratio of 1e3 misses it, but a binary at 0 leaves its row slack
  over the coordinates' bounds.
- `supportcase26` keeps two fixed chains of 18 and 22 jobs apart: each
  pair across the chains holds a gap of 4.12 one way or the other, one
  binary and two rows with M = 1e4. The objective sums the times.
- `neos-3754480-nidda` has 50 binaries, each turning on rows of the form
  C = C0001 · b with M = 1e4 and two rows with M of 134 to 1247.

What they share: every free binary is the one binary of a row with two or
more other columns of both signs, and its coefficient is at least 1e3
times the median of theirs or one of its values leaves the row slack over
their bounds. Call such a binary a switch. `shape.txt` finds that shape,
with no switch in two partitioning rows, in exactly these four of the 54
instances of MIPLIB 3 and 2017. `neos-3046615-murg` has switches, but each
sits in two partitioning rows, and is left out.

## What was built

`bigm_for_point` in `src/mip.c`:
- **Detector:** the shape above, on the presolved model at the root
  bounds. A partitioning row over switches is one disjunction, and every
  other switch is one by its two values.
- **Evaluation:** with the switches fixed, the rest is a linear program on
  a copy of the tree's relaxation, solved again from its last basis after
  each move. General integer columns stay relaxed.
- **Moves:** a partitioning row takes another of its switches, or a
  switch flips. Two kinds are tried: those whose reduced costs promise a
  lower objective, ranked by that promise, and those that loosen a row the
  program's point holds tight, ranked by the violation they cause at that
  point. Within each, those that cause none come first. The first move
  that lowers the objective is taken.
- **Kicks:** at a local optimum the best point takes one to
  `MIP_BIGM_KICK` (4) random moves drawn from its own list, one more after
  each kick that finds nothing better and back to one after one that
  does (Hansen and Mladenović's variable neighbourhood search). A kick
  whose program is infeasible is drawn again, up to 10 times.
- **Starts:** at the root, the side each switch's rows favour at the
  relaxation's point, then the incumbent. In the tree, each new incumbent
  and, every `MIP_BIGM_EVERY` (100) nodes, its own best point, while its
  work stays under `MIP_BIGM_SHARE` (30%) of the tree's. The wait doubles
  after a run from its own point that finds nothing. A sub-MIP of such a
  model has the same shape, so the moves also run at each sub-MIP's root.
- **Budget:** `MIP_BIGM_WORK` (20000) times (nonzeros + columns + rows) a
  run.
- **Result:** the best point; general integer columns are rounded and the
  rest solved, or else a RENS sub-MIP boxes them between floor and ceiling
  (`graphdraw-domain`'s coordinates sit at halves).

The relaxation's coordinates alone start nothing on `glass4` and
`graphdraw-domain`: the program of the sides they favour is infeasible. On `supportcase26` they are the start that
works: from the incumbent no move improves and every random flip closes a
cycle between the chains, while from the coordinates 17 steps take it
from 2783 to 1972.

## Development

At 3e8, ranking every move by its reduced cost or every move by the
violation it causes each won on some models and lost on others; the
landed order takes the free moves first. Before kicks the descent stopped
at a local optimum after 1% to 31% of its budget on all four. At a node
past the root, the tree's relaxation carries the node's bounds, so the
search resets every integer column to the root's bounds and finishes on
its own copy.

`variants.txt` reads kick sizes and shares at 1e9. Larger kicks and a
30% share help `glass4` and `graphdraw-domain`; `neos-3754480-nidda`
reaches its optimum with kicks of two. The four models' primal gap sum
lies between 0.58 and 0.61 for every setting with a 30% share, against
1.50 in the record at 1e10, so the setting is noise next to the change.
The variable kick was kept as the one rule without a size to pick.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: 0.997x, no instance changes
status, and none has the shape. Five end at other nodes:

| instance | work | nodes |
|---|---|---|
| `mod008` | 0.88x | 1,507 to 1,239 |
| `blend2` | 0.92x | 4,337 to 4,190 |
| `bell5` | 1.00x | 21,034 to 20,971 |
| `misc07` | 1.00x | 9,485 to 9,548 |
| `p0282` | 1.02x | 1,717 to 1,783 |

They move because the detector's pass adds its work to the root and to
each sub-MIP's root.

MIPLIB 2017 at 1e10, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-469) | 6.5397 | 6.5166 | 13.0562 |
| this change | 5.5108 | 6.5323 | 12.0431 |

The four models of the shape, the incumbent and its distance from the
optimum:

| instance | before | this change | dual gap |
|---|---|---|---|
| `glass4` | 5.90e9, 1.0 | 1.70e9, 0.417 | 0.333, the same |
| `graphdraw-domain` | 28362, 0.441 | 20788, 0.056 | 0.304 to 0.308 |
| `neos-3754480-nidda` | 13624.5, 0.053 | 12941.74, the optimum | 1.0, the same |
| `supportcase26` | 1764.0, 0.011 | 1776.8, 0.018 | 0.147 to 0.150 |

`search.txt` traces the runs:
- `glass4` has no incumbent until the dive from node 1000 finds 5.6e9 at
  node 1205. The run from it ends at 2.03e9, and runs from its own point
  reach 1.80e9 at node 2077 and 1.70e9 at node 6865. Ten more find
  nothing. 15 runs take 6.4% of the 1e10.
- `graphdraw-domain`: 25611 at the root from the jump's 28362, then
  24418 at node 1088 and 20788 at node 1234; the best program there is
  20522, and with its coordinates made integral the point is 20788. 13
  runs, 4.5%.
- `neos-3754480-nidda`: 13055 at the root from the relaxation's sides.
  At node 15602 a RINS sub-MIP finds 13020, by the moves at its own root,
  and the run from that incumbent reaches the optimum, 12941.74. 3.8%.
- `supportcase26`: 1972 at the root from the relaxation's sides, against
  2202 from lock rounding, and 1955 at node 1703 from a RINS sub-MIP, by
  the moves at its root. From there the tree's integral nodes improve it
  to 1776.8, against 1764.0 before; no run from the tree's incumbents
  finds a better point. The moves take 13.4%, with the runs at the roots
  of its sub-MIPs.

The other instances move only by the work the detector adds: the primal
side of `neos-3046615-murg` 0.077 to 0.064 and its dual 0.717 to 0.729,
`csched007` 0.316 to 0.313 and its dual 0.148 to 0.145; `markshare2`
ends at 129 against 84, both 1.0 away.

## The verdict

Landed, on by default with the rounding heuristics. `--no-heuristics`
turns it off. TODO row J32 keeps `glass4`, 0.42 away, and
`supportcase26`, which needs moves of a job past several of the other
chain's.
