# 02-356 — hull cuts on short integer rows

Taken on 2026-10-07 on the tree of 253b7fe, for TODO row J7. MIPLIB 3 at
J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model.

## Why neos-3381206-awhea's root never moved

`neos-3381206-awhea` is a bin packing: 475 bins, each a row
`45a + 36b + 31c + 14d <= 100y` with `a, b` in [0, 2], `c` in [0, 3], `d` in
[0, 7] integer and `y` binary, four demand rows and the bins' count as the
objective (optimum 453). JAOS's root stayed at the LP's 415.24 through six
rounds and 2147 cuts. At the LP's point 144 bins hold `b = 2` at `y = 0.72`,
and JAOS's MIR cut for such a bin is `0.236a + 0.5b + 0.042c <= y`.

SCIP 10 (pyscipopt 6.1.0, presolve, heuristics and root propagation off,
`scvar.py` and `scvar2.py`): Gomory cuts, implied bounds and c-MIR alone each
leave the root at 415.24 to 415.27; c-MIR with flow covers reaches 451.96
(`scip-awhea.txt`). The cuts left in its LP (`scuts.py`) are per-bin rows such
as `a + b + (3/17)c <= 2y`.

`awfam.py` adds one inequality to all 475 bins of the LP and solves it:

| per-bin inequality | LP bound |
|---|---|
| none | 415.24 |
| `0.5a + 0.5b <= y` | 415.24 |
| `17a + 17b + 3c <= 34y` | 415.24 |
| `0.236a + 0.5b + 0.042c <= y` | 415.24 |
| `a + b + 0.375c <= 2y` | 427.56 |
| `a + b + 0.5c <= 2y` | 452.25 |

`a + b + 0.5c <= 2y` is a facet of one bin's integer points. No single MIR
of the row gives it (the divisor that makes `c`'s coefficient 0.5 makes
`b`'s 0.857).

## The separator

`hull_round` in `src/mip.c`: a row of at most 8 integer columns whose box
holds at most 1024 integer points, with a fractional column at the point,
has its integer points listed. An LP over the row's columns and a
right-hand side finds the inequality with coefficients in [-1, 1] that
holds at every listed point and that the point violates most; the
right-hand side is recomputed exactly over the points. Each row costs one
small LP. On `awhea` the bound stays at 415.24 for six rounds of about 400
hull cuts while the LP moves its load from bin to bin, then rises from
round 7 (423.09) to round 13 (452.25). JAOS's MIR rounds end after a round
that lifts the bound by less than `MIP_MIR_MORE_STALL` from round 6 on, so
the keep rule matters.

## The arms

| arm | rule | MIPLIB 3 | 2017 solved | 2017 gap sum |
|---|---|---|---|---|
| base | 253b7fe | 1.000x | 4 | 13.94 |
| `h` | hull cuts, MIR rounds stop as before | 0.999x | 4 | 14.30 |
| `hk` | and no stall while a round finds any hull cut | 1.035x | 5 | 14.20 |
| `hk10` | and no stall while hull cuts reach a tenth of the rows; unit rows skipped | 0.984x | 5 | 14.19 |

`h` and `hk` list the points of every short row; `hk10` skips a row whose
coefficients share one magnitude with sides that are multiples of it (a
single such row is its own hull), which takes `p0201` (1.051x),
`p0282` (1.022x) and `stein27` (1.151x) back to 1.000x. Under `hk`,
`p0033` runs eleven rounds with one hull cut each and its root heuristics,
budgeted on the root's work, take it to 1.915x; under `hk10` it reads
0.722x with 13 nodes against 101.

On the 2017 set only three models change. `neos-3381206-awhea` solves at
the root (2.05e9 work units). `neos-2657525-crna` ends at 53.8 against 115.2
(its gap stays at the cap). `graphdraw-domain` ends at 28362 against 21399:
its root is the same to the bit with no hull cut in any round, the row scan
adds 0.15% to the root's work, and its heuristics, whose budgets follow the
root's work, find 1 point instead of 8 over the same 42000 nodes. All of the
gap sum's rise is that model.

SCIP with presolve off reaches at most 714.2 on `beasleyC3`'s root, below
JAOS's (`scip-beasleyC3.txt`), so SCIP's lead there is its presolve. On
`ic97_potential` SCIP's c-MIR alone lifts the root from 3868 to 3911
(`scip-ic97_potential.txt`); JAOS's aggregated MIR finds 26 cuts in its
first round that lift nothing, and kept every round it adds 49 and stops.
