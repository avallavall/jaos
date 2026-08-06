# bench — the Netlib acceptance gate

This directory is how the first milestone gets judged. It is a development
tool, not part of what JAOS ships: nothing here is built by `make all` and
nothing here is linked into the library.

```sh
bench/fetch.sh      # download and verify the instances (once)
make netlib         # build the runner, fetch if needed, run the gate
```

`make netlib` writes the per-instance table to `bench/results/netlib.txt` and
exits non-zero unless every instance met every condition.

## What is here

| File | |
|---|---|
| `netlib.manifest` | the instance list: pinned sha256, expected shape, reference optimum |
| `fetch.sh` | downloads each instance and refuses any whose checksum does not match |
| `run.c` | solves each one and judges it against the manifest and the checker |
| `results/` | output of a run; ignored by git except for this directory itself |

The instance files never enter the repository (PLAN 2.10). The manifest is
what stands in for them, so a checkout plus a network connection reproduces
exactly the set any other checkout would run.

## Where the instances come from

The dataset published with Thorsten Koch, *The Final NETLIB-LP Results*
(ZIB-Report 03-05), at `https://www.zib.de/koch/perplex/data/netlib/mps/`.
Plain MPS, gzipped, 94 instances.

Netlib's own copies are in a packed format that needs its `emps` expander to
read, so they are not usable directly. Koch's are the same problems already
expanded, from the work that produced the reference values this gate judges
against — the instances and the optima come from one source rather than two
that have to be reconciled.

## What each instance is judged on

Four things, and three of them come from outside this solver:

1. **Shape.** The file must load with the row and column counts the manifest
   records. A reader that dropped a row would otherwise go unnoticed until
   the objective happened to move, and the checker cannot catch it — checker
   and solver read the same stored matrix, so a mis-built model makes them
   agree about the wrong problem (D18).
2. **Objective.** Within `1e-6 · max(1, |reference|)` of the reference
   optimum (PLAN 2.6).
3. **The independent checker.** Primal feasibility, dual sign conditions,
   complementary slackness and the primal-dual gap, all judged in the
   original unscaled problem (D18).
4. **Determinism.** The model is solved twice and the two runs must agree on
   status, iteration count, work units, and the bits of the objective and of
   every published value (D8).

No wall-clock figure is produced anywhere. Speed is an M2 question and needs
a controlled host before any number about it means anything (D17).

## The reference values, and why Koch rather than netlib

Netlib's own readme carries a table of optima computed with MINOS 5.3. Koch
recomputed them in exact rational arithmetic and found some of the published
ones wrong. On the set here the two sources disagree by more than this gate's
tolerance on eight instances:

    80bau3b   ganges   greenbea   greenbeb   nesm   pilot   pilot-we   scrs8

`greenbea` differs in the third significant figure. So the manifest takes
Koch's value wherever it exists — the nearest double to the exact rational he
proved — and falls back to the netlib readme only for `maros-r7` and
`pilot87`, which his published results do not cover. The `source` column on
each line says which.

## Cross-checks that were run once, on the way in

The manifest's shapes are not simply Koch's word for it. Netlib's canonical
summary table and the plain-text header of each of netlib's own packed files
both carry row, column and nonzero counts, and neither needs `emps` to read.
Koch's row count is the canonical one minus the objective row on every
instance but `boeing1`, where netlib carries one additional free row, and his
column counts match on all of them.

That is what stands in for the byte-level comparison against canonical
expansions that PLAN 2.10 originally called for, which would have needed the
`emps` expander to perform.

## What is not here

The Kennington subset (16 instances) and the infeasible subset (29) are
distributed only in netlib's packed format, and no institutional source
republishes them as plain MPS. They are absent from this gate until that is
resolved, which is an open question rather than a decision. Their loss is not
equal: the infeasible set is the only thing that exercises the `INFEASIBLE`
classification on models nobody constructed to be infeasible.
