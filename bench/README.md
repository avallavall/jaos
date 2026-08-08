# bench — the Netlib acceptance gate

This directory is how the first milestone gets judged. It is a development
tool, not part of what JAOS ships: nothing here is built by `make all` and
nothing here is linked into the library.

```sh
bench/fetch.sh      # download and verify the instances (once)
make netlib         # build the runner, fetch if needed, run the gate
```

## Three sets

The M1 gate asks for three (PLAN 2.9), and all three are now pinned:

| set | instances | what it asks | run with |
|---|---|---|---|
| standard | 94 | solved to a verified optimum | `make netlib` |
| Kennington | 16 | the same, for correctness only | `make netlib-kennington` |
| infeasible | 29 | classified `INFEASIBLE`, no false optima | `make netlib-infeas` |

The infeasible set is the only thing in M1 that looks for a *wrong* answer
rather than confirming a right one: it asks whether a model with no feasible
point ever comes back with an optimum. Not a hypothetical failure mode — the
revert of 2026-08-07 was forced by its mirror image, a feasible instance
reported `INFEASIBLE`, and nothing in the suite caught it.

`-e infeasible` switches the acceptance rule. Shape and determinism still
hold; there is no reference objective and nothing for the checker to judge;
an instance coming back `optimal` is flagged `<-- FALSE OPTIMUM` and fails
the run.

Each set fetches into its own directory. That is not tidiness: `greenbea`
names a feasible model in the standard set and a different, infeasible one
here — the original, from which the feasible version was repaired — and two
models must never share a path.

## Where the instances come from, and emps

The standard set is Koch's plain-MPS mirror, the dataset published with
Thorsten Koch, *The Final NETLIB-LP Results* (ZIB-Report 03-05), at
`https://www.zib.de/koch/perplex/data/netlib/mps/` — plain MPS, gzipped, 94
instances. He mirrors exactly the instances his paper verified, which is why
no expander was ever needed for it, and it means the instances and the
reference optima come from one source rather than two that have to be
reconciled. Netlib's own copies of the same problems are in a packed format
that needs its `emps` expander to read, so they are not usable directly.

Kennington and the infeasible set are served by netlib in its packed form, so
`fetch.sh` expands them with netlib's own `emps` — downloaded, verified
against a pinned sha256, built into a temporary directory, and **never stored
here**, exactly like the instances (PLAN 2.10). `emps.c` carries no licence,
no copyright notice and no public-domain declaration, so redistributing it is
not something an Apache-2.0 project can do cleanly; using it as a dev-time
tool is. PLAN Q6 records the decision.

```sh
bench/fetch.sh -m bench/netlib-infeas.manifest \
    -b https://netlib.org/lp/infeas -p emps bench/instances-infeas
```

`-p` selects the pipeline: `mps-gz` (gunzip only), `gz-emps`, or `emps`.

`make netlib` writes the per-instance table to `bench/results/netlib.txt` and
exits non-zero unless every instance met every condition.

## What is here

| File | |
|---|---|
| `netlib.manifest`, `netlib-kennington.manifest`, `netlib-infeas.manifest` | one instance list per set: pinned sha256, expected shape, reference optimum |
| `netlib.baseline`, `netlib-kennington.baseline`, `netlib-infeas.baseline` | what each instance did last time, so a regression can be seen |
| `fetch.sh` | downloads each instance and refuses any whose checksum does not match |
| `run.c` | solves each one and judges it against the manifest, the checker, and the baseline |
| `results/` | output of a run; ignored by git except for this directory itself |

The instance files never enter the repository (PLAN 2.10). The manifest is
what stands in for them, so a checkout plus a network connection reproduces
exactly the set any other checkout would run.

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

**Three figures in the record judge nothing**, and are there because a
verdict that only records its own outcome cannot be argued with later.
`rowrel` is the worst row residue as a fraction of what that row carries;
D24 refused to let the primal test become relative and keeps the measurement
here instead. `Q` and `N` are the two sums the gap is the difference of —
`gap = |Q − N| / (1 + |P| + |D|)` — so that a small gap can be told apart
from two large halves cancelling, and so that `P − P* <= Q` is a bound a
reader can check rather than a consequence of a hypothesis nobody is
testing. Across the standard 94 the difference is not academic: on 22 of the
94 instances that reach an optimum, `Q` is more than twice `|Q − N|`, so the
gap those instances report is not the bound they are entitled to. The count
was 35 of 93 when this was first measured; it falls as the answers get more
accurate, which is the direction it should move.

## The baseline, and the question the gate cannot answer

The gate is all-or-nothing: it reports `NOT MET` until every instance meets
every condition above, and it reported `NOT MET` for almost the whole of M1.
That makes it useless for the question actually asked of every change along
the way — *did this make anything worse?* A run that fixed one instance and
broke two scores exactly like the run before it. Worse, the summary counts
can come out identical when the gains and the losses cancel, which reads as
"nothing happened" when two things did.

All three sets now report `PASS`, which does not retire the problem — it is
the state in which the summary line is *guaranteed* to say nothing. From here
on, the per-instance diff is the only thing that can report a regression at
all.

That is not hypothetical. It is how ten commits reached `main` in August 2026
carrying a wrong answer on `pilot-we`, a checker rejection on `pilotnov` and a
seventy-sevenfold slowdown on `grow22`, under a summary line that never moved.

So `bench/netlib.baseline` records what every instance did, and `make netlib`
compares against it:

```sh
make netlib             # run the gate and diff every instance against the baseline
make netlib-baseline    # rewrite the baseline from this run
```

All three sets have one, and now that all three pass they need it equally.
An instance that still ends `INFEASIBLE` after eighty times the work has
regressed, and only the per-instance diff can say so.

```sh
make netlib-infeas      make netlib-infeas-baseline
make netlib-kennington  make netlib-kennington-baseline
```

Each instance is judged on the four predicates above plus its work count. Any
of them going from holding to not holding is a regression and fails the run on
its own, whatever the gate says. Work is allowed to grow by up to 2×; past
that it is reported too, because an instance that still reaches the same
optimum after eighty times the iterations has not kept working — it has become
a work-limit failure for any caller with a budget.

Improvements are printed as well as regressions. A baseline that only ever
tightens is one nobody remembers to loosen.

A run that compared against no baseline says so in its own record file, as
`baseline: NOT COMPARED`. Without that line a results file produced by
`make netlib-baseline` is indistinguishable from a checked one — which went
wrong immediately, the first record committed next to a baseline having come
from a different build than the baseline did.

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
Koch's value everywhere: the nearest double to the exact rational he proved.
The `source` column on each line says so.

**Where the last two came from.** `maros-r7` and `pilot87` used to fall back
to the netlib readme, because Koch's exact rationals were published at
`zib.de/koch/perplex` and that path no longer resolves, and because reading
them off the report's PDF reproduced only 23 of the 92 values already known
to be his — not enough to set ground truth with.

The same report's **PostScript** is enough. It is dvips output and carries
the whole table as literal strings, so the values come out with nothing but
the typesetting undone — `Fc(\000)` is the minus sign, `Fa(:)` the decimal
point, `Fq(n)` the exponent, and kerning splits names and mantissas across
strings.

```sh
curl -O https://opus4.kobv.de/opus4-zib/files/727/ZR-03-05.ps
python3 bench/koch-refs.py ZR-03-05.ps bench/netlib.manifest > refs.txt
python3 bench/koch-verify.py refs.txt bench/netlib.manifest
```

`koch-verify.py` is the part that matters: it checks the extraction against
every reference pinned here and reports **82 reproduced exactly, double for
double, and none in disagreement**. Eighty of those were pinned from Koch
before any of this ran, independently transcribed, and that is what validates
the decoding rather than assuming it — the two new values come out of the
same pass that reproduces eighty already-known ones bit for bit.

The twelve it does not reach are rows whose names the reassembly fails to
recover; they were already pinned and nothing here touches them. Both scripts
are dev-time tools — nothing builds them, nothing links them, and the library
does not depend on Python.

```
pilot87    301.71072827  ->  301.7103473331105     relative 1.26e-6
maros-r7   1497185.1665  ->  1497185.166479644     relative 1.36e-11
```

The gate's tolerance is 1e-6 relative, so `pilot87` was being judged against
a reference outside tolerance of the exact optimum. No verdict moves: it
misses its objective by fifteen tolerances against either number. What
changes is that the gate is now honest about what it measures against.

`maros-r7` is the instance Koch singles out as the largest value in the set,
needing 47040 bits — over 14,000 decimal digits — for its exact rational.

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

Nothing of the M1 gate. All three sets are present and all three are pinned;
what used to stand here — that Kennington and the infeasible subset were
absent because netlib serves them packed and no institutional source
republishes them as plain MPS — was resolved by using netlib's own `emps` as
a dev-time tool (PLAN Q6), and the section is kept only so the record shows
what changed rather than pretending the gap never existed.

The one reference value that used to be missing is here too: every line of
the manifest is now sourced `koch`, taken from the report's PostScript and
checked against the 80 references already pinned. See the section above.

What remains genuinely absent is exact verification of a *solution* rather
than of an objective — that is PLAN Q8, and it opens with M2.
