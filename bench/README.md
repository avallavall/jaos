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
| `netlib.baseline` | what each instance did last time, so a regression can be seen |
| `fetch.sh` | downloads each instance and refuses any whose checksum does not match |
| `run.c` | solves each one and judges it against the manifest, the checker, and the baseline |
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

## The baseline, and the question the gate cannot answer

The gate is all-or-nothing: until every instance meets every condition above
it reports `NOT MET`, and it will keep reporting `NOT MET` for the whole of
M1. That makes it useless for the question actually asked of every change
along the way — *did this make anything worse?* A run that fixed one instance
and broke two scores exactly like the run before it. Worse, the summary
counts can come out identical when the gains and the losses cancel, which
reads as "nothing happened" when two things did.

That is not hypothetical. It is how ten commits reached `main` in August 2026
carrying a wrong answer on `pilot-we`, a checker rejection on `pilotnov` and a
seventy-sevenfold slowdown on `grow22`, under a summary line that never moved.

So `bench/netlib.baseline` records what every instance did, and `make netlib`
compares against it:

```sh
make netlib             # run the gate and diff every instance against the baseline
make netlib-baseline    # rewrite the baseline from this run
```

Each instance is judged on the four predicates above plus its work count. Any
of them going from holding to not holding is a regression and fails the run on
its own, whatever the gate says. Work is allowed to grow by up to 2×; past
that it is reported too, because an instance that still reaches the same
optimum after eighty times the iterations has not kept working — it has become
a work-limit failure for any caller with a budget.

Improvements are printed as well as regressions. A baseline that only ever
tightens is one nobody remembers to loosen.

Updating it is a separate command on purpose, and never a side effect of
running the gate. A baseline that rewrites itself records whatever just
happened as correct, which is the one thing it must not do. Regenerate it when
a change's effect on these numbers has been read and accepted — and say so in
the commit.

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

There is one thing both reference sets agree on and both leave out: an
objective constant declared by an `RHS` entry on the objective row. JAOS
applies it, under the convention CPLEX documents and
`docs/format-support.md` records, so a correct answer differs from both
published values by exactly the constant. The `objconst` column carries it
and the gate compares against reference plus constant. It is nonzero on one
instance of this set, `e226`; `grow7`, `grow15` and `grow22` carry the same
entry with a value of zero, which is why nothing else shows it.

Fixing that in the reader instead would have been the wrong trade: it would
break every model whose author meant the constant, to agree with two
reference sets that predate the convention.

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
