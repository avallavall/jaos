# bench — the Netlib acceptance gate

This directory is how the first milestone gets judged. It is a development
tool, not part of what JAOS ships: nothing here is built by `make all` and
nothing here is linked into the library.

```sh
bench/fetch.sh      # download and verify the instances (once)
make netlib         # build the runner, fetch if needed, run the gate
make netlib J=10    # the same run, ten instances at a time
```

## `J` — the same record, in a tenth of the time

Every number the record carries is an integer the solver computed, and the
instances do not depend on each other. So `J=N` solves N of them at once, one
process each, and the parent reassembles the table in manifest order: the
record comes out byte-identical to a sequential run, which is checked by
diffing rather than asserted (D57). The standard set takes 84 s instead of
eight minutes, the infeasible set 9 s instead of two minutes, Kennington
8 min 21 s instead of thirty.

**What `J` does invalidate is the seconds**, and the runner prints a line
saying so on every parallel run. Concurrent solves compete for cache and
memory bandwidth, so each one's time is inflated by an amount nobody
measured. The record above it is unaffected — it is integers — but the time
ratio a change is judged on (D45) has to come from `J=1`.

Ten suits a six-core machine on the standard set. Kennington is bounded by
memory rather than cores; six to eight there.

## Three sets

The M1 gate asks for three (PLAN 2.9), and all three are now pinned:

| set | instances | what it asks | run with |
|---|---|---|---|
| standard | 94 | solved to a verified optimum | `make netlib` |
| Kennington | 16 | the same, for correctness only | `make netlib-kennington` |
| infeasible | 29 | classified `INFEASIBLE`, no false optima | `make netlib-infeas` |

**139 instances, 139 answers, 110 digests.** This file owns those numbers and
everything else should cite rather than restate them. The distinction is not
pedantry: only the 94 standard and 16 Kennington instances produce a **solution
digest**, because only they have a solution. The 29 infeasible ones produce a
**refusal verdict** — `expected=infeasible verdict=ok det=ok` — and carry no
`digest=` field at all, which `grep -c digest=` will confirm.

So "all 139 answers unmoved" is the true claim and the strong one. "All 139
digests" is not: it invites a reader to check 139 against the record and find
110. That phrase was in 47 places across this repository and wrong in every one
of them; D93 states the precise form, and this table is where the composition
lives.

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
   every published value (D8). **The basis is cleared between them**, and that
   is what keeps this a statement about the solver: a solve that finds an
   optimum leaves its basis on the model, so without the clear the second run
   is a warm re-solve, reaches the same optimum in no iterations, and reports
   different work. All 94 instances said DIVERGED the day warm starting landed
   and every one of them was still optimal (D68).

No wall-clock figure is produced anywhere. Speed is an M2 question and needs
a controlled host before any number about it means anything (D17).

**Seven figures in the record judge nothing**, and are there because a
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

The remaining four say whether `Q` is a bound at all. `drop` is the largest
multiplier whose term the duality identity could not take — its sign points at
an infinite bound, so the term is minus infinity and gets dropped, and `Q`
then belongs to a different problem — and `cert` is whether any were dropped.
**54 of the 110 accepted answers across both feasible sets are now
certified**, against 12 before the checker began bounding unbounded variables
by what the rows imply — and the largest term the identity still cannot take
is 3e-08, with everything below it at 1e-14 or smaller. What is left
uncertified is arithmetic rather than structure (D91). `rsub` is that bound as
a fraction of the objective; it reads 6.9e-05 where `pilot` is right and
5.02e-03 where it is wrong, and the baseline watches it. `sub` is a certified lower bound on the suboptimality and `rays` counts
the directions that could not be quantified; `sub` is sound and, on this
evidence, uninformative — it reads the same on answers known to be wrong as on
correct ones, because the step it uses cannot move at a vertex (D73).

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

**And the suboptimality the answer carries, which no predicate can see.**
The `checker` predicate reports sign conditions and the gap over bounds the
model declared; it says nothing about how far from optimal the point may be,
which is a separate quantity and the one D47 was about. `relative_suboptimality`
is that bound as a fraction of the objective, it is carried in the baseline,
and growth past 2× above a floor of 1e-9 is a regression. Both numbers are
measured: the quantity is deterministic, and the case this exists to catch —
`pilot` going from 6.9e-05 where it is right to 5.02e-03 where it is wrong —
is a factor of 73 (D88, D91).

This is the check that would have caught D82, where a change published an
answer out of tolerance with every checker number green and this gate passed
it. It catches the move from right to wrong, not wrongness itself.

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

## A verdict commits its readings

The seconds never enter `bench/results/*.txt` or a baseline. That rule is not
relaxed by anything here, and it is the reason this section exists rather than
the record simply widening to hold them: a baseline that changes every run
cannot detect a regression.

But a plan whose *deliverable is a verdict* — a same-instance time ratio, a
profile, anything the record cannot carry — has to leave its raw readings
somewhere, or the one number that decided the phase becomes the one number
nobody can check. They go in
`.planning/phases/<phase>/<plan>-MEASUREMENT/`: the timing logs, any callgrind
annotations, and the analysis script that turned them into the figure.

Phase 1 is why. Verification re-derived 27 of its 29 must-haves straight from
the repository, and the two it could not were the measurement that decided the
phase — the twelve timing logs and both callgrind annotations had never been
committed. An independent audit of that verdict found a negative control
sitting unused in the campaign record, worth more than the verdict itself, and
it could only run because the logs happened to still be in a scratchpad. By the
time anyone looked again they were gone, and they are not recoverable.

The point is not distrust of whoever took the measurement. It is that a
geometric mean is one line and the data under it is a thousand, and the line is
the part that cannot be re-read.

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

## `make warm` — the other question the gate cannot answer

The gate solves each instance **once, from a fresh load**, which is precisely
the case warm re-solve does not touch. So the gate can prove warm starting
broke nothing and can never say what it is worth. `bench/warm.c` is what says.

It applies one branch-and-bound branching step per instance — the lowest
structural column whose optimal value is not an integer, branched down to
`x_j <= floor(x_j*)` or up to `ceil(x_j*)` — and then solves the perturbed
model twice: **warm**, resuming from the basis the anchor solve left, and
**cold**, the same model after `jaos_clear_basis`. A branch rather than an
arbitrary nudge, because its size comes from the model's own numbers and not
from a constant chosen here, and because it is the workload phase 7 will run
millions of times.

It reports geometric means of per-instance ratios (D46), iterations as
`(warm+1)/(cold+1)` so that a solve finishing in no iterations does not
annihilate the mean, and — like the gate — **no wall-clock number reaches
`bench/results/warm.txt`**. It fails when warm and cold disagree on a verdict
or an objective, or when the independent checker refuses **either** answer:
warm starting is a starting point and never a claim, so a disagreement is a
defect rather than a trade-off.

**Both answers, and it used to be one.** Checking the warm answer alone looked
sufficient because cold is the reference and the gate already checks cold
answers — but the gate checks them on the models as *loaded*, and a branch has
moved away from that. The perturbed model's cold solve was therefore the only
published answer in this repository that nothing judged, and on `pilot87` it
was the one that was wrong (D92). The verdict line names which side was
refused, because "the pair was refused" does not say where to look.

`make warm-kennington` runs the same campaign on the large set, at about half
as much again as `netlib-kennington` costs, since it solves three times per
instance where the gate solves twice.

The reading is in D69. Two things it records that make the ratios mean
anything: the cold number is checked against the gate's own iteration counts,
so a branch that made the model easier from scratch would show; and the anchor
objective is kept, so a branch that cut nothing off would show as an optimum
that never moved.

It also skips more than it might look like it should — 2 instances of the
standard set and 5 of Kennington, including all four `ken-*`. Those are models
whose optimal values land on integers, so there is no fractional column to
branch on. A skip is not evidence about warm starting in either direction, and
the count is printed rather than averaged over.

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
