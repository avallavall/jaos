# 02-460 — cuts below depth 3 and node MIR, for the bound per node, refused

Taken on 2026-11-15 on the tree of 48cf4eb with the shipped switches, for
TODO row J7. `binkar10_1.sh` and `enigma.sh` take the single-instance
readings; the MIPLIB 3 and MIPLIB 2017 records are `bench/run` with the
options named below, one solve at a time.

## Why these switches

02-459 found that JAOS's small open instances lack bound per node, not
nodes. On `binkar10_1` HiGHS 1.15.1 reaches a root bound of 6701.2 after a
restart where JAOS reaches 6693.2, then 6707.0 at 647 nodes and 6719.5 at
2,388, and closes at 4,066 with about 96 cuts in its LP from a pool of
about 3,000 and 9,500 conflicts. JAOS keeps no cut below the root there
(96 cuts, none below it) and finds 221 conflicts.

`binkar10_1.txt`, each switch under 1e10 work units: only cuts to depth
10 with node MIR move the bound, from 6718.07 to 6724.37 of the optimum's
6742.20; `--restart` does not fire, and `--cut-depth 10`, `--node-mir`,
`--propagate 4` and `--node-cut-cap 0` alone leave it lower.

## MIPLIB 3 and MIPLIB 2017

`d10mir` is `--cut-depth 10 --node-mir`, `s3` and `s4` add
`--node-cut-stall` 1e-3 and 1e-4. MIPLIB 3 under 1e10 work units against
`bench/miplib.baseline`, MIPLIB 2017's 30 at 1e10 by 02-328's `m17sum.py`:

| configuration | MIPLIB 3 work | the gate | MIPLIB 2017 primal, dual, sum |
|---|---|---|---|
| defaults | 1.00x | | 7.0744, 6.5961, 13.6705 |
| d10mir | 2.96x | `bell5` 96.5x, `enigma` 12.5x, `gt2` 4.0x | 7.2303, 6.5861, 13.8164 |
| d10mir, s3 | 0.81x | `enigma` 5.2x | 8.4827, 6.5530, 15.0357 |
| d10mir, s4 | 1.90x | `bell5` stops at 1e10, `enigma` 5.2x | |

`d10mir` raises the bound of the three that HiGHS and SCIP close and JAOS
does not: `binkar10_1` from 0.0036 of its optimum to 0.0026, `beasleyC3`
0.0159 to 0.0146 with a better point, `neos-911970` 0.0518 to 0.0484. But
`bell5`'s tree grows from 20,089 nodes to 1,876,848. The stall at 1e-3
holds `bell5` and takes MIPLIB 3 to 0.81x of its work, `misc07` to 0.50x,
`p0282` 0.65x and `misc03` 0.76x; it also ends the cut rounds that lifted
`binkar10_1` (0.0037), and on MIPLIB 2017 the tree loses `markshare_4_0`'s
optimum and `csched007`'s point, so the sum rises though the dual falls.

`enigma.txt`: any `--node-cut-stall` takes `enigma` from 895 nodes to
13,082, at depth 3 without node MIR as at depth 10 with it, from 1e-3 to
1e-2; its tree needs the shallow nodes' cut rounds that the stall ends.

## The verdict

Refused: no setting holds both sets. `bench/refusals.txt`'s
`node-cuts-depth-mir-stall` says what reopens it.
