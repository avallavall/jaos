# 02-363 — cycle cuts over rows with two continuous columns, refused

Taken on 2026-10-08 on the tree of 1e48db8, for TODO row J7.

## The idea

On `ic97_potential` SCIP's c-MIR cuts are cycle cuts over the integer
columns alone: the tension rows around a cycle of the event graph are
summed so the potentials cancel. JAOS's aggregation does not build those
sums, and its root stays at the LP's 3868.46.

`cycle.diff` adds a cut family for any row whose continuous columns are
exactly two, with coefficients `a` and `-a`, beside at least one integer
column. Each finite side of such a row is an arc between the two
continuous columns: `v_q - v_p + J(x) >= b`, with `J` the row's integer
part over `a`. Around a directed cycle the `v` cancel, so
`sum J(x) >= sum b` is a valid row over integer columns alone. Each round
takes the arcs in order of their slack at the LP point, finds for each
the path back with the least slack (Dijkstra, the arc's own row left
out), and hands the cycle's sum to the single-row MIR (`mir_side`).
`JAOS_CYC` sets the rounds (20 in the diff, 0 is the control) and
`JAOS_CYCN` the cuts a round (200).

`potrows.py` scans the 2017 set: 9 of the 30 models carry such rows (`csched007` 198, `csched008` 190, `glass4` 130, `ic97_potential`
1046, `mad` 10, `neos-3754480-nidda` 100, `pk1` 15, `supportcase26` 792,
`timtab1` 27).

## The reading

Each model to the end of its root (`--node-limit 1 --no-heuristics`):

| model | root, off | root, on | cycle cuts | root work, on/off |
|---|---|---|---|---|
| `ic97_potential` | 3868.46 | 3874.46 | 1074 | 1.08x |
| `ic97_potential`, 1000 cuts a round | | 3875.46 | 1268 | 5.75x |
| `ic97_potential`, 50 cuts a round | | 3870.46 | 350 | 1.00x |
| `glass4` | 800002400 | 800002400 | 25 | 1.40x |
| `timtab1` | 427178.16 | 427178.16 | 0 | 1.00x |
| `csched007` | 287.76 | 287.76 | 0 | 1.00x |
| `csched008` | 171 | 171 | 0 | 1.00x |
| `supportcase26` | 1288.10 | 1288.10 | 0 | 1.04x |
| `neos-3754480-nidda` | -1216923 | -1216923 | 0 | 1.00x |
| `pk1` | 0 | 0 | 0 | 1.00x |
| `mad` | 0 | 0 | 0 | 1.00x |

SCIP's root on `ic97_potential` is 3895.8 and HiGHS's 3898.4, so the
family closes 6 of the 27 points between JAOS and SCIP. The dual gap of
the model moves by 0.0015 of the reference 3942, and it still holds no
incumbent. On the other eight the family finds no violated cut or none
that moves the root. Refused as `cycle-cuts-two-potentials`.
