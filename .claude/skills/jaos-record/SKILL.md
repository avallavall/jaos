---
name: jaos-record
description: How a landed change gets written down in this repository — which of the five places takes which part of it, what a decision entry has to contain to be worth having, the two-to-six-line rule for the changelog, and the commit voice. Load after a change is measured and before writing any of it up, and load it again before claiming a document is up to date.
---

# Writing a change down

The design of this project is written, not reconstructed from the code. That
only works if every landed change lands in the documents too, and in the
right one. A change recorded in the wrong place is worse than one not
recorded: it is found later, by someone who then trusts it.

## Which document takes which part

Five places, and the split is by *kind of statement*, never by topic.

| the statement | goes in |
|---|---|
| this feature exists / is partial / is missing | `SPECS.md` — and a `missing` feature gets an `absent <where> <regex>` line in `docs/claims.txt`, so `record-check` fails the day it is built |
| this is open, and here is where it sits in the order | `TODO.md` |
| this is closed, and here is the measurement that closed it | `DECISIONS.md` |
| this changed, and this is what it cost | `CHANGELOG.md` |
| this constant has this value, and here is its sweep on both sides | `docs/tolerances.md`, beside the constant in the source; `record-check` fails when the two disagree |
| this number was measured | `bench/measurements/<id>/`, the readings that decided the verdict, re-derivable |
| this was refused, and here is what would reopen it | `TODO.md`'s refusals table AND a line in `bench/refusals.txt` (`D<n> / script-or-MANUAL / reopens when`) |

`SPECS.md` is present tense only and `record-check` enforces its vocabulary:
a status is one of `done`, `partial`, `missing`, `measured and refused`, `out
of scope`, `pass`, `green at HEAD`, `not started`; a `partial` row says what
is **Missing:**; history words (`used to`, `before D`, `re-taken`, `was
closed`) fail, because history lives in `DECISIONS.md`.

**`PLAN.md` is archived** at `docs/archive/PLAN.md` since 2026-08-12. Cite its
sections when a source comment already does — the redirect table keeps those
alive — but never write open work into it. Open work goes to
`TODO.md`.

**A measured number has an owner, and everything else cites rather than
restates it.** This is the rule the project learned the expensive way: an audit
found the certification count wrong in five documents at once, because a
derived total (`38 + 16 = 54`) was copied as a literal and one addend moved
underneath it. The counts that never drifted are the ones with a single stated
owner and no arithmetic on top — the instance sizes 94/16/29, owned by
`bench/README.md` — and the ones that live in executable form, like
`WORK_PINNED` and the work ceiling in `tests/test_simplex.c`, which came
through six-for-six across every document. Prefer a pointer to a figure.
Never restate a *derived* total; give the addends their owner and let the
reader add.

The two mistakes that actually happen:

- **Reasoning in the changelog.** The changelog is a changelog. Two to six
  lines, what changed and what it cost, and a `(Dn)` pointing at where the
  argument lives. If an entry is arguing, it is in the wrong file.
- **A closed question left open in `TODO.md`.** When a decision
  closes, the roadmap loses the open item and gains either nothing or a line
  recording what closed it. A roadmap that still lists what is finished cannot
  be read for what is next, which is the only thing it is for.

A refusal is a closed decision, not an absence. "Measured and rejected" is
the most valuable kind of entry this project has — it is what stops the same
week being spent twice — and it gets a `DECISIONS.md` entry, a row in
`TODO.md`'s refusals table, and a line in `bench/refusals.txt`. **A refusal
without a reopen condition is not finished.** D36, D76 and D156 had none until
D206. A condition a script can test gets the script, in
`bench/measurements/<id>/`, exiting 0 when the refusal holds, 1 when it
flipped, 2 when it could not run; `make refusals` runs them.

## What a decision entry has to contain

The heading is the decision itself, not the topic. `## D63 — The gap's
iterations are weight restarts, and the threshold that causes them is what
keeps the answers right`, not `## D63 — Pricing weights`. Read the index and
you have the argument; open the entry for the numbers.

Then, in the body, all four of these or it is not finished:

1. **The question, as it was actually asked**, including what was expected.
2. **The measurement.** Per instance, with the instances named. A geometric
   mean of per-instance ratios, never a sum over a set — a total is a statement about whichever instances carry it (D46). **Run**
   `geomean.py`; do not restate its number.
3. **What was refuted.** Anything tried that did not work, and why it did
   not, in enough detail that nobody re-tries it. This is the part that pays.
4. **What is left open**, handed explicitly to `TODO.md`.

Add the index line at the top of the file, with the anchor: headings are
linked from the index and cited from source comments, so **a heading that
changes breaks live references** — several hundred citations across most of
the entries. Do not restate that count here: three separate audits counting it
got three different answers, because it changes on every commit and the scope
("which directories?") is never stated the same way twice. If you need the
number, measure it; do not inherit it. Number the entry one past the last;
never renumber.

## What the changelog entry looks like

Under `## [Unreleased]`, in `### Added` / `### Changed` / `### Fixed`.
Newest first. Two to six lines. The shape that works:

> `jaos_set_coefficient`: one matrix entry can be changed, created or
> removed. Three operations under one name, because the stored matrix keeps
> its columns ascending by row index with no duplicates and no explicit
> zeros. Unlike a bound or a cost it invalidates the row-wise mirror and the
> scaling, both of which are computed from the matrix (D67).

What it *does*, the one thing a reader would get wrong, the cost, the
pointer. Not the reasoning, not the alternatives, not the measurement table.

## The cost belongs in the entry, and so does the null result

Every entry states what it cost, and "nothing" is a result: *110 solution
digests and 29 infeasibility verdicts unmoved, over 139 instances* is a claim,
it is checkable, and it is the strongest sentence available about a change that
was meant to be invisible. Say it that way and not as "all 139 digests" — 139
is the **instance** count; only the 94 standard and 16 Kennington instances
produce a solution digest, and the 29 infeasible ones produce a refusal verdict
(`expected=infeasible verdict=ok det=ok`). The loose form is not merely
imprecise: it is what a reader checks against `grep -c digest=` and finds
missing. An entry that
reports only what was gained is half an entry.

## The commit

`type: a sentence in the project's voice`, lower case, no full stop. Types
in use: `feat` `fix` `perf` `docs` `bench` `build` `diag`.

The sentence says what is now true, usually with a second clause after a
comma carrying the thing that was surprising:

```
perf: the column stops being copied twice to meet the pivot's multipliers
fix: the comparison measured last night's solver and said nothing about it
diag: pilot87's iteration guard is not a cycle, and Bland's rule is why
feat: the checker certifies suboptimality, and it never needed the factorization
docs: the re-entry's clean-up does need to borrow, and pilot87 is the price
```

Not `perf: optimize LU column handling`. The subject line is read years later
by someone scanning for when a behaviour changed, and a category is not an
answer to that.

**Commits are at Claude's discretion here; pushes always need explicit
approval.** Land the documents in the same commit as the code they describe,
or in one immediately after — a commit that changes behaviour and leaves the
documents for later is how the two get out of step.

## A comment is a contract

A source comment states what holds at that line and cites `(Dn)`. The
argument lives in `DECISIONS.md` and is never repeated in the source: a
comment that argues is in the wrong file, the same rule as reasoning in the
changelog. An invariant a comment states is an assert or a test, not a
sentence; D201 is the receipt (`s->col` had five writers and a correct,
prominent comment, and D30 was caused by violating it). `record-check` scans
`.claude/**/*.md` too, so a D-number cited in a skill must exist.

## Before claiming a document is up to date

**Run** `make record-check` first. It settles the mechanical half: every
D-number cited exists, index anchors resolve, `docs/tolerances.md` matches
the source, `SPECS.md`'s labels and history words, `docs/claims.txt`,
measurement-directory citations, evidence-script anchors. What is left for
the eye: whether a sentence is right, the section 8 bars, `TODO.md`'s
ordering.

Then the reopen conditions. Grep `bench/refusals.txt` and `TODO.md`'s
refusals table for the mechanism the change touched; if it touched pricing,
the re-entry, presolve's families or the LU kernels, run `make refusals`. A
met condition reopens the item in `TODO.md` in the same commit. D184's
condition was met on 2026-08-25 and nothing looked for a day (D206).

Check the *status tables*, not just the prose. `SPECS.md` carries a status
per feature and a bars-it-has-to-clear table with measured figures in it; a
change that moves a measured figure and leaves the table alone has made the
table wrong rather than stale. The same goes for `TODO.md`'s phase
ordering and its open questions — re-attribute after every entry that lands,
because a ranking three changes stale describes a solver that no longer exists.
