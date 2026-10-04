# 02-345 — the restart carries the root's cuts, on in network mode

Taken on 2026-10-05 on the tree of faab77e, for TODO rows J7 and J11.
MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model,
against `bench/results/miplib.txt` and `miplib2017.txt` of faab77e.

## Why

Since 02-343 the restart behind `--restart` fires. On `p200x1188c` the root
reaches 13137, its reduced costs fix 479 of 1188 binaries against the
incumbent 15550, and the second root starts from the same relaxation as
the first, 9869.2, since the fixed columns sit at those values in it. Its
cut rounds then stall at 10168, and the tree's bound at the limit falls
from 13767 to 10905. HiGHS restarts and keeps its cuts.

## What was built

At the restart, the cuts in the first root's relaxation are carried into
the second root's relaxation where its point violates them, and the first
root's other cuts go into its pool. Carrying all 1076 of the first root's
cuts into the relaxation at once spent the 9e9 work units left on that one
solve and ended with no incumbent; the first root's relaxation held 110 of
them, and those 103 that the second root's point violates are the ones
carried. A sub-MIP never restarts. The restart is on by default in network
mode.

## Readings

| arm | form | MIPLIB 3, geometric | changed | gap sum | `p200x1188c` bound |
|---|---|---|---|---|---|
| faab77e | | 1 | | 14.931 | 13767 |
| `rsc` | `-O mip_restart=1`, cuts carried, every model | 1.061x | `mod008` 3.235x, `rgn` 1.814x, `gt2` 0.862x, `gen` 0.826x | 14.850 | 14999 |
| `rsnet` | the landed form: on in network mode | 0.992x | `gen` 0.826x | 14.850 | 14999 |

On `p200x1188c` (`one-p2rsc.log`) the second root starts at 13138.8, fixes
151 more column bounds by its reduced costs, and the tree ends with the
optimum 15078 over a bound of 14999 after 7013 nodes. No other 2017 model
changes.
