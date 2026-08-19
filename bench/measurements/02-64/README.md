# The record checks itself now, because a broken index link survived a commit

Taken 2026-08-19. Not a decision: a script and what it says today.

## Why it exists

D153's entry was appended with a heredoc that ran in the wrong working
directory. The body landed in `bench/measurements/02-62/DECISIONS.md`, a file
nobody meant to create, while the real `DECISIONS.md` gained an index line
pointing at a heading that did not exist. **That survived a commit**, and it
was found by accident an hour later, not by any check.

`check-record.sh` is the check that would have caught it.

## What it checks

| # | check | why |
|---|---|---|
| 1 | every index entry has a heading with the same number | the break above |
| 2 | every heading has an index entry | its mirror — an entry nobody can find from the index |
| 3 | an anchor names its own decision number | catches a copy-pasted index line |
| 4 | numbers contiguous and unique | `DECISIONS.md` is append-only and never renumbered |
| 5 | every `bench/measurements/<id>/` is cited somewhere | a record nothing points at is a record nobody reads |

## What it says at this commit

```
index entries: 153, without a heading: none
headings: 153, without an index entry: none
anchors pointing at another number: none
highest D153, duplicates: none, gaps: none
directories: 61, uncited: none
RECORD CONSISTENT
```

## One thing it deliberately does NOT check

**Anchor slugs.** The file carries two conventions: entries up to about D148
collapse the em dash to a single hyphen, later ones keep the two hyphens
GitHub actually generates from `D1 — Language`. The split predates this
script, it is cosmetic, and rewriting 150 anchors to settle it is not worth a
commit. Checking by decision number is robust to both and catches the failure
that actually happened.

The first version of this script did compare slugs, with a hand-written
approximation of GitHub's rule, and reported **106 of 153 headings as
orphaned**. That number is its own refutation: a check that says most of a
long-standing file is broken is a broken check. Recorded because the same
mistake is easy to make again.

## Running it

```
bash bench/measurements/02-64/check-record.sh
```

Read-only. Exits non-zero on a break, so it can go in front of a commit that
touches the record.
