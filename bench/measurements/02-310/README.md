# 02-310 — the MIP cut settings read again after d6245e0

Taken on 2026-09-24 for TODO row H2, on the tree of 0b2e2e2. Since d6245e0
a node LP starts from its parent's basis whole and costs less, which
shifts what a cut is worth against a node. Each setting below is one
option changed on MIPLIB 3 through the bench runner's `-O` (`sweep.sh`,
two settings at a time at `-j 2`, a 4 GB cap on each process), set against
the committed `bench/results/miplib.txt` by `../02-303/cmpmip.py`;
`sweep-against-default.txt` holds every instance's ratio.

| option | default | tried | work against the default |
|---|---|---|---|
| `mip_cut_rounds` | 1 | 2, 3 | 1.061x, 1.232x |
| `mip_cut_depth` | 3 | 0, 1, 2, 6, 10 | 1.037x, 1.232x, 1.157x, 1.128x, 1.230x |
| `mip_node_cut_cap` | 4 | 2, 8 | 1.268x, 1.387x |
| `mip_cover_rounds` | 4 | 2, 8 | 1.171x, 1.021x |
| `mip_mir_rounds` | 6 | 3, 10 | 1.077x, 1.049x |

Every setting reads worse than the default on both sides, so the defaults
stand.
