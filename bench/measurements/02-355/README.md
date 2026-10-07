# 02-355 — a tree with no incumbent dives

Taken on 2026-10-05 on the tree of 94e8ce0, for TODO row J7 (the models
with no incumbent: `glass4`, `timtab1`, `ic97_potential`, `csched007`,
`csched008`). MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work
units a model. `envdive.diff` reads two switches from the environment:
`JAOS_NOINC` (B, the resumes a dive may take from its stack) and
`JAOS_NOINC_AFTER` (A, the node count from which the rule applies). While
the tree holds no incumbent and has run A nodes, each node pushes its
second child on a stack and solves the first next, as `--dive
--dive-backtrack B` does. When a dive ends, the tree resumes from the
stack up to B times; after that the stack goes back to the open set and
the next pick by bound starts a new dive. Once a point is found the tree
goes back to its own order.

Before this, `glass4` found a point only under `--dive`, and the
feasibility jump, the dive and the pump at the root failed on all five.

## The readings

The 2017 gap sum counts each model's primal and dual gap as
`bench/measurements/02-328/m17sum.py` does. MIPLIB 3 is the geometric mean
of work against the gate reading.

| arm | file stem | 2017 incumbents | 2017 gap sum | MIPLIB 3 |
|---|---|---|---|---|
| base | `bench/results/miplib2017.txt` | 25 | 14.45 | 1.000x |
| B 16 from node 0 | `bt16-after0` | 27 | 14.01 | 1.049x |
| B 100 from node 0 | `bt100-after0` | 28 | 13.03 | 1.119x |
| B 1000 from node 0 | `bt1000-after0` | | | 1.102x |
| B 100 after 200 | `bt100-after200` | 27 | 13.98 | 1.067x |
| B 100 after 500 | `bt100-after500` | 28 | 13.64 | 1.112x |
| B 100 after 1000 | `bt100-after1000` | 27 | 13.94 | 1.000x |
| B 100 after 2000 | `bt100-after2000` | 27 | 14.11 | 1.000x |

From node 0 the rule finds points on `timtab1` (1214160, the reference
764772), `glass4` (5.15e9, the reference 1.2e9) and `csched007` (378, the
reference 351), and better points on `neos-2657525-crna`, `mad` and
`neos-3754480-nidda`, whose first incumbent came late. It costs MIPLIB 3:
`enigma` finds its first point near node 900 and takes 8x the work under a
dive (19128 nodes against 895), `flugpl` 1.6x.

From node 1000 no MIPLIB 3 model is touched: every one holds a point by
then, and the 24 answers, node counts and digests are the same. On the
2017 set only `timtab1` (1128183) and `glass4` (5.50e9) change; every
other model reads the same to the bit. `glass4`'s gap stays at the cap of
1, so the sum falls by `timtab1`'s share alone. `csched007` finds 551 from
node 500 and none from 1000; `ic97_potential` and `csched008` find none in
any arm (`csched008` reaches 11 nodes).

## The rule

`MIP_NOINC_DIVE_AFTER` 1000 and `MIP_NOINC_DIVE_BACKTRACKS` 100, on by
default, and only where `--dive` is off: a tree run with `--dive` keeps the
user's settings. The gates of the landing re-take MIPLIB 3 and the 2017 set
with the rule in the library; they must read as `bt100-after1000` here.
