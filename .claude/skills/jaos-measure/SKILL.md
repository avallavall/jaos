---
name: jaos-measure
description: How to judge any change to JAOS — performance, numerical or algorithmic — before accepting it. Load before proposing, running or believing a measurement in this repo. Covers deterministic work units as the unit of "faster", running the three instance sets, reading a per-instance baseline diff, using solution digests as proof that a change is a no-op, and the specific ways measurements here have been wrong before.
---

# Measuring in JAOS

A change is not accepted here because it is principled. It is accepted
because the instance sets say what it cost, on both sides of every number.

## Three steps here are not improvised

Each of these has exactly one right answer, each has been got wrong before,
and a wrong answer looks like a right one. **Run them; do not reimplement
them.** If one is wrong, fix the script — that is the point of it being a
script.

```sh
# before launching anything: tree settled, no runner in flight, suite green
bash .claude/skills/jaos-measure/scripts/preflight.sh

# after a run: the per-instance diff against the committed record
python3 .claude/skills/jaos-measure/scripts/record_diff.py bench/results/*.txt

# any ratio, ever
python3 .claude/skills/jaos-measure/scripts/geomean.py --metric work old.txt new.txt
python3 .claude/skills/jaos-measure/scripts/geomean.py --pairs timings.txt
```

`record_diff.py` reports which instances came back bit-identical, which
predicates moved, which work ratios passed 2x, and whether the record's format
changed under you. `geomean.py` prints the geometric mean of per-instance
ratios and the ratio of totals side by side, with the second labelled as not
the result — because D46 is a fact about this set, not a style preference.

Each was validated by injecting the faults it exists to catch and confirming
it caught them; a green run from them means something.

## "Faster" means fewer work units, not fewer seconds

The solver counts a deterministic integer of work in its kernels, reported by
`jaos_work_units()`. It is identical on every machine and in every run, which
is exactly what a wall clock is not. **No wall-clock time goes in the
record.** A number nobody can reproduce is not evidence, and a timing taken
on a developer laptop is noise wearing a decimal point.

The consequence is worth stating because it surprises people: **compiler
optimisation levels, memory layout and cache behaviour do not move work
units at all.** What work units *do* measure is doing less: fewer nonzeros
touched, fewer eliminations, fewer refactorizations, fewer iterations.

If you add a kernel, bill it. A counter that reports everything except the
last thing it did is lying by one solve.

**And the counter is optimistic by a factor that is not constant (D45).** M2
bought 2.953x in units and 1.866x in seconds; the real cost of a billed unit
spans 14.7x across the timed set. A ranking by work units alone can be wrong
by an order of magnitude depending on which instance carries the total. So a
change is judged on three things, not one: **digests for correctness, work
units for determinism and cross-machine comparability, and a same-instance
time ratio for what the other two cannot see.**

## The time ratio, and how to take one that means anything

Seconds are development numbers. They are printed on every run, they are
read, and they **never enter `bench/results/*.txt` or a baseline** — a
baseline that changes every run cannot detect a regression. That is the whole
of the rule; it is not a ban on measuring time, it is a ban on recording it.

Some changes move nothing else. `restrict`, a layout change, a flag: the work
counter cannot see them by construction, so seconds are the only evidence
there is, and refusing to take them means the change cannot be judged at all.

The protocol, and every clause of it has a reason:

- **`-j 1`.** A parallel run's seconds are inflated and the runner says so on
  every line of output (D57). Twelve solves contending for cache is not the
  workload anyone ships.
- **Two binaries, same machine, same session.** Build the candidate and its
  parent — `git show HEAD:src/file.c` gets you the parent without disturbing
  the tree — and keep both runners.
- **Alternate and take the minimum**, three rounds or more. Not the mean: the
  mean measures whatever else the machine was doing, the minimum measures the
  run that was least interrupted. This is what `bench/compare` does and why.
- **Name the instances.** The runner takes instance names as trailing
  arguments, so a ratio does not need the whole set: `build/bench/run -j 1
  maros-r7 pilot87 dfl001`. Pick instances where the changed code dominates —
  two of those three are 74.1% of the standard set's work (D46).
- **Report a geometric mean of per-instance ratios**, never a total. **Run**
  `scripts/geomean.py`; it prints both and labels which one is the answer.
- **Expect a percentage from anything that is not algorithmic.** Every
  optimisation flag in the shipping build is worth about 3% together, against
  1.1122x for PGO (D62). A "factor" from a flag is a measurement error until
  proven otherwise.

**The noise floor is not one number, and the smallest one is the wrong one to
reach for.** Three figures are in circulation and they measure different things:

| figure | what it is |
|---|---|
| **1.3%** (D60) | estimated on the harness, one way |
| **1.4%** (D81) | measured across four separate sessions, and consistent with D60's 1.3% |
| **6.27%** (D93) | what *this* Windows/WSL host actually repeats to, measured the way D81 measured its 1.4% |

Any claim smaller than the floor of the host you are on is a claim about the
machine. On this host that floor is **6.27%**, not 1.3% — D93 reached
INCONCLUSIVE on a 2.91% reading for exactly this reason, and a negative control
of instances the change provably could not speed up read 3.0–6.4%. Quoting
D60's 1.3% here would have made that false result look like a comfortable pass.

Derived bars inherit the same problem: D-13's 4.2% is `3 × 1.4%` (**D81's**
repeatability — *not* D83's, whose 1.4% is Clp landing within 1.4% of HiGHS on
total time, a different quantity). A bar of 4.2% cannot be tested on a host
whose own repeatability is 6.27%; say so rather than reporting a verdict.

## Run all three instance sets, and let the largest decide

```
make netlib J=12             # the standard feasible set    ~85 s
make netlib-infeas J=12      # models with no feasible point ~10 s
make netlib-kennington J=12  # much larger, correctness only ~8 min
```

**`J=N` or they run sequentially and cost minutes instead of seconds
(D57).** Forgetting it is the single most expensive habit available here: the
standard set is 8 minutes at `J=1` and 84 seconds at `J=12`. The instances
are independent and every figure the record carries is an integer the solver
computed, so solving them at once cannot move one — the seconds are the only
thing `-j` invalidates.

All three, every time, before believing anything. This is not caution, it is
a lesson with a receipt: a repair once looked perfect on the standard set
**and** on the infeasible set, and the largest Kennington instance caught it
costing 3.2x the work. The Kennington models are an order of magnitude beyond
the others in rows, columns and iterations, and they are where a change that
scales badly finally shows. A candidate that has not been through them has
not been measured.

The infeasible set exists to catch one specific catastrophe: a model with no
feasible point coming back with an answer. It has a mirror image that is just
as bad and has actually happened — a feasible model reported INFEASIBLE — so
watch both directions.

Build and run from WSL; the Windows side has no compiler.

**The exit code is lost by interpolating `$?`, not by crossing into WSL.**
Measured three ways in one session: `wsl … bash -c 'exit 7'` from PowerShell
leaves `LASTEXITCODE=7`, and a script returning 1 leaves `LASTEXITCODE=1` —
the boundary itself is faithful. What lies is `$?` written *inside* the
command string: through the Bash tool it is expanded before reaching WSL and
came back `True`, and through PowerShell it read `0` on a script that had just
exited 1. So:

- **Write the commands to a script file and run that.** Inside a script `$?`
  is correct, and the same fix covers heredocs through the Bash tool eating
  backslashes.
- If you must run one line, read the status from **`LASTEXITCODE`** in
  PowerShell rather than echoing `$?` inside the string — and note that a
  trailing `| head` or `| grep` replaces it with the pipe's status.

`/tmp` does not persist between `wsl` invocations, so anything one step hands
to the next belongs in one script.

The paths, because guessing them costs a full LTO rebuild each time: the
library is `build/release/libjaos.a`, the acceptance runner is
**`build/bench/run`** and the warm-start campaign is `build/bench/warm` —
`build/release/` holds objects, not programs.

## The summary line is not the result

The runner diffs every instance against a committed baseline and reports
regressions and improvements per predicate: solved, shape, objective, checker,
determinism, and work beyond a threshold. **Read the per-instance diff** —
**run** `scripts/record_diff.py` rather than reading two 94-line tables side
by side, which is where the eye stops being reliable.

A gate that already passes is precisely the one whose summary line cannot
show a change, which is the whole reason baselines exist for the sets that
pass. And grouping instances by the size of the number reported is how the
most accurate answer in the set once spent months in the wrong bucket: the
magnitude a checker prints is a Lagrange multiplier's magnitude, and a
multiplier's size says nothing about how far anything is from where it
should be.

Never edit a manifest or a baseline while a run is in flight.

## Baselines are rewritten on purpose, never as a side effect

Rewriting the baseline is a separate command from running the gate, and that
separation is deliberate: a baseline that updates itself records whatever
just happened as correct, which is the one thing it must not do.

Rewrite after a change whose effect has been read and accepted. Leaving them
stale is not conservative — it degrades every later diff into noise, because
old differences accumulate and nobody can tell which line is new.

## Digest equality is a free and very strong proof

Every record line carries a hash of the published solution. Two uses:

- **A change that should not alter behaviour must leave every digest
  identical.** Comment edits, refactors, and any reordering claimed to be
  equivalent. If one digest moves, the change was not what it claimed.
- **A change that should alter one instance should alter exactly that one.**
  Diff the new record against the previously committed record and the moved
  instances name themselves. A tight footprint is evidence; a scattered one
  is a warning.

Compare against the committed record, not against memory.

## A result that is too clean is a broken instrument

Every measurement failure in this project has announced itself as a number
that was *better* than it had any right to be, never as something breaking.
Three in one session:

- A comparison reporting JAOS at 0.0015s and **0 iterations** on `25fv47`
  against a true 0.49s and 9459 — it was timing a warm re-solve, because the
  driver solved the same model N times and a solve now leaves its basis
  behind (D80).
- A no-op check printing NO-OP CONFIRMED when the build had failed and no gate
  had run. An unchanged `bench/results/` is also what "nothing wrote to it"
  looks like.
- A five-point constant sweep reading **exactly 1.0000x** at every setting on
  both sets, because `make` cannot see a `CFLAGS` change and one binary was
  measured six times (D82).

So, before believing a good number: **ask what it would look like if the thing
had not run at all**, and check the difference. Concretely — a check script
must verify each step actually executed rather than inferring it from an
unchanged file; a sweep must open with a canary that *forces* the effect on
and abort if the numbers do not move; and a control that cannot change (JAOS
against itself across comparison rungs, iterations exactly 1.000x) is worth
more than any headline in the same table.

## How measurements here have been wrong before

- **A green result is not a proof.** A checker rule once passed the whole unit
  suite and every instance while certifying the entire feasible region as
  optimal. When changing a checker, a tolerance or any predicate: build the
  case it must reject, and confirm that it does.
- **An instrument that finds nothing is worth nothing until it is shown able
  to find something.** A reader fuzzer was accepted only after injecting a
  real one-token buffer overflow and watching it get caught.
- **Instrumentation that applies a change and then undoes it is not
  bit-exact.** `x += d` followed by `x -= d` does not restore `x` in floating
  point. Such a build is fine for measuring a residual and useless for
  comparing iteration counts against a baseline.
- **The characteristic failure of a repair here is fixing the instance in
  front of it and breaking another.** Of six measured repairs of one residue,
  four were rejected, and every rejection failed that way: a feasible model
  returned INFEASIBLE, or an instance losing its answer to a tripped
  iteration guard. Assume this is happening until all three sets say it is
  not.
- **An instance disagreeing with a published reference is not evidence about
  which of them is wrong.** Check the reference before changing the solver.

## Finish every source edit before launching a campaign

A run takes tens of minutes and is only valid for the tree that produced it.
Even a comment edit mid-run leaves you unable to say what was measured.
Land the whole change, then measure.

**Run** `scripts/preflight.sh` instead of checking this by eye. It also
catches the case no amount of care catches: another session's runner already
writing `bench/results/`. That is not hypothetical — it was caught happening
the first time the script was run, together with a results file sitting empty
because a campaign was mid-write. Both are invisible in the output of the run
you are about to start.

## Never put a measurement worktree under `build/`

`make clean` is `rm -rf build`, and `make configs` runs it between each of its
five configurations. **A worktree at `build/diag/wt-*` is deleted by anyone
else's `make configs`, mid-campaign, with no error on your side** — the
directory simply stops existing.

44 of this repository's measurement scripts use exactly that location, from
02-28 onward. It was safe while one thing ran at a time. It is not safe now:
`CLAUDE.md` routes campaigns to `jaos-measurer` while the main context keeps
working, and that is two things at once by design. It happened on D166 — a
`jaos-measurer` campaign lost its whole worktree to the main context's
`make configs` and had to relaunch (`bench/measurements/02-76/`).

Put it in `$(mktemp -d)`, which is outside the repository and cleans itself up:

```sh
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
git worktree add --detach "$D/wt" "$ref" || exit 2
# ... and `git worktree remove --force "$D/wt"; git worktree prune` at the end,
# because the trap removes the directory but not git's registration of it.
```

The older scripts have not been converted. If you re-run one, move its
worktree first or make sure nothing else is building.
