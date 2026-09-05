# 02-184 — two rules for a stale measurement record, one kept and one refused

D269 left one item open:

> no tool notices when a measurement directory's README is older than the
> `.txt` it cites, which is exactly the trap D264 fell into

On 2026-09-05 a second, closer trap happened. A session picked `02-181` as
the next free id by reading the tail of `ls bench/measurements/`, which sorts
`02-90` after `02-181`. `02-181` was D276's. The Write tool created into it
without an error and overwrote D276's `README.md`, and it surfaced two hours
later as a `CHANGELOG.md` line citing one directory for two decisions.

So two rules were measured before either was added to
`tools/record-check.py`. A predicate here is wrong about a third of the time,
and the firing population is what says which third.

## What is here

| file | what it does |
|---|---|
| `measure-readme-rules.py` / `readme-rules.txt` | both rules run over all 179 tracked measurement directories, with the firing population printed in full |
| `canary-readme-id.sh` / `.txt` | the kept rule watched catching the case it exists for, and going quiet again |

## Rule A — a README heading must name its own directory. KEPT.

Judged on the 123 directories whose first heading already uses the
`# 02-NNN` form. The other 56 predate the convention and are skipped rather
than renamed, because renumbering breaks live citations.

**It fires 0 times.** That is the whole point of the canary beside it: a
predicate that finds nothing is worth nothing until it has been shown able to
find something. `canary-readme-id.sh` rewrites one README's heading to name
`02-999`, requires `record-check` to go red, requires the message to name
both the wrong id and the real directory — an arm-1 failure from anything
else in the record would look identical — and then requires it to go green
again.

## Rule B — a reading committed after the README that names it. REFUSED.

227 README/`.txt` pairs. The raw form fires 5 times. Two of those are
header-only: commit `499c142` added an `# objects: dev` line to two readings
under `02-179` without moving a number, and a README does not go stale
because a header was annotated. Filtering to commits that move a
non-comment line leaves **3**.

Each of the three was read:

| | what the later commit did | stale? |
|---|---|---|
| `02-134/assert-control.txt` | the reading was re-taken; the only data line that differs is the shell's `789 Aborted` becoming `861 Aborted` — a process id | **no** |
| `02-179/proofs-netlib.txt` | re-run against the release objects; the seconds column moved and every verdict stayed | **no** — the README quotes verdict counts, and says separately that its seconds are not the gate's |
| `02-73/fold-and-chain.txt` | two rows went from `presolve=INFEASIBLE` to `presolve=not refused solve=optimal`, a real change of result | **no** — the README's table already carries both columns; it was written to describe the change |

**Nothing in the population is a defect, so the rule would ship red on three
directories it is wrong about.** A rule that has to be argued with on every
build is worse than no rule: the three standing `M3` warnings here are
already read as noise.

The refusal is in `bench/refusals.txt` with what would reopen it. In short:
the rule cannot separate a re-take from a stale README without reading the
numbers, and a process id in a captured line is enough to defeat the
`non-comment line` refinement. What would reopen it is a reading format
whose data lines are separable from its noise — a `#`-prefixed provenance
block and nothing else outside it — at which point the comparison is on the
data alone and the three false positives all disappear.

## What Rule A does not cover

It does not catch a second decision writing into another's directory while
that directory's README is left alone, which is half of what happened on
2026-09-05. Nothing in the files says which decision owns a directory; the
heading is the closest thing, and it only helps once the heading is wrong.
The check that would catch it is `git status --short bench/measurements/`
showing an `M` where only `??` belongs, and that is a habit rather than a
rule — it is written into the working notes, not into `record-check`.
