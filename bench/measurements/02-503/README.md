# 02-503 — the root's LP prices as a lone LP does

Taken on 2026-11-19 on 7b9eef96, for J36's next step: inside the MIP,
`mzzv11`'s root LP ran 248,058 iterations in 300 s without ending, where the
same relaxation solved on its own ends in 63,258 (02-501).

## The cause

The tree solves its relaxation with `cfg.node_solve` set. In the dual
simplex that flag changes what a steepest-edge weight that drifts past
`DSE_DRIFT` does. In a lone LP, exact weights that drift hand pricing to
dual Devex. In a node solve they are all set back to 1 and the walk goes
on with them, and they drift again. Below the root this never matters,
since those nodes price by dual Devex from their first iteration (02-479).
The root's first solve starts from the slack basis, where weights of 1 are
exact, and then prices for the rest of its walk with weights that restart
at 1 at every drift.

Two variants were built on a copy of 7b9eef96 (`variants.sh`, switched by
`JAOS_DX`):

- **cold**: a node solve that starts from the slack basis hands over to
  dual Devex as a lone LP does. A node solve from a basis still restarts its
  set.
- **all**: every node solve does.

## MIPLIB 3 (`miplib3.sh`, `miplib3-read.txt`)

| variant | instances moved | work, geometric | work, total |
|---|---|---|---|
| cold | 0 of 24 | 1.000x | 1.000x |
| all | 24 of 24 | 0.997x | 0.911x |

None of MIPLIB 3's root LPs drifts. The all variant moves every tree
through the cut re-solves and the probes, from `enigma` 0.57x to `misc03`
1.53x. That is a new path for every instance with no gain on the set, so
it was not taken (`node-devex-on-drift-everywhere` in
`bench/refusals.txt`). Cold landed.

## The root LP alone

`root3.sh` and `root.sh` solve the root LP and stop: `--node-limit 1`, no
cuts, no heuristics, reliability 0, one thread, 300 s or 600 s. The only
node solve in such a run is the cold root solve, so both scripts read the
landed rule.

| instance | before | after |
|---|---|---|
| `mzzv11` | not done at 600 s (37,181 iterations, 2.53e11 units) | 63,258 iterations, 3.08e10 units, 97 s |
| `dano3_3` | 280,779 iterations, 7.94e10 units, 170 s | 45,213 iterations, 1.10e10 units, 32 s |
| `qap10` | 379,018 iterations, 1.12e11 units, 220 s | 22,721 iterations, 2.99e9 units, 8.9 s |

(`root3-read.txt`; the seconds are under a load of about 8.)

On the 33 of 02-501's 94 whose 300 s ended inside the root LP
(`root33.txt`, `root33-read.txt`), the root LP now ends within 300 s on
four:

| instance | before | after |
|---|---|---|
| `mzzv11` | not done, 223,500 iterations | 63,258 iterations, 86 s |
| `uccase9` | not done, 182,911 iterations | 34,583 iterations, 51 s |
| `uccase12` | not done, 87,036 iterations | 86,737 iterations, 170 s |
| `neos-3988577-wolgan` | not done, 93,957 iterations | 96,133 iterations, 267 s |

On the other 27 the iteration counts at 300 s are within a few percent of
each other. Their weights drift little, so what they lack is the cost of
an iteration (02-501). `neos-3402454-bohle` and `square47` did not start
under the script's 3 GB cap.

## The whole solve at 300 s

`full6.sh` runs six of them with the default settings on one thread at
300 s, as 02-491 does (`full6-read.txt`):

| instance | before | after |
|---|---|---|
| `qap10` | time limit at the root, incumbent 544, bound 334 | **optimal at 340** in 14 nodes |
| `uccase12` | time limit inside the root LP, no bound | 3 nodes, incumbent 11507.4056, bound 11507.3345 |
| `dano3_3` | time limit at the root, bound 576.2316 | 9 nodes, bound 576.2432 |
| `mzzv11` | time limit inside the root LP, no bound | at the root, bound -22944 |
| `uccase9` | time limit inside the root LP, no bound | at the root, bound 10867.05 |
| `neos-3988577-wolgan` | time limit inside the root LP, no bound | at the root, bound 119 |

`qap10` joins the 20 that 02-491 solved, and two more leave the root.

## The gates

On the change over 7b9eef96: `make sanitize` passes, and `make test` passes
once this folder exists. `netlib`, `netlib-infeas` and `netlib-kennington`
read `baseline: 0 regressed, 0 improved, 0 new, 0 at other nodes`.
`primal barrier pdlp concurrent warm` leave their records as they were.
`make miplib` reads `gate: PASS` with 0 at other nodes, and its record is
unchanged. In `make miplib2017` one line moves: `neos-3754480-nidda`'s work
at the limit, 10000004378 to 10000007432, at the same 175,820 nodes,
iterations and bound. `m17sum.py` reads 11 solved, 29 incumbents and sum
9.2468 before and after.
