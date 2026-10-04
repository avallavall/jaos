# 02-335 — the MIP switches that are off, read again

Taken on 2026-10-04 on the tree of 3dfc0f6, for TODO row J11, the way
02-325 read them: each switch turned on alone, MIPLIB 3 at J=2 and 4 GB a
solve, the 2017 set at 1e10 work units, J=2 and 5 GB, with 02-329's
`arms.sh` and `-O` options. The base is `bench/results/miplib.txt` and
`bench/results/miplib2017.txt` of 3dfc0f6.

| arm | option | MIPLIB 3 solved | work, geometric | work, sum | 2017 gap sum | 02-325's reading |
|---|---|---|---|---|---|---|
| zh | `mip_zero_half_rounds=2` | 24 | 1.092x | 1.034x | 0.999x | 1.200x, 0.999x |
| cl | `mip_cover_lift=true` | 24 | 1.043x | 1.041x | 0.984x | 1.042x, 1.001x |
| lb | `mip_local_branching=10` | 24 | 1.983x | 1.966x | 0.988x | 2.023x, 0.948x |
| pr | `mip_propagate=4` | 24 | 1.070x | 1.129x | 1.047x | 1.081x, 1.033x |
| rc | `mip_rcfix=true` | 24 | 0.944x | 0.996x | 0.999x | 1.010x, 1.000x |
| pb | `mip_probing=true` | 23 | 1.034x | 1.325x | 1.005x | 1.075x, 1.000x |

Reduced-cost fixing lands: on MIPLIB 3 `mod008` reads 0.586x, `p0282`
0.596x, `rgn` 0.788x, `p0201` 0.870x, `p0033` 0.887x and `lseu` 0.928x,
against `gt2` 1.189x and `gen` 1.072x. On the 2017 set `mas74`'s incumbent
goes from 12536.2 to 12367.6 and its bound from 11213.80 to 11217.87,
`mas76`'s bound from 39675.87 to 39689.32, and `neos-2657525-crna`'s
incumbent from 53.8 to 825.1, the feasibility jump's point. The shipped
code with `MIP_RCFIX` on writes the records of `rc` line for line.

The others stay off. Lifted covers take `misc03` to 2.534x (240 to 887
nodes). Local branching loses a solved 2017 model. Probing runs `bell5` out
of memory at 4 GB, as in 02-325.

`miqp-rc.txt` is QPLIB's 17 convex MIQPs on the shipped code, as in 02-332.
One changes against 02-334's `miqp-b9.txt`: `QPLIB_10050`'s bound at the
limit goes from -26.2339 to -26.2296 in 851 nodes where it ran 831.
