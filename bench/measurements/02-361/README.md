# 02-361 — the MIP switches that are off, read on the tree of 2026-10-08

Taken on 2026-10-08 on the tree of d45bad0 (hull cuts, MIR bound flips and
the no-incumbent dive in), for TODO row J11, the way 02-335 read them: each
switch on alone, MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10
work units, J=2 and 5 GB (`arms.sh`, `relarms.sh`). The base is
`bench/results/miplib.txt` and `bench/results/miplib2017.txt` of 5c51216:
MIPLIB 3 all solved, the 2017 set 5 solved, 28 incumbents, gap sum 13.48.

| arm | option | MIPLIB 3 solved | work, geometric | work, sum | 2017 gap sum | 02-335 |
|---|---|---|---|---|---|---|
| `bzh` | `mip_zero_half_rounds=2` | 24 | 1.225x | 1.194x | 13.50 | 1.092x |
| `bcl` | `mip_cover_lift=true` | 24 | 1.040x | 1.003x | 13.86 | 1.043x |
| `bpr` | `mip_propagate=4` | 24 | 1.224x | 1.336x | 13.49 | 1.070x |
| `bcf` | `mip_conflicts=true` | 24 | 1.000x | 1.000x | 13.48 | |
| `brs` | `mip_restart=true` | 24 | 1.085x | 1.006x | 13.48 | |
| `bpb` | `mip_probing=true` | 23 | 1.051x | 1.114x | 13.45 | 1.034x |
| `r4c1` | `mip_reliability=4`, `mip_probe_cap=1` | 24 | 1.256x | 1.147x | 15.86 | |
| `r2c1` | `mip_reliability=2`, `mip_probe_cap=1` | 24 | 1.223x | 1.114x | 15.92 | |

None lands. The `bcf` arm re-reads the default: conflict analysis is on
since `MIP_CONFLICTS` landed, so its records match the base to the bit
(`misc07` finds 152 conflicts over 530 binaries either way). Probing runs `bell5` out of memory, as in 02-325
and 02-335. Strong branching at reliability 4 halves or better the work on
`bell5` (0.446x), `blend2` (0.357x), `gt2` (0.467x) and `misc07` (0.523x)
but takes `p0033` to 4.661x, `p0201` to 4.490x and `misc03` to 3.059x, and
on the 2017 set it loses an incumbent and raises the primal gap sum from
6.87 to 9.33.
