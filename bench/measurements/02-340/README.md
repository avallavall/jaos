# 02-340 — a Gomory and a MIR round at every node

Taken on 2026-10-04 on the tree of 53fcab9, for TODO row J7, after
02-338 read `binkar10_1`'s bound at 6726.6 under these settings. No code
change: the arms set existing options through `bench/run -O`. MIPLIB 3 at
J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model, against
`bench/results/miplib.txt` and `miplib2017.txt`.

- `nm50`: `mip_node_mir=1`, `mip_cut_depth=100000`, `mip_node_cut_cap=50`.
- `nm4`: `mip_node_mir=1`, `mip_cut_depth=100000`, the default cap of 4.

The node's cuts are read over its own bounds and stay in its subtree, as
D296 and D310 measured them on MIPLIB 3 alone.

| arm | MIPLIB 3, geometric | sum | worst | 2017 gap sum | dual part | `binkar10_1` bound | `pk1` bound | `p200x1188c` bound |
|---|---|---|---|---|---|---|---|---|
| base | 1 | 1 | | 15.28 | 7.288 | 6713.1 | 8.42 | 10789 |
| `nm50` | 1.500x | 2.817x | `bell5` 104.8x (923215 nodes), `enigma` 7.6x, `gt2` 6.7x | 17.91 | 7.540 | 6726.6 | 5.96 | 10221 |
| `nm4` | 1.001x | 1.143x | `gt2` 4.4x and a numerical error, `enigma` 2.7x | 16.17 | 7.564 | 6721.6 | 6.49 | 10244 |

The cuts shrink most MIPLIB 3 trees (`lseu` 5670 to 255 nodes, `flugpl`
1937 to 44) and cost more than the nodes saved on the rest. On the 2017
set the bounds that come from the count of nodes fall (`pk1`, `mas74`,
`mas76`, `neos-3754480-nidda`), since each node costs more, and
`tr12-30`'s incumbent goes from 143000 to 161556 and 206612. Only
`binkar10_1`, `gen-ip054` and `csched007` gain bound.

`gt2` under `nm4` ends `NUMERICAL_ERROR` at node 979 where the default
solves it; the setting is not a default, so this stays a note here.
