# JAOS — Just Another Optimization Solver

A mathematical-programming solver written from scratch in C23. No external
dependencies, Apache 2.0, Linux/GCC only.

## Read these before changing anything

The design is written down. Do not reconstruct it from the code.

- `SPECS.md` — what JAOS is built to be, and where every feature stands
- `.planning/ROADMAP.md` — what is open, in the order it will be done
- `DECISIONS.md` — closed decisions and the measurement that closed them
- `CHANGELOG.md` — what changed and what it cost, a few lines per entry
- `bench/README.md` — the acceptance gate
- `bench/compare/README.md` — how JAOS is compared against other solvers
- `docs/` — tolerances, work units, scaling, format support

Reasoning about a **closed** decision goes in `DECISIONS.md`. What is still
**open** goes in `.planning/`. A feature's existence or absence goes in
`SPECS.md`. The changelog is a changelog, not a decision record.

**`PLAN.md` is archived** at `docs/archive/PLAN.md` since 2026-08-12, replaced
by `.planning/ROADMAP.md` + `.planning/REQUIREMENTS.md`. Do not plan from it.
It is kept because 88 comments across `src/`, `include/`, `tests/` and `docs/`
cite it by section number, and its redirect table is what makes `PLAN 2.5` and
the rest resolve. It also disagrees with the roadmap on three points **on
purpose** — phase order, whether the re-entry oscillation is open work, and
several defects `DECISIONS.md` has since closed. Where they differ, the
roadmap wins; `.planning/INGEST-CONFLICTS.md` records why for each.

`.planning/` is GSD's. `ROADMAP.md` and `REQUIREMENTS.md` are what is open,
`intel/` is the ingest of the documents above, and `codebase/` is a map of the
tree written by `/gsd-map-codebase`.

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
  The rule reaches the tooling too: an installed third-party skill carrying
  executable scripts runs on the machine that builds and measures this solver,
  was never pinned by checksum and appears in no manifest. Read its `scripts/`
  before it runs once — `skill-authoring` carries the procedure.
- **Work units are the unit of cost, and every run also reports its time.**
  The units make regressions detectable across machines and go in the
  record; the seconds say whether the units bought anything, are printed on
  every run, and **never enter `bench/results/*.txt` or a baseline** — a
  baseline that changes every run cannot detect a regression.
- **A change is judged on three things** (D45): solution digests for
  correctness, work units for determinism, and a same-instance time ratio to
  catch what the other two cannot see.
- **A plan whose deliverable is a verdict commits its raw readings.** The
  seconds still never enter `bench/results/*.txt` or a baseline — that rule is
  unchanged. They go in a separate per-phase directory,
  `.planning/phases/<phase>/<plan>-MEASUREMENT/`, holding the timing logs, any
  callgrind annotations and the analysis script, so the verdict is re-derivable
  by someone who does not trust the summary. Phase 1 is why: 27 of its 29
  must-haves were re-derived from the repository and the two that could not be
  were the measurement that decided the phase. The audit that found its
  negative control only worked because the logs happened to still be in a
  scratchpad, and by the time anyone looked again they were gone.
## The skills, and the moment each one is for

A skill nobody routes to is one you are relying on the description to summon.
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
question — how the C does it, once the algorithm is settled. Reaching for
`c-perf` first is the standard way to spend a week buying 3%.

And the three subagents, each for work better done in a context that is not
this one:

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
- **Finish every source edit before launching a campaign.** A run takes tens
  of minutes and is only valid for the tree that produced it.
- **Never rewrite a baseline as a side effect** of running a gate.
- Commits are at Claude's discretion; **pushes always need explicit
  approval**.
