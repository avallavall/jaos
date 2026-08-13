# JAOS — Just Another Optimization Solver

A mathematical-programming solver written from scratch in C23. No external
dependencies, Apache 2.0, Linux/GCC only.

## The record — five documents, and where a statement goes

The design is written down. Do not reconstruct it from the code.

- `SPECS.md` — what JAOS is built to be, and where every feature stands
- `TODO.md` — what is open, in the order it should happen
- `DECISIONS.md` — closed decisions and the measurement that closed each.
  Append-only; number one past the last, never renumber. A refusal is a
  closed decision, and the most valuable kind of entry this file has.
- `CHANGELOG.md` — what changed and what it cost, 2–6 lines per entry
- `docs/` — tolerances, work units, scaling, format support: the contracts
  behind every constant in the code
- `bench/` — the gate (`README.md`), the cross-solver comparison
  (`compare/`), and raw measurement records (`measurements/<id>/`)

A statement lives in exactly one of them; the others point at it. A measured
number has one owner — never restate a derived total.

There is no `.planning/` and no GSD in this repository (retired 2026-08-13,
D98). If a `/gsd-*` command is invoked here, say so and point at `TODO.md`.
`PLAN.md` stays archived at `docs/archive/PLAN.md` only because 88 comments
cite it by section number; never plan from it.

## The loop — every change goes through this

1. **Finish every source edit first.** A campaign is only valid for the tree
   that produced it; even a comment edit mid-run invalidates it.
2. `make test && make sanitize` under WSL.
3. **If solver internals changed: `numerics-reviewer` on the diff, before any
   campaign.** A finding after the campaign costs the campaign. Every finding
   gets a disposition: fixed, refused with the reason, or carried with a
   destination.
4. **All three sets, every time**: `make netlib netlib-infeas
   netlib-kennington J=12`. Read the per-instance diff against the committed
   baselines with the `jaos-measure` scripts, never the summary line alone.
   Digests prove correctness and no-ops; work units are the cost; a time
   ratio only where units cannot see the change (`J=1`, minimum over
   alternating rounds, geometric mean — and this host repeats to 6.27%, D93).
5. **Land it**: commit; a `CHANGELOG.md` entry; a `DECISIONS.md` entry if a
   measurement closed a question; the `SPECS.md` row if a feature moved; any
   new constant carries its sweep beside it in the source and in
   `docs/tolerances.md`; raw readings that decided a verdict go to
   `bench/measurements/<id>/` so the verdict is re-derivable.
6. **Baselines are rewritten only by the `*-baseline` targets**, deliberately,
   after the change is read and accepted — never as a side effect, and never
   while the gate is red.
7. Cross the item off `TODO.md` in the same commit — and check TODO's
   refusals table: if the change satisfies a reopen condition, that question
   is live again. A refusal's premise can expire (D24 did, caught by D94).

A verdict that accepts or rejects a candidate is judged by `jaos-measurer` in
a context that did not produce the numbers. Phase 1's two most valuable
findings came from these independent re-reads and from nowhere else.

## Build and test — WSL only

The Windows side has no compiler. GCC 14 minimum.

```
wsl -d Ubuntu-24.04 -- bash /mnt/c/path/to/script.sh
```

`make test` · `make sanitize` (ASan+UBSan) · `make all`

The campaigns, and **all of them take `J=N` — pass it or they run sequentially
and cost minutes instead of seconds** (D57). Times below are `J=12`:

`make netlib` (~85 s) · `make netlib-infeas` (~10 s) ·
`make netlib-kennington` (~8 min) · `make warm` (~2 min) ·
`make warm-kennington` (~4 min) · `make compare` · `make pgo`

The three `netlib*` targets are the gate. `warm*` measures what warm
re-solving buys and is not a gate: it reports a ratio, not a verdict.

Two traps. **`$?` does not survive Git Bash → WSL** — echoing it inside the
`wsl … bash -c '…'` string does not rescue it, because it is expanded before
it reaches WSL. **Write the commands to a script file and run that**; inside a
script `$?` is correct. The same fix covers the second trap, heredocs through
the Bash tool eating backslashes. Simpler still: invoke `wsl` from the
PowerShell tool, which does no path rewriting — but then PowerShell is the
outer shell, so put any pipeline inside WSL (`bash -c "… | grep …"`). `/tmp`
does not persist between `wsl` invocations.

## Rules that are not obvious from the code

- **Bit-identical results on every machine and every run.** No clock may
  decide anything, no iteration order may depend on an address, no
  reassociating floating point, no unseeded randomness. `-ffp-contract=off`
  in the Makefile is load-bearing, not decoration.
- **Every number needs a measurement on both sides.** A tolerance, a
  threshold, an interval. Fitting a constant to one instance is how this
  project loses weeks.
- **No dependencies, and no code read from other solvers.** Papers, theses
  and textbooks only. Two exceptions exist, both closed and neither extended:
  netlib's `emps` as a dev-time converter, and Unity for the test suite.
  The rule reaches the tooling too: read a third-party skill's `scripts/`
  before it runs once — `skill-authoring` carries the procedure.
- **Work units are the unit of cost, and every run also reports its time.**
  The units make regressions detectable across machines and go in the
  record; the seconds say whether the units bought anything, and **never
  enter `bench/results/*.txt` or a baseline** — a baseline that changes every
  run cannot detect a regression.
- **A change is judged on three things** (D45): solution digests for
  correctness, work units for determinism, and a same-instance time ratio to
  catch what the other two cannot see.

## The skills, and the moment each one is for

Load these at the moment named, not when the work is already finished.

| at this moment | load |
|---|---|
| before running or believing any campaign | `jaos-measure` |
| before changing a tolerance, or diagnosing a wrong answer | `fp-numerics` |
| before instrumenting an instance | `jaos-debug` |
| before adding or changing a test, or a checker predicate | `jaos-testing` |
| before writing a landed change up, and before claiming a document is current | `jaos-record` |
| before planning performance work on the algorithm | `sparse-simplex-perf` |
| before optimising C, or proposing a compiler flag | `c-perf` |
| before creating or editing a skill or an agent | `skill-authoring` |

The two performance skills are not interchangeable. `sparse-simplex-perf` is
the factor-of-N question — what the solver does. `c-perf` is the percentage
question — how the C does it, once the algorithm is settled.

The three subagents, each for work better done in a context that is not this
one — nothing spawns them automatically; the loop's steps 3 and the verdict
line above are where they run:

| | |
|---|---|
| `jaos-measurer` | runs every set on a finished candidate and returns ACCEPT / REJECT / INCONCLUSIVE with the per-instance evidence |
| `numerics-reviewer` | reviews a diff for the defect classes tests do not catch — borrowed scratch, reproducibility, tolerance space, repairs that hide a residue |
| `literature-scout` | finds and verifies published technique, with citations checked against the publisher |

## Working habits

- **Measure before repairing.** Every failure in this project that looked
  like a tolerance turned out to be something else.
- **A green result is not a proof.** When changing a checker or a predicate,
  build the case it must reject and confirm it does.
- **Report a geometric mean of per-instance ratios**, never a sum over a set:
  two instances are 74% of the standard set's total (D46).
- Commits are at Claude's discretion; **pushes always need explicit
  approval**.
