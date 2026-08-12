---
name: jaos-record
description: How a landed change gets written down in this repository — which of the four documents takes which part of it, what a decision entry has to contain to be worth having, the two-line rule for the changelog, and the commit voice. Load after a change is measured and before writing any of it up, and load it again before claiming a document is up to date.
---

# Writing a change down

The design of this project is written, not reconstructed from the code. That
only works if every landed change lands in the documents too, and in the
right one. A change recorded in the wrong place is worse than one not
recorded: it is found later, by someone who then trusts it.

## Which document takes which part

Four documents, and the split is by *kind of statement*, never by topic.

| the statement | goes in |
|---|---|
| this feature exists / is partial / is missing | `SPECS.md` |
| this is open, and here is where it sits in the order | `PLAN.md` |
| this is closed, and here is the measurement that closed it | `DECISIONS.md` |
| this changed, and this is what it cost | `CHANGELOG.md` |

The two mistakes that actually happen:

- **Reasoning in the changelog.** The changelog is a changelog. Two to six
  lines, what changed and what it cost, and a `(Dn)` pointing at where the
  argument lives. If an entry is arguing, it is in the wrong file.
- **A closed question left in `PLAN.md`.** When a decision closes, the plan
  loses the open item and gains either nothing or a row in **Settled — do not
  re-derive**. A plan that still lists what is finished cannot be read for
  what is next, which is the only thing it is for.

A refusal is a closed decision, not an absence. "Measured and rejected" is
the most valuable kind of entry this project has — it is what stops the same
week being spent twice — and it gets a `DECISIONS.md` entry *and* a row in
the settled table.

## What a decision entry has to contain

The heading is the decision itself, not the topic. `## D63 — The gap's
iterations are weight restarts, and the threshold that causes them is what
keeps the answers right`, not `## D63 — Pricing weights`. Read the index and
you have the argument; open the entry for the numbers.

Then, in the body, all four of these or it is not finished:

1. **The question, as it was actually asked**, including what was expected.
2. **The measurement.** Per instance, with the instances named. A geometric
   mean of per-instance ratios, never a sum over a set — two instances are
   74% of the standard set's total (D46), so a total is a statement about
   those two.
3. **What was refuted.** Anything tried that did not work, and why it did
   not, in enough detail that nobody re-tries it. This is the part that pays.
4. **What is left open**, handed explicitly to `PLAN.md`.

Add the index line at the top of the file, with the anchor: headings are
linked from the index and cited from source comments, so **a heading that
changes breaks live references** — 79 citations across 29 decisions today.
Number the entry one past the last; never renumber.

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

Every entry states what it cost, and "nothing" is a result: *all 139 digests
unmoved* is a claim, it is checkable, and it is the strongest sentence
available about a change that was meant to be invisible. An entry that
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

## Before claiming a document is up to date

Check the *status tables*, not just the prose. `SPECS.md` carries a status
per feature and a bars-it-has-to-clear table with measured figures in it; a
change that moves a measured figure and leaves the table alone has made the
table wrong rather than stale. The same goes for `PLAN.md`'s ranked list in
phase 6 — re-attribute after every entry that lands, because a ranking three
changes stale describes a solver that no longer exists.
